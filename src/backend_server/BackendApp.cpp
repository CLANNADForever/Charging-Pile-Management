#include "BackendApp.h"

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
    return json{
        {"id", u.id},
        {"phone", u.phone.toStdString()},
        {"nickname", u.nickname.toStdString()},
        {"balance_cents", u.balanceCents},
        {"status", static_cast<int>(u.status)},
        {"registered_at", iso.toStdString()},
    };
}

void replyJson(httplib::Response& res, const json& j, int status = 200) {
    res.status = status;
    res.set_content(j.dump(), "application/json; charset=utf-8");
}

}  // namespace

BackendApp::BackendApp(const QString& dbPath)
    : dbPath_(dbPath), auth_(&store_) {}

BackendApp::~BackendApp() = default;

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
}

}  // namespace backend
}  // namespace ncs
