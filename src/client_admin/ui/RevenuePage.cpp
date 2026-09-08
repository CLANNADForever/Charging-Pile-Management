#include "RevenuePage.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPushButton>
#include <QVBoxLayout>

#include <QtCharts/QBarCategoryAxis>
#include <QtCharts/QBarSeries>
#include <QtCharts/QBarSet>
#include <QtCharts/QCategoryAxis>
#include <QtCharts/QChart>
#include <QtCharts/QChartView>
#include <QtCharts/QLineSeries>
#include <QtCharts/QValueAxis>

#include "core/service/StatsService.h"
#include "theme/AdminTheme.h"

namespace {

QChart *buildRevenueChart(const QList<DailyStat> &stats)
{
    auto *chart = new QChart;

    auto *series = new QLineSeries;
    series->setName(QStringLiteral("营收(元)"));
    for (int i = 0; i < stats.size(); ++i)
        series->append(i, stats[i].revenue);
    QPen pen(AdminTheme::Blue);
    pen.setWidth(2);
    series->setPen(pen);
    chart->addSeries(series);

    auto *axisX = new QCategoryAxis;
    axisX->setLabelsPosition(QCategoryAxis::AxisLabelsPositionOnValue);
    for (int i = 0; i < stats.size(); ++i)
        axisX->append(stats[i].date, i);
    axisX->setRange(0, qMax(0, stats.size() - 1));
    axisX->setLabelsAngle(-45);

    double maxRev = 0.0;
    for (const DailyStat &s : stats)
        maxRev = qMax(maxRev, s.revenue);
    auto *axisY = new QValueAxis;
    axisY->setRange(0.0, maxRev * 1.15);
    axisY->setLabelFormat(QStringLiteral("%.0f"));

    chart->addAxis(axisX, Qt::AlignBottom);
    chart->addAxis(axisY, Qt::AlignLeft);
    series->attachAxis(axisX);
    series->attachAxis(axisY);

    AdminTheme::styleChart(chart);
    return chart;
}

QChart *buildOrderChart(const QList<DailyStat> &stats)
{
    auto *chart = new QChart;

    auto *barSet = new QBarSet(QStringLiteral("订单量"));
    barSet->setColor(AdminTheme::Cyan);
    for (const DailyStat &s : stats)
        barSet->append(s.orderCount);

    auto *series = new QBarSeries;
    series->append(barSet);
    chart->addSeries(series);

    QStringList categories;
    for (const DailyStat &s : stats)
        categories << s.date;
    auto *axisX = new QBarCategoryAxis;
    axisX->append(categories);
    axisX->setLabelsAngle(-45);

    int maxOrder = 0;
    for (const DailyStat &s : stats)
        maxOrder = qMax(maxOrder, s.orderCount);
    auto *axisY = new QValueAxis;
    axisY->setRange(0, maxOrder * 1.2);
    axisY->setLabelFormat(QStringLiteral("%.0f"));

    chart->addAxis(axisX, Qt::AlignBottom);
    chart->addAxis(axisY, Qt::AlignLeft);
    series->attachAxis(axisX);
    series->attachAxis(axisY);

    AdminTheme::styleChart(chart);
    return chart;
}

} // namespace

RevenuePage::RevenuePage(QWidget *parent)
    : AdminPage(parent)
{
    auto *lay = new QVBoxLayout(this);
    lay->setContentsMargins(24, 24, 24, 24);
    lay->setSpacing(16);

    auto *title = new QLabel(QStringLiteral("销售业绩分析"), this);
    title->setObjectName(QStringLiteral("pageTitle"));
    lay->addWidget(title);

    // 3 张 KPI 卡
    auto *kpiRow = new QHBoxLayout;
    kpiRow->setSpacing(16);
    auto addKpi = [](QHBoxLayout *row, const QString &label, QLabel *&value) {
        auto *card = new QFrame;
        card->setObjectName(QStringLiteral("card"));
        auto *c = new QVBoxLayout(card);
        c->setContentsMargins(20, 18, 20, 18);
        c->setSpacing(6);
        auto *l = new QLabel(label, card);
        l->setObjectName(QStringLiteral("kpiLabel"));
        value = new QLabel(QStringLiteral("0.00"), card);
        value->setObjectName(QStringLiteral("kpiValue"));
        c->addWidget(l);
        c->addWidget(value);
        row->addWidget(card, 1);
    };
    addKpi(kpiRow, QStringLiteral("今日营收(元)"), m_todayValue);
    addKpi(kpiRow, QStringLiteral("本月营收(元)"), m_monthValue);
    addKpi(kpiRow, QStringLiteral("总营收(元)"), m_totalValue);
    lay->addLayout(kpiRow);

    // 营收趋势卡
    auto *trendCard = new QFrame;
    trendCard->setObjectName(QStringLiteral("card"));
    auto *tc = new QVBoxLayout(trendCard);
    tc->setContentsMargins(20, 16, 20, 16);
    tc->setSpacing(12);

    auto *trendHeader = new QHBoxLayout;
    auto *trendTitle = new QLabel(QStringLiteral("营收趋势"), trendCard);
    trendTitle->setObjectName(QStringLiteral("cardTitle"));
    trendHeader->addWidget(trendTitle);
    trendHeader->addStretch();

    auto *btn7 = new QPushButton(QStringLiteral("近 7 日"), trendCard);
    auto *btn30 = new QPushButton(QStringLiteral("近 30 日"), trendCard);
    for (auto *b : { btn7, btn30 }) {
        b->setObjectName(QStringLiteral("secondaryButton"));
        b->setCheckable(true);
        b->setCursor(Qt::PointingHandCursor);
    }
    btn7->setChecked(true);
    trendHeader->addWidget(btn7);
    trendHeader->addWidget(btn30);

    connect(btn7, &QPushButton::clicked, this, [this, btn7, btn30]() {
        m_rangeDays = 7;
        btn7->setChecked(true);
        btn30->setChecked(false);
        rebuildCharts();
    });
    connect(btn30, &QPushButton::clicked, this, [this, btn7, btn30]() {
        m_rangeDays = 30;
        btn7->setChecked(false);
        btn30->setChecked(true);
        rebuildCharts();
    });

    tc->addLayout(trendHeader);
    m_revenueChart = new QChartView(trendCard);
    m_revenueChart->setRenderHint(QPainter::Antialiasing);
    m_revenueChart->setBackgroundBrush(Qt::NoBrush);
    m_revenueChart->setMinimumHeight(260);
    tc->addWidget(m_revenueChart);

    lay->addWidget(trendCard, 2);

    // 订单量卡
    auto *orderCard = new QFrame;
    orderCard->setObjectName(QStringLiteral("card"));
    auto *oc = new QVBoxLayout(orderCard);
    oc->setContentsMargins(20, 16, 20, 16);
    oc->setSpacing(12);
    auto *orderTitle = new QLabel(QStringLiteral("每日订单量"), orderCard);
    orderTitle->setObjectName(QStringLiteral("cardTitle"));
    oc->addWidget(orderTitle);
    m_orderChart = new QChartView(orderCard);
    m_orderChart->setRenderHint(QPainter::Antialiasing);
    m_orderChart->setBackgroundBrush(Qt::NoBrush);
    m_orderChart->setMinimumHeight(240);
    oc->addWidget(m_orderChart);

    lay->addWidget(orderCard, 2);

    rebuildCharts();
}

void RevenuePage::refresh()
{
    rebuildCharts();
}

void RevenuePage::rebuildCharts()
{
    const auto stats = StatsService::instance().dailyStats(m_rangeDays);
    const RevenueSummary sum = StatsService::instance().revenueSummary();

    m_todayValue->setText(QString::number(sum.today, 'f', 2));
    m_monthValue->setText(QString::number(sum.month, 'f', 2));
    m_totalValue->setText(QString::number(sum.total, 'f', 2));

    auto *oldLine = m_revenueChart->chart();
    m_revenueChart->setChart(buildRevenueChart(stats));
    delete oldLine;

    auto *oldBar = m_orderChart->chart();
    m_orderChart->setChart(buildOrderChart(stats));
    delete oldBar;
}
