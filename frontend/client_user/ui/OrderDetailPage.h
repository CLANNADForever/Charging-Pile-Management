#pragma once

#include "Page.h"
#include "core/service/ChargeService.h"

// 订单详情 (UC-U-10 详情)。
class OrderDetailPage : public Page
{
    Q_OBJECT
public:
    explicit OrderDetailPage(const QString &orderNo, QWidget *parent = nullptr);

signals:
    void settleRequested(const QString &orderNo);

private:
    Order m_order;
};
