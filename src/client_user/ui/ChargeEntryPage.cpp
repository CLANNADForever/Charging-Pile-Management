#include "ChargeEntryPage.h"

#include <QFrame>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

#include "core/service/ChargeService.h"

ChargeEntryPage::ChargeEntryPage(QWidget *parent)
    : Page(parent)
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(20, 20, 20, 20);
    root->addStretch(1);

    auto *card = new QFrame(this);
    card->setObjectName(QStringLiteral("card"));
    auto *lay = new QVBoxLayout(card);
    lay->setContentsMargins(24, 32, 24, 32);
    lay->setSpacing(10);

    m_stateLabel = new QLabel(card);
    m_stateLabel->setObjectName(QStringLiteral("sectionTitle"));
    m_stateLabel->setAlignment(Qt::AlignCenter);

    m_hintLabel = new QLabel(card);
    m_hintLabel->setObjectName(QStringLiteral("hintLabel"));
    m_hintLabel->setAlignment(Qt::AlignCenter);
    m_hintLabel->setWordWrap(true);

    m_actionBtn = new QPushButton(card);
    m_actionBtn->setObjectName(QStringLiteral("primaryButton"));
    m_actionBtn->setCursor(Qt::PointingHandCursor);
    connect(m_actionBtn, &QPushButton::clicked, this, [this]() {
        const QString text = m_actionBtn->text();
        if (text.contains(QStringLiteral("去支付")))
            emit settleRequested(m_orderNo);
        else if (text.contains(QStringLiteral("订单")))
            emit openOrders();
        else
            emit goHome();
    });

    lay->addWidget(m_stateLabel);
    lay->addWidget(m_hintLabel);
    lay->addSpacing(8);
    lay->addWidget(m_actionBtn);

    root->addWidget(card);
    root->addStretch(1);

    refresh();
}

void ChargeEntryPage::refresh()
{
    const QList<Order> all = ChargeService::instance().listOrders(-1);
    Order pending;
    bool hasActive = false;
    for (const Order &o : all) {
        if (o.status == 2) {
            if (pending.orderNo.isEmpty())
                pending = o;
        } else if (o.status == 0 || o.status == 1) {
            hasActive = true;
        }
    }
    if (!pending.orderNo.isEmpty() &&
        ChargeService::instance().isPendingPay(pending.orderNo)) {
        m_orderNo = pending.orderNo;
        m_stateLabel->setText(QStringLiteral("您有待支付的账单"));
        m_hintLabel->setText(QStringLiteral("支付后再开新的充电"));
        m_actionBtn->setText(QStringLiteral("去支付"));
    } else if (hasActive) {
        m_stateLabel->setText(QStringLiteral("您有进行中的充电订单"));
        m_hintLabel->setText(QStringLiteral("可继续充电，也可在订单管理中结束/支付"));
        m_actionBtn->setText(QStringLiteral("查看我的订单"));
    } else {
        m_stateLabel->setText(QStringLiteral("开始充电"));
        m_hintLabel->setText(QStringLiteral("请先在首页选择充电站"));
        m_actionBtn->setText(QStringLiteral("去首页"));
    }
}
