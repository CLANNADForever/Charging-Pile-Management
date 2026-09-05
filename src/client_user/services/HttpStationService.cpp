#include "HttpStationService.h"

#include <QJsonArray>
#include <QJsonObject>

namespace ncs {
namespace client {

namespace {
ncs::Station stationFromJson(const QJsonObject& o) {
    ncs::Station s;
    s.id = o.value(QStringLiteral("id")).toInt();
    s.name = o.value(QStringLiteral("name")).toString();
    s.address = o.value(QStringLiteral("address")).toString();
    s.latitude = o.value(QStringLiteral("latitude")).toDouble();
    s.longitude = o.value(QStringLiteral("longitude")).toDouble();
    s.totalPiles = o.value(QStringLiteral("total_piles")).toInt();
    s.pricePerKwhCents = static_cast<ncs::MoneyCents>(
        o.value(QStringLiteral("price_cents")).toDouble());
    s.freePiles = o.value(QStringLiteral("free_piles")).toInt();
    return s;
}
ncs::Device deviceFromJson(const QJsonObject& o) {
    ncs::Device d;
    d.id = o.value(QStringLiteral("id")).toInt();
    d.stationId = o.value(QStringLiteral("station_id")).toInt();
    d.type = static_cast<ncs::DeviceType>(o.value(QStringLiteral("type")).toInt());
    d.state = static_cast<ncs::DeviceState>(o.value(QStringLiteral("state")).toInt());
    d.powerKw = o.value(QStringLiteral("power_kw")).toDouble();
    d.energyKwh = o.value(QStringLiteral("energy_kwh")).toDouble();
    return d;
}
}  // namespace

HttpStationService::HttpStationService(QString baseUrl)
    : client_(std::move(baseUrl)) {}

void HttpStationService::listStations(StationListCallback done) {
    client_.get(QStringLiteral("/api/stations"),
                [done = std::move(done)](const HttpJsonClient::Reply& h) {
                    QVector<ncs::Station> out;
                    QString err;
                    if (!h.transportOk) {
                        err = h.error;
                    } else if (h.code != 0) {
                        err = h.message;
                    } else {
                        const QJsonArray arr = h.data.toArray();
                        for (const auto& v : arr)
                            out.push_back(stationFromJson(v.toObject()));
                    }
                    done(out, err);
                });
}

void HttpStationService::listDevices(int stationId, DeviceListCallback done) {
    client_.get(QStringLiteral("/api/stations/%1/devices").arg(stationId),
                [done = std::move(done)](const HttpJsonClient::Reply& h) {
                    QVector<ncs::Device> out;
                    QString err;
                    if (!h.transportOk) {
                        err = h.error;
                    } else if (h.code != 0) {
                        err = h.message;
                    } else {
                        const QJsonArray arr = h.data.toArray();
                        for (const auto& v : arr)
                            out.push_back(deviceFromJson(v.toObject()));
                    }
                    done(out, err);
                });
}

}  // namespace client
}  // namespace ncs
