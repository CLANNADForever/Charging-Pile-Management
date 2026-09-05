#include "HttpStationService.h"

#include <QJsonArray>
#include <QJsonObject>

namespace ncs {
namespace client {

HttpStationService::HttpStationService(QString baseUrl)
    : client_(std::move(baseUrl)) {}

QVector<ncs::Station> HttpStationService::listStations() {
    QVector<ncs::Station> out;
    const auto r = client_.get(QStringLiteral("/api/stations"));
    if (!r.ok || !r.root.isArray())
        return out;
    const QJsonArray arr = r.root.toArray();
    for (const auto& v : arr) {
        const QJsonObject o = v.toObject();
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
        out.push_back(s);
    }
    return out;
}

QVector<ncs::Device> HttpStationService::listDevices(int stationId) {
    QVector<ncs::Device> out;
    const auto r = client_.get(
        QStringLiteral("/api/stations/%1/devices").arg(stationId));
    if (!r.ok || !r.root.isArray())
        return out;
    const QJsonArray arr = r.root.toArray();
    for (const auto& v : arr) {
        const QJsonObject o = v.toObject();
        ncs::Device d;
        d.id = o.value(QStringLiteral("id")).toInt();
        d.stationId = o.value(QStringLiteral("station_id")).toInt();
        d.type =
            static_cast<ncs::DeviceType>(o.value(QStringLiteral("type")).toInt());
        d.state =
            static_cast<ncs::DeviceState>(o.value(QStringLiteral("state")).toInt());
        d.powerKw = o.value(QStringLiteral("power_kw")).toDouble();
        d.energyKwh = o.value(QStringLiteral("energy_kwh")).toDouble();
        out.push_back(d);
    }
    return out;
}

}  // namespace client
}  // namespace ncs
