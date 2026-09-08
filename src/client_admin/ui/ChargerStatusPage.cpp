#include "ChargerStatusPage.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QProgressBar>
#include <QVBoxLayout>

#include <QtCharts/QChart>
#include <QtCharts/QChartView>
#include <QtCharts/QPieSeries>
#include <QtCharts/QPieSlice>

#include "core/service/StatsService.h"
#include "theme/AdminTheme.h"

namespace {

QChart *buildPieChart(const ChargerStatusOverview &ov)
{
    auto *chart = new QChart;

    auto *series = new QPieSeries;
    series->append(QStringLiteral("空闲"), ov.idleCount)->setColor(AdminTheme::Green);
    series->append(QStringLiteral("使用中"), ov.usingCount)->setColor(AdminTheme::Amber);
    series->append(QStringLiteral("故障"), ov.faultCount)->setColor(AdminTheme::Red);
    series->append(QStringLiteral("预约中"), ov.reservedCount)->setColor(QColor(0x7C, 0x5C, 0xFF));
    series->append(QStringLiteral("重启中"), ov.rebootingCount)->setColor(QColor(0x00, 0xA6, 0xCF));
    series->setHoleSize(0.45); // 环形
    chart->addSeries(series);

    AdminTheme::styleChart(chart);
    return chart;
}

} // namespace

ChargerStatusPage::ChargerStatusPage(QWidget *parent)
    : AdminPage(parent)
{
    auto *lay = new QVBoxLayout(this);
    lay->setContentsMargins(24, 24, 24, 24);
    lay->setSpacing(16);

    auto *title = new QLabel(QStringLiteral("电桩状态总览"), this);
    title->setObjectName(QStringLiteral("pageTitle"));
    lay->addWidget(title);

    // 3 张状态卡
    auto *statRow = new QHBoxLayout;
    statRow->setSpacing(16);
    auto addStat = [](QHBoxLayout *row, const QString &label, QLabel *&value, const QColor &color) {
        auto *card = new QFrame;
        card->setObjectName(QStringLiteral("card"));
        auto *c = new QVBoxLayout(card);
        c->setContentsMargins(20, 18, 20, 18);
        c->setSpacing(6);
        auto *l = new QLabel(label, card);
        l->setObjectName(QStringLiteral("kpiLabel"));
        value = new QLabel(QStringLiteral("0"), card);
        value->setObjectName(QStringLiteral("kpiValue"));
        value->setStyleSheet(QStringLiteral("color: %1;").arg(color.name()));
        c->addWidget(l);
        c->addWidget(value);
        row->addWidget(card, 1);
    };
    addStat(statRow, QStringLiteral("在用"), m_usingValue, AdminTheme::Amber);
    addStat(statRow, QStringLiteral("闲置"), m_idleValue, AdminTheme::Green);
    addStat(statRow, QStringLiteral("故障"), m_faultValue, AdminTheme::Red);
    lay->addLayout(statRow);

    // 饼图 + 健康度
    auto *bottomRow = new QHBoxLayout;
    bottomRow->setSpacing(16);

    auto *pieCard = new QFrame;
    pieCard->setObjectName(QStringLiteral("card"));
    auto *pc = new QVBoxLayout(pieCard);
    pc->setContentsMargins(20, 16, 20, 16);
    auto *pieTitle = new QLabel(QStringLiteral("状态占比"), pieCard);
    pieTitle->setObjectName(QStringLiteral("cardTitle"));
    pc->addWidget(pieTitle);
    m_pieChart = new QChartView(pieCard);
    m_pieChart->setRenderHint(QPainter::Antialiasing);
    m_pieChart->setBackgroundBrush(AdminTheme::Card);
    m_pieChart->setMinimumHeight(260);
    pc->addWidget(m_pieChart);
    bottomRow->addWidget(pieCard, 3);

    auto *healthCard = new QFrame;
    healthCard->setObjectName(QStringLiteral("card"));
    auto *hc = new QVBoxLayout(healthCard);
    hc->setContentsMargins(24, 20, 24, 20);
    hc->setSpacing(10);
    auto *healthTitle = new QLabel(QStringLiteral("设备健康度"), healthCard);
    healthTitle->setObjectName(QStringLiteral("cardTitle"));
    m_healthValue = new QLabel(QStringLiteral("--"), healthCard);
    m_healthValue->setStyleSheet(QStringLiteral("font-size: 44px; font-weight: bold; color: #2DD4A7;"));
    m_healthBar = new QProgressBar(healthCard);
    m_healthBar->setRange(0, 100);
    m_healthBar->setValue(0);
    m_healthBar->setTextVisible(false);
    auto *hint = new QLabel(QStringLiteral("健康度 = (空闲 + 使用中) / 总数"), healthCard);
    hint->setObjectName(QStringLiteral("hintLabel"));
    hc->addWidget(healthTitle);
    hc->addStretch();
    hc->addWidget(m_healthValue);
    hc->addWidget(m_healthBar);
    hc->addWidget(hint);
    bottomRow->addWidget(healthCard, 2);

    lay->addLayout(bottomRow, 1);

    rebuild();
}

void ChargerStatusPage::refresh()
{
    rebuild();
}

void ChargerStatusPage::rebuild()
{
    const ChargerStatusOverview ov = StatsService::instance().chargerStatusOverview();

    auto percent = [&ov](int count) {
        if (ov.total <= 0)
            return QStringLiteral("0.0%");
        return QString::number(double(count) / double(ov.total) * 100.0, 'f', 1) + QStringLiteral("%");
    };
    m_usingValue->setText(QStringLiteral("%1 (%2)").arg(ov.usingCount).arg(percent(ov.usingCount)));
    m_idleValue->setText(QStringLiteral("%1 (%2)").arg(ov.idleCount).arg(percent(ov.idleCount)));
    m_faultValue->setText(QStringLiteral("%1 (%2)").arg(ov.faultCount).arg(percent(ov.faultCount)));

    m_healthValue->setText(QString::number(ov.health, 'f', 1) + QStringLiteral("%"));
    m_healthBar->setValue(int(ov.health));

    auto *old = m_pieChart->chart();
    m_pieChart->setChart(buildPieChart(ov));
    delete old;
}
