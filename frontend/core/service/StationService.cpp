#include "StationService.h"

#include <cmath>

#include <QJsonArray>
#include <QJsonObject>

#include "core/net/BackendClient.h"

namespace {
constexpr double kPi = 3.14159265358979323846;

bool feHasAdmin() { return !ncsfe::BackendClient::token().isEmpty(); }
QString feTier(double kw)
{
    if (kw >= 180.0)
        return QStringLiteral("超充");
    if (kw >= 30.0)
        return QStringLiteral("快充");
    return QStringLiteral("慢充");
}
int feChargerStatus(int st)
{
    switch (st) {  // 0 空闲 1 使用 2 故障；后端 3 预约/4 重启按占用(1)
        case 0: return 0;
        case 1: return 1;
        case 2: return 2;
        default: return 1;
    }
}
Charger feCharger(const QJsonObject &o)
{
    Charger c;
    c.id = o.value(QStringLiteral("id")).toInt();
    c.stationId = o.value(QStringLiteral("station_id")).toInt();
    c.code = QStringLiteral("GG-%1").arg(c.id, 2, 10, QLatin1Char('0'));
    c.power = o.value(QStringLiteral("power_kw")).toDouble();
    c.type = feTier(c.power);
    c.status = feChargerStatus(o.value(QStringLiteral("state")).toInt());
    c.totalCount = o.contains(QStringLiteral("sessions"))
                       ? o.value(QStringLiteral("sessions")).toInt()
                       : 0;
    c.totalMinutes = o.contains(QStringLiteral("charging_sec"))
                         ? qRound(o.value(QStringLiteral("charging_sec")).toDouble() / 60.0)
                         : 0;
    return c;
}
Station feStation(const QJsonObject &o)
{
    Station s;
    s.id = o.value(QStringLiteral("id")).toInt();
    s.name = o.value(QStringLiteral("name")).toString();
    s.address = o.value(QStringLiteral("address")).toString();
    s.latitude = o.value(QStringLiteral("latitude")).toDouble();
    s.longitude = o.value(QStringLiteral("longitude")).toDouble();
    s.unitPrice = o.value(QStringLiteral("price_cents")).toDouble() / 100.0;
    s.openHours = o.value(QStringLiteral("open_hours")).toString();
    if (o.contains(QStringLiteral("amenities")) &&
        o.value(QStringLiteral("amenities")).isArray())
        for (const QJsonValue &v : o.value(QStringLiteral("amenities")).toArray())
            s.facilities << v.toString();
    s.hasCoupon = o.value(QStringLiteral("is_promo")).toBool();
    s.parkingFree = o.value(QStringLiteral("parking")).toInt() == 1;
    return s;
}
QJsonArray feItems(const ncsfe::BackendClient::Reply &r)
{
    if (r.data.isArray())
        return r.data.toArray();
    if (r.data.isObject())
        return r.data.toObject().value(QStringLiteral("items")).toArray();
    return QJsonArray();
}
}  // namespace

StationService &StationService::instance()
{
    static StationService s;
    return s;
}

StationService::StationService()
{
    // 桩数据:6 个电站(分布在杭州市区),每站若干电桩(快慢充混合,含故障桩)。
    auto addStation = [this](int id, const QString &name, const QString &addr,
                             double lat, double lon, double price,
                             const QString &hours, const QStringList &fac,
                             bool coupon, bool parkingFree,
                             int fast, int slow, int ultra = 0) {
        Station s;
        s.id = id;
        s.name = name;
        s.address = addr;
        s.latitude = lat;
        s.longitude = lon;
        s.unitPrice = price;
        s.openHours = hours;
        s.facilities = fac;
        s.hasCoupon = coupon;
        s.parkingFree = parkingFree;
        m_stations.append(s);

        int n = 1;
        auto addCharger = [this, id, &n](const QString &code, const QString &type,
                                         double power, int status) {
            Charger c;
            c.id = m_chargers.size() + 1;
            c.stationId = id;
            c.code = code;
            c.type = type;
            c.power = power;
            c.status = status;
            c.totalCount = 40 + (n * 13) % 200;
            c.totalMinutes = 200 + (n * 37) % 4800;
            m_chargers.append(c);
            ++n;
        };

        const QString prefix = name.left(2).isEmpty() ? QStringLiteral("CD") : name.left(2);
        for (int i = 0; i < ultra; ++i)
            addCharger(prefix + QStringLiteral("-%1").arg(n, 2, 10, QLatin1Char('0')),
                       QStringLiteral("超充"), 360.0, (i == 0 && id == 2) ? 2 : 0);
        for (int i = 0; i < fast; ++i)
            addCharger(prefix + QStringLiteral("-%1").arg(n, 2, 10, QLatin1Char('0')),
                       QStringLiteral("快充"), 120.0, (id == 4 && i == 1) ? 2 : 0);
        for (int i = 0; i < slow; ++i)
            addCharger(prefix + QStringLiteral("-%1").arg(n, 2, 10, QLatin1Char('0')),
                       QStringLiteral("慢充"), 7.0, 0);
    };

    addStation(1, QStringLiteral("未来科技城超级充电站"), QStringLiteral("余杭区文一西路 998 号"),
               30.2841, 120.0442, 1.28, QStringLiteral("00:00-24:00"),
               { QStringLiteral("卫生间"), QStringLiteral("休息室"), QStringLiteral("便利店"), QStringLiteral("雨棚") },
               true, true, 6, 2, 2);
    addStation(2, QStringLiteral("滨江公园慢充站"), QStringLiteral("滨江区江南大道 228 号"),
               30.2091, 120.2102, 0.98, QStringLiteral("06:00-22:00"),
               { QStringLiteral("卫生间"), QStringLiteral("饮用水") },
               false, true, 2, 6);
    addStation(3, QStringLiteral("高铁站东广场充电站"), QStringLiteral("江干区东宁路 1 号"),
               30.2912, 120.2101, 1.50, QStringLiteral("00:00-24:00"),
               { QStringLiteral("卫生间"), QStringLiteral("餐饮"), QStringLiteral("休息室") },
               false, false, 8, 0, 2);
    addStation(4, QStringLiteral("软件园一期充电站"), QStringLiteral("西湖区文三路 90 号"),
               30.2741, 120.1201, 1.20, QStringLiteral("07:00-21:00"),
               { QStringLiteral("卫生间"), QStringLiteral("便利店"), QStringLiteral("雨棚") },
               true, false, 5, 2);
    addStation(5, QStringLiteral("西湖文化广场充电站"), QStringLiteral("拱墅区文晖路 419 号"),
               30.2801, 120.1651, 1.35, QStringLiteral("08:00-20:00"),
               { QStringLiteral("卫生间"), QStringLiteral("休息室") },
               false, true, 4, 3);
    addStation(6, QStringLiteral("城北汽车城充电站"), QStringLiteral("拱墅区石祥路 589 号"),
               30.3201, 120.1451, 1.10, QStringLiteral("00:00-24:00"),
               { QStringLiteral("卫生间"), QStringLiteral("餐饮"), QStringLiteral("自动售货机") },
               true, true, 6, 4, 2);
}

QList<Station> StationService::listStations() const
{
    const ncsfe::BackendClient::Reply r = ncsfe::BackendClient::get(
        feHasAdmin() ? QStringLiteral("/api/admin/stations")
                     : QStringLiteral("/api/stations"));
    QList<Station> out;
    if (r.ok) {
        for (const QJsonValue &v : feItems(r))
            out.append(feStation(v.toObject()));
    }
    return out;  // 离线/失败返回空(不兜底假数据)
}

Station StationService::stationDetail(int id) const
{
    const QList<Station> list = listStations();
    for (const Station &s : list) {
        if (s.id == id)
            return s;
    }
    return Station();
}

QList<Charger> StationService::chargersByStation(int stationId) const
{
    const QString path =
        feHasAdmin() ? QStringLiteral("/api/admin/devices?station_id=%1").arg(stationId)
                     : QStringLiteral("/api/stations/%1/devices").arg(stationId);
    const ncsfe::BackendClient::Reply r = ncsfe::BackendClient::get(path);
    if (r.ok) {
        QList<Charger> result;
        for (const QJsonValue &v : feItems(r))
            result.append(feCharger(v.toObject()));
        return result;
    }
    return QList<Charger>();  // 离线/失败返回空
}

Charger StationService::chargerById(int id) const
{
    QList<Charger> pool;
    if (feHasAdmin()) {
        pool = allChargers();
    } else {
        const QList<Station> stations = listStations();
        for (const Station &s : stations)
            pool.append(chargersByStation(s.id));
    }
    for (const Charger &c : pool)
        if (c.id == id)
            return c;
    return Charger();
}

bool StationService::setChargerStatus(int id, int status, QString *err)
{
    if (ncsfe::BackendClient::token().isEmpty()) {
        if (err) *err = QStringLiteral("未登录管理端");
        return false;
    }
    const bool on = (status == 2);  // 2=故障；0=恢复
    const ncsfe::BackendClient::Reply r = ncsfe::BackendClient::post(
        QStringLiteral("/api/admin/devices/%1/fault").arg(id),
        QJsonObject{{QStringLiteral("on"), on}});
    if (!r.ok) {
        if (err) *err = r.message.isEmpty() ? QStringLiteral("操作失败") : r.message;
        return false;
    }
    if (err) err->clear();
    return true;
}

bool StationService::rebootCharger(int id, QString *err)
{
    if (ncsfe::BackendClient::token().isEmpty()) {
        if (err) *err = QStringLiteral("未登录管理端");
        return false;
    }
    const ncsfe::BackendClient::Reply r = ncsfe::BackendClient::post(
        QStringLiteral("/api/admin/devices/%1/restart").arg(id),
        QJsonObject{});
    if (!r.ok) {
        if (err) *err = r.message.isEmpty() ? QStringLiteral("重启失败") : r.message;
        return false;
    }
    if (err) err->clear();
    return true;
}

void StationService::incrementChargerCount(int id)
{
    for (Charger &c : m_chargers) {
        if (c.id == id) {
            ++c.totalCount;
            return;
        }
    }
}

StationService::Location StationService::currentLocation() const
{
    Location loc;
    loc.latitude = 30.2741;
    loc.longitude = 120.1551;
    loc.label = QStringLiteral("杭州市西湖区");
    return loc;
}

double StationService::haversineKm(double lat1, double lon1, double lat2, double lon2)
{
    const double dLat = (lat2 - lat1) * kPi / 180.0;
    const double dLon = (lon2 - lon1) * kPi / 180.0;
    const double a = std::sin(dLat / 2.0) * std::sin(dLat / 2.0)
        + std::cos(lat1 * kPi / 180.0) * std::cos(lat2 * kPi / 180.0)
        * std::sin(dLon / 2.0) * std::sin(dLon / 2.0);
    return 2.0 * 6371.0 * std::asin(std::sqrt(a));
}

QList<Charger> StationService::allChargers() const
{
    if (feHasAdmin()) {
        const ncsfe::BackendClient::Reply r = ncsfe::BackendClient::get(
            QStringLiteral("/api/admin/devices?station_id=-1"));
        if (r.ok) {
            QList<Charger> out;
            for (const QJsonValue &v : feItems(r))
                out.append(feCharger(v.toObject()));
            return out;
        }
    }
    return QList<Charger>();  // 未登录管理端/离线返回空
}

void StationService::addCharger(int stationId, const QString &code,
                                const QString &type, double power)
{
    if (ncsfe::BackendClient::token().isEmpty())
        return;
    const int t = power < 30.0 ? 1 : 0;
    ncsfe::BackendClient::post(
        QStringLiteral("/api/admin/stations/%1/devices").arg(stationId),
        QJsonObject{{QStringLiteral("count"), 1},
                    {QStringLiteral("type"), t},
                    {QStringLiteral("power_kw"), power}});
}

bool StationService::deleteCharger(int chargerId)
{
    if (ncsfe::BackendClient::token().isEmpty())
        return false;
    const ncsfe::BackendClient::Reply r = ncsfe::BackendClient::sendDelete(
        QStringLiteral("/api/admin/devices/%1").arg(chargerId));
    return r.ok;
}

int StationService::addStation(const QString &name, const QString &address,
                               double lat, double lon, double price,
                               int chargerCount, double defaultPower)
{
    if (ncsfe::BackendClient::token().isEmpty())
        return -1;
    const qint64 priceCents = static_cast<qint64>(std::llround(price * 100.0));
    const ncsfe::BackendClient::Reply r = ncsfe::BackendClient::post(
        QStringLiteral("/api/admin/stations"),
        QJsonObject{{QStringLiteral("name"), name},
                    {QStringLiteral("address"), address},
                    {QStringLiteral("latitude"), lat},
                    {QStringLiteral("longitude"), lon},
                    {QStringLiteral("price_cents"), double(priceCents)}});
    if (!r.ok || !r.data.isObject())
        return -1;
    const int sid = r.data.toObject().value(QStringLiteral("id")).toInt(-1);
    if (sid <= 0)
        return -1;
    if (chargerCount > 0) {
        const int t = defaultPower < 30.0 ? 1 : 0;
        ncsfe::BackendClient::post(
            QStringLiteral("/api/admin/stations/%1/devices").arg(sid),
            QJsonObject{{QStringLiteral("count"), chargerCount},
                        {QStringLiteral("type"), t},
                        {QStringLiteral("power_kw"), defaultPower}});
    }
    return sid;
}

bool StationService::updateStation(int id, const QString &name, const QString &address,
                                   double lat, double lon, double price)
{
    if (ncsfe::BackendClient::token().isEmpty())
        return false;
    const qint64 priceCents = static_cast<qint64>(std::llround(price * 100.0));
    const ncsfe::BackendClient::Reply r = ncsfe::BackendClient::patch(
        QStringLiteral("/api/admin/stations/%1").arg(id),
        QJsonObject{{QStringLiteral("name"), name},
                    {QStringLiteral("address"), address},
                    {QStringLiteral("latitude"), lat},
                    {QStringLiteral("longitude"), lon},
                    {QStringLiteral("price_cents"), double(priceCents)}});
    return r.ok;
}

bool StationService::deleteStation(int id)
{
    if (ncsfe::BackendClient::token().isEmpty())
        return false;
    const ncsfe::BackendClient::Reply r = ncsfe::BackendClient::sendDelete(
        QStringLiteral("/api/admin/stations/%1").arg(id));
    return r.ok;
}
