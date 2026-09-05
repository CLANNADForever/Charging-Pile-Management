// 端到端联测：线程里起真实 ncs 后端(BackendApp/httplib)，HttpUserService 走
// HTTP 完成 send-code/login，验证客户端服务接口与后端契约一致。
#include <QtTest>
#include <thread>
#include <chrono>
#include <memory>
#include <unistd.h>

#include "services/HttpUserService.h"
#include "BackendApp.h"

class TstE2e : public QObject {
    Q_OBJECT
private slots:
    void initTestCase();
    void cleanupTestCase();
    void sendCodeOkAndHint();
    void invalidPhoneFails();
    void loginRegistersAndIdempotent();
    void wrongCodeFails();

private:
    QString dbPath_;
    std::unique_ptr<ncs::backend::BackendApp> app_;
    int port_ = 0;
    std::thread th_;
};

void TstE2e::initTestCase() {
    dbPath_ = QStringLiteral("/tmp/ncs_e2e_%1.db").arg(::getpid());
    app_ = std::make_unique<ncs::backend::BackendApp>(dbPath_);
    QVERIFY2(app_->init(), qPrintable(app_->lastError()));
    port_ = app_->server().bind_to_any_port("127.0.0.1");
    th_ = std::thread([this] { app_->server().listen_after_bind(); });
    std::this_thread::sleep_for(std::chrono::milliseconds(250));
}

void TstE2e::cleanupTestCase() {
    if (app_) {
        app_->server().stop();
        th_.join();
    }
    ::unlink(dbPath_.toLocal8Bit().constData());
}

static ncs::client::HttpUserService makeSvc(int port) {
    return ncs::client::HttpUserService(
        QStringLiteral("http://127.0.0.1:%1").arg(port));
}

void TstE2e::sendCodeOkAndHint() {
    auto svc = makeSvc(port_);
    const auto r = svc.requestCode(QStringLiteral("13800138000"));
    QVERIFY(r.ok);
    QVERIFY(r.message.contains(QStringLiteral("123456")));
}

void TstE2e::invalidPhoneFails() {
    auto svc = makeSvc(port_);
    const auto r = svc.requestCode(QStringLiteral("123"));
    QVERIFY(!r.ok);
    QVERIFY(r.message.contains(QStringLiteral("11 位手机号")));
}

void TstE2e::loginRegistersAndIdempotent() {
    auto svc = makeSvc(port_);
    const auto r1 = svc.login(QStringLiteral("13911112222"),
                              QStringLiteral("123456"));
    QVERIFY2(r1.ok, qPrintable(r1.message));
    QCOMPARE(r1.user.phone, QStringLiteral("13911112222"));
    QCOMPARE(r1.user.balanceCents, ncs::MoneyCents(0));
    QVERIFY(!r1.user.nickname.isEmpty());

    const auto r2 = svc.login(QStringLiteral("13911112222"),
                              QStringLiteral("123456"));
    QVERIFY(r2.ok);
    QCOMPARE(r2.user.id, r1.user.id);  // 幂等：同一用户
}

void TstE2e::wrongCodeFails() {
    auto svc = makeSvc(port_);
    const auto r = svc.login(QStringLiteral("13911112222"),
                             QStringLiteral("000000"));
    QVERIFY(!r.ok);
    QVERIFY(!r.message.isEmpty());
}

#include "tst_e2e.moc"

QTEST_MAIN(TstE2e)
