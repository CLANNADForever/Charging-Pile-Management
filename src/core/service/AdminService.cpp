#include "AdminService.h"

#include <QJsonObject>

#include "core/net/BackendClient.h"

AdminService &AdminService::instance()
{
    static AdminService s;
    return s;
}

bool AdminService::login(const QString &account, const QString &password)
{
    // 在线：调后端 /api/admin/login(账号密码与角色由后端权威判定)，成功记录 token 与账号。
    const ncsfe::BackendClient::Reply r = ncsfe::BackendClient::post(
        QStringLiteral("/api/admin/login"),
        QJsonObject{{QStringLiteral("username"), account},
                    {QStringLiteral("password"), password}});
    if (!r.ok) {
        ncsfe::BackendClient::setToken(QString());
        m_account.clear();
        return false;
    }
    ncsfe::BackendClient::setToken(
        r.data.toObject().value(QStringLiteral("token")).toString());
    m_account = account;
    return true;
}
