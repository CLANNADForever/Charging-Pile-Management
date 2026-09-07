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
    // 离线/后端不可达时的回退演示用户(在线登录成功后 current 来自后端)。
    auto add = [this](int id, const QString &phone, const QString &nickname,
                      double balance, bool frozen, const QString &reg) {
        User u;
        u.id = id;
        u.phone = phone;
        u.nickname = nickname;
        u.balance = balance;
        u.frozen = frozen;
        u.avatarPath.clear();
        u.registeredAt = reg;
        m_users.append(u);
    };
    add(1, QStringLiteral("13800001111"), QStringLiteral("陈马星宇"), 128.50, false, QStringLiteral("2026-07-01 09:00:00"));
    add(2, QStringLiteral("13800002222"), QStringLiteral("充电达人"), 56.00, false, QStringLiteral("2026-07-12 14:30:00"));
    add(3, QStringLiteral("13800003333"), QStringLiteral("新能源车主"), 0.00, true, QStringLiteral("2026-08-03 18:20:00"));
    add(4, QStringLiteral("13911114444"), QStringLiteral("滴滴司机王"), 210.80, false, QStringLiteral("2026-08-15 10:00:00"));
    add(5, QStringLiteral("13755556666"), QStringLiteral("特斯拉ModelY"), 33.20, false, QStringLiteral("2026-08-28 21:10:00"));
    add(6, QStringLiteral("13622227777"), QStringLiteral("比亚迪车主"), 12.00, true, QStringLiteral("2026-09-01 08:45:00"));
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

User UserService::loginOrRegister(const QString &phone)
{
    // 在线：走后端免密登录(演示验证码固定 123456；UI 自管验证码 UI 后仅传手机号)。
    const ncsfe::BackendClient::Reply r = ncsfe::BackendClient::post(
        QStringLiteral("/api/auth/login"),
        QJsonObject{{QStringLiteral("phone"), phone},
                    {QStringLiteral("code"), QStringLiteral("123456")}});
    if (r.ok) {
        m_current = userFromJson(
            r.data.toObject().value(QStringLiteral("user")).toObject());
        return m_current;
    }
    if (!r.ok && r.code == 1 &&
        r.message.contains(QStringLiteral("冻结"))) {
        User u;
        u.phone = phone;
        u.frozen = true;
        m_current = u;
        return u;
    }
    // 离线/后端不可达 → 本地自动注册(页面仍可打开)。
    for (const User &u : m_users) {
        if (u.phone == phone) {
            m_current = u;
            return m_current;
        }
    }
    User u;
    u.id = m_users.size() + 1;
    u.phone = phone;
    u.nickname = QStringLiteral("用户") + phone.right(4);
    u.balance = 50.00;
    u.frozen = false;
    u.registeredAt = QStringLiteral("2026-09-06 12:00:00");
    m_users.append(u);
    m_current = u;
    return m_current;
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
    if (ncsfe::BackendClient::token().isEmpty()) {
        // 未登录管理端 → 本地回退
        if (keyword.isEmpty())
            return m_users;
        QList<User> result;
        for (const User &u : m_users)
            if (u.phone.contains(keyword))
                result.append(u);
        return result;
    }
    const QByteArray kw = QUrl::toPercentEncoding(keyword);
    const ncsfe::BackendClient::Reply r = ncsfe::BackendClient::get(
        QStringLiteral("/api/admin/users?phone=%1&status=0").arg(QString::fromUtf8(kw)));
    QList<User> out;
    if (r.ok && r.data.isArray()) {
        for (const QJsonValue &v : r.data.toArray())
            out.append(userFromJson(v.toObject()));
        return out;
    }
    // 失败回退本地
    if (keyword.isEmpty())
        return m_users;
    QList<User> result;
    for (const User &u : m_users)
        if (u.phone.contains(keyword))
            result.append(u);
    return result;
}

bool UserService::setUserStatus(const QString &phone, bool frozen)
{
    if (!ncsfe::BackendClient::token().isEmpty()) {
        // 在线：按手机号定位用户 id 后调后端冻结/解冻
        const QByteArray kw = QUrl::toPercentEncoding(phone);
        const ncsfe::BackendClient::Reply lr = ncsfe::BackendClient::get(
            QStringLiteral("/api/admin/users?phone=%1&status=0").arg(QString::fromUtf8(kw)));
        if (lr.ok && lr.data.isArray()) {
            const QJsonArray arr = lr.data.toArray();
            if (!arr.isEmpty()) {
                const int uid = arr.first().toObject().value(QStringLiteral("id")).toInt();
                const ncsfe::BackendClient::Reply fr = ncsfe::BackendClient::post(
                    QStringLiteral("/api/admin/users/%1/freeze").arg(uid),
                    QJsonObject{{QStringLiteral("frozen"), frozen}});
                if (fr.ok) {
                    for (User &u : m_users)
                        if (u.phone == phone)
                            u.frozen = frozen;
                    return true;
                }
            }
        }
    }
    for (User &u : m_users) {
        if (u.phone == phone) {
            u.frozen = frozen;
            if (m_current.phone == phone)
                m_current.frozen = frozen;
            return true;
        }
    }
    return false;
}
