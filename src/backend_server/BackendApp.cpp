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

namespace {

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

void replyJson(httplib::Response& res, const json& j, int status = 200) {
    res.status = status;
    res.set_content(j.dump(), "application/json; charset=utf-8");
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
        replyJson(res, json{{"ok", true},
                            {"service", kService},
                            {"version", kVersion}});
    });

    srv_.Post("/api/auth/send-code", [this](const httplib::Request& req,
                                            httplib::Response& res) {
        QString phone;
        try {
            const auto j = json::parse(req.body);
            phone = QString::fromStdString(j.at("phone").get<std::string>());
        } catch (...) {
            replyJson(res, json{{"ok", false},
                                {"message", "请求体需为 JSON 且含 phone"}},
                      400);
            return;
        }
        const AuthReply r = auth_.sendCode(phone);
        replyJson(res, json{{"ok", r.ok}, {"message", r.message.toStdString()}});
    });

    srv_.Post("/api/auth/login", [this](const httplib::Request& req,
                                        httplib::Response& res) {
        QString phone, code;
        try {
            const auto j = json::parse(req.body);
            phone = QString::fromStdString(j.at("phone").get<std::string>());
            code = QString::fromStdString(j.at("code").get<std::string>());
        } catch (...) {
            replyJson(res, json{{"ok", false},
                                {"message", "请求体需为 JSON 且含 phone/code"}},
                      400);
            return;
        }
        const AuthReply r = auth_.login(phone, code);
        json j{{"ok", r.ok}, {"message", r.message.toStdString()}};
        if (r.ok)
            j["user"] = userToJson(r.user);
        replyJson(res, j);
    });

    srv_.Get("/api/stations", [this](const httplib::Request&, httplib::Response& res) {
        const auto stations = store_.listStations();
        json arr = json::array();
        for (const auto& st : stations)
            arr.push_back(stationToJson(st));
        replyJson(res, arr);
    });

    srv_.Get(R"(/api/stations/(\d+)/devices)",
             [this](const httplib::Request& req, httplib::Response& res) {
                 if (req.matches.size() < 2) {
                     replyJson(res, json::array(), 400);
                     return;
                 }
                 const int id = std::stoi(req.matches[1]);
                 const auto devices = store_.listDevicesByStation(id);
                 json arr = json::array();
                 for (const auto& d : devices)
                     arr.push_back(deviceToJson(d));
                 replyJson(res, arr);
             });
}

// ---------- 模拟器 TCP(JSON-lines 心跳) ----------

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
        // 每连接一条读线程；多桩并发安全(心跳处理只进 sink)。
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
                // 忽略坏行(节流/截断)，继续
            }
        }
    }
    ::close(fd);
}

}  // namespace backend
}  // namespace ncs
