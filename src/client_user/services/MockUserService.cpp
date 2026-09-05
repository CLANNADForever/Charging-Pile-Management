#include "MockUserService.h"

#include <QMutexLocker>

#include "phone.h"

namespace ncs {
namespace client {

LoginResult MockUserService::requestCode(const QString& phone) {
    LoginResult r;
    if (!ncs::is_valid_phone11(phone)) {
        r.message = QStringLiteral("请输入 11 位手机号");
        return r;
    }
    r.ok = true;
    r.message = QStringLiteral("验证码已发送（模拟）：") + ncs::demo_sms_code();
    return r;
}

LoginResult MockUserService::login(const QString& phone, const QString& code) {
    LoginResult r;
    if (!ncs::is_valid_phone11(phone)) {
        r.message = QStringLiteral("请输入 11 位手机号");
        return r;
    }
    if (code != ncs::demo_sms_code()) {
        r.message = QStringLiteral("验证码错误");
        return r;
    }

    QMutexLocker locker(&mutex_);
    User& u = users_[phone];  // 不存在则自动注册新用户
    if (u.phone.isEmpty()) {
        u.phone = phone;
        u.nickname = QStringLiteral("充电用户") + phone.right(4);
        u.balanceCents = 0;
    }
    r.ok = true;
    r.message = QStringLiteral("登录成功");
    r.user = u;
    return r;
}

void MockUserService::clear() {
    QMutexLocker locker(&mutex_);
    users_.clear();
}

}  // namespace client
}  // namespace ncs
