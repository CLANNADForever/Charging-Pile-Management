#ifndef NCS_BACKEND_CORE_AUTHSERVICE_H
#define NCS_BACKEND_CORE_AUTHSERVICE_H

#include <QString>

#include "entities.h"

namespace ncs {
namespace backend {

class Store;

struct AuthReply {
    bool ok = false;
    QString message;
    ncs::User user;
};

// 免密登录业务(校验/注册/风控)。网络层 HttpServer 调用，可被单测直连。
class AuthService {
public:
    explicit AuthService(Store* store) : store_(store) {}

    // 校验手机号并返回"已下发验证码"提示(演示：不真发短信)
    AuthReply sendCode(const QString& phone) const;

    // 校验验证码；号码不存在自动注册；冻结用户拒绝登录
    AuthReply login(const QString& phone, const QString& code);

private:
    Store* store_;
};

}  // namespace backend
}  // namespace ncs

#endif  // NCS_BACKEND_CORE_AUTHSERVICE_H
