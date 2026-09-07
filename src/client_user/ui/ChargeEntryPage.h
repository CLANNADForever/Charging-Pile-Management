#pragma once

#include <QString>

#include "Page.h"

class QLabel;
class QPushButton;

// 充电入口页(底部「充电」tab)：按真实后端状态给引导——
// 有待支付账单 → 去支付；有进行中(预约/充电)订单 → 去订单管理；否则引导去首页选站。
class ChargeEntryPage : public Page
{
    Q_OBJECT
public:
    explicit ChargeEntryPage(QWidget *parent = nullptr);

    void refresh();

signals:
    void settleRequested(const QString &orderNo);
    void openOrders();
    void goHome();

private:
    QLabel *m_stateLabel = nullptr;
    QLabel *m_hintLabel = nullptr;
    QPushButton *m_actionBtn = nullptr;
    QString m_orderNo;
};
