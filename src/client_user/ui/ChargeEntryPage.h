#pragma once

#include <QString>

#include "Page.h"

class QLabel;
class QPushButton;

// 充电入口页(底部「充电」tab):拦截未结算订单 / 引导选站 (UC-U-06)。
class ChargeEntryPage : public Page
{
    Q_OBJECT
public:
    explicit ChargeEntryPage(QWidget *parent = nullptr);

    // 每次切到该 tab 时刷新状态
    void refresh();

signals:
    void settleRequested(const QString &orderNo);
    void goHome();

private:
    QLabel *m_stateLabel = nullptr;
    QLabel *m_hintLabel = nullptr;
    QPushButton *m_actionBtn = nullptr;
    QString m_activeOrderNo;
    bool m_hasUnsettled = false;
};
