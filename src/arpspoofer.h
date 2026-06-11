#pragma once

#include <QObject>
#include <QMap>
#include <QMutex>
#include <QTimer>
#include <QThread>

// Forward declarations for pcap types (avoids including pcap.h in header)
struct pcap;
typedef pcap pcap_t;

struct DeviceControl
{
    QString ip;
    QString mac;
    int downloadCapKBps = 0;   // 0 = unlimited
    int uploadCapKBps = 0;     // 0 = unlimited
    bool blocked = false;
    bool spoofEnabled = false;

    // Internal byte counters (reset every second)
    qint64 bytesDownThisSecond = 0;
    qint64 bytesUpThisSecond = 0;

    // Live speed (computed each second)
    qint64 currentDownKBps = 0;
    qint64 currentUpKBps = 0;

    // Cumulative totals
    qint64 totalBytesDown = 0;
    qint64 totalBytesUp = 0;
};

class ArpSpoofer : public QObject
{
    Q_OBJECT

public:
    explicit ArpSpoofer(QObject *parent = nullptr);
    ~ArpSpoofer();

    // Initialize with a network adapter name (Npcap device string)
    bool initialize(const QString &adapterName, const QString &localIp,
                    const QString &localMac, const QString &gatewayIp);

    // Discover gateway MAC via ARP request
    bool resolveGatewayMac();

    // Per-device spoofing control
    void startSpoofing(const QString &deviceIp, const QString &deviceMac);
    void stopSpoofing(const QString &deviceIp);
    void stopAll();

    // Speed caps (0 = unlimited)
    void setDownloadCap(const QString &ip, int kbps);
    void setUploadCap(const QString &ip, int kbps);
    void blockDevice(const QString &ip, bool block);

    bool isRunning() const { return m_running; }
    bool isDeviceSpoofed(const QString &ip) const;
    DeviceControl deviceInfo(const QString &ip) const;

signals:
    void speedUpdate(const QString &ip, qint64 downKBps, qint64 upKBps);
    void spoofStateChanged(const QString &ip, bool active);
    void error(const QString &message);
    void initialized();

private:
    // ARP packet building (42-byte raw Ethernet frame)
    QByteArray buildArpPacket(const QByteArray &srcMac, const QByteArray &dstMac,
                              const QByteArray &senderMac, const QByteArray &senderIp,
                              const QByteArray &targetMac, const QByteArray &targetIp,
                              quint16 opcode = 2);

    // Core operations
    void spoof(const QString &victimIp, const QByteArray &victimMac);
    void unspoof(const QString &victimIp, const QByteArray &victimMac);

    // Thread functions
    void redirectorLoop();
    void spoofTimerTick();
    void resetCounters();

    // Helper: parse "AA-BB-CC-DD-EE-FF" to 6-byte array
    static QByteArray parseMac(const QString &mac);
    // Helper: parse "192.168.1.1" to 4-byte array
    static QByteArray parseIp(const QString &ip);
    // Helper: compare byte arrays
    static bool macEquals(const unsigned char *a, const QByteArray &b);
    static bool ipEquals(const unsigned char *a, const QByteArray &b);

    pcap_t *m_pcap = nullptr;
    bool m_running = false;

    // Network info
    QString m_adapterName;
    QString m_localIp;
    QString m_localMac;
    QString m_gatewayIp;
    QString m_gatewayMac;

    // Parsed byte arrays for fast comparison
    QByteArray m_localMacBytes;
    QByteArray m_localIpBytes;
    QByteArray m_gatewayMacBytes;
    QByteArray m_gatewayIpBytes;

    // Spoofed devices
    mutable QMutex m_mutex;
    QMap<QString, DeviceControl> m_devices;

    // Timers
    QTimer *m_spoofTimer = nullptr;   // Re-poison every 3 seconds
    QTimer *m_resetTimer = nullptr;   // Reset byte counters every 1 second

    // Redirector thread
    QThread *m_redirectorThread = nullptr;
    bool m_redirectorRunning = false;
};
