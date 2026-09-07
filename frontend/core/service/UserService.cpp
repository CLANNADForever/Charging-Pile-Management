#include "UserService.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QUrl>

#include "core/net/BackendClient.h"

UserService &UserService::instance()
{
    static UserService s;
    return s;
}

UserService::UserService()
{
    // 不预置假用户；管理端列表/写操作在未登录或离线时返回空/失败并提示。
}

namespace {
User userFromJson(const QJsonObject &o)
{
    User u;
    u.id = o.value(QStringLiteral("id")).toInt();
    u.phone = o.value(QStringLiteral("phone")).toString();
    u.nickname = o.value(QStringLiteral("nickname")).toString();
    u.balance = o.value(QStringLiteral("balance_cents")).toDouble() / 100.0;
    u.frozen = o.value(QStringLiteral("status")).toInt() == 1;
    u.avatarPath.clear();
    u.registeredAt = o.value(QStringLiteral("registered_at")).toString();
    return u;
}
}  // namespace

QString UserService::requestCode(const QString &phone)
{
    const ncsfe::BackendClient::Reply r = ncsfe::BackendClient::post(
        QStringLiteral("/api/auth/send-code"),
        QJsonObject{{QStringLiteral("phone"), phone}});
    if (r.ok)
        return r.message;  // 后端提示含演示验证码
    // 失败：绝不静默。区分“传输失败”与“业务拒绝”尽量给可读提示。
    if (!r.message.isEmpty())
        return r.message;
    return QStringLiteral("获取验证码失败(网络不可达后端)");
}

QString UserService::login(const QString &phone, const QString &code)
{
    if (code.isEmpty())
        return QStringLiteral("请先获取并输入验证码");
    const ncsfe::BackendClient::Reply r = ncsfe::BackendClient::post(
        QStringLiteral("/api/auth/login"),
        QJsonObject{{QStringLiteral("phone"), phone},
                    {QStringLiteral("code"), code}});
    if (r.ok) {
        m_current = userFromJson(
            r.data.toObject().value(QStringLiteral("user")).toObject());
        return QString();
    }
    if (!r.message.isEmpty())
        return r.message;
    return QStringLiteral("登录失败(网络不可达后端)");
}

bool UserService::isValidPhone(const QString &phone)
{
    if (phone.length() != 11)
        return false;
    if (!phone.startsWith(QLatin1Char('1')))
        return false;
    for (QChar c : phone) {
        if (!c.isDigit())
            return false;
    }
    return true;
}

void UserService::updateNickname(const QString &nickname)
{
    m_current.nickname = nickname;
}

void UserService::updateAvatar(const QString &avatarPath)
{
    m_current.avatarPath = avatarPath;
}

void UserService::recharge(double amount)
{
    m_current.balance += amount;
}

void UserService::deduct(double amount)
{
    if (m_current.balance < amount)
        m_current.balance = 0.0;
    else
        m_current.balance -= amount;
}

QList<User> UserService::listUsers(const QString &keyword) const
{
    if (ncsfe::BackendClient::token().isEmpty())
        return QList<User>();  // 未登录管理端：不给假数据
    const QByteArray kw = QUrl::toPercentEncoding(keyword);
    const ncsfe::BackendClient::Reply r = ncsfe::BackendClient::get(
        QStringLiteral("/api/admin/users?phone=%1&status=0").arg(QString::fromUtf8(kw)));
    QList<User> out;
    if (r.ok && r.data.isArray())
        for (const QJsonValue &v : r.data.toArray())
            out.append(userFromJson(v.toObject()));
    return out;
}

bool UserService::setUserStatus(const QString &phone, bool frozen)
{
    if (ncsfe::BackendClient::token().isEmpty())
        return false;
    const QByteArray kw = QUrl::toPercentEncoding(phone);
    const ncsfe::BackendClient::Reply lr = ncsfe::BackendClient::get(
        QStringLiteral("/api/admin/users?phone=%1&status=0").arg(QString::fromUtf8(kw)));
    if (!lr.ok || !lr.data.isArray() || lr.data.toArray().isEmpty())
        return false;
    const int uid = lr.data.toArray().first().toObject()
                        .value(QStringLiteral("id")).toInt();
    const ncsfe::BackendClient::Reply fr = ncsfe::BackendClient::post(
        QStringLiteral("/api/admin/users/%1/freeze").arg(uid),
        QJsonObject{{QStringLiteral("frozen"), frozen}});
    return fr.ok;
}
