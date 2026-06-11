#pragma once

#include <QString>

class CredentialStore
{
public:
    static bool savePassword(const QString &password);
    static QString loadPassword();
    static bool saveSecret(const QString &name, const QByteArray &value);
    static QByteArray loadSecret(const QString &name);
    static QString lastError();
};
