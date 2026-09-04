// Mock 实现：账本为实例成员(每个 service 自持)，验证码固定 123456，
// 供无后端阶段驱动 UI 与测试；测试间互不串状态。
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

    void clear() { users_.clear(); }  // 便于测试重置

private:
    QHash<QString, User> users_;
};

}  // namespace client
}  // namespace ncs

#endif  // NCS_CLIENT_SERVICES_MOCKUSERSERVICE_H
