#include "MockUserService.h"

#include <QRegularExpression>

namespace ncs {
namespace client {

namespace {
const QRegularExpression& phoneRe() {
    static const QRegularExpression re(QStringLiteral("^1[0-9]{10}$"));
    return re;
}
const QString kMockCode = QStringLiteral("123456");
}  // namespace

QHash<QString, User>& MockUserService::users() {
    static QHash<QString, User> store;
    return store;
}

LoginResult MockUserService::requestCode(const QString& phone) {
    LoginResult r;
    if (!phoneRe().match(phone).hasMatch()) {
        r.message = QStringLiteral("请输入 11 位手机号");
        return r;
    }
    r.ok = true;
    r.message = QStringLiteral("验证码已发送（模拟）：") + kMockCode;
    return r;
}

LoginResult MockUserService::login(const QString& phone, const QString& code) {
    LoginResult r;
    if (!phoneRe().match(phone).hasMatch()) {
        r.message = QStringLiteral("请输入 11 位手机号");
        return r;
    }
    if (code != kMockCode) {
        r.message = QStringLiteral("验证码错误");
        return r;
    }
    User& u = users()[phone];  // 不存在则自动注册新用户
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

}  // namespace client
}  // namespace ncs
