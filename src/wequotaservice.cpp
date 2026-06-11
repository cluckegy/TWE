#include "wequotaservice.h"
#include "credentialstore.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QDateTime>
#include <QNetworkCookieJar>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSettings>
#include <QSslSocket>
#include <QUuid>

namespace {
const QUrl kApiBase(QStringLiteral("https://my.te.eg"));
const QUrl kCaptchaUrl(QStringLiteral("https://captcha.te.eg/api/Captcha/GenerateCaptcha"));
}

WeQuotaService::WeQuotaService(QObject *parent)
    : QObject(parent)
{
    qRegisterMetaType<QuotaInfo>("QuotaInfo");
    m_network.setCookieJar(new QNetworkCookieJar(&m_network));
    QSettings settings;
    m_deviceId = settings.value(QStringLiteral("we/deviceId")).toString();
    if (m_deviceId.isEmpty()) {
        m_deviceId = QUuid::createUuid().toString(QUuid::Id128);
        settings.setValue(QStringLiteral("we/deviceId"), m_deviceId);
    }
    m_initTime = settings.value(QStringLiteral("we/initTime")).toString();
    if (m_initTime.isEmpty()) {
        m_initTime = QString::number(QDateTime::currentMSecsSinceEpoch());
        settings.setValue(QStringLiteral("we/initTime"), m_initTime);
    }

    const QByteArray storedSession = CredentialStore::loadSecret(
        QStringLiteral("we-session"));
    const QJsonObject session = QJsonDocument::fromJson(storedSession).object();
    const qint64 savedAt = session.value(QStringLiteral("savedAt")).toVariant().toLongLong();
    const bool recent = savedAt > 0
        && QDateTime::currentMSecsSinceEpoch() - savedAt < 24LL * 60 * 60 * 1000;
    if (recent) {
        m_landline = session.value(QStringLiteral("landline")).toString();
        m_accountId = session.value(QStringLiteral("accountId")).toString();
        m_subscriberId = session.value(QStringLiteral("subscriberId")).toString();
        m_customerName = session.value(QStringLiteral("customerName")).toString();
        m_offerId = session.value(QStringLiteral("offerId")).toString();
        m_sessionToken = QByteArray::fromBase64(
            session.value(QStringLiteral("token")).toString().toLatin1());
    } else {
        CredentialStore::saveSecret(QStringLiteral("we-session"), {});
    }
}

void WeQuotaService::checkQuota(const QString &landlineNumber, const QString &password)
{
    if (m_busy)
        return;

    if (!QSslSocket::supportsSsl()) {
        const QString detail = QStringLiteral(
            "دعم HTTPS غير متاح. تأكد من وجود مكتبات OpenSSL بجوار TWE.exe.");
        emit serverResponse(detail, false);
        emit failed(detail);
        return;
    }

    const QString number = landlineNumber.trimmed();
    if (number.size() != 10 || !number.startsWith(QLatin1Char('0'))) {
        emit failed(QStringLiteral("رقم الأرضي يجب أن يتكون من 10 أرقام ويبدأ بـ 0."));
        return;
    }
    for (const QChar ch : number) {
        if (!ch.isDigit()) {
            emit failed(QStringLiteral("رقم الأرضي يجب أن يحتوي على أرقام فقط."));
            return;
        }
    }
    if (password.isEmpty()) {
        emit failed(QStringLiteral("اكتب كلمة مرور حساب WE."));
        return;
    }

    const QString accountId = QStringLiteral("FBB") + number.mid(1);
    clearSession();
    m_landline = number;
    m_password = password;
    m_accountId = accountId;
    m_busy = true;
    m_authenticating = true;
    emit busyChanged(true);
    emit progressChanged(QStringLiteral("جارٍ التحقق من متطلبات تسجيل الدخول..."));
    requestCaptcha(m_landline);
}

bool WeQuotaService::hasActiveSession() const
{
    return !m_subscriberId.isEmpty() && !m_offerId.isEmpty()
        && !m_sessionToken.isEmpty();
}

void WeQuotaService::refreshSession()
{
    if (m_busy)
        return;
    if (!hasActiveSession()) {
        emit failed(QStringLiteral("الجلسة غير متاحة. سجّل الدخول مرة واحدة لتفعيل التحديث."));
        return;
    }

    m_busy = true;
    m_authenticating = false;
    emit busyChanged(true);
    emit progressChanged(QStringLiteral("جارٍ تحديث الاستهلاك من الجلسة الحالية..."));
    fetchQuota(m_subscriberId, m_offerId, m_sessionToken, m_customerName);
}

void WeQuotaService::authenticate()
{
    m_authenticating = true;
    emit progressChanged(QStringLiteral("جارٍ التحقق من رقم الأرضي وكلمة المرور..."));
    QJsonObject body{
        {QStringLiteral("acctId"), m_accountId},
        {QStringLiteral("appLocale"), QStringLiteral("en-US")},
        {QStringLiteral("password"), m_password}
    };

    post(QStringLiteral("/echannel/service/besapp/base/rest/busiservice/v1/auth/userAuthenticate"),
         body, [this](const QJsonObject &root) {
        const QJsonObject responseBody = root.value(QStringLiteral("body")).toObject();
        const QJsonObject customer = responseBody.value(QStringLiteral("customer")).toObject();
        const QJsonObject subscriber = responseBody.value(QStringLiteral("subscriber")).toObject();
        const QString token = responseBody.value(QStringLiteral("token")).toString();
        const QString subscriberId = subscriber.value(QStringLiteral("subscriberId")).toVariant().toString();
        const QString customerName = customer.value(QStringLiteral("custName")).toString();

        if (token.isEmpty() || subscriberId.isEmpty()) {
            m_authenticating = false;
            m_busy = false;
            emit busyChanged(false);
            emit failed(QStringLiteral("استجابة تسجيل الدخول من WE غير مكتملة."));
            return;
        }
        m_subscriberId = subscriberId;
        m_customerName = customerName;
        m_sessionToken = token.toUtf8();
        m_authenticating = false;
        emit progressChanged(QStringLiteral("تم تسجيل الدخول. جارٍ تحميل بيانات الباقة..."));
        fetchOffering(subscriberId, token.toUtf8(), customerName);
    });
}

void WeQuotaService::fetchOffering(const QString &subscriberId, const QByteArray &token,
                                   const QString &customerName)
{
    emit progressChanged(QStringLiteral("جارٍ تحميل بيانات الباقة المشترك بها..."));
    QJsonObject body{
        {QStringLiteral("msisdn"), m_accountId},
        {QStringLiteral("numberServiceType"), QStringLiteral("FBB")},
        {QStringLiteral("groupId"), QString()}
    };

    post(QStringLiteral("/echannel/service/besapp/base/rest/busiservice/cz/v1/auth/getSubscribedOfferings"),
         body, [this, subscriberId, token, customerName](const QJsonObject &root) {
        const QJsonArray offers = root.value(QStringLiteral("body")).toObject()
                                      .value(QStringLiteral("offeringList")).toArray();
        if (offers.isEmpty()) {
            m_busy = false;
            emit busyChanged(false);
            emit failed(QStringLiteral("لم يتم العثور على باقة إنترنت فعالة."));
            return;
        }
        const QString offerId = offers.first().toObject()
                                    .value(QStringLiteral("mainOfferingId")).toVariant().toString();
        m_subscriberId = subscriberId;
        m_customerName = customerName;
        m_sessionToken = token;
        m_offerId = offerId;
        persistSession();
        fetchQuota(subscriberId, offerId, token, customerName);
    }, token);
}

void WeQuotaService::fetchQuota(const QString &subscriberId, const QString &offerId,
                                const QByteArray &token, const QString &customerName)
{
    emit progressChanged(QStringLiteral("جارٍ تحميل تفاصيل الاستهلاك..."));
    QJsonObject body{
        {QStringLiteral("subscriberId"), subscriberId},
        {QStringLiteral("mainOfferId"), offerId}
    };

    post(QStringLiteral("/echannel/service/besapp/base/rest/busiservice/cz/cbs/bb/queryFreeUnit"),
         body, [this, customerName](const QJsonObject &root) {
        const QJsonArray quotas = root.value(QStringLiteral("body")).toArray();
        if (quotas.isEmpty()) {
            m_busy = false;
            emit busyChanged(false);
            emit failed(QStringLiteral("لم تصل تفاصيل الاستهلاك من WE."));
            return;
        }

        const QJsonObject quota = quotas.first().toObject();
        QuotaInfo info;
        info.customerName = customerName;
        info.offerName = quota.value(QStringLiteral("offerName")).toString();
        info.remaining = quota.value(QStringLiteral("remain")).toDouble();
        info.total = quota.value(QStringLiteral("total")).toDouble();
        const double used = quota.value(QStringLiteral("used")).toDouble();
        info.usagePercentage = info.total > 0.0 ? used * 100.0 / info.total : 0.0;
        info.renewalDate = QDateTime::fromMSecsSinceEpoch(
            quota.value(QStringLiteral("effectiveTime")).toVariant().toLongLong());
        info.expiryDate = QDateTime::fromMSecsSinceEpoch(
            quota.value(QStringLiteral("expireTime")).toVariant().toLongLong());

        m_busy = false;
        emit busyChanged(false);
        emit serverResponse(QStringLiteral("اكتمل تحديث الاستهلاك من السيرفر."), true);
        emit quotaReady(info);
    }, token);
}

void WeQuotaService::post(const QString &path, const QJsonObject &body,
                          const std::function<void(const QJsonObject &)> &onSuccess,
                          const QByteArray &csrfToken)
{
    QNetworkRequest request(kApiBase.resolved(QUrl(path)));
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    request.setRawHeader("Accept", "application/json, text/plain, */*");
    request.setRawHeader("Accept-Language", "en-US,en;q=0.9,ar;q=0.8");
    request.setRawHeader("User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64)");
    request.setRawHeader("Origin", "https://my.te.eg");
    request.setRawHeader("Referer", "https://my.te.eg/echannel/");
    request.setRawHeader("channelId", "702");
    request.setRawHeader("isCoporate", "false");
    request.setRawHeader("isMobile", "false");
    request.setRawHeader("isSelfcare", "true");
    request.setRawHeader("languageCode", "en-US");
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setTransferTimeout(20000);
    if (!csrfToken.isEmpty())
        request.setRawHeader("csrftoken", csrfToken);
    else
        request.setRawHeader("csrftoken", "");

    QNetworkReply *reply = m_network.post(request, QJsonDocument(body).toJson(QJsonDocument::Compact));
    connect(reply, &QNetworkReply::finished, this, [this, reply, onSuccess, path]() {
        const QByteArray payload = reply->readAll();
        const auto networkError = reply->error();
        const QString networkMessage = reply->errorString();
        const int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        reply->deleteLater();

        if (networkError != QNetworkReply::NoError) {
            if (httpStatus == 401 || httpStatus == 403) {
                clearSession();
            }
            m_authenticating = false;
            m_busy = false;
            emit busyChanged(false);
            const QString detail = QStringLiteral("فشل اتصال WE (HTTP %1): %2")
                                       .arg(httpStatus > 0 ? QString::number(httpStatus)
                                                           : QStringLiteral("---"),
                                              networkMessage);
            emit serverResponse(detail, false);
            emit failed(detail);
            return;
        }

        QJsonParseError parseError;
        const QJsonDocument document = QJsonDocument::fromJson(payload, &parseError);
        if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
            m_authenticating = false;
            m_busy = false;
            emit busyChanged(false);
            const QString detail = QStringLiteral("وصل رد غير صالح من WE عند %1.").arg(path);
            emit serverResponse(detail, false);
            emit failed(detail);
            return;
        }

        const QJsonObject root = document.object();
        const QJsonObject header = root.value(QStringLiteral("header")).toObject();
        const QString code = header.value(QStringLiteral("retCode")).toVariant().toString();
        if (code != QStringLiteral("0")) {
            m_busy = false;
            emit busyChanged(false);
            const bool loginRequest = path.contains(QStringLiteral("userAuthenticate"));
            const QString detail = apiError(
                root, loginRequest
                    ? QStringLiteral("رفضت خدمة WE تسجيل الدخول. تحقق من البيانات أو رمز التحقق.")
                    : QStringLiteral("رفضت خدمة WE طلب تحديث البيانات. حاول تسجيل الدخول مرة أخرى."));
            const QString errorNo = header.value(QStringLiteral("errorNo")).toString();
            const QString lower = detail.toLower();
            const bool needsCaptcha = m_authenticating
                && (errorNo.startsWith(QStringLiteral("603010250721"))
                    || lower.contains(QStringLiteral("captcha"))
                    || lower.contains(QStringLiteral("verification code"))
                    || lower.contains(QStringLiteral("entered code")));
            m_authenticating = false;

            if (lower.contains(QStringLiteral("session"))
                || lower.contains(QStringLiteral("token"))
                || lower.contains(QStringLiteral("login"))) {
                clearSession();
            }

            if (needsCaptcha) {
                emit captchaRequired();
                requestCaptcha(m_landline);
                return;
            }

            emit serverResponse(
                QStringLiteral("رد السيرفر: %1 (الكود %2)").arg(detail, code), false);
            emit failed(detail);
            return;
        }
        onSuccess(root);
    });
}

QString WeQuotaService::apiError(const QJsonObject &root, const QString &fallback)
{
    const QJsonObject header = root.value(QStringLiteral("header")).toObject();
    QString message = header.value(QStringLiteral("errorMsg")).toString();
    if (message.isEmpty())
        message = header.value(QStringLiteral("retMsg")).toString();
    return message.isEmpty() ? fallback : message;
}

void WeQuotaService::requestCaptcha(const QString &landlineNumber)
{
    emit progressChanged(QStringLiteral("جارٍ جلب رمز التحقق (Captcha)..."));

    QJsonObject body{
        {QStringLiteral("merchantName"), QStringLiteral("E-Care")},
        {QStringLiteral("serviceName"), QStringLiteral("Login")},
        {QStringLiteral("identifier"), landlineNumber.trimmed()}
    };

    QNetworkRequest request(kCaptchaUrl);
    request.setHeader(QNetworkRequest::ContentTypeHeader,
                      QStringLiteral("application/json; charset=UTF-8"));
    request.setRawHeader("Accept", "application/json, text/plain, */*");
    request.setRawHeader("Accept-Language", "en-US,en;q=0.9");
    request.setRawHeader("Cache-Control", "no-cache");
    request.setRawHeader("Pragma", "no-cache");
    request.setRawHeader("Origin", "https://my.te.eg");
    request.setRawHeader("Referer", "https://my.te.eg/");
    request.setRawHeader("User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/149.0.0.0 Safari/537.36 Edg/149.0.0.0");
    request.setTransferTimeout(20000);

    QNetworkReply *reply = m_network.post(request, QJsonDocument(body).toJson(QJsonDocument::Compact));
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        const QByteArray payload = reply->readAll();
        const auto networkError = reply->error();
        const QString networkMessage = reply->errorString();
        reply->deleteLater();

        if (networkError != QNetworkReply::NoError) {
            m_busy = false;
            m_authenticating = false;
            emit busyChanged(false);
            emit failed(QStringLiteral("تعذر جلب رمز التحقق: %1").arg(networkMessage));
            return;
        }

        QJsonParseError parseError;
        const QJsonDocument document = QJsonDocument::fromJson(payload, &parseError);
        if (parseError.error != QJsonParseError::NoError) {
            emit failed(QStringLiteral("فشل في معالجة استجابة الـ Captcha من السيرفر."));
            return;
        }

        QJsonObject root = document.object();
        QString status = root.value(QStringLiteral("status")).toString();
        if (status == QStringLiteral("Success")) {
            QString token = root.value(QStringLiteral("token")).toString();
            QString captchaBase64 = root.value(QStringLiteral("captcha")).toString();
            const bool requireInteraction =
                root.value(QStringLiteral("requireInteraction")).toBool(true);

            if (!requireInteraction) {
                authenticateWithCaptcha(QString(), token);
                return;
            }

            const int separator = captchaBase64.indexOf(QLatin1Char(','));
            if (separator >= 0)
                captchaBase64 = captchaBase64.mid(separator + 1);

            const QByteArray imageBytes =
                QByteArray::fromBase64(captchaBase64.toLatin1());
            if (token.isEmpty() || imageBytes.isEmpty()) {
                m_busy = false;
                m_authenticating = false;
                emit busyChanged(false);
                emit failed(QStringLiteral("وصلت صورة Captcha غير صالحة من WE."));
                return;
            }

            m_busy = false;
            emit busyChanged(false);
            emit captchaRequired();
            emit captchaReady(captchaBase64, token);
        } else {
            QString detail = root.value(QStringLiteral("message")).toString();
            if (detail.isEmpty())
                detail = root.value(QStringLiteral("error")).toString();
            emit failed(detail.isEmpty()
                            ? QStringLiteral("فشل في جلب الـ Captcha من خادم WE (الحالة: %1).")
                                  .arg(status)
                            : detail);
        }
    });
}

void WeQuotaService::authenticateWithCaptcha(const QString &captchaCode, const QString &captchaToken)
{
    m_authenticating = true;
    m_busy = true;
    emit busyChanged(true);
    emit progressChanged(QStringLiteral("جارٍ تسجيل الدخول برمز التحقق..."));

    QJsonObject body{
        {QStringLiteral("acctId"), m_accountId},
        {QStringLiteral("password"), m_password},
        {QStringLiteral("appLocale"), QStringLiteral("en-US")},
        {QStringLiteral("isSelfcare"), QStringLiteral("Y")},
        {QStringLiteral("isMobile"), QStringLiteral("N")},
        {QStringLiteral("imgCacheKey"), captchaToken},
        {QStringLiteral("imgCode"), captchaCode.trimmed()}
    };

    post(QStringLiteral("/echannel/service/besapp/base/rest/busiservice/v1/auth/userAuthenticate"),
         body, [this](const QJsonObject &root) {
        const QJsonObject responseBody = root.value(QStringLiteral("body")).toObject();
        const QJsonObject customer = responseBody.value(QStringLiteral("customer")).toObject();
        const QJsonObject subscriber = responseBody.value(QStringLiteral("subscriber")).toObject();
        const QString token = responseBody.value(QStringLiteral("token")).toString();
        const QString subscriberId = subscriber.value(QStringLiteral("subscriberId")).toVariant().toString();
        const QString customerName = customer.value(QStringLiteral("custName")).toString();

        if (token.isEmpty() || subscriberId.isEmpty()) {
            m_authenticating = false;
            m_busy = false;
            emit busyChanged(false);
            emit failed(QStringLiteral("استجابة تسجيل الدخول من WE غير مكتملة."));
            return;
        }
        m_subscriberId = subscriberId;
        m_customerName = customerName;
        m_sessionToken = token.toUtf8();
        m_authenticating = false;
        emit progressChanged(QStringLiteral("تم تسجيل الدخول. جارٍ تحميل بيانات الباقة..."));
        fetchOffering(subscriberId, token.toUtf8(), customerName);
    });
}

void WeQuotaService::cancelRequest()
{
    m_busy = false;
    m_authenticating = false;
    emit busyChanged(false);
    emit failed(QStringLiteral("تم إلغاء العملية."));
}

void WeQuotaService::persistSession()
{
    if (!hasActiveSession())
        return;
    const QJsonObject session{
        {QStringLiteral("landline"), m_landline},
        {QStringLiteral("accountId"), m_accountId},
        {QStringLiteral("subscriberId"), m_subscriberId},
        {QStringLiteral("customerName"), m_customerName},
        {QStringLiteral("offerId"), m_offerId},
        {QStringLiteral("token"), QString::fromLatin1(m_sessionToken.toBase64())},
        {QStringLiteral("savedAt"), QDateTime::currentMSecsSinceEpoch()}
    };
    CredentialStore::saveSecret(
        QStringLiteral("we-session"),
        QJsonDocument(session).toJson(QJsonDocument::Compact));
}

void WeQuotaService::clearSession()
{
    m_subscriberId.clear();
    m_customerName.clear();
    m_offerId.clear();
    m_sessionToken.clear();
    CredentialStore::saveSecret(QStringLiteral("we-session"), {});
}
