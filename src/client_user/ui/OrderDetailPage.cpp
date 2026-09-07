#include "OrderDetailPage.h"

#include <functional>

#include <QFrame>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

#include "common/Toast.h"
#include "common/Utils.h"
#include "core/service/ChargeService.h"

OrderDetailPage::OrderDetailPage(const QString &orderNo, QWidget *parent)
    : Page(parent)
    , m_orderNo(orderNo)
    , m_order(ChargeService::instance().orderDetail(orderNo))
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    root->addWidget(makeHeader(QStringLiteral("订单详情")));

    auto *body = new QVBoxLayout;
    body->setContentsMargins(16, 16, 16, 20);
    body->setSpacing(16);

    m_statusLabel = new QLabel(this);
    m_statusLabel->setAlignment(Qt::AlignCenter);
    m_statusLabel->setStyleSheet(QStringLiteral("font-size:22px; font-weight:bold;"));
    body->addWidget(m_statusLabel);

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
    body->addWidget(card);

    m_actions = new QVBoxLayout;
    body->addLayout(m_actions);

    body->addStretch(1);
    root->addLayout(body, 1);

    refreshOrder();
}

void OrderDetailPage::refreshOrder()
{
    m_order = ChargeService::instance().orderDetail(m_orderNo);
    m_statusLabel->setText(Utils::orderStatusText(m_order.status));
    m_statusLabel->setStyleSheet(
        QStringLiteral("color:%1; font-size:22px; font-weight:bold;")
            .arg(Utils::orderStatusColor(m_order.status).name()));
    rebuildActions();
}

void OrderDetailPage::rebuildActions()
{
    // 清空动作区
    while (m_actions->count() > 0) {
        QLayoutItem *it = m_actions->takeAt(0);
        if (QWidget *w = it->widget())
            w->deleteLater();
        delete it;
    }
    const auto addAction = [this](const QString &text, std::function<void()> fn) {
        auto *b = new QPushButton(text, this);
        b->setObjectName(QStringLiteral("primaryButton"));
        connect(b, &QPushButton::clicked, this, [fn = std::move(fn)] { fn(); });
        m_actions->addWidget(b);
    };

    switch (m_order.status) {
        case 0: {  // 预约
            addAction(QStringLiteral("开始充电"), [this] { onStart(); });
            auto *cancel = new QPushButton(QStringLiteral("取消预约"), this);
            cancel->setObjectName(QStringLiteral("secondaryButton"));
            connect(cancel, &QPushButton::clicked, this, [this] { onCancel(); });
            m_actions->addWidget(cancel);
            break;
        }
        case 1:  // 充电中
            addAction(QStringLiteral("结束充电(生成账单)"), [this] { onSettle(); });
            break;
        case 2:  // 已完成(待支付或已支付)
            if (ChargeService::instance().isPendingPay(m_orderNo))
                addAction(QStringLiteral("立即支付 ¥ %1")
                              .arg(Utils::formatMoney(m_order.amount)),
                          [this] { emit settleRequested(m_orderNo); });
            break;
        default:
            break;  // 已取消等无可操作
    }
}

void OrderDetailPage::onStart()
{
    ChargeService::instance().startCharge(m_orderNo);
    if (!ChargeService::instance().lastError().isEmpty()) {
        Toast::show(this, ChargeService::instance().lastError());
        return;
    }
    Toast::show(this, QStringLiteral("已开始充电"));
    refreshOrder();
}

void OrderDetailPage::onCancel()
{
    ChargeService::instance().cancelReservation(m_orderNo);
    if (!ChargeService::instance().lastError().isEmpty()) {
        Toast::show(this, ChargeService::instance().lastError());
        return;
    }
    Toast::show(this, QStringLiteral("已取消预约"));
    refreshOrder();
}

void OrderDetailPage::onSettle()
{
    const Order after = ChargeService::instance().settle(
        m_orderNo, m_order.energy, double(m_order.amount), m_order.durationSec);
    if (after.orderNo.isEmpty()) {
        Toast::show(this, ChargeService::instance().lastError());
        return;
    }
    emit settleRequested(m_orderNo);  // 去结算页支付
}
