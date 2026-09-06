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
#include <QStringList>
#include <sqlite3.h>

#include "BackendApp.h"
#include <nlohmann/json.hpp>
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

        // 4 新端点 HTTP 覆盖(充值/昵称/历史/头像)
        auto rc = cli.Post("/api/wallet/recharge",
                           "{\"phone\":\"13800138000\",\"amount_cents\":250}",
                           "application/json");
        check(rc && rc->body.find("\"code\":0") != std::string::npos &&
                  rc->body.find("\"balance_cents\":250") != std::string::npos,
              "http: recharge +250");

        auto pr = cli.Patch("/api/user/profile",
                            "{\"phone\":\"13800138000\",\"nickname\":\"HTTP阿甘\"}",
                            "application/json");
        check(pr && pr->body.find("\"code\":0") != std::string::npos &&
                  pr->body.find("\"nickname\"") != std::string::npos,
              "http: nickname PATCH ok");

        auto rb = cli.Post("/api/orders",
                           "{\"phone\":\"13800138000\",\"device_id\":1}",
                           "application/json");
        const bool rOk = rb && rb->body.find("\"code\":0") != std::string::npos;
        int oid = 0;
        if (rOk) {  // 简单解析订单 id
            const auto j = nlohmann::json::parse(rb->body);
            oid = j["data"]["id"].get<int>();
        }
        check(rOk, "http: reserve device1");
        if (oid > 0) {
            cli.Post("/api/orders/" + std::to_string(oid) + "/start", "{}",
                     "application/json");
            cli.Post("/api/orders/" + std::to_string(oid) + "/finish", "{}",
                     "application/json");
            cli.Post("/api/orders/" + std::to_string(oid) + "/pay", "{}",
                     "application/json");
        }
        auto hs = cli.Get("/api/orders/history?phone=13800138000&limit=10&offset=0");
        check(hs && hs->body.find("\"total\":1") != std::string::npos,
              "http: history total 1 after paid order");

        static const unsigned char kPng[8] = {0x89, 'P', 'N', 'G', 0x0d, 0x0a, 0x1a, 0x0a};
        auto av = cli.Post("/api/user/avatar?phone=13800138000&ext=png",
                           std::string(reinterpret_cast<const char*>(kPng), 8), "application/octet-stream");
        check(av && av->body.find("\"code\":0") != std::string::npos &&
                  av->body.find("avatar_13800138000.png") != std::string::npos,
              "http: avatar png upload");

        // ========== 管理端(B2) RBAC / 鉴权 / 审计 / 监控 / 统计 HTTP 覆盖 ==========
        const int simPort2 = freePort();
        check(app.startSimListener(simPort2), "b2: sim listener start");
        app.startRestartSweeper(1);  // 重启超时 1s 自恢复(离线兜底)

        auto lg = cli.Post("/api/admin/login",
                           "{\"username\":\"admin\",\"password\":\"admin123\"}",
                           "application/json");
        check(lg && lg->body.find("\"code\":0") != std::string::npos &&
                  lg->body.find("\"role\":\"super\"") != std::string::npos,
              "admin: login super + role");
        std::string adminTok;
        { const auto j = nlohmann::json::parse(lg->body); adminTok = j["data"]["token"].get<std::string>(); }
        check(!adminTok.empty(), "admin: got token");
        auto auth = [&] { httplib::Headers h; h.emplace("Authorization", "Bearer " + adminTok); return h; };
        auto getAuth = [&](const std::string& p) { return cli.Get(p, auth()); };
        auto postAuth = [&](const std::string& p, const std::string& b) {
            return cli.Post(p, auth(), b, "application/json"); };
        auto delAuth = [&](const std::string& p) {
            return cli.Delete(p, auth(), "", "application/json"); };

        // 未认证 / 伪 token 拒绝
        auto unauth = cli.Get("/api/admin/users");
        check(unauth && unauth->body.find("\"code\":3") != std::string::npos,
              "admin: no token -> code3(401)");
        httplib::Headers fake;
        fake.emplace("Authorization", "Bearer deadbeef");
        auto fakeR = cli.Get("/api/admin/users", fake);
        check(fakeR && fakeR->body.find("\"code\":3") != std::string::npos,
              "admin: bad token -> code3(401)");

        // 用户搜索 + 冻结/解冻 + 状态筛选
        auto users = getAuth("/api/admin/users?phone=13800138000");
        check(users && users->body.find("\"code\":0") != std::string::npos &&
                  users->body.find("13800138000") != std::string::npos,
              "admin: search user");
        int uid = -1;
        { const auto j = nlohmann::json::parse(users->body); if (!j["data"].empty()) uid = j["data"][0]["id"].get<int>(); }
        if (uid > 0) {
            auto fr = postAuth("/api/admin/users/" + std::to_string(uid) + "/freeze", "{\"frozen\":true}");
            check(fr && fr->body.find("\"code\":0") != std::string::npos, "admin: freeze user");
            auto fz = getAuth("/api/admin/users?phone=13800138000&status=2");
            check(fz && fz->body.find("\"code\":0") != std::string::npos &&
                      fz->body.find("\"status\":1") != std::string::npos,
                  "admin: freeze user appears under status=2 filter");
            auto uf = postAuth("/api/admin/users/" + std::to_string(uid) + "/freeze", "{\"frozen\":false}");
            check(uf && uf->body.find("\"code\":0") != std::string::npos, "admin: unfreeze user");
        }

        // 资产 CRUD(仅 super) + 孤儿桩拒绝 + 禁删保护
        auto ns = postAuth("/api/admin/stations",
                           "{\"name\":\"测试站\",\"address\":\"北京测试路1号\","
                           "\"latitude\":39.9,\"longitude\":116.3,\"price_cents\":150}");
        check(ns && ns->body.find("\"code\":0") != std::string::npos, "admin: create station");
        int sid = -1;
        { const auto j = nlohmann::json::parse(ns->body); sid = j["data"]["id"].get<int>(); }
        if (sid > 0) {
            auto orph = postAuth("/api/admin/stations/99999/devices",
                                 "{\"count\":2,\"type\":0,\"power_kw\":120}");
            check(orph && orph->body.find("\"code\":1") != std::string::npos &&
                      orph->body.find("\xe7\xab\x99\xe4\xb8\x8d\xe5\xad\x98\xe5\x9c\xa8") != std::string::npos,
                  "admin: batch devices on missing station REJECTED(no orphan)");
            auto nd = postAuth("/api/admin/stations/" + std::to_string(sid) + "/devices",
                               "{\"count\":2,\"type\":0,\"power_kw\":120}");
            check(nd && nd->body.find("\"code\":0") != std::string::npos, "admin: create 2 devices");
            auto ds = delAuth("/api/admin/stations/" + std::to_string(sid));
            check(ds && ds->body.find("\"code\":1") != std::string::npos,
                  "admin: delete station with devices REJECTED");
            auto dv = getAuth("/api/admin/devices?station_id=" + std::to_string(sid));
            int did0 = -1, did1 = -1;
            { const auto j = nlohmann::json::parse(dv->body);
              if (j["data"]["items"].size() >= 2) {
                  did0 = j["data"]["items"][0]["id"].get<int>();
                  did1 = j["data"]["items"][1]["id"].get<int>(); } }
            if (did0 > 0) {
                auto dd = delAuth("/api/admin/devices/" + std::to_string(did0));
                check(dd && dd->body.find("\"code\":0") != std::string::npos, "admin: delete idle device");
            }
            if (did1 > 0) {
                delAuth("/api/admin/devices/" + std::to_string(did1));
                auto ds2 = delAuth("/api/admin/stations/" + std::to_string(sid));
                check(ds2 && ds2->body.find("\"code\":0") != std::string::npos,
                      "admin: delete empty station ok");
            }
        }

        // R1：站运营属性 JSON 与建站持久化
        {
            auto pubs = cli.Get("/api/stations");
            check(pubs && pubs->body.find("\"amenities_mask\":339") != std::string::npos &&
                      pubs->body.find("\"is_promo\":true") != std::string::npos &&
                      pubs->body.find("\"min_charge_cents\":0") != std::string::npos &&
                      pubs->body.find("\"open_hours\":\"00:00-24:00\"") != std::string::npos,
                  "R1: seeded station JSON has ops fields");
            check(pubs && pubs->body.find("\xe5\x8d\xab\xe7\x94\x9f\xe9\x97\xb4") != std::string::npos,
                  "R1: amenities array has 卫生间");
            auto ns2 = postAuth("/api/admin/stations",
                                "{\"name\":\"R1富字段站\",\"address\":\"测试路2号\","
                                "\"latitude\":39.9,\"longitude\":116.4,\"price_cents\":300,"
                                "\"amenities_mask\":11,\"parking\":1,\"location\":1,"
                                "\"is_promo\":true,\"open_hours\":\"08:00-22:00\","
                                "\"min_charge_cents\":500}");
            int sid2 = -1;
            { const auto j = nlohmann::json::parse(ns2->body);
              if (ns2 && ns2->body.find("\"code\":0") != std::string::npos)
                  sid2 = j["data"]["id"].get<int>(); }
            check(sid2 > 0, "R1: create station with full fields");
            if (sid2 > 0) {
                auto qa = getAuth("/api/admin/stations?q=R1");
                check(qa && qa->body.find("\"code\":0") != std::string::npos &&
                          qa->body.find("\"amenities_mask\":11") != std::string::npos &&
                          qa->body.find("\"parking\":1") != std::string::npos &&
                          qa->body.find("\"min_charge_cents\":500") != std::string::npos &&
                          qa->body.find("\"open_hours\":\"08:00-22:00\"") != std::string::npos,
                      "R1: created station persists ops fields");
                // 名字数组到 mask 的入库路径(用 amenities 名字数组建站)
                auto ns3 = postAuth("/api/admin/stations",
                                    "{\"name\":\"R1名字站\",\"address\":\"测试路3号\","
                                    "\"latitude\":39.8,\"longitude\":116.5,\"price_cents\":200,"
                                    "\"amenities\":[\"卫生间\",\"休息室\"]}");
                int sid3 = -1;
                { const auto j3 = nlohmann::json::parse(ns3->body);
                  if (ns3 && ns3->body.find("\"code\":0") != std::string::npos)
                      sid3 = j3["data"]["id"].get<int>(); }
                check(sid3 > 0, "R1: create station with amenities name array");
                if (sid3 > 0) {
                    auto q3 = getAuth("/api/admin/stations?q=R1\xe5\x90\x8d\xe5\xad\x97");
                    auto q3s = getAuth("/api/admin/stations?q=R1");
                    check(q3s && q3s->body.find("\"amenities_mask\":3") != std::string::npos &&
                              q3s->body.find("\"amenities\":[\"\xe5\x8d\xab\xe7\x94\x9f\xe9\x97\xb4\",\"\xe4\xbc\x91\xe6\x81\xaf\xe5\xae\xa4\"]") != std::string::npos,
                          "R1: amenities name array -> mask 3");
                    delAuth("/api/admin/stations/" + std::to_string(sid3));
                }
                delAuth("/api/admin/stations/" + std::to_string(sid2));
            }
        }
        // R2/R3：seed 三档价 JSON
        {
            auto p2 = cli.Get("/api/stations");
            check(p2 && p2->body.find("\"price_slow_cents\":140") != std::string::npos &&
                      p2->body.find("\"price_fast_cents\":200") != std::string::npos &&
                      p2->body.find("\"price_ultra_cents\":280") != std::string::npos,
                  "R2: seeded station three-tier prices in JSON");
        }
        // 设备列表筛选 + 运维/审计/统计查询(此时 seed 故障桩 #6 仍在 → state=2 total=1)
        {
            auto devs = getAuth("/api/admin/devices?state=2");
            check(devs && devs->body.find("\"code\":0") != std::string::npos &&
                      devs->body.find("\"items\"") != std::string::npos &&
                      devs->body.find("\"total\":1") != std::string::npos,
                  "admin: devices filter state=fault finds seed fault pile");
            auto ops = getAuth("/api/admin/logs/ops");
            check(ops && ops->body.find("\"code\":0") != std::string::npos, "admin: ops log list");
            auto audit = getAuth("/api/admin/logs/audit");
            check(audit && audit->body.find("\"code\":0") != std::string::npos &&
                      audit->body.find("login") != std::string::npos,
                  "admin: audit log has login entry");
            auto st = getAuth("/api/admin/stats/overview");
            check(st && st->body.find("\"code\":0") != std::string::npos &&
                      st->body.find("\"device_health\"") != std::string::npos &&
                      st->body.find("\"online_rate\"") != std::string::npos,
                  "admin: stats overview fields present");
            auto daily = getAuth("/api/admin/stats/daily?days=7");
            check(daily && daily->body.find("\"code\":0") != std::string::npos,
                  "admin: stats daily list");
        }

        // operator / viewer 角色门禁(operator 可重启故障桩 #6; viewer 全拒写)
        {
            auto olg = cli.Post("/api/admin/login",
                                "{\"username\":\"operator\",\"password\":\"operator123\"}",
                                "application/json");
            check(olg && olg->body.find("\"code\":0") != std::string::npos &&
                      olg->body.find("\"role\":\"operator\"") != std::string::npos,
                  "operator: login role operator");
            std::string ot;
            { const auto j = nlohmann::json::parse(olg->body); ot = j["data"]["token"].get<std::string>(); }
            httplib::Headers oh;
            oh.emplace("Authorization", "Bearer " + ot);
            auto of = cli.Post("/api/admin/users/1/freeze", oh, "{\"frozen\":true}", "application/json");
            check(of && of->body.find("\"code\":4") != std::string::npos,
                  "operator: freeze -> code4(403)");
            auto our = cli.Get("/api/admin/users", oh);
            check(our && our->body.find("\"code\":0") != std::string::npos,
                  "operator: read users ok");
            auto orr = cli.Post("/api/admin/devices/6/restart", oh, "{}", "application/json");
            check(orr && orr->body.find("\"code\":0") != std::string::npos,
                  "operator: restart fault pile allowed");

            auto vg = cli.Post("/api/admin/login",
                               "{\"username\":\"viewer\",\"password\":\"viewer123\"}",
                               "application/json");
            check(vg && vg->body.find("\"code\":0") != std::string::npos &&
                      vg->body.find("\"role\":\"viewer\"") != std::string::npos,
                  "viewer: login role viewer");
            std::string vt;
            { const auto j = nlohmann::json::parse(vg->body); vt = j["data"]["token"].get<std::string>(); }
            httplib::Headers vh;
            vh.emplace("Authorization", "Bearer " + vt);
            auto vf = cli.Post("/api/admin/users/1/freeze", vh, "{\"frozen\":true}", "application/json");
            check(vf && vf->body.find("\"code\":4") != std::string::npos, "viewer: freeze -> code4");
            auto vr = cli.Post("/api/admin/devices/1/restart", vh, "{}", "application/json");
            check(vr && vr->body.find("\"code\":4") != std::string::npos, "viewer: restart -> code4");
            auto vo = cli.Get("/api/admin/stats/overview", vh);
            check(vo && vo->body.find("\"code\":0") != std::string::npos,
                  "viewer: read stats ok");
        }

        // 模拟器自主故障 → 故障 → 远程重启 → 心跳恢复 全链路(#7)
        {
            const int fd = socket(AF_INET, SOCK_STREAM, 0);
            bool okConn = false;
            if (fd >= 0) {
                sockaddr_in a{};
                a.sin_family = AF_INET;
                a.sin_port = htons(static_cast<uint16_t>(simPort2));
                inet_pton(AF_INET, "127.0.0.1", &a.sin_addr);
                okConn = connect(fd, reinterpret_cast<sockaddr*>(&a), sizeof(a)) == 0;
            }
            check(okConn, "b2: sim connect");
            if (okConn) {
                sendLine(fd, "{\"type\":\"register\",\"devices\":[7]}\n");
                sendLine(fd, "{\"type\":\"heartbeat\",\"device_id\":7,\"voltage\":220.0,\"current\":0.0,\"temperature\":25.0,\"sim_state\":0}\n");
                sendLine(fd, "{\"type\":\"heartbeat\",\"device_id\":7,\"voltage\":220.0,\"current\":0.0,\"temperature\":25.0,\"sim_state\":2}\n");
                int st7 = -1;
                for (int k = 0; k < 10 && st7 != 2; ++k) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(200));
                    auto q = getAuth("/api/admin/devices?q=7");
                    if (q) { const auto j = nlohmann::json::parse(q->body);
                             if (!j["data"]["items"].empty()) st7 = j["data"]["items"][0]["state"].get<int>(); }
                }
                check(st7 == 2, "b2: sim fault -> device Fault");
                auto rr = postAuth("/api/admin/devices/7/restart", "{}");
                check(rr && rr->body.find("\"code\":0") != std::string::npos &&
                          rr->body.find("\"state\":4") != std::string::npos,
                      "b2: restart -> Rebooting(4)");
                sendLine(fd, "{\"type\":\"heartbeat\",\"device_id\":7,\"voltage\":220.0,\"current\":0.0,\"temperature\":25.0,\"sim_state\":0}\n");
                int stAfter = -1;
                for (int k = 0; k < 10 && stAfter != 0; ++k) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(200));
                    auto q = getAuth("/api/admin/devices?q=7");
                    if (q) { const auto j = nlohmann::json::parse(q->body);
                             if (!j["data"]["items"].empty()) stAfter = j["data"]["items"][0]["state"].get<int>(); }
                }
                check(stAfter == 0, "b2: heartbeat recovery -> Idle");
                auto ops2 = getAuth("/api/admin/logs/ops");
                check(ops2 && ops2->body.find("\"op_type\":\"restart\"") != std::string::npos &&
                          ops2->body.find("\"op_type\":\"recover\"") != std::string::npos,
                      "b2: ops log restart+recover");
                auto audit2 = getAuth("/api/admin/logs/audit");
                check(audit2 && audit2->body.find("device.restart") != std::string::npos,
                      "b2: audit device.restart entry");
                close(fd);
            }
            // 回归(隐形故障)：业务进行中(Reserved)被忽略的故障，桩回到 Idle 后必须"显现"
            {
                const int fd8 = socket(AF_INET, SOCK_STREAM, 0);
                bool okC = false;
                if (fd8 >= 0) {
                    sockaddr_in a{};
                    a.sin_family = AF_INET;
                    a.sin_port = htons(static_cast<uint16_t>(simPort2));
                    inet_pton(AF_INET, "127.0.0.1", &a.sin_addr);
                    okC = connect(fd8, reinterpret_cast<sockaddr*>(&a), sizeof(a)) == 0;
                }
                check(okC, "b2: hidden-fault sim connect");
                if (okC) {
                    sendLine(fd8, "{\"type\":\"register\",\"devices\":[8]}\n");
                    sendLine(fd8, "{\"type\":\"heartbeat\",\"device_id\":8,\"voltage\":220.0,\"current\":0.0,\"temperature\":25.0,\"sim_state\":0}\n");
                    auto rv = cli.Post("/api/orders",
                                       "{\"phone\":\"13800138000\",\"device_id\":8}",
                                       "application/json");
                    int oid8 = 0;
                    { const auto j = nlohmann::json::parse(rv->body);
                      if (rv && rv->body.find("\"code\":0") != std::string::npos)
                          oid8 = j["data"]["id"].get<int>(); }
                    check(oid8 > 0, "b2: reserve dev8 (hidden-fault regression)");
                    if (oid8 > 0) {
                        // Reserved 期间设备上报故障 → 业务忽略(不落 Fault)
                        sendLine(fd8, "{\"type\":\"heartbeat\",\"device_id\":8,\"voltage\":220.0,\"current\":0.0,\"temperature\":25.0,\"sim_state\":2}\n");
                        int s8 = -1;
                        for (int k = 0; k < 10 && s8 != 3; ++k) {
                            std::this_thread::sleep_for(std::chrono::milliseconds(200));
                            auto q = getAuth("/api/admin/devices?q=8");
                            if (q) { const auto j = nlohmann::json::parse(q->body);
                                     if (!j["data"]["items"].empty()) s8 = j["data"]["items"][0]["state"].get<int>(); }
                        }
                        check(s8 == 3, "b2: fault ignored while Reserved");
                        cli.Post("/api/orders/" + std::to_string(oid8) + "/cancel",
                                 "{}", "application/json");
                        int sIdle = -1;
                        for (int k = 0; k < 10 && sIdle != 0; ++k) {
                            std::this_thread::sleep_for(std::chrono::milliseconds(200));
                            auto q = getAuth("/api/admin/devices?q=8");
                            if (q) { const auto j = nlohmann::json::parse(q->body);
                                     if (!j["data"]["items"].empty()) sIdle = j["data"]["items"][0]["state"].get<int>(); }
                        }
                        check(sIdle == 0, "b2: dev8 back Idle after cancel");
                        // 同一故障持续上报，桩回 Idle 后必须"显现"为 Fault(不能被去重缓存吞掉)
                        sendLine(fd8, "{\"type\":\"heartbeat\",\"device_id\":8,\"voltage\":220.0,\"current\":0.0,\"temperature\":25.0,\"sim_state\":2}\n");
                        int sF = -1;
                        for (int k = 0; k < 10 && sF != 2; ++k) {
                            std::this_thread::sleep_for(std::chrono::milliseconds(200));
                            auto q = getAuth("/api/admin/devices?q=8");
                            if (q) { const auto j = nlohmann::json::parse(q->body);
                                     if (!j["data"]["items"].empty()) sF = j["data"]["items"][0]["state"].get<int>(); }
                        }
                        check(sF == 2, "b2: hidden fault appears after release (regression)");
                        // 恢复干净：远程重启 + 设备自愈
                        postAuth("/api/admin/devices/8/restart", "{}");
                        sendLine(fd8, "{\"type\":\"heartbeat\",\"device_id\":8,\"voltage\":220.0,\"current\":0.0,\"temperature\":25.0,\"sim_state\":0}\n");
                    }
                    close(fd8);
                }
            }
            app.stopSimListener();
        }
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

    // 5) 充电闭环：结束生成待支付账单；支付才扣款；未支付禁止开新桩
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
              "chg: reserved + price snapshot 200");
        st.getDeviceById(1, &dev);
        check(dev.state == ncs::DeviceState::Reserved, "chg: device Reserved");
        ncs::Station s1;
        st.getStationById(1, &s1);
        check(s1.freePiles == 1, "chg: free ->1");
        ncs::Order o2;
        QString err2;
        check(!cs.reserve(QStringLiteral("13800138000"), 1, &o2, &err2),
              "chg: double reserve same device blocked");
        check(cs.start(o.id, &err), "chg: start ok");
        check(cs.finish(o.id, &err), "chg: finish ok (生成账单不扣款)");
        ncs::Order done;
        st.getOrderById(o.id, &done);
        check(done.status == ncs::OrderStatus::Completed && done.amountCents == 400 &&
                  done.energyKwh == 2.0,
              "chg: bill amount 400 (2.0kWh x 200)");
        ncs::User after;
        st.findUserByPhone(QStringLiteral("13800138000"), &after);
        check(after.balanceCents == 0, "chg: finish does NOT deduct (balance 0)");
        st.getDeviceById(1, &dev);
        st.getStationById(1, &s1);
        check(dev.state == ncs::DeviceState::Idle && s1.freePiles == 2,
              "chg: device freed after finish");

        ncs::Order o3;
        QString err3;
        check(!cs.reserve(QStringLiteral("13800138000"), 2, &o3, &err3) &&
                  err3.contains(QStringLiteral("支付")),
              "chg: unpaid bill blocks new pile");
        check(st.countUnpaidByPhone(QStringLiteral("13800138000")) == 1,
              "chg: one unpaid order");

        check(cs.pay(o.id, &err), "chg: pay ok");
        ncs::Order paid;
        st.getOrderById(o.id, &paid);
        st.findUserByPhone(QStringLiteral("13800138000"), &after);
        check(paid.status == ncs::OrderStatus::Paid && after.balanceCents == -400,
              "chg: after pay -> Paid, balance -400");
        check(st.countUnpaidByPhone(QStringLiteral("13800138000")) == 0,
              "chg: unpaid cleared");
        check(cs.reserve(QStringLiteral("13800138000"), 2, &o3, &err3),
              "chg: reserve allowed after pay");
    }
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

    // 7) 钱包/昵称/历史
    const QString walDb = tempDb("wal");
    {
        ncs::backend::Store st;
        check(st.open(walDb), "wal store.open");
        ncs::User u0;
        st.ensureUserByPhone(QStringLiteral("13800138000"), &u0);
        check(st.addBalanceByPhone(QStringLiteral("13800138000"), 500), "wal recharge +500");
        ncs::User u;
        st.findUserByPhone(QStringLiteral("13800138000"), &u);
        check(u.balanceCents == 500, "wal balance 500");
        check(st.setNickname(QStringLiteral("13800138000"), QStringLiteral("阿甘")), "wal setNickname");
        st.findUserByPhone(QStringLiteral("13800138000"), &u);
        check(u.nickname == QStringLiteral("阿甘"), "wal nickname persisted");
        // 建一笔已支付订单 → 历史
        ncs::backend::ChargeService cs(&st, [](int, bool) {}, [](int) { return 1.0; });
        ncs::Order o;
        QString err;
        cs.reserve(QStringLiteral("13800138000"), 1, &o, &err);
        cs.start(o.id, &err);
        cs.finish(o.id, &err);
        cs.pay(o.id, &err);
        check(st.countHistoryByPhone(QStringLiteral("13800138000")) == 1, "wal history count 1");
        const auto hist = st.listHistoryByPhone(QStringLiteral("13800138000"), 20, 0);
        check(hist.size() == 1 && hist[0].status == ncs::OrderStatus::Paid &&
                  hist[0].amountCents == 200,
              "wal history paid amount 200");
    }

    // 8) B2 后端直测：孤儿桩 / 事务计数 / 审计运维 / 聚合统计
    const QString b2Db = tempDb("b2");
    {
        ncs::backend::Store st;
        check(st.open(b2Db), "b2 store.open");
        ncs::backend::DeviceFilter all;  // -1 = 全部
        check(st.countDevicesAdmin(all) == 9, "b2: seed 9 devices");
        check(st.createDevices(99999, 0, 3, 120) == -1,
              "b2: createDevices on missing station -> -1");
        check(st.countDevicesAdmin(all) == 9,
              "b2: no orphan devices after rejected batch");
        check(st.createDevices(1, 0, 2, 120) == 1, "b2: createDevices batch ok");
        ncs::Station s1;
        st.getStationById(1, &s1);
        check(s1.totalPiles == 5 && s1.freePiles == 4,
              "b2: station counters +2 (total5/free4)");
        ncs::backend::DeviceFilter idle1;
        idle1.stationId = 1;
        idle1.state = 0;
        QVector<ncs::backend::DeviceRow> rows;
        st.listDevicesAdmin(idle1, 10, 0, &rows);
        check(rows.size() >= 3 && !rows[0].stationName.isEmpty(),
              "b2: idle devices aggregated with station name");
        const int idToDel = rows[0].dev.id;
        check(st.deleteDeviceIfIdle(idToDel) == 1, "b2: delete idle device ok");
        st.getStationById(1, &s1);
        check(s1.totalPiles == 4 && s1.freePiles == 3,
              "b2: delete rolls back counters");
        check(st.deleteDeviceIfIdle(idToDel) == -1, "b2: double delete -> -1");
        check(st.appendAudit(QStringLiteral("admin"), QStringLiteral("station.create"),
                             QStringLiteral("测试建站"), true),
              "b2: appendAudit ok");
        check(st.appendDeviceOp(1, QStringLiteral("restart"),
                                QStringLiteral("admin"), QStringLiteral("重启")),
              "b2: appendDeviceOp ok");
        const auto ops = st.listDeviceOps(10, 0);
        check(ops.size() == 1 && ops[0].deviceId == 1 &&
                  ops[0].opType == QStringLiteral("restart"),
              "b2: deviceOps list round-trip");
        const auto audit = st.listAudit(10, 0);
        check(audit.size() == 1 &&
                  audit[0].action == QStringLiteral("station.create") &&
                  audit[0].result == QStringLiteral("ok"),
              "b2: audit list round-trip");
        check(st.deviceStateCounts().size() == 5, "b2: deviceStateCounts 5 bins");
        const auto daily = st.dailyRevenue(7);
        check(daily.size() == 7, "b2: dailyRevenue zero-filled 7 days");
        const auto agg = st.revenueWindow(QString(), QString());
        check(agg.orders == 0 && agg.cents == 0, "b2: revenue empty orders 0");
    }

    // 9) R1：Station 运营属性(Store 直测)
    const QString r1Db = tempDb("r1");
    {
        ncs::backend::Store st;
        check(st.open(r1Db), "r1 store.open");
        ncs::Station s1;
        st.getStationById(1, &s1);
        check(s1.amenities == 339 && s1.isPromo && s1.minChargeCents == 0 &&
                  s1.parking == 1 && s1.location == 0 &&
                  s1.openHours == QStringLiteral("00:00-24:00"),
              "r1: seed station ops attributes");
        const QStringList names = ncs::stationAmenityNames(s1.amenities);
        check(names.contains(QStringLiteral("卫生间")) &&
                  names.contains(QStringLiteral("有人值守")),
              "r1: amenity names decode");
        check(ncs::stationAmenityMask(names) == s1.amenities,
              "r1: amenity name->mask roundtrip");
        ncs::backend::StationFields f;
        f.amenities = 3;
        f.parking = 2;
        f.location = 1;
        f.isPromo = true;
        f.openHours = QStringLiteral("08:00-22:00");
        f.minChargeCents = 500;
        const int id = st.createStation(QStringLiteral("R1Store站"),
                                        QStringLiteral("测试路9号"), 39.5, 116.1,
                                        ncs::MoneyCents(300), f);
        check(id > 0, "r1: createStation with fields");
        ncs::Station s2;
        st.getStationById(id, &s2);
        check(s2.amenities == 3 && s2.parking == 2 && s2.location == 1 &&
                  s2.isPromo && s2.openHours == QStringLiteral("08:00-22:00") &&
                  s2.minChargeCents == 500,
              "r1: created station fields round-trip");
        f.parking = 0;
        f.isPromo = false;
        f.minChargeCents = 0;
        check(st.updateStation(id, s2.name, s2.address, s2.latitude, s2.longitude,
                               s2.pricePerKwhCents, f),
              "r1: updateStation fields");
        st.getStationById(id, &s2);
        check(s2.parking == 0 && !s2.isPromo && s2.minChargeCents == 0,
              "r1: updated fields persisted");
    }

    // 10) R2/R3：分档计价 / 功率档分布 / 同站异档不同账单
    const QString w1Db = tempDb("w1");
    {
        ncs::backend::Store st;
        check(st.open(w1Db), "w1 store.open");
        check(ncs::power_tier(7) == ncs::PowerTier::Slow &&
                  ncs::power_tier(30) == ncs::PowerTier::Fast &&
                  ncs::power_tier(120) == ncs::PowerTier::Fast &&
                  ncs::power_tier(179) == ncs::PowerTier::Fast &&
                  ncs::power_tier(180) == ncs::PowerTier::Ultra &&
                  ncs::power_tier(250) == ncs::PowerTier::Ultra,
              "w1: power_tier boundaries");
        bool hasSlow = false, hasFast = false, hasUltra = false;
        for (int i = 1; i <= 3; ++i)
            for (const auto& d : st.listDevicesByStation(i)) {
                const auto t = ncs::power_tier(d.powerKw);
                if (t == ncs::PowerTier::Slow)
                    hasSlow = true;
                else if (t == ncs::PowerTier::Fast)
                    hasFast = true;
                else if (t == ncs::PowerTier::Ultra)
                    hasUltra = true;
            }
        check(hasSlow && hasFast && hasUltra,
              "w1: seed devices cover slow/fast/ultra tiers");
        ncs::Station s1;
        st.getStationById(1, &s1);
        check(s1.priceSlowCents == 140 && s1.priceUltraCents == 280,
              "w1: seed station tier prices persisted");
        ncs::Station fallback = s1;
        fallback.priceSlowCents = 0;
        check(ncs::stationTierPriceCents(fallback, 7) == ncs::MoneyCents(200),
              "w1: missing slow tier falls back to fast");
        ncs::backend::ChargeService cs(&st, [](int, bool) {}, [](int) { return 2.0; });
        auto bill = [&](const QString& phone, int dev) {
            ncs::User u;
            st.ensureUserByPhone(phone, &u);
            ncs::Order o;
            QString err;
            cs.reserve(phone, dev, &o, &err);
            cs.start(o.id, &err);
            cs.finish(o.id, &err);
            ncs::Order done;
            st.getOrderById(o.id, &done);
            return done.amountCents;
        };
        const ncs::MoneyCents fastAmt = bill(QStringLiteral("13900000001"), 1);
        const ncs::MoneyCents slowAmt = bill(QStringLiteral("13900000002"), 2);
        check(fastAmt == ncs::MoneyCents(400) && slowAmt == ncs::MoneyCents(280),
              "w1: same station fast(200x2=400) vs slow(140x2=280) bills differ");
    }
    ::unlink(w1Db.toLocal8Bit().constData());

    // 11) 老库升级回归：R1/R2 列 ALTER 后，分档价列需回填 price_cents(避免慢/超 0 元)
    const QString upDb = tempDb("up");
    {
        sqlite3* raw = nullptr;
        check(sqlite3_open(upDb.toUtf8().constData(), &raw) == SQLITE_OK,
              "up: open raw old-schema db");
        const char* ddl =
            "CREATE TABLE stations (id INTEGER PRIMARY KEY AUTOINCREMENT, name TEXT NOT NULL,"
            " address TEXT NOT NULL, latitude REAL NOT NULL DEFAULT 0,"
            " longitude REAL NOT NULL DEFAULT 0, total_piles INTEGER NOT NULL DEFAULT 0,"
            " price_cents INTEGER NOT NULL DEFAULT 0, free_piles INTEGER NOT NULL DEFAULT 0);"
            "CREATE TABLE users (id INTEGER PRIMARY KEY AUTOINCREMENT, phone TEXT NOT NULL UNIQUE,"
            " nickname TEXT NOT NULL, balance_cents INTEGER NOT NULL DEFAULT 0,"
            " status INTEGER NOT NULL DEFAULT 0, registered_at TEXT NOT NULL);"
            "CREATE TABLE devices (id INTEGER PRIMARY KEY AUTOINCREMENT, station_id INTEGER NOT NULL,"
            " type INTEGER NOT NULL DEFAULT 0, state INTEGER NOT NULL DEFAULT 0,"
            " power_kw REAL NOT NULL DEFAULT 0, energy_kwh REAL NOT NULL DEFAULT 0);"
            "CREATE TABLE orders (id INTEGER PRIMARY KEY AUTOINCREMENT, phone TEXT NOT NULL,"
            " station_id INTEGER NOT NULL, device_id INTEGER NOT NULL,"
            " unit_price_cents INTEGER NOT NULL DEFAULT 0, amount_cents INTEGER NOT NULL DEFAULT 0,"
            " energy_kwh REAL NOT NULL DEFAULT 0, status INTEGER NOT NULL DEFAULT 0,"
            " started_at TEXT NOT NULL, finished_at TEXT);"
            "CREATE TABLE admins (username TEXT PRIMARY KEY, password TEXT NOT NULL);"
            "INSERT INTO stations(name,address,latitude,longitude,total_piles,price_cents,free_piles)"
            " VALUES ('旧站','旧地址',1.0,2.0,2,200,2);"
            "INSERT INTO devices(station_id,type,state,power_kw,energy_kwh)"
            " VALUES (1,0,0,120,0),(1,0,0,7,0);"
            "INSERT INTO users(phone,nickname,balance_cents,status,registered_at)"
            " VALUES ('13800000000','u',0,0,'2026-01-01T00:00:00Z');";
        char* err = nullptr;
        check(sqlite3_exec(raw, ddl, nullptr, nullptr, &err) == SQLITE_OK,
              "up: seed old-schema rows");
        sqlite3_close(raw);

        ncs::backend::Store st;
        check(st.open(upDb), "up: open old db with new Store");
        ncs::Station s;
        st.getStationById(1, &s);
        check(s.pricePerKwhCents == 200 && s.priceSlowCents == 200 &&
                  s.priceUltraCents == 200,
              "up: tier price cols added & backfilled to price_cents(=200)");
        check(ncs::stationTierPriceCents(s, 7) == ncs::MoneyCents(200) &&
                  ncs::stationTierPriceCents(s, 180) == ncs::MoneyCents(200),
              "up: upgraded old station prices slow/ultra == fast(200)");
    }
    ::unlink(upDb.toLocal8Bit().constData());

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
