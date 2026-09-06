#include "BackendApp.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include <QDate>
#include <QDir>
#include <QFile>
#include <QDateTime>
#include <QRandomGenerator>
#include <QStringList>

#include "billing.h"
#include "phone.h"
#include "core/ChargeService.h"
#include <nlohmann/json.hpp>

namespace ncs {
namespace backend {

using nlohmann::json;

// 统一响应信封: {"code":0,"message":"ok","data":...}
//   code 0=成功; 1=业务失败; 2=请求解析错误(HTTP 400); 其它见各接口说明
namespace {
constexpr int kCodeOk = 0;
constexpr int kCodeBiz = 1;
constexpr int kCodeBadReq = 2;  // 400
constexpr int kCodeUnauth = 3;  // 401 未认证
constexpr int kCodeForbid = 4;  // 403 无权限

void reply(httplib::Response& res, int code, const char* msg, json data) {
    if (code == kCodeBadReq)
        res.status = 400;
    else if (code == kCodeUnauth)
        res.status = 401;
    else if (code == kCodeForbid)
        res.status = 403;
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
    json am = json::array();
    for (const QString& n : ncs::stationAmenityNames(s.amenities))
        am.push_back(n.toStdString());
    return json{{"id", s.id},
                {"name", s.name.toStdString()},
                {"address", s.address.toStdString()},
                {"latitude", s.latitude},
                {"longitude", s.longitude},
                {"total_piles", s.totalPiles},
                {"price_cents", s.pricePerKwhCents},
                {"free_piles", s.freePiles},
                // R1 运营属性
                {"amenities_mask", s.amenities},
                {"amenities", std::move(am)},
                {"parking", s.parking},
                {"location", s.location},
                {"is_promo", s.isPromo},
                {"open_hours", s.openHours.toStdString()},
                {"min_charge_cents", s.minChargeCents}};
}
json deviceToJson(const ncs::Device& d) {
    return json{{"id", d.id},
                {"station_id", d.stationId},
                {"type", static_cast<int>(d.type)},
                {"state", static_cast<int>(d.state)},
                {"power_kw", d.powerKw},
                {"energy_kwh", d.energyKwh}};
}
json orderToJson(const ncs::Order& o) {
    auto iso = [](const QDateTime& t) {
        return t.isValid() ? t.toUTC().toString(Qt::ISODate).toStdString()
                           : std::string();
    };
    return json{{"id", o.id},
                {"phone", o.phone.toStdString()},
                {"station_id", o.stationId},
                {"device_id", o.deviceId},
                {"unit_price_cents", o.unitPriceCents},
                {"amount_cents", o.amountCents},
                {"energy_kwh", o.energyKwh},
                {"status", static_cast<int>(o.status)},
                {"started_at", iso(o.startedAt)},
                {"finished_at", iso(o.finishedAt)}};
}
}  // namespace

namespace {

std::string bearerToken(const httplib::Request& req) {
    const std::string h = req.get_header_value("Authorization");
    static const std::string kPre = "Bearer ";
    if (h.size() > kPre.size() && h.compare(0, kPre.size(), kPre) == 0)
        return h.substr(kPre.size());
    return std::string();
}

// 角色门禁：super 全部；operator 仅设备远程重启；viewer 只读
bool canDo(const QString& role, const char* perm) {
    if (role == QLatin1String("super"))
        return true;
    if (role == QLatin1String("operator") &&
        QString::fromLatin1(perm) == QLatin1String("device.restart"))
        return true;
    return false;
}

int intParam(const httplib::Request& req, const char* name, int dflt) {
    const std::string v = req.get_param_value(name);
    return v.empty() ? dflt : std::atoi(v.c_str());
}

QString dayStartIso(const QDate& d) {
    return QStringLiteral("%1T00:00:00").arg(d.toString(Qt::ISODate));
}

json adminStationToJson(const ncs::Station& st, int online) {
    const int total = st.totalPiles;
    int offline = total - online;
    if (offline < 0)
        offline = 0;
    const int rate = total > 0 ? qRound(100.0 * online / total) : 0;
    json j = stationToJson(st);
    j["online"] = online;
    j["offline"] = offline;
    j["online_rate"] = rate;  // 0-100 整数
    return j;
}

json adminDeviceToJson(const DeviceRow& r, const SimLive& live) {
    return json{{"id", r.dev.id},
                {"station_id", r.dev.stationId},
                {"station_name", r.stationName.toStdString()},
                {"type", static_cast<int>(r.dev.type)},
                {"state", static_cast<int>(r.dev.state)},
                {"online", live.online},
                {"power_kw", live.online ? live.powerKw : 0.0},
                {"energy_kwh", live.online ? live.energyKwh : 0.0},
                {"last_ts", live.lastTsMs},
                {"sessions", r.sessions},
                {"charging_sec", r.chargeSec}};
}

json dailyToJson(const DailyRevenue& d) {
    return json{{"day", d.day.toStdString()},
                {"revenue_cents", d.cents},
                {"orders", d.orders},
                {"energy_kwh", d.energyKwh}};
}

json auditToJson(const AuditRow& a) {
    return json{{"id", a.id},
                {"username", a.username.toStdString()},
                {"action", a.action.toStdString()},
                {"detail", a.detail.toStdString()},
                {"result", a.result.toStdString()},
                {"created_at",
                 a.at.isValid()
                     ? a.at.toUTC().toString(Qt::ISODate).toStdString()
                     : std::string()}};
}

json deviceOpToJson(const DeviceOpRow& o) {
    return json{{"id", o.id},
                {"device_id", o.deviceId},
                {"op_type", o.opType.toStdString()},
                {"op_by", o.opBy.toStdString()},
                {"detail", o.detail.toStdString()},
                {"created_at",
                 o.at.isValid()
                     ? o.at.toUTC().toString(Qt::ISODate).toStdString()
                     : std::string()}};
}

}  // namespace

BackendApp::BackendApp(const QString& dbPath)
    : dbPath_(dbPath), auth_(&store_) {
    charge_ = std::make_unique<ChargeService>(
        &store_,
        [this](int deviceId, bool start) { sendSimCommand(deviceId, start); },
        [this](int deviceId) { return simEnergy(deviceId); });
}

BackendApp::~BackendApp() {
    stopRestartSweeper();
    stopReserveSweeper();
    stopSimListener();
}

bool BackendApp::init() {
    if (!store_.open(dbPath_)) {
        error_ = QStringLiteral("打开数据库失败: ") + dbPath_;
        return false;
    }
    const QString uploads = QDir::current().absoluteFilePath(QStringLiteral("uploads"));
    QDir().mkpath(uploads);
    srv_.set_mount_point("/uploads", uploads.toStdString());

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

    srv_.Post("/api/orders", [this](const httplib::Request& req,
                                    httplib::Response& res) {
        QString phone;
        int deviceId = 0;
        try {
            const auto j = json::parse(req.body);
            phone = QString::fromStdString(j.at("phone").get<std::string>());
            deviceId = j.at("device_id").get<int>();
        } catch (...) {
            reply(res, kCodeBadReq, "body 需为 JSON 且含 phone/device_id", nullptr);
            return;
        }
        ncs::Order o;
        QString err;
        if (charge_->reserve(phone, deviceId, &o, &err))
            replyOk(res, orderToJson(o));
        else
            replyBizErr(res, err);
    });

    srv_.Post(R"(/api/orders/(\d+)/start)",
              [this](const httplib::Request& req, httplib::Response& res) {
                  if (req.matches.size() < 2) {
                      reply(res, kCodeBadReq, "缺订单 id", nullptr);
                      return;
                  }
                  const int id = std::stoi(req.matches[1]);
                  QString err;
                  if (charge_->start(id, &err))
                      replyOk(res, nullptr);
                  else
                      replyBizErr(res, err);
              });

    srv_.Post(R"(/api/orders/(\d+)/finish)",
              [this](const httplib::Request& req, httplib::Response& res) {
                  if (req.matches.size() < 2) {
                      reply(res, kCodeBadReq, "缺订单 id", nullptr);
                      return;
                  }
                  const int id = std::stoi(req.matches[1]);
                  QString err;
                  if (!charge_->finish(id, &err)) {
                      replyBizErr(res, err);
                      return;
                  }
                  ncs::Order o;
                  store_.getOrderById(id, &o);  // 结算后的订单(含金额/电量)
                  replyOk(res, orderToJson(o));
              });

    srv_.Post(R"(/api/orders/(\d+)/cancel)",
              [this](const httplib::Request& req, httplib::Response& res) {
                  if (req.matches.size() < 2) {
                      reply(res, kCodeBadReq, "缺订单 id", nullptr);
                      return;
                  }
                  const int id = std::stoi(req.matches[1]);
                  QString err;
                  if (charge_->cancel(id, &err))
                      replyOk(res, nullptr);
                  else
                      replyBizErr(res, err);
              });

    // 充电中实时信息(供 C 端轮询)：能量取模拟器最近上报，金额为估算
    srv_.Get(R"(/api/orders/(\d+)/live)",
             [this](const httplib::Request& req, httplib::Response& res) {
                 if (req.matches.size() < 2) {
                     reply(res, kCodeBadReq, "缺订单 id", nullptr);
                     return;
                 }
                 const int id = std::stoi(req.matches[1]);
                 ncs::Order o;
                 if (!store_.getOrderById(id, &o)) {
                     replyBizErr(res, QStringLiteral("订单不存在"));
                     return;
                 }
                 double energy = simEnergy(o.deviceId);
                 if (energy < 0.0)
                     energy = 0.0;
                 replyOk(res, json{{"status", static_cast<int>(o.status)},
                                   {"energy_kwh", energy},
                                   {"amount_cents", ncs::charging_amount_cents(energy, o.unitPriceCents)}});
             });

    srv_.Post(R"(/api/orders/(\d+)/pay)",
              [this](const httplib::Request& req, httplib::Response& res) {
                  if (req.matches.size() < 2) {
                      reply(res, kCodeBadReq, "缺订单 id", nullptr);
                      return;
                  }
                  const int id = std::stoi(req.matches[1]);
                  QString err;
                  if (!charge_->pay(id, &err)) {
                      replyBizErr(res, err);
                      return;
                  }
                  ncs::Order o;
                  store_.getOrderById(id, &o);
                  replyOk(res, orderToJson(o));
              });

    // 我的充电会话(活跃: Reserved/Charging/Completed待支付)
    srv_.Get("/api/orders/active",
             [this](const httplib::Request& req, httplib::Response& res) {
                 QString phone;
                 if (!req.has_param("phone") ||
                     (phone = QString::fromStdString(req.get_param_value("phone")))
                         .isEmpty()) {
                     reply(res, kCodeBadReq, "需 phone 查询参数", nullptr);
                     return;
                 }
                 const auto orders = store_.listActiveOrdersByPhone(phone);
                 json arr = json::array();
                 for (const auto& o : orders)
                     arr.push_back(orderToJson(o));
                 replyOk(res, std::move(arr));
             });

    // 模拟充值: balance += amount_cents
    srv_.Post("/api/wallet/recharge",
              [this](const httplib::Request& req, httplib::Response& res) {
                  QString phone;
                  ncs::MoneyCents amount = 0;
                  try {
                      const auto j = json::parse(req.body);
                      phone = QString::fromStdString(j.at("phone").get<std::string>());
                      amount = j.at("amount_cents").get<ncs::MoneyCents>();
                  } catch (...) {
                      reply(res, kCodeBadReq, "body 需 phone/amount_cents", nullptr);
                      return;
                  }
                  if (!ncs::is_valid_phone11(phone)) {
                      replyBizErr(res, QStringLiteral("非法手机号"));
                      return;
                  }
                  if (amount <= 0) {
                      replyBizErr(res, QStringLiteral("充值金额需为正数"));
                      return;
                  }
                  ncs::User u;
                  if (!store_.findUserByPhone(phone, &u)) {
                      replyBizErr(res, QStringLiteral("用户未注册"));
                      return;
                  }
                  if (!store_.addBalanceByPhone(phone, amount)) {
                      replyBizErr(res, QStringLiteral("充值失败(用户不存在或写盘失败)"));
                      return;
                  }
                  if (!store_.findUserByPhone(phone, &u)) {
                      replyBizErr(res, QStringLiteral("读取用户失败"));
                      return;
                  }
                  replyOk(res, userToJson(u));
              });

    // 改昵称 (PATCH)
    srv_.Patch("/api/user/profile",
               [this](const httplib::Request& req, httplib::Response& res) {
                   QString phone, nickname;
                   try {
                       const auto j = json::parse(req.body);
                       phone = QString::fromStdString(j.at("phone").get<std::string>());
                       nickname = QString::fromStdString(j.at("nickname").get<std::string>());
                   } catch (...) {
                       reply(res, kCodeBadReq, "body 需 phone/nickname", nullptr);
                       return;
                   }
                   if (!ncs::is_valid_phone11(phone)) {
                       replyBizErr(res, QStringLiteral("非法手机号"));
                       return;
                   }
                   nickname = nickname.trimmed();
                   if (nickname.isEmpty() || nickname.size() > 20) {
                       replyBizErr(res, QStringLiteral("昵称需 1-20 字"));
                       return;
                   }
                   ncs::User u;
                   if (!store_.findUserByPhone(phone, &u)) {
                       replyBizErr(res, QStringLiteral("用户未注册"));
                       return;
                   }
                   if (!store_.setNickname(phone, nickname)) {
                       replyBizErr(res, QStringLiteral("保存失败(用户不存在)"));
                       return;
                   }
                   if (!store_.findUserByPhone(phone, &u)) {
                       replyBizErr(res, QStringLiteral("读取用户失败"));
                       return;
                   }
                   replyOk(res, userToJson(u));
               });

    // 历史订单(已支付, 可翻页)
    srv_.Get("/api/orders/history",
             [this](const httplib::Request& req, httplib::Response& res) {
                 const QString phone =
                     QString::fromStdString(req.get_param_value("phone"));
                 if (!ncs::is_valid_phone11(phone)) {
                     replyBizErr(res, QStringLiteral("非法手机号"));
                     return;
                 }
                 int limit = std::atoi(req.get_param_value("limit").c_str());
                 int offset = std::atoi(req.get_param_value("offset").c_str());
                 limit = qBound(1, limit, 100);       // 限流
                 offset = qMax(0, offset);
                 const auto items = store_.listHistoryByPhone(phone, limit, offset);
                 const qint64 total = store_.countHistoryByPhone(phone);
                 json arr = json::array();
                 for (const auto& x : items)
                     arr.push_back(orderToJson(x));
                 replyOk(res, json{{"items", std::move(arr)}, {"total", total}});
             });

    // 头像上传: POST /api/user/avatar?phone=xxx&ext=png, body=PNG 字节
    srv_.Post("/api/user/avatar",
              [this](const httplib::Request& req, httplib::Response& res) {
                  const QString phone =
                      QString::fromStdString(req.get_param_value("phone"));
                  if (!ncs::is_valid_phone11(phone)) {
                      replyBizErr(res, QStringLiteral("非法手机号"));
                      return;
                  }
                  QString ext =
                      QString::fromStdString(req.get_param_value("ext")).toLower();
                  if (ext != QStringLiteral("png")) {
                      replyBizErr(res, QStringLiteral("仅支持 png(客户端会转码)"));
                      return;
                  }
                  if (req.body.size() <= 0 || req.body.size() > 2 * 1024 * 1024) {
                      replyBizErr(res, QStringLiteral("图片大小需在 0-2MB"));
                      return;
                  }
                  // PNG 魔数校验
                  static const unsigned char kPng[8] = {0x89, 'P', 'N', 'G',
                                               0x0d, 0x0a, 0x1a, 0x0a};
                  const bool isPng =
                      static_cast<size_t>(req.body.size()) >= sizeof(kPng) &&
                      std::memcmp(req.body.data(), kPng, sizeof(kPng)) == 0;
                  if (!isPng) {
                      replyBizErr(res, QStringLiteral("文件不是 PNG"));
                      return;
                  }
                  const QString base = QStringLiteral("avatar_%1.png").arg(phone);
                  const QString dir =
                      QDir::current().absoluteFilePath(QStringLiteral("uploads"));
                  QFile out(dir + QLatin1Char('/') + base);
                  if (!out.open(QIODevice::WriteOnly)) {
                      replyBizErr(res, QStringLiteral("无法打开上传目录"));
                      return;
                  }
                  const qint64 wrote =
                      out.write(req.body.data(), static_cast<qint64>(req.body.size()));
                  const bool ok = out.flush() &&
                                  out.size() == static_cast<qint64>(req.body.size()) &&
                                  wrote == static_cast<qint64>(req.body.size());
                  out.close();
                  if (!ok) {
                      replyBizErr(res, QStringLiteral("写盘失败"));
                      return;
                  }
                  replyOk(res,
                          json{{"url", (QStringLiteral("/uploads/") + base).toStdString()}});
              });

    // ========== 管理端 (B2：三角色 RBAC + 鉴权 + 审计) ==========
    // 角色: super=全部; operator=只读+设备远程重启; viewer=只读。
    // 除 login 外所有 /api/admin/* 需 Authorization: Bearer <token>。
    // 信封 code: 0 成功 / 1 业务 / 2 参数(400) / 3 未认证(401) / 4 无权限(403)。

    srv_.Post("/api/admin/login",
              [this](const httplib::Request& req, httplib::Response& res) {
                  QString user, pass;
                  try {
                      const auto j = json::parse(req.body);
                      user = QString::fromStdString(j.at("username").get<std::string>());
                      pass = QString::fromStdString(j.at("password").get<std::string>());
                  } catch (...) {
                      reply(res, kCodeBadReq, "body 需 username/password", nullptr);
                      return;
                  }
                  QString role;
                  if (!store_.authenticateAdmin(user, pass, &role)) {
                      replyBizErr(res, QStringLiteral("账号或密码错误"));
                      return;
                  }
                  const QString token = issueAdminToken(user, role);
                  store_.appendAudit(user, QStringLiteral("login"),
                                     QStringLiteral("管理员登录成功"), true);
                  replyOk(res, json{{"username", user.toStdString()},
                                    {"role", role.toStdString()},
                                    {"token", token.toStdString()}});
              });

    // 用户查询(可加状态筛选: 0 全部/1 正常/2 冻结)
    srv_.Get("/api/admin/users",
             [this](const httplib::Request& req, httplib::Response& res) {
                 QString user, role;
                 if (!requireAdmin(req, res, &user, &role))
                     return;
                 const QString phone =
                     QString::fromStdString(req.get_param_value("phone"));
                 const int status = intParam(req, "status", 0);
                 const auto users = store_.searchUsers(phone, status);
                 json arr = json::array();
                 for (const auto& u : users)
                     arr.push_back(userToJson(u));
                 replyOk(res, std::move(arr));
             });

    // 冻结/解冻(仅 super；自动审计)
    srv_.Post(R"(/api/admin/users/(\d+)/freeze)",
              [this](const httplib::Request& req, httplib::Response& res) {
                  QString user, role;
                  if (!requireAdmin(req, res, &user, &role))
                      return;
                  if (!canDo(role, "user.freeze")) {
                      reply(res, kCodeForbid, "无权限执行该操作(仅超级管理员)", nullptr);
                      return;
                  }
                  if (req.matches.size() < 2) {
                      reply(res, kCodeBadReq, "缺用户 id", nullptr);
                      return;
                  }
                  bool frozen = true;
                  try {
                      frozen = json::parse(req.body).value("frozen", true);
                  } catch (...) {
                  }
                  const int id = std::stoi(req.matches[1]);
                  const bool st = store_.setUserStatus(id, frozen ? 1 : 0);
                  store_.appendAudit(
                      user, QStringLiteral("user.freeze"),
                      QStringLiteral("%1用户 #%2")
                          .arg(frozen ? QStringLiteral("冻结")
                                      : QStringLiteral("解冻"))
                          .arg(id),
                      st);
                  if (!st) {
                      replyBizErr(res, QStringLiteral("用户不存在或保存失败"));
                      return;
                  }
                  replyOk(res, nullptr);
              });

    // 站列表(带在线率，供资产管理/复杂筛选)
    srv_.Get("/api/admin/stations",
             [this](const httplib::Request& req, httplib::Response& res) {
                 QString user, role;
                 if (!requireAdmin(req, res, &user, &role))
                     return;
                 const QString q =
                     QString::fromStdString(req.get_param_value("q")).trimmed();
                 const auto devSt = store_.listDeviceStations();
                 std::map<int, int> devToSt;
                 for (const auto& p : devSt)
                     devToSt[p.first] = p.second;
                 std::map<int, int> stOnline;
                 for (const SimLive& l : simLiveSnapshot()) {
                     const auto it = devToSt.find(l.deviceId);
                     if (it != devToSt.end())
                         ++stOnline[it->second];
                 }
                 json arr = json::array();
                 for (const auto& st : store_.listStations()) {
                     if (!q.isEmpty() &&
                         !st.name.contains(q, Qt::CaseInsensitive) &&
                         !st.address.contains(q, Qt::CaseInsensitive))
                         continue;
                     arr.push_back(adminStationToJson(st, stOnline[st.id]));
                 }
                 replyOk(res, std::move(arr));
             });

    // 建站(仅 super；审计)
    srv_.Post("/api/admin/stations",
              [this](const httplib::Request& req, httplib::Response& res) {
                  QString user, role;
                  if (!requireAdmin(req, res, &user, &role))
                      return;
                  if (!canDo(role, "station.write")) {
                      reply(res, kCodeForbid, "无权限执行该操作(仅超级管理员)", nullptr);
                      return;
                  }
                  QString name, address;
                  double lat = 0, lng = 0;
                  ncs::MoneyCents price = 0;
                  try {
                      const auto j = json::parse(req.body);
                      name = QString::fromStdString(j.at("name").get<std::string>());
                      address = QString::fromStdString(j.at("address").get<std::string>());
                      lat = j.value("latitude", 0.0);
                      lng = j.value("longitude", 0.0);
                      price = j.value("price_cents", 0LL);
                  } catch (...) {
                      reply(res, kCodeBadReq, "body 需 name/address", nullptr);
                      return;
                  }
                  if (name.trimmed().isEmpty() || address.trimmed().isEmpty()) {
                      replyBizErr(res, QStringLiteral("名称与地址不能为空"));
                      return;
                  }
                  StationFields f;
                  {
                      try {
                          const auto j = json::parse(req.body);
                          if (j.contains("amenities_mask"))
                              f.amenities = j.value("amenities_mask", 0);
                          else if (j.contains("amenities") && j.at("amenities").is_array()) {
                              QStringList names;
                              for (const auto& v : j.at("amenities"))
                                  names << QString::fromStdString(v.get<std::string>());
                              f.amenities = ncs::stationAmenityMask(names);
                          }
                          f.parking = j.value("parking", 0);
                          f.location = j.value("location", 0);
                          f.isPromo = j.value("is_promo", false);
                          if (j.contains("open_hours") && !j.at("open_hours").is_null())
                              f.openHours = QString::fromStdString(
                                  j.at("open_hours").get<std::string>());
                          f.minChargeCents = j.value("min_charge_cents", 0LL);
                      } catch (...) {
                      }
                  }
                  const int id = store_.createStation(
                      name.trimmed(), address.trimmed(), lat, lng,
                      qMax<ncs::MoneyCents>(0, price), f);
                  const bool ok = id > 0;
                  store_.appendAudit(user, QStringLiteral("station.create"),
                                     QStringLiteral("新建站 %1").arg(name.trimmed()),
                                     ok);
                  if (!ok) {
                      replyBizErr(res, QStringLiteral("建站失败"));
                      return;
                  }
                  replyOk(res, json{{"id", id}});
              });

    // 改站(仅 super；审计)
    srv_.Patch(R"(/api/admin/stations/(\d+))",
               [this](const httplib::Request& req, httplib::Response& res) {
                   QString user, role;
                   if (!requireAdmin(req, res, &user, &role))
                       return;
                   if (!canDo(role, "station.write")) {
                       reply(res, kCodeForbid, "无权限执行该操作(仅超级管理员)", nullptr);
                       return;
                   }
                   if (req.matches.size() < 2) {
                       reply(res, kCodeBadReq, "缺站 id", nullptr);
                       return;
                   }
                   QString name, address;
                   double lat = 0, lng = 0;
                   ncs::MoneyCents price = 0;
                   try {
                       const auto j = json::parse(req.body);
                       name = QString::fromStdString(j.at("name").get<std::string>());
                       address = QString::fromStdString(j.at("address").get<std::string>());
                       lat = j.value("latitude", 0.0);
                       lng = j.value("longitude", 0.0);
                       price = j.value("price_cents", 0LL);
                   } catch (...) {
                       reply(res, kCodeBadReq, "body 需 name/address", nullptr);
                       return;
                   }
                   const int id = std::stoi(req.matches[1]);
                   StationFields f;
                   {
                       try {
                           const auto j = json::parse(req.body);
                           if (j.contains("amenities_mask"))
                               f.amenities = j.value("amenities_mask", 0);
                           else if (j.contains("amenities") && j.at("amenities").is_array()) {
                               QStringList names;
                               for (const auto& v : j.at("amenities"))
                                   names << QString::fromStdString(v.get<std::string>());
                               f.amenities = ncs::stationAmenityMask(names);
                           }
                           f.parking = j.value("parking", 0);
                           f.location = j.value("location", 0);
                           f.isPromo = j.value("is_promo", false);
                           if (j.contains("open_hours") && !j.at("open_hours").is_null())
                               f.openHours = QString::fromStdString(
                                   j.at("open_hours").get<std::string>());
                           f.minChargeCents = j.value("min_charge_cents", 0LL);
                       } catch (...) {
                       }
                   }                   const bool ok = store_.updateStation(
                       id, name.trimmed(), address.trimmed(), lat, lng,
                       qMax<ncs::MoneyCents>(0, price), f);
                   store_.appendAudit(user, QStringLiteral("station.update"),
                                      QStringLiteral("修改站 #%1").arg(id), ok);
                   if (!ok) {
                       replyBizErr(res, QStringLiteral("站不存在或保存失败"));
                       return;
                   }
                   replyOk(res, nullptr);
               });

    // 删站(仅 super；审计；禁删保护)
    srv_.Delete(R"(/api/admin/stations/(\d+))",
                [this](const httplib::Request& req, httplib::Response& res) {
                    QString user, role;
                    if (!requireAdmin(req, res, &user, &role))
                        return;
                    if (!canDo(role, "station.write")) {
                        reply(res, kCodeForbid, "无权限执行该操作(仅超级管理员)", nullptr);
                        return;
                    }
                    if (req.matches.size() < 2) {
                        reply(res, kCodeBadReq, "缺站 id", nullptr);
                        return;
                    }
                    const int id = std::stoi(req.matches[1]);
                    if (store_.countDevicesByStation(id) > 0) {
                        replyBizErr(res, QStringLiteral("站下仍有电桩，禁止删除"));
                        return;
                    }
                    const bool ok = store_.deleteStationById(id);
                    store_.appendAudit(user, QStringLiteral("station.delete"),
                                       QStringLiteral("删除站 #%1").arg(id), ok);
                    if (!ok) {
                        replyBizErr(res, QStringLiteral("站不存在或删除失败"));
                        return;
                    }
                    replyOk(res, nullptr);
                });

    // 批量建桩(仅 super；审计；站存在校验+整批事务)
    srv_.Post(R"(/api/admin/stations/(\d+)/devices)",
              [this](const httplib::Request& req, httplib::Response& res) {
                  QString user, role;
                  if (!requireAdmin(req, res, &user, &role))
                      return;
                  if (!canDo(role, "device.write")) {
                      reply(res, kCodeForbid, "无权限执行该操作(仅超级管理员)", nullptr);
                      return;
                  }
                  if (req.matches.size() < 2) {
                      reply(res, kCodeBadReq, "缺站 id", nullptr);
                      return;
                  }
                  int count = 0, type = 0;
                  double powerKw = 0;
                  try {
                      const auto j = json::parse(req.body);
                      count = j.value("count", 0);
                      type = j.value("type", 0);
                      powerKw = j.value("power_kw", 0.0);
                  } catch (...) {
                      reply(res, kCodeBadReq, "body 需 count/type/power_kw", nullptr);
                      return;
                  }
                  count = qBound(1, count, 200);
                  if (type != 0 && type != 1) {
                      replyBizErr(res, QStringLiteral("type 需 0(快)/1(慢)"));
                      return;
                  }
                  const int id = std::stoi(req.matches[1]);
                  const int cid = store_.createDevices(id, type, count, powerKw);
                  store_.appendAudit(
                      user, QStringLiteral("device.create"),
                      QStringLiteral("向站 #%1 批量建桩 %2 台").arg(id).arg(count),
                      cid > 0);
                  if (cid < 0) {
                      replyBizErr(res, cid == -1 ? QStringLiteral("站不存在，无法建桩")
                                                 : QStringLiteral("建桩失败(已整批回滚)"));
                      return;
                  }
                  replyOk(res, json{{"created", count}});
              });

    // 删桩(仅 super；审计；非空闲拒绝 + 计数事务回退)
    srv_.Delete(R"(/api/admin/devices/(\d+))",
                [this](const httplib::Request& req, httplib::Response& res) {
                    QString user, role;
                    if (!requireAdmin(req, res, &user, &role))
                        return;
                    if (!canDo(role, "device.write")) {
                        reply(res, kCodeForbid, "无权限执行该操作(仅超级管理员)", nullptr);
                        return;
                    }
                    if (req.matches.size() < 2) {
                        reply(res, kCodeBadReq, "缺桩 id", nullptr);
                        return;
                    }
                    const int id = std::stoi(req.matches[1]);
                    const int r = store_.deleteDeviceIfIdle(id);
                    if (r < 0) {
                        replyBizErr(res, QStringLiteral("桩不存在"));
                        return;
                    }
                    if (r == 0) {
                        replyBizErr(res, QStringLiteral("桩使用中(非空闲)，禁止删除"));
                        return;
                    }
                    store_.appendAudit(user, QStringLiteral("device.delete"),
                                       QStringLiteral("删除桩 #%1").arg(id), true);
                    forgetDevice(id);
                    replyOk(res, nullptr);
                });

    // 设备列表(复杂筛选 + 实时在线/功率/电量 + 累计次数/时长，带分页)
    srv_.Get("/api/admin/devices",
             [this](const httplib::Request& req, httplib::Response& res) {
                 QString user, role;
                 if (!requireAdmin(req, res, &user, &role))
                     return;
                 DeviceFilter f;
                 f.stationId = intParam(req, "station_id", -1);
                 f.type = intParam(req, "type", -1);
                 f.state = intParam(req, "state", -1);
                 f.q = QString::fromStdString(req.get_param_value("q")).trimmed();
                 int page = std::max(1, intParam(req, "page", 1));
                 int pageSize = qBound(1, intParam(req, "page_size", 50), 200);
                 const int offset = (page - 1) * pageSize;
                 std::map<int, SimLive> liveMap;
                 for (const SimLive& l : simLiveSnapshot())
                     liveMap[l.deviceId] = l;
                 QVector<DeviceRow> rows;
                 store_.listDevicesAdmin(f, pageSize, offset, &rows);
                 json arr = json::array();
                 for (const auto& r : rows) {
                     SimLive blank;
                     const auto it = liveMap.find(r.dev.id);
                     arr.push_back(adminDeviceToJson(r, it != liveMap.end() ? it->second : blank));
                 }
                 replyOk(res,
                         json{{"items", std::move(arr)},
                              {"total", store_.countDevicesAdmin(f)}});
             });

    // 远程重启(故障→重启中→恢复；super/operator 可操作)
    srv_.Post(R"(/api/admin/devices/(\d+)/restart)",
              [this](const httplib::Request& req, httplib::Response& res) {
                  QString user, role;
                  if (!requireAdmin(req, res, &user, &role))
                      return;
                  if (!canDo(role, "device.restart")) {
                      reply(res, kCodeForbid, "无权限执行远程重启(需运维角色以上)", nullptr);
                      return;
                  }
                  if (req.matches.size() < 2) {
                      reply(res, kCodeBadReq, "缺桩 id", nullptr);
                      return;
                  }
                  const int id = std::stoi(req.matches[1]);
                  QString err;
                  if (!adminRestartDevice(id, user, &err)) {
                      replyBizErr(res, err);
                      return;
                  }
                  replyOk(res, json{{"state", static_cast<int>(ncs::DeviceState::Rebooting)}});
              });

    // 运维日志 / 审计日志(任意已登录角色可看；分页)
    srv_.Get("/api/admin/logs/ops",
             [this](const httplib::Request& req, httplib::Response& res) {
                 QString user, role;
                 if (!requireAdmin(req, res, &user, &role))
                     return;
                 const int page = std::max(1, intParam(req, "page", 1));
                 const int pageSize = qBound(1, intParam(req, "page_size", 50), 200);
                 const int offset = (page - 1) * pageSize;
                 const auto items = store_.listDeviceOps(pageSize, offset);
                 json arr = json::array();
                 for (const auto& o : items)
                     arr.push_back(deviceOpToJson(o));
                 replyOk(res, json{{"items", std::move(arr)},
                                   {"total", store_.countDeviceOps()}});
             });

    srv_.Get("/api/admin/logs/audit",
             [this](const httplib::Request& req, httplib::Response& res) {
                 QString user, role;
                 if (!requireAdmin(req, res, &user, &role))
                     return;
                 const int page = std::max(1, intParam(req, "page", 1));
                 const int pageSize = qBound(1, intParam(req, "page_size", 50), 200);
                 const int offset = (page - 1) * pageSize;
                 const auto items = store_.listAudit(pageSize, offset);
                 json arr = json::array();
                 for (const auto& a : items)
                     arr.push_back(auditToJson(a));
                 replyOk(res, json{{"items", std::move(arr)},
                                   {"total", store_.countAudit()}});
             });

    // 经营统计概览(字段全在后端按订单/设备现算，前端壳可整体替换)
    srv_.Get("/api/admin/stats/overview",
             [this](const httplib::Request& req, httplib::Response& res) {
                 QString user, role;
                 if (!requireAdmin(req, res, &user, &role))
                     return;
                 const QDate today = QDateTime::currentDateTimeUtc().date();
                 const QString t0 = dayStartIso(today);
                 const QString t1 = dayStartIso(today.addDays(1));
                 const QString m0 =
                     dayStartIso(QDate(today.year(), today.month(), 1));
                 const RevenueAgg todayAgg = store_.revenueWindow(t0, t1);
                 const RevenueAgg monthAgg = store_.revenueWindow(m0, t1);
                 const RevenueAgg totalAgg = store_.revenueWindow(QString(), QString());
                 auto aggJson = [](const RevenueAgg& a) {
                     return json{{"revenue_cents", a.cents},
                                 {"orders", a.orders},
                                 {"energy_kwh", a.energyKwh}};
                 };
                 const auto counts = store_.deviceStateCounts();
                 json health{{"idle", counts[0]},
                             {"charging", counts[1]},
                             {"fault", counts[2]},
                             {"reserved", counts[3]},
                             {"rebooting", counts[4]}};
                 const auto devSt = store_.listDeviceStations();
                 std::map<int, int> devToSt;
                 for (const auto& p : devSt)
                     devToSt[p.first] = p.second;
                 std::map<int, int> stOnline;
                 for (const SimLive& l : simLiveSnapshot()) {
                     const auto it = devToSt.find(l.deviceId);
                     if (it != devToSt.end())
                         ++stOnline[it->second];
                 }
                 json stArr = json::array();
                 for (const auto& st : store_.listStations())
                     stArr.push_back(adminStationToJson(st, stOnline[st.id]));
                 int devTotal = 0;
                 for (const int c : counts)
                     devTotal += c;
                 replyOk(res, json{{"today", aggJson(todayAgg)},
                                   {"month", aggJson(monthAgg)},
                                   {"total", aggJson(totalAgg)},
                                   {"device_health", health},
                                   {"devices_total", devTotal},
                                   {"devices_online",
                                    static_cast<long long>(simLiveSnapshot().size())},
                                   {"stations", std::move(stArr)}});
             });

    // 按日营收(近 N 天，含今天；供折线/表格)
    srv_.Get("/api/admin/stats/daily",
             [this](const httplib::Request& req, httplib::Response& res) {
                 QString user, role;
                 if (!requireAdmin(req, res, &user, &role))
                     return;
                 const int days = qBound(1, intParam(req, "days", 7), 90);
                 const auto rows = store_.dailyRevenue(days);
                 json arr = json::array();
                 for (const auto& d : rows)
                     arr.push_back(dailyToJson(d));
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
                const std::string type = j.value("type", "");
                if (type == "register") {
                    std::vector<int> ids;
                    for (const auto& v : j.value("devices", json::array()))
                        ids.push_back(v.get<int>());
                    registerSimDevices(fd, ids);
                } else if (type == "heartbeat") {
                    Heartbeat hb;
                    hb.deviceId = j.value("device_id", 0);
                    hb.voltage = j.value("voltage", 0.0);
                    hb.current = j.value("current", 0.0);
                    hb.temperature = j.value("temperature", 0.0);
                    hb.powerKw = j.value("power_kw", 0.0);
                    hb.energyKwh = j.value("energy_kwh", 0.0);
                    hb.tsMs = j.value("ts", 0LL);
                    hb.simState = j.value("sim_state", -1);
                    const long long nowMs = QDateTime::currentMSecsSinceEpoch();
                    {
                        std::lock_guard<std::mutex> lk(simMu_);
                        deviceEnergy_[hb.deviceId] = hb.energyKwh;
                        devicePower_[hb.deviceId] = hb.powerKw;
                        deviceLastTs_[hb.deviceId] = hb.tsMs > 0 ? hb.tsMs : nowMs;
                    }
                    sink_.onHeartbeat(hb);
                    applySimState(hb.deviceId, hb.simState);
                }
            } catch (...) {
            }
        }
    }
    unregisterSimFd(fd);
    ::close(fd);
}

void BackendApp::registerSimDevices(int fd, const std::vector<int>& ids) {
    std::lock_guard<std::mutex> lk(simMu_);
    const long long nowMs = QDateTime::currentMSecsSinceEpoch();
    for (const int id : ids) {
        deviceFd_[id] = fd;
        deviceEnergy_[id] = 0.0;
        devicePower_[id] = 0.0;
        deviceLastTs_[id] = nowMs;
    }
}

void BackendApp::unregisterSimFd(int fd) {
    std::lock_guard<std::mutex> lk(simMu_);
    for (auto it = deviceFd_.begin(); it != deviceFd_.end();) {
        if (it->second != fd) {
            ++it;
            continue;
        }
        const int id = it->first;
        deviceEnergy_.erase(id);
        devicePower_.erase(id);
        deviceLastTs_.erase(id);
        it = deviceFd_.erase(it);
    }
}

bool BackendApp::sendSimCommand(int deviceId, bool start) {
    std::lock_guard<std::mutex> lk(simMu_);
    const auto it = deviceFd_.find(deviceId);
    if (it == deviceFd_.end())
        return false;
    const json j{{"cmd", start ? "start" : "stop"}, {"device_id", deviceId}};
    const std::string line = j.dump() + "\n";
    const ssize_t n = send(it->second, line.data(), line.size(), MSG_NOSIGNAL);
    return n >= 0;
}

double BackendApp::simEnergy(int deviceId) const {
    std::lock_guard<std::mutex> lk(simMu_);
    const auto it = deviceEnergy_.find(deviceId);
    return it == deviceEnergy_.end() ? -1.0 : it->second;
}

bool BackendApp::sendSimRestart(int deviceId) {
    std::lock_guard<std::mutex> lk(simMu_);
    const auto it = deviceFd_.find(deviceId);
    if (it == deviceFd_.end())
        return false;
    const json j{{"cmd", "restart"}, {"device_id", deviceId}};
    const std::string line = j.dump() + "\n";
    const ssize_t n = send(it->second, line.data(), line.size(), MSG_NOSIGNAL);
    return n >= 0;
}

void BackendApp::forgetDevice(int deviceId) {
    std::lock_guard<std::mutex> lk(simMu_);
    deviceFd_.erase(deviceId);
    deviceEnergy_.erase(deviceId);
    devicePower_.erase(deviceId);
    deviceLastTs_.erase(deviceId);
}

QVector<SimLive> BackendApp::simLiveSnapshot() const {
    QVector<SimLive> out;
    std::lock_guard<std::mutex> lk(simMu_);
    out.reserve(static_cast<int>(deviceFd_.size()));
    for (const auto& kv : deviceFd_) {
        SimLive l;
        l.deviceId = kv.first;
        l.online = true;
        const auto pe = devicePower_.find(kv.first);
        if (pe != devicePower_.end())
            l.powerKw = pe->second;
        const auto en = deviceEnergy_.find(kv.first);
        if (en != deviceEnergy_.end())
            l.energyKwh = en->second;
        const auto ts = deviceLastTs_.find(kv.first);
        if (ts != deviceLastTs_.end())
            l.lastTsMs = ts->second;
        out.push_back(l);
    }
    return out;
}

QString BackendApp::issueAdminToken(const QString& username,
                                    const QString& role) {
    QString token;
    token.reserve(64);
    for (int i = 0; i < 32; ++i) {
        token += QStringLiteral("%1")
                     .arg(QRandomGenerator::global()->bounded(256), 2, 16,
                          QLatin1Char('0'));
    }
    std::lock_guard<std::mutex> lk(sessionMu_);
    sessions_[token] = Session{username, role};
    return token;
}

bool BackendApp::adminSession(const QString& token, QString* username,
                              QString* role) const {
    if (token.isEmpty())
        return false;
    std::lock_guard<std::mutex> lk(sessionMu_);
    const auto it = sessions_.find(token);
    if (it == sessions_.end())
        return false;
    if (username)
        *username = it->second.username;
    if (role)
        *role = it->second.role;
    return true;
}

bool BackendApp::requireAdmin(const httplib::Request& req,
                              httplib::Response& res, QString* username,
                              QString* role) {
    const QString token = QString::fromStdString(bearerToken(req));
    if (!adminSession(token, username, role)) {
        reply(res, kCodeUnauth, "未登录或登录已过期，请重新登录", nullptr);
        return false;
    }
    return true;
}

// 心跳驱动的故障/恢复流转(仅设备自主侧，不动业务进行中的桩)
// 心跳驱动的故障/恢复流转。
// 注意：不设"上次已见过 sim_state"的缓存去重——业务进行中(Reserved/Charging)
// 被忽略的故障若被缓存记住，会在桩回到 Idle 后因去重提前 return，造成"隐形故障"。
// 改为：每次心跳都用"当前 DB 状态 + 本次 sim_state"独立判定；无需变更则直接返回
// (轻量只读探针，不开事务)，需要变更才开事务并按事务内最新状态落库。
void BackendApp::applySimState(int deviceId, int simState) {
    if (simState < 0)
        return;
    ncs::Device probe;
    if (!store_.getDeviceById(deviceId, &probe))  // 桩不存在/已删：忽略
        return;
    const bool needChange =
        (simState == 2 && probe.state == ncs::DeviceState::Idle) ||
        (simState == 4 && probe.state == ncs::DeviceState::Fault) ||
        (simState != 2 && simState != 4 &&
         (probe.state == ncs::DeviceState::Fault ||
          probe.state == ncs::DeviceState::Rebooting));
    if (!needChange)
        return;  // 已一致，或 Reserved/Charging 业务进行中(下个心跳再评估)
    if (!store_.beginTx())  // 服务忙则下个心跳重试
        return;
    ncs::Device d;
    bool changed = false;
    QString op, detail;
    if (store_.getDeviceById(deviceId, &d)) {  // 事务内以最新状态为准(防与预约/结算交错)
        if (simState == 2) {  // 设备自主上报故障
            if (d.state == ncs::DeviceState::Idle) {
                store_.setDeviceState(deviceId,
                                      static_cast<int>(ncs::DeviceState::Fault));
                store_.adjustStationFree(d.stationId, -1);
                changed = true;
                op = QStringLiteral("fault");
                detail = QStringLiteral("设备上报故障");
            }
            // Reserved/Charging 中忽略，避免破坏进行中订单
        } else if (simState == 4) {
            if (d.state == ncs::DeviceState::Fault)
                store_.setDeviceState(
                    deviceId, static_cast<int>(ncs::DeviceState::Rebooting));
            // 运维日志由 adminRestartDevice 记录，不在此重复
        } else {  // 恢复正常(0/1)
            if (d.state == ncs::DeviceState::Fault) {
                store_.setDeviceState(deviceId,
                                      static_cast<int>(ncs::DeviceState::Idle));
                store_.adjustStationFree(d.stationId, 1);
                changed = true;
                op = QStringLiteral("recover");
                detail = QStringLiteral("设备自愈恢复正常");
            } else if (d.state == ncs::DeviceState::Rebooting) {
                store_.setDeviceState(deviceId,
                                      static_cast<int>(ncs::DeviceState::Idle));
                store_.adjustStationFree(d.stationId, 1);
                changed = true;
                op = QStringLiteral("recover");
                detail = QStringLiteral("远程重启成功，恢复正常");
                std::lock_guard<std::mutex> lk(rebootingMu_);
                rebootingSince_.erase(deviceId);
            }
        }
    }
    if (changed)
        store_.appendDeviceOp(deviceId, op, QString(), detail);
    store_.commitTx();
}

bool BackendApp::adminRestartDevice(int deviceId, const QString& opBy,
                                    QString* err) {
    ncs::Device d;
    if (!store_.getDeviceById(deviceId, &d)) {
        if (err)
            *err = QStringLiteral("桩不存在");
        return false;
    }
    if (d.state != ncs::DeviceState::Fault) {
        if (err)
            *err = QStringLiteral("仅故障桩可远程重启");
        return false;
    }
    if (!store_.beginTx()) {
        if (err)
            *err = QStringLiteral("服务繁忙，请稍后再试");
        return false;
    }
    if (!store_.setDeviceState(deviceId,
                               static_cast<int>(ncs::DeviceState::Rebooting))) {
        store_.rollbackTx();
        if (err)
            *err = QStringLiteral("置重启态失败");
        return false;
    }
    store_.appendDeviceOp(deviceId, QStringLiteral("restart"), opBy,
                          QStringLiteral("对故障桩下发远程重启"));
    store_.commitTx();
    {
        std::lock_guard<std::mutex> lk(rebootingMu_);
        rebootingSince_[deviceId] =
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now().time_since_epoch())
                .count();
    }
    store_.appendAudit(opBy, QStringLiteral("device.restart"),
                       QStringLiteral("重启桩 #%1").arg(deviceId), true);
    sendSimRestart(deviceId);  // 在线则即时下发；离线走超时强制恢复
    return true;
}

void BackendApp::restartLoop(int rebootSec) {
    const long long timeoutMs = static_cast<long long>(rebootSec) * 1000LL;
    while (restartRunning_.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        std::vector<int> expired;
        {
            std::lock_guard<std::mutex> lk(rebootingMu_);
            const long long now =
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now().time_since_epoch())
                    .count();
            for (const auto& kv : rebootingSince_)
                if (now - kv.second >= timeoutMs)
                    expired.push_back(kv.first);
        }
        for (const int id : expired) {
            if (!store_.beginTx())
                continue;
            ncs::Device d;
            if (!store_.getDeviceById(id, &d) ||
                d.state != ncs::DeviceState::Rebooting) {
                store_.rollbackTx();
                std::lock_guard<std::mutex> lk(rebootingMu_);
                rebootingSince_.erase(id);
                continue;
            }
            store_.setDeviceState(id, static_cast<int>(ncs::DeviceState::Idle));
            store_.adjustStationFree(d.stationId, 1);
            store_.appendDeviceOp(id, QStringLiteral("recover"), QString(),
                                  QStringLiteral("重启超时未收到设备上报，强制恢复空闲"));
            store_.commitTx();
            std::lock_guard<std::mutex> lk(rebootingMu_);
            rebootingSince_.erase(id);
        }
    }
}

bool BackendApp::startRestartSweeper(int rebootSec) {
    if (restartRunning_.load())
        return false;
    restartRunning_.store(true);
    restartThread_ = std::thread([this, rebootSec] { restartLoop(rebootSec); });
    return true;
}

void BackendApp::stopRestartSweeper() {
    restartRunning_.store(false);
    if (restartThread_.joinable())
        restartThread_.join();
}

bool BackendApp::startReserveSweeper(int timeoutSec) {
    if (!charge_ || sweepRunning_.load())
        return false;
    sweepRunning_.store(true);
    sweepThread_ = std::thread([this, timeoutSec] {
        const int stepMs = std::max(1000, std::min(30000, timeoutSec * 1000 / 2));
        while (sweepRunning_.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(stepMs));
            if (!sweepRunning_.load())
                break;
            const int n = charge_->sweepExpiredReservations(timeoutSec);
            if (n > 0) {
                std::printf("[sweep] released %d expired reservation(s)", n);
                std::putchar(10);
            }
        }
    });
    return true;
}

void BackendApp::stopReserveSweeper() {
    sweepRunning_.store(false);
    if (sweepThread_.joinable())
        sweepThread_.join();
}

}  // namespace backend
}  // namespace ncs
