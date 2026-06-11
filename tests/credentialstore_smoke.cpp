#include "../src/credentialstore.h"

#include <QCoreApplication>
#include <QTextStream>

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("TWE-Credential-Test"));
    app.setOrganizationName(QStringLiteral("TWE"));

    QTextStream output(stdout);
    const QString password = QStringLiteral("  test password !@#  ");
    if (!CredentialStore::savePassword(password)) {
        output << "SAVE_FAILED " << CredentialStore::lastError() << Qt::endl;
        return 1;
    }
    if (CredentialStore::loadPassword() != password) {
        output << "PASSWORD_MISMATCH" << Qt::endl;
        return 2;
    }

    const QByteArray secret("session-test");
    if (!CredentialStore::saveSecret(QStringLiteral("smoke"), secret)
        || CredentialStore::loadSecret(QStringLiteral("smoke")) != secret) {
        output << "SECRET_FAILED " << CredentialStore::lastError() << Qt::endl;
        return 3;
    }

    CredentialStore::savePassword({});
    CredentialStore::saveSecret(QStringLiteral("smoke"), {});
    output << "CREDENTIAL_STORE_OK" << Qt::endl;
    return 0;
}
