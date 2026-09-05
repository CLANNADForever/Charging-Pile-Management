#include "MockUserService.h"

#include <QMutexLocker>

#include "phone.h"

namespace ncs {
namespace client {

void MockUserService::requestCode(const QString& phone, LoginCallback done) {
    LoginResult r;
    if (!ncs::is_valid_phone11(phone)) {
        r.message = QStringLiteral("请输入 11 位手机号");
        done(r);
        return;
    }
    r.ok = true;
    r.message = QStringLiteral("验证码已发送（模拟）：") + ncs::demo_sms_code();
    done(r);
}

void MockUserService::login(const QString& phone, const QString& code,
                            LoginCallback done) {
    LoginResult r;
    if (!ncs::is_valid_phone11(phone)) {
        r.message = QStringLiteral("请输入 11 位手机号");
        done(r);
        return;
    }
    if (code != ncs::demo_sms_code()) {
        r.message = QStringLiteral("验证码错误");
        done(r);
        return;
    }

    QMutexLocker locker(&mutex_);
    User& u = users_[phone];
    if (u.phone.isEmpty()) {
        u.phone = phone;
        u.nickname = QStringLiteral("充电用户") + phone.right(4);
        u.balanceCents = 0;
    }
    r.ok = true;
    r.message = QStringLiteral("登录成功");
    r.user = u;
    done(r);
}

void MockUserService::recharge(const QString& phone, ncs::MoneyCents amountCents,
                                 LoginCallback done) {
    LoginResult r;
    QMutexLocker locker(&mutex_);
    auto it = users_.find(phone);
    if (it == users_.end()) {
        r.message = QStringLiteral("请先登录");
        done(r);
        return;
    }
    it->balanceCents += amountCents;
    r.ok = true;
    r.message = QStringLiteral("充值成功");
    r.user = it.value();
    done(r);
}

void MockUserService::setNickname(const QString& phone, const QString& nickname,
                                  LoginCallback done) {
    LoginResult r;
    QMutexLocker locker(&mutex_);
    auto it = users_.find(phone);
    if (it == users_.end()) {
        r.message = QStringLiteral("请先登录");
        done(r);
        return;
    }
    it->nickname = nickname;
    r.ok = true;
    r.user = it.value();
    done(r);
}

void MockUserService::uploadAvatar(const QString&, const QByteArray&,
                                   AvatarCallback done) {
    done(QString(), QString());
}

void MockUserService::downloadAvatar(const QString&, BytesCallback done) {
    done(QByteArray());
}

void MockUserService::clear() {
    QMutexLocker locker(&mutex_);
    users_.clear();
}

}  // namespace client
}  // namespace ncs
