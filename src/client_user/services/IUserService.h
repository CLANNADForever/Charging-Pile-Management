// C 端业务服务接口(异步回调风格，避免阻塞 UI；充电页实时刷新依赖此约定)。
#ifndef NCS_CLIENT_SERVICES_IUSERSERVICE_H
#define NCS_CLIENT_SERVICES_IUSERSERVICE_H

#include <functional>

#include <QString>

#include "entities.h"

namespace ncs {
namespace client {

struct LoginResult {
    bool ok = false;
    QString message;  // 面向用户的提示
    User user;
};
using LoginCallback = std::function<void(const LoginResult&)>;

class IUserService {
public:
    virtual ~IUserService() = default;
    virtual void requestCode(const QString& phone, LoginCallback done) = 0;
    virtual void login(const QString& phone, const QString& code,
                       LoginCallback done) = 0;
};

}  // namespace client
}  // namespace ncs

#endif  // NCS_CLIENT_SERVICES_IUSERSERVICE_H
