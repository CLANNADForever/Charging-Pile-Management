#include "HttpUserService.h"

#include <QDateTime>
#include <QJsonArray>
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

}  // namespace client
}  // namespace ncs
