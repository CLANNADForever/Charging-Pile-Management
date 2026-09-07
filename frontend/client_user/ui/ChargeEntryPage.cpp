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

    m_actionBtn = new QPushButton(card);
    m_actionBtn->setObjectName(QStringLiteral("primaryButton"));
    m_actionBtn->setCursor(Qt::PointingHandCursor);
    connect(m_actionBtn, &QPushButton::clicked, this, [this]() {
        if (m_hasUnsettled)
            emit settleRequested(m_activeOrderNo);
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
    if (ChargeService::instance().hasUnsettledOrder()) {
        m_hasUnsettled = true;
        m_activeOrderNo = ChargeService::instance().activeOrder().orderNo;
        m_stateLabel->setText(QStringLiteral("您有未完成的充电订单"));
        m_hintLabel->setText(QStringLiteral("请先完成结算后再开始新的充电"));
        m_actionBtn->setText(QStringLiteral("去结算"));
    } else {
        m_hasUnsettled = false;
        m_stateLabel->setText(QStringLiteral("开始充电"));
        m_hintLabel->setText(QStringLiteral("请先在首页选择充电站"));
        m_actionBtn->setText(QStringLiteral("去首页"));
    }
}
