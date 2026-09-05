// 端到端联测：线程里起真实后端(BackendApp/httplib)，Http 客户端服务异步回调覆盖契约。
#include <QtTest>
#include <memory>
#include <thread>
#include <chrono>
#include <unistd.h>

#include "services/HttpUserService.h"
#include "services/HttpStationService.h"
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
    void stationsThroughService();

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

static QString baseUrl(int port) {
    return QStringLiteral("http://127.0.0.1:%1").arg(port);
}

void TstE2e::sendCodeOkAndHint() {
    ncs::client::HttpUserService svc(baseUrl(port_));
    bool done = false;
    ncs::client::LoginResult r;
    svc.requestCode(QStringLiteral("13800138000"),
                    [&](const ncs::client::LoginResult& x) { r = x; done = true; });
    QTRY_VERIFY_WITH_TIMEOUT(done, 3000);
    QVERIFY(r.ok);
    QVERIFY(r.message.contains(QStringLiteral("123456")));
}

void TstE2e::invalidPhoneFails() {
    ncs::client::HttpUserService svc(baseUrl(port_));
    bool done = false;
    ncs::client::LoginResult r;
    svc.requestCode(QStringLiteral("123"),
                    [&](const ncs::client::LoginResult& x) { r = x; done = true; });
    QTRY_VERIFY_WITH_TIMEOUT(done, 3000);
    QVERIFY(!r.ok);
    QVERIFY(r.message.contains(QStringLiteral("11 位手机号")));
}

void TstE2e::loginRegistersAndIdempotent() {
    ncs::client::HttpUserService svc(baseUrl(port_));
    bool d1 = false, d2 = false;
    ncs::client::LoginResult r1, r2;
    svc.login(QStringLiteral("13911112222"), QStringLiteral("123456"),
              [&](const ncs::client::LoginResult& x) { r1 = x; d1 = true; });
    QTRY_VERIFY_WITH_TIMEOUT(d1, 3000);
    QVERIFY2(r1.ok, qPrintable(r1.message));
    QCOMPARE(r1.user.phone, QStringLiteral("13911112222"));
    QCOMPARE(r1.user.balanceCents, ncs::MoneyCents(0));

    svc.login(QStringLiteral("13911112222"), QStringLiteral("123456"),
              [&](const ncs::client::LoginResult& x) { r2 = x; d2 = true; });
    QTRY_VERIFY_WITH_TIMEOUT(d2, 3000);
    QVERIFY(r2.ok);
    QCOMPARE(r2.user.id, r1.user.id);
}

void TstE2e::wrongCodeFails() {
    ncs::client::HttpUserService svc(baseUrl(port_));
    bool done = false;
    ncs::client::LoginResult r;
    svc.login(QStringLiteral("13911112222"), QStringLiteral("000000"),
              [&](const ncs::client::LoginResult& x) { r = x; done = true; });
    QTRY_VERIFY_WITH_TIMEOUT(done, 3000);
    QVERIFY(!r.ok);
    QVERIFY(!r.message.isEmpty());
}

void TstE2e::stationsThroughService() {
    ncs::client::HttpStationService svc(baseUrl(port_));
    bool done = false;
    QVector<ncs::Station> stations;
    QString err;
    svc.listStations([&](const QVector<ncs::Station>& st, const QString& e) {
        stations = st;
        err = e;
        done = true;
    });
    QTRY_VERIFY_WITH_TIMEOUT(done, 3000);
    QVERIFY2(err.isEmpty(), qPrintable(err));
    QCOMPARE(stations.size(), 3);
    QVERIFY(stations.first().freePiles > 0);

    bool d2 = false;
    QVector<ncs::Device> devices;
    svc.listDevices(stations.first().id,
                    [&](const QVector<ncs::Device>& dv, const QString& e) {
                        devices = dv;
                        err = e;
                        d2 = true;
                    });
    QTRY_VERIFY_WITH_TIMEOUT(d2, 3000);
    QVERIFY2(err.isEmpty(), qPrintable(err));
    QVERIFY(devices.size() >= 2);
}

#include "tst_e2e.moc"
QTEST_MAIN(TstE2e)
