#pragma once

#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QObject>

struct QuotaInfo
{
    QString customerName;
    QString offerName;
    double remaining = 0.0;
    double total = 0.0;
    double usagePercentage = 0.0;
    QDateTime renewalDate;
    QDateTime expiryDate;
};

class WeQuotaService : public QObject
{
    Q_OBJECT

public:
    explicit WeQuotaService(QObject *parent = nullptr);
    void checkQuota(const QString &landlineNumber, const QString &password);
    void refreshSession();
    bool hasActiveSession() const;
    void authenticateWithCaptcha(const QString &captchaCode, const QString &captchaToken);
    void cancelRequest();

signals:
    void quotaReady(const QuotaInfo &info);
    void failed(const QString &message);
    void busyChanged(bool busy);
    void progressChanged(const QString &message);
    void serverResponse(const QString &message, bool success);
    void captchaRequired();
    void captchaReady(const QString &captchaBase64, const QString &captchaToken);


private:
    void post(const QString &path, const QJsonObject &body,
              const std::function<void(const QJsonObject &)> &onSuccess,
              const QByteArray &csrfToken = {});
    void authenticate();
    void requestCaptcha(const QString &landlineNumber);
    void fetchOffering(const QString &subscriberId, const QByteArray &token,
                       const QString &customerName);
    void fetchQuota(const QString &subscriberId, const QString &offerId,
                    const QByteArray &token, const QString &customerName);
    void persistSession();
    void clearSession();
    static QString apiError(const QJsonObject &root, const QString &fallback);

    QNetworkAccessManager m_network;
    QString m_landline;
    QString m_password;
    QString m_accountId;
    QString m_subscriberId;
    QString m_customerName;
    QString m_offerId;
    QByteArray m_sessionToken;

    QString m_deviceId;
    QString m_initTime;
    bool m_busy = false;
    bool m_authenticating = false;
};

Q_DECLARE_METATYPE(QuotaInfo)
