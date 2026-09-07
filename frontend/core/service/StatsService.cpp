#include "StatsService.h"

#include "StationService.h"

#include <QDate>
#include <cmath>

StatsService &StatsService::instance()
{
    static StatsService s;
    return s;
}

QList<DailyStat> StatsService::dailyStats(int days) const
{
    // 桩:确定性模拟(基于日序号),叠加周末效应 + 周期波动,保证每次运行一致。
    QList<DailyStat> result;
    const QDate today = QDate::currentDate();
    for (int i = days - 1; i >= 0; --i) {
        const QDate d = today.addDays(-i);
        const int seed = d.dayOfYear();

        const bool weekend = d.dayOfWeek() >= 6; // 周六/周日
        const double wave = 1.0 + 0.30 * std::sin(seed * 0.55);
        const double noise = 0.85 + 0.30 * double((seed * 131) % 97) / 97.0;

        DailyStat s;
        s.date = d.toString(QStringLiteral("MM-dd"));
        s.revenue = 3400.0 * wave * noise * (weekend ? 1.28 : 1.0);
        s.orderCount = int(s.revenue / 36.0) + 4;
        result.append(s);
    }
    return result;
}

RevenueSummary StatsService::revenueSummary() const
{
    const QList<DailyStat> stats = dailyStats(30);
    RevenueSummary sum;
    sum.today = stats.isEmpty() ? 0.0 : stats.last().revenue;

    double monthTotal = 0.0;
    for (const DailyStat &s : stats)
        monthTotal += s.revenue;
    sum.month = monthTotal;
    sum.total = monthTotal * 8.6; // 桩:约 8.6 个月累计
    return sum;
}

ChargerStatusOverview StatsService::chargerStatusOverview() const
{
    ChargerStatusOverview ov;
    const auto stations = StationService::instance().listStations();
    for (const Station &st : stations) {
        const auto chargers = StationService::instance().chargersByStation(st.id);
        for (const Charger &c : chargers) {
            ++ov.total;
            if (c.status == 0)
                ++ov.idleCount;
            else if (c.status == 1)
                ++ov.usingCount;
            else if (c.status == 2)
                ++ov.faultCount;
        }
    }
    ov.health = ov.total > 0
        ? double(ov.idleCount + ov.usingCount) / double(ov.total) * 100.0
        : 0.0;
    return ov;
}
