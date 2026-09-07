#pragma once

#include "AdminPage.h"

class QLabel;
class QChartView;

// 销售业绩分析页(UC-A-03)。
class RevenuePage : public AdminPage
{
    Q_OBJECT
public:
    explicit RevenuePage(QWidget *parent = nullptr);
    void refresh() override;

private:
    void rebuildCharts();

    QLabel *m_todayValue = nullptr;
    QLabel *m_monthValue = nullptr;
    QLabel *m_totalValue = nullptr;

    QChartView *m_revenueChart = nullptr;
    QChartView *m_orderChart = nullptr;

    int m_rangeDays = 7; // 近 7 日 / 近 30 日
};
