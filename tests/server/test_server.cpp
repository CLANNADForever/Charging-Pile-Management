// NCS 后端集成测试：临时 SQLite 库 + 内存? 真实库；起服务打 HTTP。
#include <cstdio>
#include <cstdlib>
#include <thread>
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
}  // namespace

int main() {
    // ---- 1) Store 直测(独立实例 + 独立连接名) ----
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

    // ---- 2) HTTP 服务端到端 ----
    const QString appDb = tempDb("app");
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

        app.server().stop();
        th.join();
    ::unlink(storeDb.toLocal8Bit().constData());
    ::unlink(appDb.toLocal8Bit().constData());

    if (g_fail == 0) {
        std::printf("backend tests: ALL PASS\n");
        return 0;
    }
    std::printf("backend tests: %d FAILED\n", g_fail);
    return 1;
}
