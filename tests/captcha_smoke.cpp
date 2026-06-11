#include "../src/wequotaservice.h"

#include <QCoreApplication>
#include <QTextStream>
#include <QTimer>

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QTextStream out(stdout);
    WeQuotaService service;

    QObject::connect(&service, &WeQuotaService::captchaReady, &app,
                     [&](const QString &image, const QString &token) {
        out << "CAPTCHA_READY image=" << image.size()
            << " token=" << token.size() << Qt::endl;
        app.exit(image.isEmpty() || token.isEmpty() ? 2 : 0);
    });
    QObject::connect(&service, &WeQuotaService::failed, &app,
                     [&](const QString &message) {
        out << "FAILED " << message << Qt::endl;
        app.exit(3);
    });
    QTimer::singleShot(30000, &app, [&]() {
        out << "TIMEOUT\n";
        app.exit(4);
    });

    service.checkQuota(QStringLiteral("0404304482"), QStringLiteral("test-password"));
    return app.exec();
}
