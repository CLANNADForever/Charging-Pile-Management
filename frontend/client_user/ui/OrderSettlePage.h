#pragma once

#include "Page.h"
#include "core/service/ChargeService.h"

// 结算小票 (UC-U-09)。
class OrderSettlePage : public Page
{
    Q_OBJECT
public:
    explicit OrderSettlePage(const QString &orderNo, QWidget *parent = nullptr);

signals:
    void doneRequested();

private:
    Order m_order;
};
