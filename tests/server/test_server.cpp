// NCS 后端集成测试：临时 SQLite + Store/AuthService + 并发 + 真实 HTTP。
#include <cstdio>
#include <cstdlib>
#include <atomic>
#include <thread>
#include <vector>
#include <chrono>
#include <unistd.h>

#include <QString>

#include "BackendApp.h"
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
QString phoneFor(int i) {  // 唯一合法 11 位号
    return QStringLiteral("139") + QString::number(10000000 + i);
}
}  // namespace

int main() {
    // ---- 1) Store 直测 ----
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

    // ---- 2) 并发：多线程自动注册不同号 + 同号并发不崩/不重复 ----
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
                  "8 threads auto-register distinct phones (no crash/loss)");
        }
        {
            // 同号并发：只会注册 1 条
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
                  "same phone concurrent -> all ok, no duplicate (count 9)");
        }
    }

    // ---- 3) HTTP 服务端到端 ----
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
                  health->body.find("\"ok\":true") != std::string::npos,
              "GET /health -> ok");

        auto sendOk = cli.Post("/api/auth/send-code",
                               "{\"phone\":\"13800138000\"}",
                               "application/json");
        check(sendOk && sendOk->body.find("123456") != std::string::npos,
              "send-code ok phone -> hint with code");

        auto sendBad = cli.Post("/api/auth/send-code",
                                "{\"phone\":\"123\"}",
                                "application/json");
        check(sendBad && sendBad->body.find("\"ok\":false") != std::string::npos,
              "send-code invalid phone -> ok=false");

        auto loginGood = cli.Post("/api/auth/login",
                                  "{\"phone\":\"13800138000\",\"code\":\"123456\"}",
                                  "application/json");
        check(loginGood && loginGood->status == 200 &&
                  loginGood->body.find("\"ok\":true") != std::string::npos &&
                  loginGood->body.find("\"balance_cents\":0") != std::string::npos,
              "login correct code -> ok + user.balance_cents 0");

        auto loginBad = cli.Post("/api/auth/login",
                                 "{\"phone\":\"13800138000\",\"code\":\"000000\"}",
                                 "application/json");
        check(loginBad && loginBad->body.find("\"ok\":false") != std::string::npos,
              "login wrong code -> ok=false");

        auto stations = cli.Get("/api/stations");
        check(stations && stations->status == 200 &&
                  stations->body.find(R"("name":"望京充电站")") != std::string::npos &&
                  stations->body.find(R"("name":"亦庄超充站")") != std::string::npos,
              "GET /api/stations -> seeded stations present");

        auto devices = cli.Get("/api/stations/1/devices");
        check(devices && devices->status == 200 &&
                  devices->body.find(R"("station_id":1)") != std::string::npos,
              "GET /api/stations/1/devices -> devices present");

        auto devicesNone = cli.Get("/api/stations/9999/devices");
        check(devicesNone && devicesNone->status == 200 &&
                  devicesNone->body == "[]",
              "GET unknown station devices -> empty array");

        app.server().stop();
        th.join();
    }

    ::unlink(storeDb.toLocal8Bit().constData());
    ::unlink(concDb.toLocal8Bit().constData());
    ::unlink(appDb.toLocal8Bit().constData());

    if (g_fail == 0) {
        std::printf("backend tests: ALL PASS\n");
        return 0;
    }
    std::printf("backend tests: %d FAILED\n", g_fail);
    return 1;
}
