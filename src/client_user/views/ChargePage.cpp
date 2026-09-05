#include "ChargePage.h"

#include <QLabel>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>

#include "services/IChargeService.h"

namespace ncs {
namespace client {

namespace {
QString statusText(ncs::OrderStatus st) {
    switch (st) {
        case ncs::OrderStatus::Reserved: return QStringLiteral("预约中");
        case ncs::OrderStatus::Charging: return QStringLiteral("充电中");
        case ncs::OrderStatus::Completed: return QStringLiteral("待支付");
        case ncs::OrderStatus::Paid: return QStringLiteral("已支付");
        case ncs::OrderStatus::Canceled: return QStringLiteral("已取消");
    }
    return QStringLiteral("未知");
}
}  // namespace

ChargePage::ChargePage(IChargeService* service, QWidget* parent)
    : QWidget(parent), service_(service) {
    setObjectName(QStringLiteral("chargePage"));
    auto* layout = new QVBoxLayout(this);
    auto* heading = new QLabel(QStringLiteral("充电会话"), this);
    heading->setAlignment(Qt::AlignCenter);

    status_ = new QLabel(this);
    status_->setObjectName(QStringLiteral("chargeStatus"));
    status_->setWordWrap(true);
    live_ = new QLabel(this);
    live_->setObjectName(QStringLiteral("chargeLive"));
    live_->setWordWrap(true);

    btnReserve_ = new QPushButton(QStringLiteral("预约(占桩)"), this);
    btnStart_ = new QPushButton(QStringLiteral("开始充电"), this);
    btnCancel_ = new QPushButton(QStringLiteral("取消预约"), this);
    btnFinish_ = new QPushButton(QStringLiteral("结束充电(生成账单)"), this);
    btnPay_ = new QPushButton(QStringLiteral("支付"), this);
    btnBack_ = new QPushButton(QStringLiteral("返回"), this);
    btnReserve_->setObjectName(QStringLiteral("btnReserve"));
    btnStart_->setObjectName(QStringLiteral("btnStart"));
    btnCancel_->setObjectName(QStringLiteral("btnCancel"));
    btnFinish_->setObjectName(QStringLiteral("btnFinish"));
    btnPay_->setObjectName(QStringLiteral("btnPay"));
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
    layout->addWidget(btnPay_);
    layout->addWidget(btnBack_);

    connect(btnReserve_, &QPushButton::clicked, this, &ChargePage::onReserve);
    connect(btnStart_, &QPushButton::clicked, this, &ChargePage::onStart);
    connect(btnCancel_, &QPushButton::clicked, this, &ChargePage::onCancel);
    connect(btnFinish_, &QPushButton::clicked, this, &ChargePage::onFinish);
    connect(btnPay_, &QPushButton::clicked, this, &ChargePage::onPay);
    connect(btnBack_, &QPushButton::clicked, this, &ChargePage::onBack);

    timer_ = new QTimer(this);
    timer_->setInterval(1000);
    connect(timer_, &QTimer::timeout, this, &ChargePage::onPoll);
    setPhase(PIdle);
}

void ChargePage::startSession(const QString& phone, int deviceId) {
    phone_ = phone;
    cur_ = ncs::Order{};
    cur_.deviceId = deviceId;
    timer_->stop();
    live_->clear();
    status_->setText(QStringLiteral("桩 #%1：点“预约”占桩；可多桩并发").arg(deviceId));
    setPhase(PIdle);
}

void ChargePage::resumeSession(const QString& phone, const ncs::Order& order) {
    phone_ = phone;
    applyOrder(order);
    live_->clear();
    switch (order.status) {
        case ncs::OrderStatus::Reserved:
            status_->setText(QStringLiteral("订单 #%1：预约中，可开始或取消").arg(order.id));
            setPhase(PReserved);
            break;
        case ncs::OrderStatus::Charging:
            status_->setText(QStringLiteral("订单 #%1：充电中").arg(order.id));
            setPhase(PCharging);
            beginPolling();
            break;
        case ncs::OrderStatus::Completed:
            status_->setText(QStringLiteral("订单 #%1：待支付").arg(order.id));
            live_->setText(QStringLiteral("用电 %1 kWh，应支付 %2 元")
                               .arg(order.energyKwh, 0, 'f', 2)
                               .arg(format_cents(order.amountCents)));
            setPhase(PBill);
            break;
        default:
            status_->setText(QStringLiteral("订单 #%1：%2")
                                 .arg(order.id)
                                 .arg(statusText(order.status)));
            setPhase(PIdle);
            break;
    }
}

void ChargePage::applyOrder(const ncs::Order& o) {
    cur_ = o;
}

void ChargePage::setPhase(Phase p) {
    phase_ = p;
    refreshUi();
    if (p == PCharging)
        beginPolling();
    else
        timer_->stop();
}

void ChargePage::refreshUi() {
    btnReserve_->setVisible(phase_ == PIdle);
    btnStart_->setVisible(phase_ == PReserved);
    btnCancel_->setVisible(phase_ == PReserved);
    btnFinish_->setVisible(phase_ == PCharging);
    btnPay_->setVisible(phase_ == PBill);
    btnPay_->setText(phase_ == PBill
                         ? QStringLiteral("支付 %1 元").arg(format_cents(cur_.amountCents))
                         : QStringLiteral("支付"));
}

void ChargePage::beginPolling() {
    timer_->start();
    onPoll();
}

void ChargePage::onReserve() {
    status_->setText(QStringLiteral("预约中…"));
    service_->reserve(phone_, cur_.deviceId, [this](const OrderResult& r) {
        if (!r.ok) {
            status_->setText(QStringLiteral("预约失败：") + r.message);
            return;
        }
        applyOrder(r.order);
        status_->setText(QStringLiteral("订单 #%1 已预约(单价 %2 元/度)")
                             .arg(r.order.id)
                             .arg(format_cents(r.order.unitPriceCents)));
        setPhase(PReserved);
    });
}

void ChargePage::onStart() {
    status_->setText(QStringLiteral("开始中…"));
    service_->start(cur_.id, [this](const OrderResult& r) {
        if (!r.ok) {
            status_->setText(QStringLiteral("开始失败：") + r.message);
            return;
        }
        applyOrder(r.order);
        status_->setText(QStringLiteral("充电中(每秒刷新)…"));
        setPhase(PCharging);
    });
}

void ChargePage::onPoll() {
    if (phase_ != PCharging || cur_.id == 0)
        return;
    service_->live(cur_.id, [this](const LiveInfo& l) {
        if (!l.ok) {
            live_->setText(QStringLiteral("查询失败：") + l.message);
            return;
        }
        if (l.status == ncs::OrderStatus::Charging) {
            live_->setText(QStringLiteral("电量 %1 kWh\n金额 %2 元(实时)")
                               .arg(l.energyKwh, 0, 'f', 2)
                               .arg(format_cents(l.amountCents)));
        } else if (l.status == ncs::OrderStatus::Completed) {
            timer_->stop();
            cur_.status = ncs::OrderStatus::Completed;
            cur_.energyKwh = l.energyKwh;
            cur_.amountCents = l.amountCents;
            status_->setText(QStringLiteral("已结束，待支付"));
            live_->setText(QStringLiteral("用电 %1 kWh，应支付 %2 元")
                               .arg(cur_.energyKwh, 0, 'f', 2)
                               .arg(format_cents(cur_.amountCents)));
            setPhase(PBill);
        }
    });
}

void ChargePage::onFinish() {
    status_->setText(QStringLiteral("结束中…"));
    service_->finish(cur_.id, [this](const OrderResult& r) {
        if (!r.ok) {
            status_->setText(QStringLiteral("结束失败：") + r.message);
            return;
        }
        applyOrder(r.order);
        status_->setText(QStringLiteral("订单 #%1 已结束，待支付").arg(r.order.id));
        live_->setText(QStringLiteral("用电 %1 kWh，应支付 %2 元")
                           .arg(r.order.energyKwh, 0, 'f', 2)
                           .arg(format_cents(r.order.amountCents)));
        setPhase(PBill);
    });
}

void ChargePage::onPay() {
    status_->setText(QStringLiteral("支付中…"));
    service_->pay(cur_.id, [this](const OrderResult& r) {
        if (!r.ok) {
            status_->setText(QStringLiteral("支付失败：") + r.message);
            return;
        }
        applyOrder(r.order);
        status_->setText(QStringLiteral("订单 #%1 已支付").arg(r.order.id));
        live_->setText(QStringLiteral("用电 %1 kWh，实付 %2 元")
                           .arg(r.order.energyKwh, 0, 'f', 2)
                           .arg(format_cents(r.order.amountCents)));
        setPhase(PPaid);
    });
}

void ChargePage::onCancel() {
    service_->cancel(cur_.id, [this](const OrderResult& r) {
        if (!r.ok) {
            status_->setText(QStringLiteral("取消失败：") + r.message);
            return;
        }
        cur_ = ncs::Order{};
        status_->setText(QStringLiteral("已取消，桩已释放"));
        live_->clear();
        setPhase(PIdle);
    });
}

void ChargePage::onBack() {
    timer_->stop();
    emit backRequested();
}

}  // namespace client
}  // namespace ncs
