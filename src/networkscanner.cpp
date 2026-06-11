#include "networkscanner.h"

#include <QtConcurrent>
#include <QFutureWatcher>
#include <QHostAddress>
#include <QNetworkInterface>
#include <QSet>
#include <QSysInfo>

#include <winsock2.h>
#include <iphlpapi.h>
#include <ws2tcpip.h>

#include <array>
#include <future>

namespace {
QString wideString(const wchar_t *value)
{
    return value ? QString::fromWCharArray(value) : QString();
}

QString socketAddress(const SOCKET_ADDRESS &address)
{
    if (!address.lpSockaddr || address.lpSockaddr->sa_family != AF_INET)
        return {};
    const auto *ipv4 = reinterpret_cast<const sockaddr_in *>(address.lpSockaddr);
    return QHostAddress(ntohl(ipv4->sin_addr.s_addr)).toString();
}

QString formatMac(const unsigned char *bytes, ULONG length)
{
    QStringList parts;
    for (ULONG i = 0; i < qMin<ULONG>(length, 6); ++i)
        parts.append(QStringLiteral("%1").arg(bytes[i], 2, 16, QLatin1Char('0')).toUpper());
    return parts.join(QLatin1Char('-'));
}

QString formatMac(const ULONG *buffer, ULONG length)
{
    const auto *bytes = reinterpret_cast<const unsigned char *>(buffer);
    QStringList parts;
    for (ULONG i = 0; i < qMin<ULONG>(length, 6); ++i)
        parts.append(QStringLiteral("%1").arg(bytes[i], 2, 16, QLatin1Char('0')).toUpper());
    return parts.join(QLatin1Char('-'));
}

QString adapterType(ULONG type)
{
    if (type == IF_TYPE_IEEE80211)
        return QStringLiteral("Wi-Fi");
    if (type == IF_TYPE_ETHERNET_CSMACD)
        return QStringLiteral("Ethernet");
    return QStringLiteral("Network");
}

QString deviceType(const QString &ip, bool gateway, bool local)
{
    if (gateway)
        return QStringLiteral("راوتر / بوابة");
    if (local)
        return QStringLiteral("هذا الكمبيوتر");
    if (ip.endsWith(QStringLiteral(".254")))
        return QStringLiteral("مقوي شبكة / نقطة وصول");
    return QStringLiteral("جهاز متصل");
}

NetworkDevice probeAddress(const NetworkAdapter &adapter, quint32 hostAddress)
{
    NetworkDevice device;
    const QHostAddress target(hostAddress);
    const QString ip = target.toString();

    if (ip == adapter.ipAddress) {
        device.name = QSysInfo::machineHostName();
        if (device.name.isEmpty())
            device.name = QStringLiteral("جهازك");
        device.ipAddress = ip;
        device.macAddress = adapter.macAddress;
        device.state = QStringLiteral("محلي");
        device.deviceType = deviceType(ip, false, true);
        return device;
    }

    ULONG macBuffer[2] = {};
    ULONG macLength = 6;
    const IPAddr destination = htonl(hostAddress);
    const DWORD result = SendARP(destination, 0, macBuffer, &macLength);
    if (result != NO_ERROR || macLength == 0)
        return {};

    device.name = ip == adapter.gateway ? QStringLiteral("الراوتر") : QStringLiteral("جهاز الشبكة");
    device.ipAddress = ip;
    device.macAddress = formatMac(macBuffer, macLength);
    device.state = QStringLiteral("نشط");
    device.gateway = ip == adapter.gateway;
    device.deviceType = deviceType(ip, device.gateway, false);
    return device;
}
}

NetworkScanner::NetworkScanner(QObject *parent)
    : QObject(parent)
{
    qRegisterMetaType<QList<NetworkDevice>>("QList<NetworkDevice>");
    qRegisterMetaType<NetworkAdapter>("NetworkAdapter");
}

QList<NetworkAdapter> NetworkScanner::adapters() const
{
    ULONG size = 16000;
    QByteArray storage(static_cast<int>(size), 0);
    auto *addresses = reinterpret_cast<IP_ADAPTER_ADDRESSES *>(storage.data());
    const ULONG flags = GAA_FLAG_INCLUDE_GATEWAYS | GAA_FLAG_SKIP_ANYCAST
                        | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER;
    ULONG result = GetAdaptersAddresses(AF_INET, flags, nullptr, addresses, &size);
    if (result == ERROR_BUFFER_OVERFLOW) {
        storage.resize(static_cast<int>(size));
        addresses = reinterpret_cast<IP_ADAPTER_ADDRESSES *>(storage.data());
        result = GetAdaptersAddresses(AF_INET, flags, nullptr, addresses, &size);
    }

    QList<NetworkAdapter> resultList;
    if (result != NO_ERROR)
        return resultList;

    for (auto *current = addresses; current; current = current->Next) {
        if (current->OperStatus != IfOperStatusUp
            || (current->IfType != IF_TYPE_IEEE80211
                && current->IfType != IF_TYPE_ETHERNET_CSMACD)
            || !current->FirstUnicastAddress) {
            continue;
        }

        const QString ip = socketAddress(current->FirstUnicastAddress->Address);
        if (ip.isEmpty() || ip.startsWith(QStringLiteral("169.254.")))
            continue;

        NetworkAdapter adapter;
        adapter.id = QString::fromLatin1(current->AdapterName);
        adapter.name = wideString(current->FriendlyName);
        adapter.type = adapterType(current->IfType);
        adapter.ipAddress = ip;
        adapter.macAddress = formatMac(current->PhysicalAddress,
                                       current->PhysicalAddressLength);
        if (current->FirstGatewayAddress)
            adapter.gateway = socketAddress(current->FirstGatewayAddress->Address);
        resultList.append(adapter);
    }
    return resultList;
}

void NetworkScanner::scan(const NetworkAdapter &adapter)
{
    if (m_busy)
        return;

    const QHostAddress local(adapter.ipAddress);
    const quint32 localValue = local.toIPv4Address();
    if (localValue == 0) {
        emit failed(QStringLiteral("عنوان كارت الشبكة غير صالح."));
        return;
    }

    m_busy = true;
    emit busyChanged(true);

    auto *watcher = new QFutureWatcher<QList<NetworkDevice>>(this);
    connect(watcher, &QFutureWatcher<QList<NetworkDevice>>::finished, this,
            [this, watcher]() {
        const QList<NetworkDevice> devices = watcher->result();
        watcher->deleteLater();
        m_busy = false;
        emit busyChanged(false);
        emit devicesReady(devices);
    });

    watcher->setFuture(QtConcurrent::run([adapter, localValue]() {
        const quint32 network = localValue & 0xFFFFFF00u;
        constexpr int workerCount = 128;
        std::array<std::future<QList<NetworkDevice>>, workerCount> workers;

        for (int worker = 0; worker < workerCount; ++worker) {
            workers[worker] = std::async(std::launch::async, [=]() {
                QList<NetworkDevice> found;
                for (int host = worker + 1; host < 255; host += workerCount) {
                    NetworkDevice device = probeAddress(adapter, network + host);
                    if (!device.ipAddress.isEmpty())
                        found.append(device);
                }
                return found;
            });
        }

        QList<NetworkDevice> devices;
        QSet<QString> seen;
        for (auto &worker : workers) {
            for (const NetworkDevice &device : worker.get()) {
                if (!seen.contains(device.macAddress)) {
                    seen.insert(device.macAddress);
                    devices.append(device);
                }
            }
        }
        std::sort(devices.begin(), devices.end(), [](const NetworkDevice &left,
                                                      const NetworkDevice &right) {
            return QHostAddress(left.ipAddress).toIPv4Address()
                   < QHostAddress(right.ipAddress).toIPv4Address();
        });
        return devices;
    }));
}
