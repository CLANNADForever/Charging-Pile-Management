#include "UserService.h"

UserService &UserService::instance()
{
    static UserService s;
    return s;
}

UserService::UserService()
{
    // 预置若干用户(桩数据),便于用户管理页展示。
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

User UserService::loginOrRegister(const QString &phone)
{
    for (const User &u : m_users) {
        if (u.phone == phone) {
            m_current = u;
            return m_current;
        }
    }
    // 未注册:自动注册
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
    if (keyword.isEmpty())
        return m_users;
    QList<User> result;
    for (const User &u : m_users) {
        if (u.phone.contains(keyword))
            result.append(u);
    }
    return result;
}

bool UserService::setUserStatus(const QString &phone, bool frozen)
{
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
