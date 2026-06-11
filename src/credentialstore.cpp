#include "credentialstore.h"

#include <QByteArray>
#include <QSettings>
#include <windows.h>
#include <wincrypt.h>

namespace {
QByteArray protect(const QByteArray &plainText)
{
    DATA_BLOB input{};
    input.pbData = reinterpret_cast<BYTE *>(const_cast<char *>(plainText.constData()));
    input.cbData = static_cast<DWORD>(plainText.size());

    DATA_BLOB output{};
    if (!CryptProtectData(&input, L"TWE My WE password", nullptr, nullptr, nullptr,
                          CRYPTPROTECT_UI_FORBIDDEN, &output)) {
        return {};
    }

    const QByteArray encrypted(reinterpret_cast<const char *>(output.pbData),
                               static_cast<int>(output.cbData));
    LocalFree(output.pbData);
    return encrypted;
}

QByteArray unprotect(const QByteArray &encrypted)
{
    DATA_BLOB input{};
    input.pbData = reinterpret_cast<BYTE *>(const_cast<char *>(encrypted.constData()));
    input.cbData = static_cast<DWORD>(encrypted.size());

    DATA_BLOB output{};
    if (!CryptUnprotectData(&input, nullptr, nullptr, nullptr, nullptr,
                            CRYPTPROTECT_UI_FORBIDDEN, &output)) {
        return {};
    }

    const QByteArray plainText(reinterpret_cast<const char *>(output.pbData),
                               static_cast<int>(output.cbData));
    LocalFree(output.pbData);
    return plainText;
}
}

bool CredentialStore::savePassword(const QString &password)
{
    QSettings settings;
    if (password.isEmpty()) {
        settings.remove(QStringLiteral("we/protectedPassword"));
        settings.remove(QStringLiteral("we/password"));
        return true;
    }

    const QByteArray encrypted = protect(password.toUtf8());
    if (encrypted.isEmpty())
        return false;

    settings.setValue(QStringLiteral("we/protectedPassword"), encrypted.toBase64());
    settings.remove(QStringLiteral("we/password"));
    settings.sync();
    return settings.status() == QSettings::NoError;
}

QString CredentialStore::loadPassword()
{
    QSettings settings;
    const QByteArray encoded = settings.value(QStringLiteral("we/protectedPassword")).toByteArray();
    if (!encoded.isEmpty())
        return QString::fromUtf8(unprotect(QByteArray::fromBase64(encoded)));

    // Migrate settings created by early development builds.
    const QString legacy = settings.value(QStringLiteral("we/password")).toString();
    if (!legacy.isEmpty())
        savePassword(legacy);
    return legacy;
}

bool CredentialStore::saveSecret(const QString &name, const QByteArray &value)
{
    QSettings settings;
    const QString key = QStringLiteral("we/secrets/%1").arg(name);
    if (value.isEmpty()) {
        settings.remove(key);
        return true;
    }

    const QByteArray encrypted = protect(value);
    if (encrypted.isEmpty())
        return false;
    settings.setValue(key, encrypted.toBase64());
    settings.sync();
    return settings.status() == QSettings::NoError;
}

QByteArray CredentialStore::loadSecret(const QString &name)
{
    QSettings settings;
    const QByteArray encoded = settings.value(
        QStringLiteral("we/secrets/%1").arg(name)).toByteArray();
    return encoded.isEmpty() ? QByteArray()
                             : unprotect(QByteArray::fromBase64(encoded));
}
