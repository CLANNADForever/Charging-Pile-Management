#include "AuthService.h"

#include "database/Store.h"
#include "phone.h"

namespace ncs {
namespace backend {

AuthReply AuthService::sendCode(const QString& phone) const {
    AuthReply r;
    if (!ncs::is_valid_phone11(phone)) {
        r.message = QStringLiteral("请输入 11 位手机号");
        return r;
    }
    r.ok = true;
    r.message = QStringLiteral("验证码已发送（模拟）：") + ncs::demo_sms_code();
    return r;
}

AuthReply AuthService::login(const QString& phone, const QString& code) {
    AuthReply r;
    if (!ncs::is_valid_phone11(phone)) {
        r.message = QStringLiteral("请输入 11 位手机号");
        return r;
    }
    if (code != ncs::demo_sms_code()) {
        r.message = QStringLiteral("验证码错误");
        return r;
    }
    if (!store_->ensureUserByPhone(phone, &r.user)) {
        r.message = QStringLiteral("数据库错误");
        return r;
    }
    if (r.user.status == ncs::UserStatus::Frozen) {
        r.message = QStringLiteral("账号已冻结，请联系客服");
        return r;
    }
    r.ok = true;
    r.message = QStringLiteral("登录成功");
    return r;
}

}  // namespace backend
}  // namespace ncs
