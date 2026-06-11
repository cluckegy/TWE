#include "mainwindow.h"

#include <QApplication>
#include <QFont>
#include <QFontDatabase>
#include <QIcon>
#include <QLocalServer>
#include <QLocalSocket>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("TWE"));
    app.setOrganizationName(QStringLiteral("TWE"));
    app.setLayoutDirection(Qt::RightToLeft);
    app.setQuitOnLastWindowClosed(false);
    app.setWindowIcon(QIcon(QStringLiteral(":/src/resources/twe-logo.png")));

    // Load FontAwesome icons
    QFontDatabase::addApplicationFont(QStringLiteral(":/src/resources/fa-solid-900.ttf"));

    QFont font(QStringLiteral("Segoe UI Variable Text"), 10);
    if (!QFontDatabase().families().contains(font.family()))
        font.setFamily(QStringLiteral("Segoe UI"));
    app.setFont(font);

    const QString serverName = QStringLiteral("TWE-SingleInstance");
    QLocalSocket activationSocket;
    activationSocket.connectToServer(serverName);
    if (activationSocket.waitForConnected(250)) {
        activationSocket.write("activate");
        activationSocket.flush();
        activationSocket.waitForBytesWritten(250);
        return 0;
    }

    QLocalServer::removeServer(serverName);
    QLocalServer instanceServer;
    if (!instanceServer.listen(serverName))
        return 1;

    MainWindow window;
    QObject::connect(&instanceServer, &QLocalServer::newConnection, &window,
                     [&instanceServer, &window]() {
        while (QLocalSocket *socket = instanceServer.nextPendingConnection()) {
            socket->waitForReadyRead(100);
            socket->deleteLater();
        }
        window.showNormal();
        window.raise();
        window.activateWindow();
    });
    window.show();
    return app.exec();
}
