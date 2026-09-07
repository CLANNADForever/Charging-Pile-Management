#pragma once

#include <QString>

// 管理员服务(管理端专属)。
// 阶段一:桩实现(固定初始账号 admin / 123456,UC-A-01);
// 后续阶段二接入数据库后替换为 SHA-256 加盐哈希校验(NFR-S-01),接口保持不变。
class AdminService
{
public:
    static AdminService &instance();

    // 校验账号密码,成功返回 true 并记录当前账号。
    bool login(const QString &account, const QString &password);

    QString currentAccount() const { return m_account; }

private:
    AdminService() = default;

    QString m_account;
};
