#include "StatsService.h"

#include <QDate>
#include <QJsonArray>
#include <QJsonObject>

#include "core/net/BackendClient.h"

StatsService &StatsService::instance()
{
    static StatsService s;
    return s;
}

QList<DailyStat> StatsService::dailyStats(int days) const
{
    QList<DailyStat> out;
    const ncsfe::BackendClient::Reply r = ncsfe::BackendClient::get(
        QStringLiteral("/api/admin/stats/daily?days=%1").arg(days));
    if (!r.ok || !r.data.isArray())
        return out;
    for (const QJsonValue &v : r.data.toArray()) {
        const QJsonObject o = v.toObject();
        DailyStat s;
        const QString day = o.value(QStringLiteral("day")).toString();  // yyyy-MM-dd
        if (day.length() >= 10)
            s.date = day.mid(5);  // MM-dd
        else
            s.date = day;
        s.revenue = o.value(QStringLiteral("revenue_cents")).toDouble() / 100.0;
        s.orderCount = o.value(QStringLiteral("orders")).toInt();
        out.append(s);
    }
    return out;
}

RevenueSummary StatsService::revenueSummary() const
{
    RevenueSummary sum;
    const ncsfe::BackendClient::Reply r =
        ncsfe::BackendClient::get(QStringLiteral("/api/admin/stats/overview"));
    if (!r.ok || !r.data.isObject())
        return sum;
    const QJsonObject d = r.data.toObject();
    const auto agg = [&](const char *key) {
        return d.value(QLatin1String(key)).toObject()
            .value(QStringLiteral("revenue_cents")).toDouble() / 100.0;
    };
    sum.today = agg("today");
    sum.month = agg("month");
    sum.total = agg("total");
    return sum;
}

ChargerStatusOverview StatsService::chargerStatusOverview() const
{
    ChargerStatusOverview ov;
    const ncsfe::BackendClient::Reply r =
        ncsfe::BackendClient::get(QStringLiteral("/api/admin/stats/overview"));
    if (!r.ok || !r.data.isObject())
        return ov;
    const QJsonObject d = r.data.toObject();
    const QJsonObject h = d.value(QStringLiteral("device_health")).toObject();
    ov.idleCount = h.value(QStringLiteral("idle")).toInt();
    ov.usingCount = h.value(QStringLiteral("charging")).toInt();
    ov.faultCount = h.value(QStringLiteral("fault")).toInt();
    ov.reservedCount = h.value(QStringLiteral("reserved")).toInt();
    ov.rebootingCount = h.value(QStringLiteral("rebooting")).toInt();
    ov.total = d.value(QStringLiteral("devices_total")).toInt();
    if (ov.total > 0)
        ov.health = double(ov.idleCount + ov.usingCount) / double(ov.total) * 100.0;
    return ov;
}
