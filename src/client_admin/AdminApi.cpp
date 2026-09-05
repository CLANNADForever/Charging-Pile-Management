#include "AdminApi.h"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonObject>
#include <QUrl>

namespace ncs {
namespace admin {

namespace {

QDateTime iso(QString s) {
    return s.isEmpty() ? QDateTime()
                       : QDateTime::fromString(s, Qt::ISODate).toUTC();
}
QString enc(const QString& s) {
    return QString::fromUtf8(QUrl::toPercentEncoding(s));
}
User userFromJson(const QJsonObject& o) {
    User u;
    u.id = o.value("id").toInt();
    u.phone = o.value("phone").toString();
    u.nickname = o.value("nickname").toString();
    u.balanceCents = static_cast<MoneyCents>(o.value("balance_cents").toDouble());
    u.status = static_cast<UserStatus>(o.value("status").toInt());
    u.registeredAt = iso(o.value("registered_at").toString());
    return u;
}
Station stationFromJson(const QJsonObject& o) {
    Station s;
    s.id = o.value("id").toInt();
    s.name = o.value("name").toString();
    s.address = o.value("address").toString();
    s.latitude = o.value("latitude").toDouble();
    s.longitude = o.value("longitude").toDouble();
    s.totalPiles = o.value("total_piles").toInt();
    s.pricePerKwhCents =
        static_cast<MoneyCents>(o.value("price_cents").toDouble());
    s.freePiles = o.value("free_piles").toInt();
    return s;
}
Device deviceFromJson(const QJsonObject& o) {
    Device d;
    d.id = o.value("id").toInt();
    d.stationId = o.value("station_id").toInt();
    d.type = static_cast<DeviceType>(o.value("type").toInt());
    d.state = static_cast<DeviceState>(o.value("state").toInt());
    d.powerKw = o.value("power_kw").toDouble();
    d.energyKwh = o.value("energy_kwh").toDouble();
    return d;
}
AdminStation adminStationFromJson(const QJsonObject& o) {
    AdminStation s;
    s.id = o.value("id").toInt();
    s.name = o.value("name").toString();
    s.address = o.value("address").toString();
    s.latitude = o.value("latitude").toDouble();
    s.longitude = o.value("longitude").toDouble();
    s.totalPiles = o.value("total_piles").toInt();
    s.freePiles = o.value("free_piles").toInt();
    s.priceCents = static_cast<MoneyCents>(o.value("price_cents").toDouble());
    s.online = o.value("online").toInt();
    s.offline = o.value("offline").toInt();
    s.onlineRate = o.value("online_rate").toInt();
    return s;
}
AdminDevice adminDeviceFromJson(const QJsonObject& o) {
    AdminDevice d;
    d.id = o.value("id").toInt();
    d.stationId = o.value("station_id").toInt();
    d.stationName = o.value("station_name").toString();
    d.type = o.value("type").toInt();
    d.state = o.value("state").toInt();
    d.online = o.value("online").toBool();
    d.powerKw = o.value("power_kw").toDouble();
    d.energyKwh = o.value("energy_kwh").toDouble();
    d.lastTs = static_cast<qint64>(o.value("last_ts").toDouble());
    d.sessions = static_cast<qint64>(o.value("sessions").toDouble());
    d.chargeSec = o.value("charging_sec").toDouble();
    return d;
}
OpLogRow opFromJson(const QJsonObject& o) {
    OpLogRow r;
    r.id = o.value("id").toInt();
    r.deviceId = o.value("device_id").toInt();
    r.opType = o.value("op_type").toString();
    r.opBy = o.value("op_by").toString();
    r.detail = o.value("detail").toString();
    r.at = iso(o.value("created_at").toString());
    return r;
}
AuditLogRow auditFromJson(const QJsonObject& o) {
    AuditLogRow r;
    r.id = o.value("id").toInt();
    r.username = o.value("username").toString();
    r.action = o.value("action").toString();
    r.detail = o.value("detail").toString();
    r.result = o.value("result").toString();
    r.at = iso(o.value("created_at").toString());
    return r;
}
QString errOf(const HttpJsonClient::Reply& h) {
    if (h.transportOk)
        return h.code == 0 ? QString() : h.message;
    return h.error;
}
}  // namespace

AdminApi::AdminApi(QString baseUrl) : cli_(std::move(baseUrl)) {}

void AdminApi::login(const QString& user, const QString& pass, LoginCb done) {
    cli_.post("/api/admin/login",
              QJsonObject{{"username", user}, {"password", pass}},
              [this, done = std::move(done)](const HttpJsonClient::Reply& h) {
                  LoginInfo info;
                  info.err = errOf(h);
                  if (!info.err.isEmpty()) {
                      done(info);
                      return;
                  }
                  const QJsonObject o = h.data.toObject();
                  info.username = o.value("username").toString();
                  info.role = o.value("role").toString();
                  info.token = o.value("token").toString();
                  if (!info.token.isEmpty()) {
                      cli_.setDefaultHeader(
                          QByteArrayLiteral("Authorization"),
                          QStringLiteral("Bearer %1").arg(info.token).toUtf8());
                  }
                  info.ok = true;
                  done(info);
              });
}

void AdminApi::listUsers(const QString& phone, UsersCb done) {
    listUsers(phone, 0, std::move(done));
}
void AdminApi::listUsers(const QString& phone, int statusFilter, UsersCb done) {
    cli_.get("/api/admin/users?phone=" + enc(phone) + "&status=" +
                 QString::number(statusFilter),
             [done = std::move(done)](const HttpJsonClient::Reply& h) {
                 QVector<User> out;
                 const QString err = errOf(h);
                 if (err.isEmpty())
                     for (const auto& v : h.data.toArray())
                         out.push_back(userFromJson(v.toObject()));
                 done(out, err);
             });
}

void AdminApi::setFrozen(int userId, bool frozen, Cb done) {
    cli_.post("/api/admin/users/" + QString::number(userId) + "/freeze",
              QJsonObject{{"frozen", frozen}},
              [done = std::move(done)](const HttpJsonClient::Reply& h) {
                  done(errOf(h));
              });
}

void AdminApi::listStations(StationsCb done) {
    cli_.get("/api/stations",
             [done = std::move(done)](const HttpJsonClient::Reply& h) {
                 QVector<Station> out;
                 const QString err = errOf(h);
                 if (err.isEmpty())
                     for (const auto& v : h.data.toArray())
                         out.push_back(stationFromJson(v.toObject()));
                 done(out, err);
             });
}

void AdminApi::listDevices(int stationId, DevicesCb done) {
    cli_.get("/api/stations/" + QString::number(stationId) + "/devices",
             [done = std::move(done)](const HttpJsonClient::Reply& h) {
                 QVector<Device> out;
                 const QString err = errOf(h);
                 if (err.isEmpty())
                     for (const auto& v : h.data.toArray())
                         out.push_back(deviceFromJson(v.toObject()));
                 done(out, err);
             });
}

void AdminApi::createStation(const QString& name, const QString& addr, double lat,
                             double lng, ncs::MoneyCents price, IdCb done) {
    cli_.post("/api/admin/stations",
              QJsonObject{{"name", name},
                          {"address", addr},
                          {"latitude", lat},
                          {"longitude", lng},
                          {"price_cents", static_cast<double>(price)}},
              [done = std::move(done)](const HttpJsonClient::Reply& h) {
                  int id = -1;
                  const QString err = errOf(h);
                  if (err.isEmpty())
                      id = h.data.toObject().value("id").toInt(-1);
                  done(id, err);
              });
}

void AdminApi::deleteStation(int id, Cb done) {
    cli_.sendDelete(QStringLiteral("/api/admin/stations/%1").arg(id),
                    [done = std::move(done)](const HttpJsonClient::Reply& h) {
                        done(errOf(h));
                    });
}

void AdminApi::createDevices(int stationId, int count, int type, double powerKw,
                             Cb done) {
    cli_.post("/api/admin/stations/" + QString::number(stationId) + "/devices",
              QJsonObject{{"count", count},
                          {"type", type},
                          {"power_kw", powerKw}},
              [done = std::move(done)](const HttpJsonClient::Reply& h) {
                  done(errOf(h));
              });
}

void AdminApi::deleteDevice(int id, Cb done) {
    cli_.sendDelete(QStringLiteral("/api/admin/devices/%1").arg(id),
                    [done = std::move(done)](const HttpJsonClient::Reply& h) {
                        done(errOf(h));
                    });
}

// ---------- B2 ----------

void AdminApi::listStationsOnline(const QString& q, StationRowsCb done) {
    cli_.get("/api/admin/stations?q=" + enc(q),
             [done = std::move(done)](const HttpJsonClient::Reply& h) {
                 QVector<AdminStation> out;
                 const QString err = errOf(h);
                 if (err.isEmpty())
                     for (const auto& v : h.data.toArray())
                         out.push_back(adminStationFromJson(v.toObject()));
                 done(out, err);
             });
}

void AdminApi::listDevicesFiltered(const QString& q, int stationId, int type,
                                   int state, int page, int pageSize,
                                   DeviceRowsCb done) {
    QString path = QStringLiteral("/api/admin/devices?q=%1&station_id=%2&type=%3"
                                  "&state=%4&page=%5&page_size=%6")
                       .arg(enc(q))
                       .arg(stationId)
                       .arg(type)
                       .arg(state)
                       .arg(page)
                       .arg(pageSize);
    cli_.get(path, [done = std::move(done)](const HttpJsonClient::Reply& h) {
        QVector<AdminDevice> out;
        qint64 total = 0;
        const QString err = errOf(h);
        if (err.isEmpty()) {
            const QJsonObject o = h.data.toObject();
            total = static_cast<qint64>(o.value("total").toDouble());
            for (const auto& v : o.value("items").toArray())
                out.push_back(adminDeviceFromJson(v.toObject()));
        }
        done(out, total, err);
    });
}

void AdminApi::restartDevice(int id, Cb done) {
    cli_.post(QStringLiteral("/api/admin/devices/%1/restart").arg(id),
              QJsonObject(),
              [done = std::move(done)](const HttpJsonClient::Reply& h) {
                  done(errOf(h));
              });
}

void AdminApi::listOps(int page, int pageSize, OpsCb done) {
    cli_.get(QStringLiteral("/api/admin/logs/ops?page=%1&page_size=%2")
                 .arg(page)
                 .arg(pageSize),
             [done = std::move(done)](const HttpJsonClient::Reply& h) {
                 QVector<OpLogRow> out;
                 qint64 total = 0;
                 const QString err = errOf(h);
                 if (err.isEmpty()) {
                     const QJsonObject o = h.data.toObject();
                     total = static_cast<qint64>(o.value("total").toDouble());
                     for (const auto& v : o.value("items").toArray())
                         out.push_back(opFromJson(v.toObject()));
                 }
                 done(out, total, err);
             });
}

void AdminApi::listAuditLogs(int page, int pageSize, AuditCb done) {
    cli_.get(QStringLiteral("/api/admin/logs/audit?page=%1&page_size=%2")
                 .arg(page)
                 .arg(pageSize),
             [done = std::move(done)](const HttpJsonClient::Reply& h) {
                 QVector<AuditLogRow> out;
                 qint64 total = 0;
                 const QString err = errOf(h);
                 if (err.isEmpty()) {
                     const QJsonObject o = h.data.toObject();
                     total = static_cast<qint64>(o.value("total").toDouble());
                     for (const auto& v : o.value("items").toArray())
                         out.push_back(auditFromJson(v.toObject()));
                 }
                 done(out, total, err);
             });
}

void AdminApi::statsOverview(OverviewCb done) {
    cli_.get("/api/admin/stats/overview",
             [done = std::move(done)](const HttpJsonClient::Reply& h) {
                 done(h.transportOk && h.code == 0 ? h.data.toObject()
                                                   : QJsonObject(),
                      errOf(h));
             });
}

void AdminApi::statsDaily(int days, OverviewCb done) {
    cli_.get(QStringLiteral("/api/admin/stats/daily?days=%1").arg(days),
             [done = std::move(done)](const HttpJsonClient::Reply& h) {
                 QJsonObject wrap;
                 if (h.transportOk && h.code == 0)
                     wrap.insert(QStringLiteral("items"), h.data.toArray());
                 done(wrap, errOf(h));
             });
}

}  // namespace admin
}  // namespace ncs
