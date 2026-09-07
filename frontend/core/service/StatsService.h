#pragma once

#include <QList>
#include <QString>

// 营收汇总(UC-A-03)
struct RevenueSummary
{
    double today = 0.0; // 今日营收(元)
    double month = 0.0; // 本月营收(元)
    double total = 0.0; // 总营收(元)
};

// 每日营收/订单量(用于趋势图)
struct DailyStat
{
    QString date;          // 展示标签,如 "08-31"
    double revenue = 0.0;  // 当日营收(元)
    int orderCount = 0;    // 当日订单量
};

// 电桩状态总览(UC-A-04)
struct ChargerStatusOverview
{
    int usingCount = 0; // 使用中
    int idleCount = 0;  // 空闲
    int faultCount = 0; // 故障
    int total = 0;
    double health = 0.0; // 健康度 = (空闲+使用中)/总数 ×100%
};

// 统计服务(管理端)。
// 阶段一:桩实现(确定性模拟数据),用于图表开发;
// 阶段二接数据库后替换为对 charging_order 的 SQL 聚合(仅 status=2),接口不变。
class StatsService
{
public:
    static StatsService &instance();

    RevenueSummary revenueSummary() const;

    // 近 N 日每日营收 + 订单量(按日期升序)。
    QList<DailyStat> dailyStats(int days) const;

    ChargerStatusOverview chargerStatusOverview() const;

private:
    StatsService() = default;
};
