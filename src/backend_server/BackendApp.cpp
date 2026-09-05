#include "BackendApp.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstdio>
#include <cstring>
#include <string>

#include <QDateTime>

#include <nlohmann/json.hpp>

namespace ncs {
namespace backend {

using nlohmann::json;

// 统一响应信封: {"code":0,"message":"ok","data":...}
//   code 0=成功; 1=业务失败; 2=请求解析错误(HTTP 400); 其它见各接口说明
namespace {
constexpr int kCodeOk = 0;
constexpr int kCodeBiz = 1;
constexpr int kCodeBadReq = 2;

void reply(httplib::Response& res, int code, const char* msg, json data) {
    if (code == kCodeBadReq)
        res.status = 400;
    else
        res.status = 200;
    json j{{"code", code}, {"message", msg}, {"data", std::move(data)}};
    res.set_content(j.dump(), "application/json; charset=utf-8");
}
void replyOk(httplib::Response& res, json data) {
    reply(res, kCodeOk, "ok", std::move(data));
}
void replyBizErr(httplib::Response& res, const QString& msg) {
    reply(res, kCodeBiz, msg.toUtf8().constData(), nullptr);
}

json userToJson(const ncs::User& u) {
    const QString iso = u.registeredAt.isValid()
                            ? u.registeredAt.toUTC().toString(Qt::ISODate)
                            : QString();
    return json{{"id", u.id},
                {"phone", u.phone.toStdString()},
                {"nickname", u.nickname.toStdString()},
                {"balance_cents", u.balanceCents},
                {"status", static_cast<int>(u.status)},
                {"registered_at", iso.toStdString()}};
}
json stationToJson(const ncs::Station& s) {
    return json{{"id", s.id},
                {"name", s.name.toStdString()},
                {"address", s.address.toStdString()},
                {"latitude", s.latitude},
                {"longitude", s.longitude},
                {"total_piles", s.totalPiles},
                {"price_cents", s.pricePerKwhCents},
                {"free_piles", s.freePiles}};
}
json deviceToJson(const ncs::Device& d) {
    return json{{"id", d.id},
                {"station_id", d.stationId},
                {"type", static_cast<int>(d.type)},
                {"state", static_cast<int>(d.state)},
                {"power_kw", d.powerKw},
                {"energy_kwh", d.energyKwh}};
}
}  // namespace

BackendApp::BackendApp(const QString& dbPath)
    : dbPath_(dbPath), auth_(&store_) {}

BackendApp::~BackendApp() {
    stopSimListener();
}

bool BackendApp::init() {
    if (!store_.open(dbPath_)) {
        error_ = QStringLiteral("打开数据库失败: ") + dbPath_;
        return false;
    }
    registerRoutes();
    return true;
}

void BackendApp::registerRoutes() {
    srv_.Get("/health", [](const httplib::Request&, httplib::Response& res) {
        replyOk(res, json{{"service", kService}, {"version", kVersion}});
    });

    srv_.Post("/api/auth/send-code", [this](const httplib::Request& req,
                                            httplib::Response& res) {
        QString phone;
        try {
            const auto j = json::parse(req.body);
            phone = QString::fromStdString(j.at("phone").get<std::string>());
        } catch (...) {
            reply(res, kCodeBadReq, "请求体需为 JSON 且含 phone", nullptr);
            return;
        }
        const AuthReply r = auth_.sendCode(phone);
        if (r.ok)
            reply(res, kCodeOk, r.message.toUtf8().constData(), nullptr);
        else
            replyBizErr(res, r.message);
    });

    srv_.Post("/api/auth/login", [this](const httplib::Request& req,
                                        httplib::Response& res) {
        QString phone, code;
        try {
            const auto j = json::parse(req.body);
            phone = QString::fromStdString(j.at("phone").get<std::string>());
            code = QString::fromStdString(j.at("code").get<std::string>());
        } catch (...) {
            reply(res, kCodeBadReq, "请求体需为 JSON 且含 phone/code", nullptr);
            return;
        }
        const AuthReply r = auth_.login(phone, code);
        if (r.ok)
            replyOk(res, json{{"user", userToJson(r.user)}});
        else
            replyBizErr(res, r.message);
    });

    srv_.Get("/api/stations", [this](const httplib::Request&, httplib::Response& res) {
        const auto stations = store_.listStations();
        json arr = json::array();
        for (const auto& st : stations)
            arr.push_back(stationToJson(st));
        replyOk(res, std::move(arr));
    });

    srv_.Get(R"(/api/stations/(\d+)/devices)",
             [this](const httplib::Request& req, httplib::Response& res) {
                 if (req.matches.size() < 2) {
                     reply(res, kCodeBadReq, "缺少站 id", nullptr);
                     return;
                 }
                 const int id = std::stoi(req.matches[1]);
                 const auto devices = store_.listDevicesByStation(id);
                 json arr = json::array();
                 for (const auto& d : devices)
                     arr.push_back(deviceToJson(d));
                 replyOk(res, std::move(arr));
             });
}

// ---------- 模拟器 TCP(JSON-lines 心跳)；协议与 HTTP 信封无关 ----------

bool BackendApp::startSimListener(int port) {
    const int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0)
        return false;
    int one = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(static_cast<uint16_t>(port));
    if (bind(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0 ||
        listen(fd, 8) != 0) {
        close(fd);
        return false;
    }
    simListenFd_ = fd;
    simRunning_.store(true);
    simThread_ = std::thread([this] { simAcceptLoop(); });
    return true;
}

void BackendApp::stopSimListener() {
    simRunning_.store(false);
    if (simListenFd_ >= 0) {
        ::shutdown(simListenFd_, SHUT_RDWR);
        ::close(simListenFd_);
        simListenFd_ = -1;
    }
    if (simThread_.joinable())
        simThread_.join();
}

void BackendApp::simAcceptLoop() {
    while (simRunning_.load()) {
        const int c = accept(simListenFd_, nullptr, nullptr);
        if (c < 0) {
            if (!simRunning_.load())
                break;
            continue;
        }
        std::thread([this, c] { handleSimConnection(c); }).detach();
    }
}

void BackendApp::handleSimConnection(int fd) {
    std::string buf;
    char chunk[512];
    for (;;) {
        const ssize_t n = recv(fd, chunk, sizeof(chunk), 0);
        if (n <= 0)
            break;
        buf.append(chunk, static_cast<size_t>(n));
        std::size_t pos;
        while ((pos = buf.find('\n')) != std::string::npos) {
            const std::string line = buf.substr(0, pos);
            buf.erase(0, pos + 1);
            if (line.empty())
                continue;
            try {
                const auto j = json::parse(line);
                if (j.value("type", "") == "heartbeat") {
                    Heartbeat hb;
                    hb.deviceId = j.value("device_id", 0);
                    hb.voltage = j.value("voltage", 0.0);
                    hb.current = j.value("current", 0.0);
                    hb.temperature = j.value("temperature", 0.0);
                    hb.powerKw = j.value("power_kw", 0.0);
                    hb.energyKwh = j.value("energy_kwh", 0.0);
                    hb.tsMs = j.value("ts", 0LL);
                    sink_.onHeartbeat(hb);
                }
            } catch (...) {
                // 忽略坏行
            }
        }
    }
    ::close(fd);
}

}  // namespace backend
}  // namespace ncs
