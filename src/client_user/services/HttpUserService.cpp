#include "HttpUserService.h"

#include <QDateTime>
#include <QEventLoop>
#include <QJsonDocument>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>
#include <QUrl>

namespace ncs {
namespace client {

namespace {

ncs::User userFromJson(const QJsonObject& o) {
    ncs::User u;
    u.id = o.value(QStringLiteral("id")).toInt();
    u.phone = o.value(QStringLiteral("phone")).toString();
    u.nickname = o.value(QStringLiteral("nickname")).toString();
    u.balanceCents = static_cast<ncs::MoneyCents>(
        o.value(QStringLiteral("balance_cents")).toDouble());
    u.status = static_cast<ncs::UserStatus>(
        o.value(QStringLiteral("status")).toInt());
    const QString iso = o.value(QStringLiteral("registered_at")).toString();
    if (!iso.isEmpty())
        u.registeredAt = QDateTime::fromString(iso, Qt::ISODate).toUTC();
    return u;
}

}  // namespace

HttpUserService::HttpUserService(QString baseUrl)
    : baseUrl_(std::move(baseUrl)) {}

HttpUserService::HttpOut HttpUserService::post(const QString& path,
                                               const QJsonObject& json) const {
    HttpOut out;
    QNetworkAccessManager mgr;
    QNetworkRequest req(QUrl(baseUrl_ + path));
    req.setHeader(QNetworkRequest::ContentTypeHeader,
                  QStringLiteral("application/json"));

    QNetworkReply* reply =
        mgr.post(req, QJsonDocument(json).toJson(QJsonDocument::Compact));

    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    timer.start(4000);  // 4s 超时
    loop.exec();

    if (reply->error() != QNetworkReply::NoError) {
        out.error = reply->errorString();
        reply->deleteLater();
        return out;
    }
    out.ok = true;
    out.status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute)
                     .toInt();
    const QByteArray data = reply->readAll();
    if (!data.isEmpty()) {
        QJsonParseError err;
        const QJsonDocument doc = QJsonDocument::fromJson(data, &err);
        if (err.error == QJsonParseError::NoError && doc.isObject())
            out.body = doc.object();
    }
    reply->deleteLater();
    return out;
}

LoginResult HttpUserService::requestCode(const QString& phone) {
    LoginResult r;
    const HttpOut h = post(QStringLiteral("/api/auth/send-code"),
                           {{QStringLiteral("phone"), phone}});
    if (!h.ok) {
        r.message = h.error.isEmpty()
                        ? QStringLiteral("网络请求失败")
                        : QStringLiteral("网络错误：") + h.error;
        return r;
    }
    r.ok = h.body.value(QStringLiteral("ok")).toBool();
    r.message = h.body.value(QStringLiteral("message")).toString();
    return r;
}

LoginResult HttpUserService::login(const QString& phone, const QString& code) {
    LoginResult r;
    const HttpOut h = post(QStringLiteral("/api/auth/login"),
                           {{QStringLiteral("phone"), phone},
                            {QStringLiteral("code"), code}});
    if (!h.ok) {
        r.message = h.error.isEmpty()
                        ? QStringLiteral("网络请求失败")
                        : QStringLiteral("网络错误：") + h.error;
        return r;
    }
    r.ok = h.body.value(QStringLiteral("ok")).toBool();
    r.message = h.body.value(QStringLiteral("message")).toString();
    if (r.ok && h.body.contains(QStringLiteral("user")))
        r.user = userFromJson(h.body.value(QStringLiteral("user")).toObject());
    return r;
}

}  // namespace client
}  // namespace ncs
