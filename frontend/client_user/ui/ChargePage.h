#pragma once

#include "Page.h"
#include "core/service/ChargeService.h"

class QLabel;
class QListWidget;
class QProgressBar;
class QPushButton;
class QStackedWidget;
class QTimer;

// 充电流程页(同一页内部状态切换):拦截 → 选桩预约 → 充电中 (UC-U-06/07/08)。
class ChargePage : public Page
{
    Q_OBJECT
public:
    explicit ChargePage(int stationId, QWidget *parent = nullptr);

signals:
    void settleRequested(const QString &orderNo);

private slots:
    void onTick();       // 充电计费(每秒)
    void onCountdown();  // 预约倒计时

private:
    void buildSelectView();
    void buildReservedView(const Order &order);
    void buildChargingView(const Order &order);
    void onReserve();
    void onStartCharge();
    void onStopCharge();

    int m_stationId = 0;
    QString m_orderNo;
    double m_power = 0.0;
    double m_unitPrice = 0.0;
    int m_simSeconds = 0;
    int m_reserveRemain = 0;

    QStackedWidget *m_viewStack = nullptr;
    QListWidget *m_chargerList = nullptr;
    QLabel *m_countdownLabel = nullptr;
    QLabel *m_durationLabel = nullptr;
    QLabel *m_energyLabel = nullptr;
    QLabel *m_feeLabel = nullptr;
    QLabel *m_powerLabel = nullptr;
    QProgressBar *m_socBar = nullptr;
    QTimer *m_reserveTimer = nullptr;
    QTimer *m_chargeTimer = nullptr;
};
