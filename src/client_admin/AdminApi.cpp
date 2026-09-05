#include "AdminApi.h"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonObject>

namespace ncs {
namespace admin {

namespace {

QDateTime iso(QString s) {
    return s.isEmpty() ? QDateTime()
                       : QDateTime::fromString(s, Qt::ISODate).toUTC();
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
}  // namespace

AdminApi::AdminApi(QString baseUrl) : cli_(std::move(baseUrl)) {}

void AdminApi::login(const QString& user, const QString& pass, BoolCb done) {
    cli_.post("/api/admin/login",
              QJsonObject{{"username", user}, {"password", pass}},
              [done = std::move(done)](const HttpJsonClient::Reply& h) {
                  if (!h.transportOk) {
                      done(false, h.error);
                      return;
                  }
                  if (h.code != 0) {
                      done(false, h.message);
                      return;
                  }
                  done(true, QString());
              });
}

void AdminApi::listUsers(const QString& phone, UsersCb done) {
    cli_.get("/api/admin/users?phone=" + phone,
             [done = std::move(done)](const HttpJsonClient::Reply& h) {
                 QVector<User> out;
                 QString err;
                 if (!h.transportOk)
                     err = h.error;
                 else if (h.code != 0)
                     err = h.message;
                 else
                     for (const auto& v : h.data.toArray())
                         out.push_back(userFromJson(v.toObject()));
                 done(out, err);
             });
}

void AdminApi::setFrozen(int userId, bool frozen, Cb done) {
    cli_.post("/api/admin/users/" + QString::number(userId) + "/freeze",
              QJsonObject{{"frozen", frozen}},
              [done = std::move(done)](const HttpJsonClient::Reply& h) {
                  done(h.transportOk && h.code == 0
                           ? QString()
                           : (!h.transportOk ? h.error : h.message));
              });
}

void AdminApi::listStations(StationsCb done) {
    cli_.get("/api/stations",
             [done = std::move(done)](const HttpJsonClient::Reply& h) {
                 QVector<Station> out;
                 QString err;
                 if (!h.transportOk)
                     err = h.error;
                 else if (h.code != 0)
                     err = h.message;
                 else
                     for (const auto& v : h.data.toArray())
                         out.push_back(stationFromJson(v.toObject()));
                 done(out, err);
             });
}

void AdminApi::listDevices(int stationId, DevicesCb done) {
    cli_.get("/api/stations/" + QString::number(stationId) + "/devices",
             [done = std::move(done)](const HttpJsonClient::Reply& h) {
                 QVector<Device> out;
                 QString err;
                 if (!h.transportOk)
                     err = h.error;
                 else if (h.code != 0)
                     err = h.message;
                 else
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
                  QString err;
                  if (!h.transportOk)
                      err = h.error;
                  else if (h.code != 0)
                      err = h.message;
                  else
                      id = h.data.toObject().value("id").toInt(-1);
                  done(id, err);
              });
}

void AdminApi::deleteStation(int id, Cb done) {
    cli_.sendDelete(QStringLiteral("/api/admin/stations/%1").arg(id),
                    [done = std::move(done)](const HttpJsonClient::Reply& h) {
                        done(h.transportOk && h.code == 0
                                 ? QString()
                                 : (!h.transportOk ? h.error : h.message));
                    });
}

void AdminApi::createDevices(int stationId, int count, int type, double powerKw,
                             Cb done) {
    cli_.post("/api/admin/stations/" + QString::number(stationId) + "/devices",
              QJsonObject{{"count", count},
                          {"type", type},
                          {"power_kw", powerKw}},
              [done = std::move(done)](const HttpJsonClient::Reply& h) {
                  done(h.transportOk && h.code == 0
                           ? QString()
                           : (!h.transportOk ? h.error : h.message));
              });
}

void AdminApi::deleteDevice(int id, Cb done) {
    cli_.sendDelete(QStringLiteral("/api/admin/devices/%1").arg(id),
                    [done = std::move(done)](const HttpJsonClient::Reply& h) {
                        done(h.transportOk && h.code == 0
                                 ? QString()
                                 : (!h.transportOk ? h.error : h.message));
                    });
}

}  // namespace admin
}  // namespace ncs
