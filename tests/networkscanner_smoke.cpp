#include "../src/networkscanner.h"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QTextStream>
#include <QTimer>

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QTextStream out(stdout);
    NetworkScanner scanner;
    const QList<NetworkAdapter> adapters = scanner.adapters();
    if (adapters.isEmpty()) {
        out << "NO_ADAPTERS\n";
        return 2;
    }

    NetworkAdapter selected = adapters.first();
    for (const NetworkAdapter &adapter : adapters) {
        if (!adapter.gateway.isEmpty()) {
            selected = adapter;
            break;
        }
    }

    out << "ADAPTER " << selected.name << " IP=" << selected.ipAddress
        << " GATEWAY=" << selected.gateway << Qt::endl;

    QElapsedTimer timer;
    timer.start();
    QObject::connect(&scanner, &NetworkScanner::devicesReady, &app,
                     [&](const QList<NetworkDevice> &devices) {
        out << "DEVICES " << devices.size() << " ELAPSED_MS=" << timer.elapsed()
            << Qt::endl;
        for (const NetworkDevice &device : devices)
            out << device.ipAddress << ' ' << device.macAddress << ' '
                << device.deviceType << Qt::endl;
        app.exit(devices.isEmpty() ? 3 : 0);
    });
    QObject::connect(&scanner, &NetworkScanner::failed, &app,
                     [&](const QString &message) {
        out << "ERROR " << message << Qt::endl;
        app.exit(4);
    });
    QTimer::singleShot(30000, &app, [&]() {
        out << "TIMEOUT\n";
        app.exit(5);
    });

    scanner.scan(selected);
    return app.exec();
}
