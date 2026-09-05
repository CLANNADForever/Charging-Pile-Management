#include "ChargePage.h"

#include <QLabel>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>

#include "services/IChargeService.h"

namespace ncs {
namespace client {

ChargePage::ChargePage(IChargeService* service, QWidget* parent)
    : QWidget(parent), service_(service) {
    setObjectName(QStringLiteral("chargePage"));
    auto* layout = new QVBoxLayout(this);
    auto* heading = new QLabel(QStringLiteral("充电会话"), this);
    heading->setAlignment(Qt::AlignCenter);

    status_ = new QLabel(QStringLiteral("请选择操作"), this);
    status_->setObjectName(QStringLiteral("chargeStatus"));
    status_->setWordWrap(true);
    live_ = new QLabel(QStringLiteral(""), this);
    live_->setObjectName(QStringLiteral("chargeLive"));
    live_->setWordWrap(true);

    btnReserve_ = new QPushButton(QStringLiteral("预约(占桩)"), this);
    btnReserve_->setObjectName(QStringLiteral("btnReserve"));
    btnStart_ = new QPushButton(QStringLiteral("开始充电"), this);
    btnStart_->setObjectName(QStringLiteral("btnStart"));
    btnCancel_ = new QPushButton(QStringLiteral("取消预约"), this);
    btnCancel_->setObjectName(QStringLiteral("btnCancel"));
    btnFinish_ = new QPushButton(QStringLiteral("结束充电并结算"), this);
    btnFinish_->setObjectName(QStringLiteral("btnFinish"));
    btnBack_ = new QPushButton(QStringLiteral("返回"), this);
    btnBack_->setObjectName(QStringLiteral("btnChargeBack"));

    layout->addWidget(heading);
    layout->addStretch();
    layout->addWidget(status_);
    layout->addWidget(live_);
    layout->addStretch();
    layout->addWidget(btnReserve_);
    layout->addWidget(btnStart_);
    layout->addWidget(btnCancel_);
    layout->addWidget(btnFinish_);
    layout->addWidget(btnBack_);

    connect(btnReserve_, &QPushButton::clicked, this, &ChargePage::onReserve);
    connect(btnStart_, &QPushButton::clicked, this, &ChargePage::onStart);
    connect(btnCancel_, &QPushButton::clicked, this, &ChargePage::onCancel);
    connect(btnFinish_, &QPushButton::clicked, this, &ChargePage::onFinish);
    connect(btnBack_, &QPushButton::clicked, this, &ChargePage::onBack);

    timer_ = new QTimer(this);
    timer_->setInterval(1000);
    connect(timer_, &QTimer::timeout, this, &ChargePage::onPoll);
    setPhase(Idle);
}

void ChargePage::startSession(const QString& phone, int deviceId) {
    phone_ = phone;
    deviceId_ = deviceId;
    orderId_ = 0;
    unitPrice_ = 0;
    status_->setText(QStringLiteral("桩 #%1：点“预约”占桩(单价开单时快照)").arg(deviceId_));
    live_->clear();
    setPhase(Idle);
}

void ChargePage::setPhase(Phase p) {
    phase_ = p;
    btnReserve_->setVisible(p == Idle);
    btnStart_->setVisible(p == Reserved);
    btnCancel_->setVisible(p == Reserved);
    btnFinish_->setVisible(p == Charging);
    const bool busy = p == Reserved || p == Charging;
    timer_->setInterval(1000);
    if (busy)
        timer_->start();
    else
        timer_->stop();
}

void ChargePage::onReserve() {
    status_->setText(QStringLiteral("预约中…"));
    service_->reserve(phone_, deviceId_, [this](const OrderResult& r) {
        if (!r.ok) {
            status_->setText(QStringLiteral("预约失败：") + r.message);
            setPhase(Idle);
            return;
        }
        orderId_ = r.order.id;
        unitPrice_ = r.order.unitPriceCents;
        status_->setText(
            QStringLiteral("已预约订单 #%1，单价 %2 元/度。请开始充电，或取消释放。")
                .arg(orderId_)
                .arg(format_cents(unitPrice_)));
        setPhase(Reserved);
    });
}

void ChargePage::onStart() {
    status_->setText(QStringLiteral("开始中…"));
    service_->start(orderId_, [this](const OrderResult& r) {
        if (!r.ok) {
            status_->setText(QStringLiteral("开始失败：") + r.message);
            return;
        }
        status_->setText(QStringLiteral("充电中(每秒刷新电量/金额)…"));
        setPhase(Charging);
        pollNow();
    });
}

void ChargePage::onPoll() {
    if (phase_ != Charging)
        return;
    service_->live(orderId_, [this](const LiveInfo& l) {
        if (!l.ok) {
            live_->setText(QStringLiteral("查询失败：") + l.message);
            return;
        }
        if (l.status != ncs::OrderStatus::Charging) {
            // 后端已把订单状态改走(如超时/异常)则停止轮询
            timer_->stop();
            live_->setText(QStringLiteral("订单状态已变化(status=%1)")
                               .arg(static_cast<int>(l.status)));
            return;
        }
        live_->setText(QStringLiteral("电量 %1 kWh\n金额 %2 元(实时)")
                           .arg(l.energyKwh, 0, 'f', 2)
                           .arg(format_cents(l.amountCents)));
    });
}

void ChargePage::pollNow() {
    onPoll();
}

void ChargePage::onFinish() {
    status_->setText(QStringLiteral("结算中…"));
    service_->finish(orderId_, [this](const OrderResult& r) {
        if (!r.ok) {
            status_->setText(QStringLiteral("结算失败：") + r.message);
            return;
        }
        timer_->stop();
        status_->setText(QStringLiteral("已完成(订单 #%1)").arg(r.order.id));
        live_->setText(QStringLiteral("用电 %1 kWh，应收 %2 元")
                           .arg(r.order.energyKwh, 0, 'f', 2)
                           .arg(format_cents(r.order.amountCents)));
        setPhase(Done);
    });
}

void ChargePage::onCancel() {
    service_->cancel(orderId_, [this](const OrderResult& r) {
        if (!r.ok) {
            status_->setText(QStringLiteral("取消失败：") + r.message);
            return;
        }
        status_->setText(QStringLiteral("已取消，桩已释放"));
        live_->clear();
        setPhase(Idle);
    });
}

void ChargePage::onBack() {
    timer_->stop();
    emit backRequested();
}

}  // namespace client
}  // namespace ncs
