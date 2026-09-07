#include "OrderSettlePage.h"

#include <QFrame>
#include <QFormLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

#include "common/Utils.h"

OrderSettlePage::OrderSettlePage(const QString &orderNo, QWidget *parent)
    : Page(parent)
    , m_order(ChargeService::instance().orderDetail(orderNo))
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    root->addWidget(makeHeader(QStringLiteral("结算小票")));

    auto *body = new QVBoxLayout;
    body->setContentsMargins(16, 8, 16, 20);
    body->setSpacing(16);

    // 成功标识
    auto *success = new QLabel(QStringLiteral("✓"), this);
    success->setAlignment(Qt::AlignCenter);
    success->setStyleSheet(QStringLiteral("color:#00B368; font-size:48px; font-weight:bold;"));
    body->addWidget(success);

    auto *successText = new QLabel(QStringLiteral("结算成功"), this);
    successText->setObjectName(QStringLiteral("sectionTitle"));
    successText->setAlignment(Qt::AlignCenter);
    body->addWidget(successText);

    // 小票卡片
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
    body->addStretch(1);

    auto *doneBtn = new QPushButton(QStringLiteral("完成"), this);
    doneBtn->setObjectName(QStringLiteral("primaryButton"));
    connect(doneBtn, &QPushButton::clicked, this, &OrderSettlePage::doneRequested);
    body->addWidget(doneBtn);

    root->addLayout(body, 1);
}
