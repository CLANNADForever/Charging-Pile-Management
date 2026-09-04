// C 端业务服务接口。UI/View 只依赖本接口，不依赖具体实现。
// 阶段一注入 MockUserService；接后端后替换为 HttpUserService(接口保持不变)。
#ifndef NCS_CLIENT_SERVICES_IUSERSERVICE_H
#define NCS_CLIENT_SERVICES_IUSERSERVICE_H

#include <QString>

#include "entities.h"

namespace ncs {
namespace client {

struct LoginResult {
    bool ok = false;
    QString message;  // 面向用户的提示
    User user;
};

class IUserService {
public:
    virtual ~IUserService() = default;
    // 发送验证码；phone 须为 11 位大陆手机号
    virtual LoginResult requestCode(const QString& phone) = 0;
    // 用验证码登录；号码不存在则自动注册
    virtual LoginResult login(const QString& phone, const QString& code) = 0;
};

}  // namespace client
}  // namespace ncs

#endif  // NCS_CLIENT_SERVICES_IUSERSERVICE_H
