#include "OrderDetailPage.h"

#include <QFrame>
#include <QFormLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

#include "common/Utils.h"

OrderDetailPage::OrderDetailPage(const QString &orderNo, QWidget *parent)
    : Page(parent)
    , m_order(ChargeService::instance().orderDetail(orderNo))
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    root->addWidget(makeHeader(QStringLiteral("订单详情")));

    auto *body = new QVBoxLayout;
    body->setContentsMargins(16, 16, 16, 16);
    body->setSpacing(16);

    // 状态标识
    auto *status = new QLabel(Utils::orderStatusText(m_order.status), this);
    status->setAlignment(Qt::AlignCenter);
    status->setStyleSheet(QStringLiteral("color:%1; font-size:22px; font-weight:bold;")
                              .arg(Utils::orderStatusColor(m_order.status).name()));
    body->addWidget(status);

    // 详情卡片
    auto *card = new QFrame(this);
    card->setObjectName(QStringLiteral("card"));
    auto *form = new QFormLayout(card);
    form->setContentsMargins(16, 16, 16, 16);
    form->setSpacing(12);

    auto addRow = [form](const QString &k, const QString &v) {
        auto *value = new QLabel(v);
        value->setObjectName(QStringLiteral("valueLabel"));
        value->setWordWrap(true);
        form->addRow(k, value);
    };

    addRow(QStringLiteral("订单号"), m_order.orderNo);
    addRow(QStringLiteral("电站名"), m_order.stationName);
    addRow(QStringLiteral("电桩编号"), m_order.chargerNo);
    addRow(QStringLiteral("开始时间"), m_order.startTime);
    addRow(QStringLiteral("结束时间"), m_order.endTime.isEmpty() ? QStringLiteral("—") : m_order.endTime);
    addRow(QStringLiteral("时长"), Utils::formatDuration(m_order.durationSec));
    addRow(QStringLiteral("电量"), QString::number(m_order.energy, 'f', 1) + QStringLiteral(" 度"));
    addRow(QStringLiteral("单价"), Utils::formatMoney(m_order.unitPrice) + QStringLiteral(" 元/度"));
    addRow(QStringLiteral("总金额"), QStringLiteral("¥ ") + Utils::formatMoney(m_order.amount));
    addRow(QStringLiteral("扣款后余额"), QStringLiteral("¥ ") + Utils::formatMoney(m_order.balanceAfter));

    body->addWidget(card);

    // 未结算订单提供「去结算」
    if (m_order.status == 0 || m_order.status == 1) {
        auto *settleBtn = new QPushButton(QStringLiteral("去结算"), this);
        settleBtn->setObjectName(QStringLiteral("primaryButton"));
        connect(settleBtn, &QPushButton::clicked, this, [this]() {
            emit settleRequested(m_order.orderNo);
        });
        body->addWidget(settleBtn);
    }

    body->addStretch(1);
    root->addLayout(body, 1);
}
