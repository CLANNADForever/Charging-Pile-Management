// 端到端联测：线程起真实后端(HTTP + 模拟器 TCP)，Http 服务异步回调覆盖登录/站桩/充电闭环。
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <QtTest>
#include <memory>
#include <thread>
#include <chrono>
#include <cstring>

#include "services/HttpUserService.h"
#include "services/HttpStationService.h"
#include "services/HttpChargeService.h"
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
    void chargeFullCircle();

private:
    QString dbPath_;
    std::unique_ptr<ncs::backend::BackendApp> app_;
    int port_ = 0;
    int simPort_ = 0;
    std::thread th_;
};

void TstE2e::initTestCase() {
    dbPath_ = QStringLiteral("/tmp/ncs_e2e_%1.db").arg(::getpid());
    app_ = std::make_unique<ncs::backend::BackendApp>(dbPath_);
    QVERIFY2(app_->init(), qPrintable(app_->lastError()));
    port_ = app_->server().bind_to_any_port("127.0.0.1");
    th_ = std::thread([this] { app_->server().listen_after_bind(); });

    const int probe = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in a{};
    a.sin_family = AF_INET;
    a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    a.sin_port = 0;
    bind(probe, reinterpret_cast<sockaddr*>(&a), sizeof(a));
    socklen_t alen = sizeof(a);
    getsockname(probe, reinterpret_cast<sockaddr*>(&a), &alen);
    simPort_ = ntohs(a.sin_port);
    close(probe);
    QVERIFY(app_->startSimListener(simPort_));
    std::this_thread::sleep_for(std::chrono::milliseconds(250));
}

void TstE2e::cleanupTestCase() {
    if (app_) {
        app_->server().stop();
        app_->stopSimListener();
        th_.join();
    }
    ::unlink(dbPath_.toLocal8Bit().constData());
}

static QString baseUrl(int port) {
    return QStringLiteral("http://127.0.0.1:%1").arg(port);
}

static int simConnect(int port) {
    const int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0)
        return -1;
    sockaddr_in a{};
    a.sin_family = AF_INET;
    a.sin_port = htons(static_cast<uint16_t>(port));
    inet_pton(AF_INET, "127.0.0.1", &a.sin_addr);
    if (connect(fd, reinterpret_cast<sockaddr*>(&a), sizeof(a)) != 0) {
        close(fd);
        return -1;
    }
    return fd;
}
static bool simSend(int fd, const std::string& line) {
    return send(fd, line.data(), line.size(), MSG_NOSIGNAL) >= 0;
}
static void loginWait(int port, const QString& phone) {
    ncs::client::HttpUserService svc(baseUrl(port));
    bool done = false;
    ncs::client::LoginResult r;
    svc.login(phone, QStringLiteral("123456"),
              [&](const ncs::client::LoginResult& x) { r = x; done = true; });
    QTRY_VERIFY_WITH_TIMEOUT(done, 3000);
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
}

void TstE2e::loginRegistersAndIdempotent() {
    ncs::client::HttpUserService svc(baseUrl(port_));
    bool d1 = false, d2 = false;
    ncs::client::LoginResult r1, r2;
    svc.login(QStringLiteral("13911112222"), QStringLiteral("123456"),
              [&](const ncs::client::LoginResult& x) { r1 = x; d1 = true; });
    QTRY_VERIFY_WITH_TIMEOUT(d1, 3000);
    QVERIFY(r1.ok);
    svc.login(QStringLiteral("13911112222"), QStringLiteral("123456"),
              [&](const ncs::client::LoginResult& x) { r2 = x; d2 = true; });
    QTRY_VERIFY_WITH_TIMEOUT(d2, 3000);
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
}

void TstE2e::stationsThroughService() {
    ncs::client::HttpStationService svc(baseUrl(port_));
    bool done = false;
    QVector<ncs::Station> stations;
    QString err;
    svc.listStations([&](const QVector<ncs::Station>& st, const QString& e) {
        stations = st; err = e; done = true;
    });
    QTRY_VERIFY_WITH_TIMEOUT(done, 3000);
    QVERIFY2(err.isEmpty(), qPrintable(err));
    QCOMPARE(stations.size(), 3);
}

void TstE2e::chargeFullCircle() {
    const QString phone = QStringLiteral("13900009999");
    loginWait(port_, phone);

    ncs::client::HttpChargeService svc(baseUrl(port_));
    const int fd = simConnect(simPort_);
    QVERIFY(fd >= 0);
    QVERIFY(simSend(fd, "{\"type\":\"register\",\"devices\":[1]}\n"));

    bool d1 = false;
    ncs::client::OrderResult r1;
    svc.reserve(phone, 1, [&](const ncs::client::OrderResult& x) { r1 = x; d1 = true; });
    QTRY_VERIFY_WITH_TIMEOUT(d1, 3000);
    QVERIFY2(r1.ok, qPrintable(r1.message));
    QCOMPARE(r1.order.status, ncs::OrderStatus::Reserved);
    QCOMPARE(r1.order.unitPriceCents, ncs::MoneyCents(200));

    bool d2 = false;
    ncs::client::OrderResult r2;
    svc.reserve(phone, 1, [&](const ncs::client::OrderResult& x) { r2 = x; d2 = true; });
    QTRY_VERIFY_WITH_TIMEOUT(d2, 3000);
    QVERIFY(!r2.ok);  // 双预约被拦

    bool d3 = false;
    ncs::client::OrderResult r3;
    svc.start(r1.order.id, [&](const ncs::client::OrderResult& x) { r3 = x; d3 = true; });
    QTRY_VERIFY_WITH_TIMEOUT(d3, 3000);
    QVERIFY2(r3.ok, qPrintable(r3.message));

    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    QVERIFY(simSend(fd, "{\"type\":\"heartbeat\",\"device_id\":1,\"energy_kwh\":2.5,"
                         "\"voltage\":220,\"current\":100,\"temperature\":30}\n"));
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    bool d4 = false;
    ncs::client::LiveInfo live;
    svc.live(r1.order.id, [&](const ncs::client::LiveInfo& x) { live = x; d4 = true; });
    QTRY_VERIFY_WITH_TIMEOUT(d4, 3000);
    QVERIFY(live.ok);
    QVERIFY2(qAbs(live.energyKwh - 2.5) < 1e-6,
             qPrintable(QString::number(live.energyKwh)));
    QCOMPARE(live.amountCents, ncs::MoneyCents(500));  // 2.5*200

    bool d5 = false;
    ncs::client::OrderResult r5;
    svc.finish(r1.order.id, [&](const ncs::client::OrderResult& x) { r5 = x; d5 = true; });
    QTRY_VERIFY_WITH_TIMEOUT(d5, 3000);
    QVERIFY2(r5.ok, qPrintable(r5.message));
    QCOMPARE(r5.order.status, ncs::OrderStatus::Completed);
    QCOMPARE(r5.order.amountCents, ncs::MoneyCents(500));

    // 未支付账单 -> 禁止开新桩
    bool d6 = false;
    ncs::client::OrderResult g;
    svc.reserve(phone, 2, [&](const ncs::client::OrderResult& x) { g = x; d6 = true; });
    QTRY_VERIFY_WITH_TIMEOUT(d6, 3000);
    QVERIFY(!g.ok);

    // 支付 -> Paid
    bool d7 = false;
    ncs::client::OrderResult pay;
    svc.pay(r1.order.id, [&](const ncs::client::OrderResult& x) { pay = x; d7 = true; });
    QTRY_VERIFY_WITH_TIMEOUT(d7, 3000);
    QVERIFY2(pay.ok, qPrintable(pay.message));
    QCOMPARE(pay.order.status, ncs::OrderStatus::Paid);

    // 支付后允许再开，再取消
    bool d8 = false;
    ncs::client::OrderResult r6;
    svc.reserve(phone, 2, [&](const ncs::client::OrderResult& x) { r6 = x; d8 = true; });
    QTRY_VERIFY_WITH_TIMEOUT(d8, 3000);
    QVERIFY(r6.ok);
    bool d9 = false;
    ncs::client::OrderResult r7;
    svc.cancel(r6.order.id, [&](const ncs::client::OrderResult& x) { r7 = x; d9 = true; });
    QTRY_VERIFY_WITH_TIMEOUT(d9, 3000);
    QVERIFY2(r7.ok, qPrintable(r7.message));

    close(fd);
}

#include "tst_e2e.moc"
QTEST_MAIN(TstE2e)
