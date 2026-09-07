#pragma once

#include "AdminPage.h"

class QChartView;
class QComboBox;
class QPushButton;
class QTableWidget;

// 智能预测页(UC-A-08)。
class PredictPage : public AdminPage
{
    Q_OBJECT
public:
    explicit PredictPage(QWidget *parent = nullptr);
    void refresh() override;

private:
    void rebuild();
    void onRunPredict();

    QComboBox *m_stationCombo = nullptr;
    QTableWidget *m_table = nullptr;
    QChartView *m_chart = nullptr;
    QPushButton *m_runBtn = nullptr;
    QPushButton *m_btn1 = nullptr;
    QPushButton *m_btn6 = nullptr;
    QPushButton *m_btn24 = nullptr;

    int m_horizon = 1; // 1 / 6 / 24 小时
};
