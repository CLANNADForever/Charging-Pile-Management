#pragma once

#include <QList>
#include <QString>

// 用户模型
struct User
{
    int id = 0;
    QString phone;
    QString nickname;
    double  balance = 0.0;
    bool    frozen = false;
    QString avatarPath;   // 空 = 默认灰头像
    QString registeredAt;
};

// 用户服务。
// 阶段一:桩实现(内存假数据),用于前端页面开发;后续阶段接入数据库后替换内部实现,接口保持不变。
class UserService
{
public:
    static UserService &instance();

    // 登录/自动注册:手机号不存在则自动注册(昵称 用户+后4位、余额 0)。
    User loginOrRegister(const QString &phone);

    // 校验手机号格式(11 位、以 1 开头)。
    static bool isValidPhone(const QString &phone);

    void updateNickname(const QString &nickname);
    void updateAvatar(const QString &avatarPath);
    void recharge(double amount);
    void deduct(double amount); // 扣款(BR-06: 余额不足扣至 0)

    const User &current() const { return m_current; }

    // —— 管理端操作(UC-A-07)——
    QList<User> listUsers(const QString &keyword = QString()) const; // 手机号模糊
    bool setUserStatus(const QString &phone, bool frozen);

private:
    UserService();

    User m_current;
    QList<User> m_users;
};
