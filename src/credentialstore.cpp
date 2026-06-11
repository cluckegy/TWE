#include "credentialstore.h"

#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QSettings>
#include <QStandardPaths>
#include <QJsonDocument>
#include <QJsonObject>
#include <windows.h>
#include <wincrypt.h>

namespace {
QString g_lastError;

QString credentialFilePath()
{
    const QString directory = QStandardPaths::writableLocation(
        QStandardPaths::AppLocalDataLocation);
    return QDir(directory).filePath(QStringLiteral("credentials.json"));
}

QJsonObject readCredentialFile()
{
    QFile file(credentialFilePath());
    if (!file.open(QIODevice::ReadOnly))
        return {};
    return QJsonDocument::fromJson(file.readAll()).object();
}

bool writeCredentialFile(const QJsonObject &object)
{
    const QFileInfo info(credentialFilePath());
    if (!QDir().mkpath(info.absolutePath())) {
        g_lastError = QStringLiteral("تعذر إنشاء مجلد حفظ بيانات TWE.");
        return false;
    }

    QSaveFile file(info.absoluteFilePath());
    if (!file.open(QIODevice::WriteOnly)) {
        g_lastError = QStringLiteral("تعذر فتح ملف بيانات TWE للكتابة.");
        return false;
    }
    file.write(QJsonDocument(object).toJson(QJsonDocument::Compact));
    if (!file.commit()) {
        g_lastError = QStringLiteral("تعذر حفظ ملف بيانات TWE.");
        return false;
    }
    return true;
}

QByteArray protect(const QByteArray &plainText)
{
    DATA_BLOB input{};
    input.pbData = reinterpret_cast<BYTE *>(const_cast<char *>(plainText.constData()));
    input.cbData = static_cast<DWORD>(plainText.size());

    DATA_BLOB output{};
    if (!CryptProtectData(&input, L"TWE My WE password", nullptr, nullptr, nullptr,
                          CRYPTPROTECT_UI_FORBIDDEN, &output)) {
        g_lastError = QStringLiteral("فشل تشفير البيانات بواسطة Windows (خطأ %1).")
                          .arg(GetLastError());
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
        g_lastError = QStringLiteral("تعذر فك تشفير البيانات المحفوظة (خطأ %1).")
                          .arg(GetLastError());
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
    g_lastError.clear();
    QSettings settings;
    if (password.isEmpty()) {
        settings.remove(QStringLiteral("we/protectedPassword"));
        settings.remove(QStringLiteral("we/password"));
        QJsonObject fileData = readCredentialFile();
        fileData.remove(QStringLiteral("password"));
        writeCredentialFile(fileData);
        return true;
    }

    const QByteArray encrypted = protect(password.toUtf8());
    if (encrypted.isEmpty())
        return false;

    settings.setValue(QStringLiteral("we/protectedPassword"), encrypted.toBase64());
    settings.remove(QStringLiteral("we/password"));
    settings.sync();
    if (settings.status() == QSettings::NoError)
        return true;

    QJsonObject fileData = readCredentialFile();
    fileData.insert(QStringLiteral("password"),
                    QString::fromLatin1(encrypted.toBase64()));
    return writeCredentialFile(fileData);
}

QString CredentialStore::loadPassword()
{
    g_lastError.clear();
    QSettings settings;
    const QByteArray encoded = settings.value(QStringLiteral("we/protectedPassword")).toByteArray();
    if (!encoded.isEmpty())
        return QString::fromUtf8(unprotect(QByteArray::fromBase64(encoded)));

    const QByteArray fileEncoded = readCredentialFile()
        .value(QStringLiteral("password")).toString().toLatin1();
    if (!fileEncoded.isEmpty())
        return QString::fromUtf8(unprotect(QByteArray::fromBase64(fileEncoded)));

    // Migrate settings created by early development builds.
    const QString legacy = settings.value(QStringLiteral("we/password")).toString();
    if (!legacy.isEmpty())
        savePassword(legacy);
    return legacy;
}

bool CredentialStore::saveSecret(const QString &name, const QByteArray &value)
{
    g_lastError.clear();
    QSettings settings;
    const QString key = QStringLiteral("we/secrets/%1").arg(name);
    if (value.isEmpty()) {
        settings.remove(key);
        QJsonObject fileData = readCredentialFile();
        QJsonObject secrets = fileData.value(QStringLiteral("secrets")).toObject();
        secrets.remove(name);
        fileData.insert(QStringLiteral("secrets"), secrets);
        writeCredentialFile(fileData);
        return true;
    }

    const QByteArray encrypted = protect(value);
    if (encrypted.isEmpty())
        return false;
    settings.setValue(key, encrypted.toBase64());
    settings.sync();
    if (settings.status() == QSettings::NoError)
        return true;

    QJsonObject fileData = readCredentialFile();
    QJsonObject secrets = fileData.value(QStringLiteral("secrets")).toObject();
    secrets.insert(name, QString::fromLatin1(encrypted.toBase64()));
    fileData.insert(QStringLiteral("secrets"), secrets);
    return writeCredentialFile(fileData);
}

QByteArray CredentialStore::loadSecret(const QString &name)
{
    g_lastError.clear();
    QSettings settings;
    const QByteArray encoded = settings.value(
        QStringLiteral("we/secrets/%1").arg(name)).toByteArray();
    if (!encoded.isEmpty())
        return unprotect(QByteArray::fromBase64(encoded));

    const QByteArray fileEncoded = readCredentialFile()
        .value(QStringLiteral("secrets")).toObject()
        .value(name).toString().toLatin1();
    return fileEncoded.isEmpty()
        ? QByteArray()
        : unprotect(QByteArray::fromBase64(fileEncoded));
}

QString CredentialStore::lastError()
{
    return g_lastError;
}
