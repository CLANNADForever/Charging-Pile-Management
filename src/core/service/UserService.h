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
// 登录走后端 send-code + login(用 UI 真实输入的验证码)，失败返回提示，绝不静默回退本地假登录。
class UserService
{
public:
    static UserService &instance();

    // 获取验证码(后端演示码)；成功返回给 UI 展示的提示(含码)，失败返回错误提示。
    QString requestCode(const QString &phone);

    // 验证码登录/自动注册；成功写入 current 返回空串，失败返回面向用户的错误。
    QString login(const QString &phone, const QString &code);

    // 从后端刷新当前用户(余额等)，成功返回 true。
    bool refreshProfile(QString *err = nullptr);

    // 校验手机号格式(11 位、以 1 开头)。
    static bool isValidPhone(const QString &phone);

    bool recharge(double amount, QString *err = nullptr);            // 走后端充值
    bool updateNickname(const QString &nickname, QString *err = nullptr); // 走后端改昵称
    bool uploadAvatar(const QString &filePath, QString *err = nullptr);   // 上传头像→本地缓存路径
    void deduct(double amount); // 仅本地展示用(BR-06 演示),不做后端扣款

    const User &current() const { return m_current; }

    // —— 管理端操作(UC-A-07)——
    QList<User> listUsers(const QString &keyword = QString()) const; // 手机号模糊(需已登录管理端)
    bool setUserStatus(const QString &phone, bool frozen);           // 冻结/解冻(需已登录管理端)

private:
    UserService();

    User m_current;
    QList<User> m_users;  // 仅管理端离线时的内存缓存(在线读写走后端)
};
