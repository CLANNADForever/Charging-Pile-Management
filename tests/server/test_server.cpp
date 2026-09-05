// NCS 后端集成测试：Store/SQLite + 并发 + HTTP + 模拟器 TCP。
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <thread>
#include <vector>

#include <QString>

#include "BackendApp.h"
#include "core/ChargeService.h"
#include "httplib.h"

namespace {
int g_fail = 0;
void check(bool ok, const char* name) {
    if (ok)
        std::printf("[ok]   %s\n", name);
    else {
        std::printf("[FAIL] %s\n", name);
        ++g_fail;
    }
}
QString tempDb(const char* tag) {
    return QStringLiteral("/tmp/ncs_ut_%1_%2.db").arg(tag).arg(::getpid());
}
QString phoneFor(int i) {
    return QStringLiteral("139") + QString::number(10000000 + i);
}
int freePort() {
    const int s = socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0)
        return -1;
    sockaddr_in a{};
    a.sin_family = AF_INET;
    a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    a.sin_port = 0;
    if (bind(s, reinterpret_cast<sockaddr*>(&a), sizeof(a)) != 0) {
        close(s);
        return -1;
    }
    socklen_t alen = sizeof(a);
    getsockname(s, reinterpret_cast<sockaddr*>(&a), &alen);
    const int port = ntohs(a.sin_port);
    close(s);
    return port;
}
bool sendLine(int fd, const char* line) {
    const ssize_t n = send(fd, line, std::char_traits<char>::length(line), MSG_NOSIGNAL);
    return n >= 0;
}
}  // namespace

int main() {
    // 1) Store 直测
    const QString storeDb = tempDb("store");
    {
        ncs::backend::Store st;
        check(st.open(storeDb), "store.open(tmp db)");
        check(st.countUsers() == 0, "countUsers()==0 on fresh");
        ncs::User u;
        check(st.ensureUserByPhone(QStringLiteral("13800138000"), &u),
              "ensureUserByPhone registers new user");
        check(u.balanceCents == 0, "new user balance 0");
        ncs::User again;
        check(st.ensureUserByPhone(QStringLiteral("13800138000"), &again) &&
                  again.id == u.id,
              "ensureUserByPhone idempotent for same phone");
        check(st.countUsers() == 1, "countUsers()==1");
        check(st.setBalanceCents(u.id, ncs::MoneyCents(500)), "setBalanceCents");
        ncs::User found;
        check(st.findUserByPhone(QStringLiteral("13800138000"), &found) &&
                  found.balanceCents == 500,
              "findUserByPhone reflects new balance");
    }

    // 2) 并发
    const QString concDb = tempDb("conc");
    {
        ncs::backend::Store st;
        check(st.open(concDb), "conc store.open");
        {
            std::atomic<int> ok{0};
            std::vector<std::thread> ts;
            for (int i = 0; i < 8; ++i) {
                ts.emplace_back([&, i] {
                    ncs::User u;
                    if (st.ensureUserByPhone(phoneFor(i), &u))
                        ++ok;
                });
            }
            for (auto& t : ts)
                t.join();
            check(ok == 8 && st.countUsers() == 8,
                  "8 threads auto-register distinct phones");
        }
        {
            std::atomic<int> ok{0};
            std::vector<std::thread> ts;
            for (int i = 0; i < 8; ++i) {
                ts.emplace_back([&] {
                    ncs::User u;
                    if (st.ensureUserByPhone(QStringLiteral("13912345678"), &u))
                        ++ok;
                });
            }
            for (auto& t : ts)
                t.join();
            check(ok == 8 && st.countUsers() == 9,
                  "same phone concurrent -> no duplicate");
        }
    }

    // 3) HTTP 端到端 + 站/桩列表
    const QString appDb = tempDb("app");
    {
        ncs::backend::BackendApp app(appDb);
        check(app.init(), "BackendApp.init()");
        const int port = app.server().bind_to_any_port("127.0.0.1");
        std::thread th([&] { app.server().listen_after_bind(); });
        std::this_thread::sleep_for(std::chrono::milliseconds(250));

        httplib::Client cli("http://127.0.0.1:" + std::to_string(port));

        auto health = cli.Get("/health");
        check(health && health->status == 200 &&
                  health->body.find("\"code\":0") != std::string::npos &&
                  health->body.find("\"service\":\"ncs-backend\"") != std::string::npos,
              "GET /health -> ok");

        auto loginGood = cli.Post("/api/auth/login",
                                  "{\"phone\":\"13800138000\",\"code\":\"123456\"}",
                                  "application/json");
        check(loginGood && loginGood->body.find("\"code\":0") != std::string::npos &&
                  loginGood->body.find("\"balance_cents\":0") != std::string::npos,
              "login -> ok");

        auto stations = cli.Get("/api/stations");
        check(stations && stations->status == 200 &&
                  stations->body.find("\"code\":0") != std::string::npos &&
                  stations->body.find(R"("name":"望京充电站")") != std::string::npos &&
                  stations->body.find(R"("name":"亦庄超充站")") != std::string::npos,
              "GET /api/stations -> seeded stations");

        auto devices = cli.Get("/api/stations/1/devices");
        check(devices && devices->status == 200 &&
                  devices->body.find("\"code\":0") != std::string::npos &&
                  devices->body.find(R"("station_id":1)") != std::string::npos,
              "GET /api/stations/1/devices -> devices");

        app.server().stop();
        th.join();
    }

    // 4) 模拟器 TCP 心跳
    const QString simDb = tempDb("sim");
    {
        ncs::backend::BackendApp app(simDb);
        check(app.init(), "sim app.init");
        const int simPort = freePort();
        check(app.startSimListener(simPort), "startSimListener(free port)");

        const int fd = socket(AF_INET, SOCK_STREAM, 0);
        bool connected = false;
        if (fd >= 0) {
            sockaddr_in a{};
            a.sin_family = AF_INET;
            a.sin_port = htons(static_cast<uint16_t>(simPort));
            inet_pton(AF_INET, "127.0.0.1", &a.sin_addr);
            connected = connect(fd, reinterpret_cast<sockaddr*>(&a), sizeof(a)) == 0;
        }
        check(connected, "sim raw tcp connect");
        if (connected) {
            sendLine(fd, "{\"type\":\"heartbeat\",\"device_id\":1,\"voltage\":220.5,\"current\":10.2,\"temperature\":30.1}\n");
            sendLine(fd, "{\"type\":\"heartbeat\",\"device_id\":2,\"voltage\":219.0,\"current\":0.0,\"temperature\":28.0}\n");
            sendLine(fd, "{\"type\":\"heartbeat\",\"device_id\":7,\"voltage\":221.0,\"current\":12.0,\"temperature\":33.0}\n");
            close(fd);
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        }
        check(app.simHeartbeatCount() >= 3,
              "sim sink received >=3 heartbeats");
        check(app.simLastDeviceId() == 7, "sim sink last device 7");
        app.stopSimListener();
    }

    // 5) 充电最小闭环(ChargeService 直测；注入能量 2.0kWh)
    const QString chgDb = tempDb("chg");
    {
        ncs::backend::Store st;
        check(st.open(chgDb), "chg store.open");
        ncs::User u;
        st.ensureUserByPhone(QStringLiteral("13800138000"), &u);
        ncs::Device dev;
        check(st.getDeviceById(1, &dev) && dev.state == ncs::DeviceState::Idle,
              "chg: device1 idle");
        ncs::backend::ChargeService cs(&st, [](int, bool) {}, [](int) { return 2.0; });
        ncs::Order o;
        QString err;
        check(cs.reserve(QStringLiteral("13800138000"), 1, &o, &err),
              "chg: reserve ok");
        check(o.status == ncs::OrderStatus::Reserved && o.unitPriceCents == 200,
              "chg: order reserved + price snapshot 200");
        check(st.getDeviceById(1, &dev) && dev.state == ncs::DeviceState::Reserved,
              "chg: device Reserved");
        ncs::Station s1;
        st.getStationById(1, &s1);
        check(s1.freePiles == 1, "chg: station1 free ->1");
        ncs::Order o2;
        QString err2;
        check(!cs.reserve(QStringLiteral("13800138000"), 1, &o2, &err2),
              "chg: double reserve blocked");
        check(cs.start(o.id, &err), "chg: start ok");
        check(cs.finish(o.id, &err), "chg: finish ok");
        ncs::Order done;
        st.getOrderById(o.id, &done);
        check(done.status == ncs::OrderStatus::Completed && done.amountCents == 400 &&
                  done.energyKwh == 2.0,
              "chg: settled amount 400 (2.0kWh x 200)");
        ncs::User after;
        st.findUserByPhone(QStringLiteral("13800138000"), &after);
        check(after.balanceCents == -400, "chg: balance -400 (欠费)");
        check(st.getDeviceById(1, &dev) && dev.state == ncs::DeviceState::Idle,
              "chg: device back Idle");
        st.getStationById(1, &s1);
        check(s1.freePiles == 2, "chg: station1 free back 2");
    }
    ::unlink(chgDb.toLocal8Bit().constData());

    // 6) 预约取消 + 超时清扫
    const QString cnlDb = tempDb("cnl");
    {
        ncs::backend::Store st;
        check(st.open(cnlDb), "cnl store.open");
        ncs::User u;
        st.ensureUserByPhone(QStringLiteral("13800138000"), &u);
        ncs::backend::ChargeService cs(&st, [](int, bool) {}, [](int) { return 0.0; });
        ncs::Order o;
        QString err;
        check(cs.reserve(QStringLiteral("13800138000"), 2, &o, &err), "cnl: reserve dev2");
        ncs::Device dev;
        st.getDeviceById(2, &dev);
        check(dev.state == ncs::DeviceState::Reserved, "cnl: dev2 Reserved");
        check(cs.cancel(o.id, &err), "cnl: cancel ok");
        st.getDeviceById(2, &dev);
        ncs::Station s1;
        st.getStationById(1, &s1);
        check(dev.state == ncs::DeviceState::Idle && s1.freePiles == 2,
              "cnl: cancel releases dev2 + free 2");
        check(cs.reserve(QStringLiteral("13800138000"), 2, &o, &err), "cnl: reserve again");
        const int swept = cs.sweepExpiredReservations(-1);  // cutoff=now+1s，保证刚建的预约过期
        st.getDeviceById(2, &dev);
        st.getStationById(1, &s1);
        ncs::Order done;
        st.getOrderById(o.id, &done);
        check(swept == 1 && dev.state == ncs::DeviceState::Idle &&
                  s1.freePiles == 2 && done.status == ncs::OrderStatus::Canceled,
              "cnl: sweep(0) auto-cancels + releases");
    }
    ::unlink(cnlDb.toLocal8Bit().constData());

    ::unlink(storeDb.toLocal8Bit().constData());
    ::unlink(concDb.toLocal8Bit().constData());
    ::unlink(appDb.toLocal8Bit().constData());
    ::unlink(simDb.toLocal8Bit().constData());

    if (g_fail == 0) {
        std::printf("backend tests: ALL PASS\n");
        return 0;
    }
    std::printf("backend tests: %d FAILED\n", g_fail);
    return 1;
}
