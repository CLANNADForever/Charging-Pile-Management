#include "HttpUserService.h"

#include <QDateTime>
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
    u.status =
        static_cast<UserStatus>(o.value(QStringLiteral("status")).toInt());
    const QString iso = o.value(QStringLiteral("registered_at")).toString();
    if (!iso.isEmpty())
        u.registeredAt = QDateTime::fromString(iso, Qt::ISODate).toUTC();
    return u;
}
}  // namespace

HttpUserService::HttpUserService(QString baseUrl)
    : client_(std::move(baseUrl)) {}

LoginResult HttpUserService::requestCode(const QString& phone) {
    LoginResult r;
    const auto h = client_.post(
        QStringLiteral("/api/auth/send-code"),
        QJsonObject{{QStringLiteral("phone"), phone}});
    if (!h.ok) {
        r.message = h.error.isEmpty()
                        ? QStringLiteral("网络请求失败")
                        : QStringLiteral("网络错误：") + h.error;
        return r;
    }
    r.ok = h.root.toObject().value(QStringLiteral("ok")).toBool();
    r.message = h.root.toObject().value(QStringLiteral("message")).toString();
    return r;
}

LoginResult HttpUserService::login(const QString& phone, const QString& code) {
    LoginResult r;
    const auto h = client_.post(
        QStringLiteral("/api/auth/login"),
        QJsonObject{{QStringLiteral("phone"), phone},
                    {QStringLiteral("code"), code}});
    if (!h.ok) {
        r.message = h.error.isEmpty()
                        ? QStringLiteral("网络请求失败")
                        : QStringLiteral("网络错误：") + h.error;
        return r;
    }
    const QJsonObject obj = h.root.toObject();
    r.ok = obj.value(QStringLiteral("ok")).toBool();
    r.message = obj.value(QStringLiteral("message")).toString();
    if (r.ok && obj.contains(QStringLiteral("user")))
        r.user = userFromJson(obj.value(QStringLiteral("user")).toObject());
    return r;
}

}  // namespace client
}  // namespace ncs
