#pragma once

#include "Page.h"
#include "core/service/ChargeService.h"

class QLabel;
class QPushButton;
class QTimer;
class QVBoxLayout;

// 订单详情(按真实后端状态给动作：预约→开始/取消；充电中→结束(出账单)；待支付→去支付)。
class OrderDetailPage : public Page
{
    Q_OBJECT
public:
    explicit OrderDetailPage(const QString &orderNo, QWidget *parent = nullptr);

signals:
    void settleRequested(const QString &orderNo);

private:
    void refreshOrder();       // 重拉订单并刷新动作区/状态
    void rebuildActions();

    void onStart();
    void onCancel();
    void onSettle();
    void onPay();
    void onLiveTick();

    QString m_orderNo;
    Order m_order;
    QLabel *m_statusLabel = nullptr;
    QLabel *m_liveLabel = nullptr;
    QTimer *m_timer = nullptr;
    QVBoxLayout *m_actions = nullptr;
};
