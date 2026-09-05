#include "HttpUserService.h"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QJsonObject>

namespace ncs {
namespace client {

namespace {
User userFromJson(const QJsonObject& o) {
    User u;
    u.id = o.value(QStringLiteral("id")).toInt();
    u.phone = o.value(QStringLiteral("phone")).toString();
    u.nickname = o.value(QStringLiteral("nickname")).toString();
    u.balanceCents = static_cast<MoneyCents>(
        o.value(QStringLiteral("balance_cents")).toDouble());
    u.status = static_cast<UserStatus>(o.value(QStringLiteral("status")).toInt());
    const QString iso = o.value(QStringLiteral("registered_at")).toString();
    if (!iso.isEmpty())
        u.registeredAt = QDateTime::fromString(iso, Qt::ISODate).toUTC();
    return u;
}
}  // namespace

HttpUserService::HttpUserService(QString baseUrl)
    : client_(std::move(baseUrl)) {}

void HttpUserService::requestCode(const QString& phone, LoginCallback done) {
    client_.post(QStringLiteral("/api/auth/send-code"),
                 QJsonObject{{QStringLiteral("phone"), phone}},
                 [done = std::move(done)](const HttpJsonClient::Reply& h) {
                     LoginResult r;
                     if (!h.transportOk) {
                         r.message = h.error.isEmpty()
                                         ? QStringLiteral("网络请求失败")
                                         : QStringLiteral("网络错误：") + h.error;
                     } else if (h.code != 0) {
                         r.message = h.message;
                     } else {
                         r.ok = true;
                         r.message = h.message;
                     }
                     done(r);
                 });
}

void HttpUserService::login(const QString& phone, const QString& code,
                            LoginCallback done) {
    client_.post(QStringLiteral("/api/auth/login"),
                 QJsonObject{{QStringLiteral("phone"), phone},
                             {QStringLiteral("code"), code}},
                 [done = std::move(done)](const HttpJsonClient::Reply& h) {
                     LoginResult r;
                     if (!h.transportOk) {
                         r.message = h.error.isEmpty()
                                         ? QStringLiteral("网络请求失败")
                                         : QStringLiteral("网络错误：") + h.error;
                     } else if (h.code != 0) {
                         r.message = h.message;
                     } else {
                         r.ok = true;
                         r.message = h.message;
                         r.user = userFromJson(
                             h.data.toObject().value(QStringLiteral("user")).toObject());
                     }
                     done(r);
                 });
}

void HttpUserService::recharge(const QString& phone, ncs::MoneyCents amountCents,
                                 LoginCallback done) {
    client_.post(QStringLiteral("/api/wallet/recharge"),
                 QJsonObject{{QStringLiteral("phone"), phone},
                             {QStringLiteral("amount_cents"),
                              static_cast<double>(amountCents)}},
                 [done = std::move(done)](const HttpJsonClient::Reply& h) {
                     LoginResult r;
                     if (!h.transportOk) {
                         r.message = QStringLiteral("网络错误：") + h.error;
                     } else if (h.code != 0) {
                         r.message = h.message;
                     } else {
                         r.ok = true;
                         r.user = userFromJson(h.data.toObject());
                     }
                     done(r);
                 });
}

void HttpUserService::setNickname(const QString& phone, const QString& nickname,
                                  LoginCallback done) {
    client_.post(QStringLiteral("/api/user/profile"),
                 QJsonObject{{QStringLiteral("phone"), phone},
                             {QStringLiteral("nickname"), nickname}},
                 [done = std::move(done)](const HttpJsonClient::Reply& h) {
                     LoginResult r;
                     if (!h.transportOk) {
                         r.message = QStringLiteral("网络错误：") + h.error;
                     } else if (h.code != 0) {
                         r.message = h.message;
                     } else {
                         r.ok = true;
                         r.user = userFromJson(h.data.toObject());
                     }
                     done(r);
                 });
}

void HttpUserService::uploadAvatar(const QString& phone, const QByteArray& bytes,
                                   AvatarCallback done) {
    auto* mgr = new QNetworkAccessManager();
    const QUrl url(baseUrl() + QStringLiteral("/api/user/avatar?phone=") + phone +
                   QStringLiteral("&ext=png"));
    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("image/png"));
    QNetworkReply* reply = mgr->post(req, bytes);
    QObject::connect(reply, &QNetworkReply::finished,
                     [reply, mgr, done = std::move(done)] {
                         QString err, url;
                         if (reply->error() == QNetworkReply::NoError) {
                             const QJsonDocument doc =
                                 QJsonDocument::fromJson(reply->readAll());
                             const QJsonObject o = doc.object();
                             if (o.value(QStringLiteral("code")).toInt(-1) == 0)
                                 url = o.value(QStringLiteral("data"))
                                           .toObject()
                                           .value(QStringLiteral("url"))
                                           .toString();
                             else
                                 err = o.value(QStringLiteral("message")).toString();
                         } else {
                             err = reply->errorString();
                         }
                         done(err, url);
                         reply->deleteLater();
                         mgr->deleteLater();
                     });
}

}  // namespace client
}  // namespace ncs
