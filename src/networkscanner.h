#pragma once

#include <QObject>

struct NetworkAdapter
{
    QString id;
    QString name;
    QString type;
    QString ipAddress;
    QString gateway;
    QString macAddress;
};

struct NetworkDevice
{
    QString name;
    QString ipAddress;
    QString macAddress;
    QString state;
    QString deviceType;
    bool gateway = false;
};

class NetworkScanner : public QObject
{
    Q_OBJECT

public:
    explicit NetworkScanner(QObject *parent = nullptr);
    QList<NetworkAdapter> adapters() const;
    void scan(const NetworkAdapter &adapter);

signals:
    void devicesReady(const QList<NetworkDevice> &devices);
    void failed(const QString &message);
    void busyChanged(bool busy);

private:
    bool m_busy = false;
};

Q_DECLARE_METATYPE(NetworkDevice)
Q_DECLARE_METATYPE(QList<NetworkDevice>)
Q_DECLARE_METATYPE(NetworkAdapter)
