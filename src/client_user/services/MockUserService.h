// Mock 实现：进程内内存账本，验证码固定 123456，供无后端阶段驱动 UI 与测试。
#ifndef NCS_CLIENT_SERVICES_MOCKUSERSERVICE_H
#define NCS_CLIENT_SERVICES_MOCKUSERSERVICE_H

#include <QHash>
#include <QString>

#include "IUserService.h"

namespace ncs {
namespace client {

class MockUserService : public IUserService {
public:
    LoginResult requestCode(const QString& phone) override;
    LoginResult login(const QString& phone, const QString& code) override;

private:
    static QHash<QString, User>& users();
};

}  // namespace client
}  // namespace ncs

#endif  // NCS_CLIENT_SERVICES_MOCKUSERSERVICE_H
