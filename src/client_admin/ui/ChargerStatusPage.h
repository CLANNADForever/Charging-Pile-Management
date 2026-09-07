#pragma once

#include "AdminPage.h"

class QLabel;
class QProgressBar;
class QChartView;

// 电桩状态总览页(UC-A-04)。
class ChargerStatusPage : public AdminPage
{
    Q_OBJECT
public:
    explicit ChargerStatusPage(QWidget *parent = nullptr);
    void refresh() override;

private:
    void rebuild();

    QLabel *m_usingValue = nullptr;
    QLabel *m_idleValue = nullptr;
    QLabel *m_faultValue = nullptr;
    QLabel *m_healthValue = nullptr;
    QProgressBar *m_healthBar = nullptr;
    QChartView *m_pieChart = nullptr;
};
