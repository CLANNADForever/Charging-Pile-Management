#pragma once

#include "Page.h"
#include "core/service/ChargeService.h"

class QLabel;
class QListWidget;
class QPushButton;
class QStackedWidget;
class QTimer;

// 充电会话页(数据页,无假电池条)：预约→开始后每秒走后端 live 刷新电量/金额/时长/SoC；结束→结算页支付。
class ChargePage : public Page
{
    Q_OBJECT
public:
    explicit ChargePage(int stationId, QWidget *parent = nullptr);

signals:
    void settleRequested(const QString &orderNo);

private slots:
    void onTick();       // 每秒走后端 live
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
    int m_reserveRemain = 0;

    QStackedWidget *m_viewStack = nullptr;
    QListWidget *m_chargerList = nullptr;
    QTimer *m_reserveTimer = nullptr;
    QTimer *m_chargeTimer = nullptr;

    QLabel *m_countdownLabel = nullptr;
    QLabel *m_liveDuration = nullptr;
    QLabel *m_livePower = nullptr;
    QLabel *m_liveEnergy = nullptr;
    QLabel *m_liveFee = nullptr;
    QLabel *m_liveSoc = nullptr;
};
