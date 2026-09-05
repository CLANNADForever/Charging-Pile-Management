// Mock 实现：账本为实例成员，验证码固定 123456；回调风格(立即同步回调)。
#ifndef NCS_CLIENT_SERVICES_MOCKUSERSERVICE_H
#define NCS_CLIENT_SERVICES_MOCKUSERSERVICE_H

#include <QHash>
#include <QMutex>
#include <QString>

#include "IUserService.h"

namespace ncs {
namespace client {

class MockUserService : public IUserService {
public:
    void requestCode(const QString& phone, LoginCallback done) override;
    void login(const QString& phone, const QString& code,
               LoginCallback done) override;

    void clear();

private:
    QHash<QString, User> users_;
    mutable QMutex mutex_;
};

}  // namespace client
}  // namespace ncs

#endif  // NCS_CLIENT_SERVICES_MOCKUSERSERVICE_H
