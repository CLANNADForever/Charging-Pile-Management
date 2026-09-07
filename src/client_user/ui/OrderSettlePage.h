#pragma once

#include "Page.h"
#include "core/service/ChargeService.h"

class QLabel;
class QPushButton;

// 结算小票(UC-U-09)：finish 生成待支付账单后展示；可"立即支付"(后端 pay, 余额不足拒付)。
class OrderSettlePage : public Page
{
    Q_OBJECT
public:
    explicit OrderSettlePage(const QString &orderNo, QWidget *parent = nullptr);

signals:
    void doneRequested();

private:
    void onPay();

    QString m_orderNo;
    Order m_order;
    bool m_pending = false;
    QLabel *m_balanceAfterLabel = nullptr;
    QLabel *m_payTip = nullptr;
    QPushButton *m_payBtn = nullptr;
    QPushButton *m_doneBtn = nullptr;
};
