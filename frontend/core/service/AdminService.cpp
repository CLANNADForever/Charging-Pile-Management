#include "AdminService.h"

AdminService &AdminService::instance()
{
    static AdminService s;
    return s;
}

bool AdminService::login(const QString &account, const QString &password)
{
    // 桩:固定初始账号 admin / 123456(UC-A-01)。
    // 后续阶段二替换为 SHA-256 加盐哈希校验(NFR-S-01)。
    if (account == QStringLiteral("admin") && password == QStringLiteral("123456")) {
        m_account = account;
        return true;
    }
    m_account.clear();
    return false;
}
