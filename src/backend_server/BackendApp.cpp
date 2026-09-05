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

#include <QDir>
#include <QFile>
#include <QDateTime>

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

BackendApp::BackendApp(const QString& dbPath)
    : dbPath_(dbPath), auth_(&store_) {
    charge_ = std::make_unique<ChargeService>(
        &store_,
        [this](int deviceId, bool start) { sendSimCommand(deviceId, start); },
        [this](int deviceId) { return simEnergy(deviceId); });
}

BackendApp::~BackendApp() {
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

    // ---------- 管理端 (B1) ----------
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
                  if (!store_.authenticateAdmin(user, pass)) {
                      replyBizErr(res, QStringLiteral("账号或密码错误"));
                      return;
                  }
                  replyOk(res, json{{"username", user.toStdString()},
                                    {"role", "admin"}});
              });

    srv_.Get("/api/admin/users",
             [this](const httplib::Request& req, httplib::Response& res) {
                 const QString phone =
                     QString::fromStdString(req.get_param_value("phone"));
                 const auto users = store_.searchUsers(phone);
                 json arr = json::array();
                 for (const auto& u : users)
                     arr.push_back(userToJson(u));
                 replyOk(res, std::move(arr));
             });

    srv_.Post(R"(/api/admin/users/(\d+)/freeze)",
              [this](const httplib::Request& req, httplib::Response& res) {
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
                  if (!store_.setUserStatus(id, frozen ? 1 : 0)) {
                      replyBizErr(res, QStringLiteral("用户不存在或保存失败"));
                      return;
                  }
                  replyOk(res, nullptr);
              });

    srv_.Post("/api/admin/stations",
              [this](const httplib::Request& req, httplib::Response& res) {
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
                  const int id = store_.createStation(
                      name.trimmed(), address.trimmed(), lat, lng,
                      qMax<ncs::MoneyCents>(0, price));
                  if (id < 0) {
                      replyBizErr(res, QStringLiteral("建站失败"));
                      return;
                  }
                  replyOk(res, json{{"id", id}});
              });

    srv_.Patch(R"(/api/admin/stations/(\d+))",
               [this](const httplib::Request& req, httplib::Response& res) {
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
                   if (!store_.updateStation(id, name.trimmed(), address.trimmed(),
                                             lat, lng,
                                             qMax<ncs::MoneyCents>(0, price))) {
                       replyBizErr(res, QStringLiteral("站不存在或保存失败"));
                       return;
                   }
                   replyOk(res, nullptr);
               });

    srv_.Delete(R"(/api/admin/stations/(\d+))",
                [this](const httplib::Request& req, httplib::Response& res) {
                    if (req.matches.size() < 2) {
                        reply(res, kCodeBadReq, "缺站 id", nullptr);
                        return;
                    }
                    const int id = std::stoi(req.matches[1]);
                    if (store_.countDevicesByStation(id) > 0) {
                        replyBizErr(res, QStringLiteral("站下仍有电桩，禁止删除"));
                        return;
                    }
                    if (!store_.deleteStationById(id)) {
                        replyBizErr(res, QStringLiteral("站不存在或删除失败"));
                        return;
                    }
                    replyOk(res, nullptr);
                });

    srv_.Post(R"(/api/admin/stations/(\d+)/devices)",
              [this](const httplib::Request& req, httplib::Response& res) {
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
                  if (!store_.createDevices(id, type, count, powerKw)) {
                      replyBizErr(res, QStringLiteral("站不存在或建桩失败"));
                      return;
                  }
                  replyOk(res, json{{"created", count}});
              });

    srv_.Delete(R"(/api/admin/devices/(\d+))",
                [this](const httplib::Request& req, httplib::Response& res) {
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
                    replyOk(res, nullptr);
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
                    {
                        std::lock_guard<std::mutex> lk(simMu_);
                        deviceEnergy_[hb.deviceId] = hb.energyKwh;
                    }
                    sink_.onHeartbeat(hb);
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
    for (const int id : ids) {
        deviceFd_[id] = fd;
        deviceEnergy_[id] = 0.0;
    }
}

void BackendApp::unregisterSimFd(int fd) {
    std::lock_guard<std::mutex> lk(simMu_);
    for (auto it = deviceFd_.begin(); it != deviceFd_.end();) {
        if (it->second == fd)
            it = deviceFd_.erase(it);
        else
            ++it;
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
