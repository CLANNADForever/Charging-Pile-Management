#include "ChargePage.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QStackedWidget>
#include <QTimer>
#include <QVBoxLayout>

#include "common/Toast.h"
#include "common/Utils.h"
#include "core/service/ChargeService.h"
#include "core/service/StationService.h"
#include "core/service/UserService.h"

namespace {
constexpr int kReserveSeconds = 15 * 60;
}

ChargePage::ChargePage(int stationId, QWidget *parent)
    : Page(parent)
    , m_stationId(stationId)
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);
    root->addWidget(makeHeader(QStringLiteral("选桩充电")));

    m_viewStack = new QStackedWidget(this);
    root->addWidget(m_viewStack, 1);

    m_reserveTimer = new QTimer(this);
    m_reserveTimer->setInterval(1000);
    connect(m_reserveTimer, &QTimer::timeout, this, &ChargePage::onCountdown);

    m_chargeTimer = new QTimer(this);
    m_chargeTimer->setInterval(1000);
    connect(m_chargeTimer, &QTimer::timeout, this, &ChargePage::onTick);

    // 有待支付账单 → 引导去支付(本页不开新预约)
    const QList<Order> orders = ChargeService::instance().listOrders(-1);
    bool pending = false;
    QString pendNo;
    for (const Order &o : orders) {
        if (o.status == 2 && !o.paid) {
            pending = true;
            pendNo = o.orderNo;
            break;
        }
    }
    if (pending) {
        QTimer::singleShot(0, this, [this, pendNo]() {
            QMessageBox box(this);
            box.setWindowTitle(QStringLiteral("提示"));
            box.setText(QStringLiteral("您有待支付的账单，请先支付"));
            box.setIcon(QMessageBox::Warning);
            box.setStandardButtons(QMessageBox::NoButton);
            box.addButton(QStringLiteral("去支付"), QMessageBox::AcceptRole);
            box.exec();
            emit settleRequested(pendNo);
        });
    } else {
        buildSelectView();
    }
}

void ChargePage::buildSelectView()
{
    auto *view = new QWidget(this);
    auto *lay = new QVBoxLayout(view);
    lay->setContentsMargins(16, 12, 16, 12);
    lay->setSpacing(12);

    auto *title = new QLabel(QStringLiteral("选择空闲电桩"), view);
    title->setObjectName(QStringLiteral("sectionTitle"));
    lay->addWidget(title);

    m_chargerList = new QListWidget(view);
    m_chargerList->setFrameShape(QFrame::NoFrame);
    m_chargerList->setSpacing(8);
    const QList<Charger> chargers =
        StationService::instance().chargersByStation(m_stationId);
    for (const Charger &c : chargers) {
        if (c.status != 0)
            continue;
        auto *item = new QListWidgetItem(m_chargerList);
        item->setText(QStringLiteral("%1   %2   %3kW")
                          .arg(c.code, c.type)
                          .arg(c.power, 0, 'f', 0));
        item->setData(Qt::UserRole, c.id);
        item->setSizeHint(QSize(0, 44));
        m_chargerList->addItem(item);
    }
    lay->addWidget(m_chargerList, 1);

    auto *reserveBtn = new QPushButton(QStringLiteral("预约"), view);
    reserveBtn->setObjectName(QStringLiteral("primaryButton"));
    connect(reserveBtn, &QPushButton::clicked, this, &ChargePage::onReserve);
    lay->addWidget(reserveBtn);

    m_viewStack->addWidget(view);
    m_viewStack->setCurrentWidget(view);
}

void ChargePage::buildReservedView(const Order &order)
{
    auto *view = new QWidget(this);
    auto *lay = new QVBoxLayout(view);
    lay->setContentsMargins(16, 24, 16, 20);
    lay->setSpacing(16);

    auto *ok = new QLabel(QStringLiteral("预约成功"), view);
    ok->setStyleSheet(QStringLiteral("color:#00B368; font-size:22px; font-weight:bold;"));
    ok->setAlignment(Qt::AlignCenter);
    lay->addWidget(ok);

    auto *info = new QLabel(
        QStringLiteral("%1\n%2 · %3kW")
            .arg(order.stationName, order.chargerNo)
            .arg(order.power, 0, 'f', 0), view);
    info->setObjectName(QStringLiteral("valueLabel"));
    info->setAlignment(Qt::AlignCenter);
    lay->addWidget(info);

    m_countdownLabel = new QLabel(view);
    m_countdownLabel->setObjectName(QStringLiteral("hintLabel"));
    m_countdownLabel->setAlignment(Qt::AlignCenter);
    lay->addWidget(m_countdownLabel);
    lay->addStretch(1);

    auto *startBtn = new QPushButton(QStringLiteral("开始充电"), view);
    startBtn->setObjectName(QStringLiteral("primaryButton"));
    connect(startBtn, &QPushButton::clicked, this, &ChargePage::onStartCharge);
    lay->addWidget(startBtn);

    m_viewStack->addWidget(view);
    m_viewStack->setCurrentWidget(view);

    m_reserveRemain = kReserveSeconds;
    m_countdownLabel->setText(QStringLiteral("预约保留 ")
                              + Utils::formatDuration(m_reserveRemain));
    m_reserveTimer->start();
}

void ChargePage::buildChargingView(const Order &order)
{
    auto *view = new QWidget(this);
    auto *lay = new QVBoxLayout(view);
    lay->setContentsMargins(20, 16, 20, 20);
    lay->setSpacing(14);

    auto *station = new QLabel(order.stationName, view);
    station->setObjectName(QStringLiteral("sectionTitle"));
    lay->addWidget(station);
    auto *charger = new QLabel(
        QStringLiteral("%1 · %2kW").arg(order.chargerNo).arg(order.power, 0, 'f', 0), view);
    charger->setObjectName(QStringLiteral("hintLabel"));
    lay->addWidget(charger);

    m_liveDuration = new QLabel(view);
    m_liveDuration->setStyleSheet(QStringLiteral("font-size:20px; font-weight:bold; color:#1A1D26;"));
    m_livePower = new QLabel(view);
    m_livePower->setObjectName(QStringLiteral("valueLabel"));
    m_liveEnergy = new QLabel(view);
    m_liveEnergy->setObjectName(QStringLiteral("valueLabel"));
    m_liveFee = new QLabel(view);
    m_liveFee->setStyleSheet(QStringLiteral("color:#2F80FF; font-size:16px; font-weight:bold;"));
    m_liveSoc = new QLabel(view);
    m_liveSoc->setObjectName(QStringLiteral("hintLabel"));
    lay->addWidget(m_liveDuration);
    lay->addWidget(m_livePower);
    lay->addWidget(m_liveEnergy);
    lay->addWidget(m_liveFee);
    lay->addWidget(m_liveSoc);
    lay->addStretch(1);

    auto *stopBtn = new QPushButton(QStringLiteral("结束充电"), view);
    stopBtn->setObjectName(QStringLiteral("primaryButton"));
    connect(stopBtn, &QPushButton::clicked, this, &ChargePage::onStopCharge);
    lay->addWidget(stopBtn);

    m_viewStack->addWidget(view);
    m_viewStack->setCurrentWidget(view);

    onTick();  // 立即取一次后端实时
    m_chargeTimer->start();
}

void ChargePage::onReserve()
{
    auto *item = m_chargerList->currentItem();
    if (!item) {
        Toast::show(this, QStringLiteral("请先选择电桩"));
        return;
    }
    const double balance = UserService::instance().current().balance;
    if (balance < 5.0) {
        Toast::show(this, QStringLiteral("余额不足，请先充值"));
        return;
    }
    const int chargerId = item->data(Qt::UserRole).toInt();
    const Order order =
        ChargeService::instance().createReservation(m_stationId, chargerId);
    if (order.status < 0) {
        Toast::show(this, ChargeService::instance().lastError());
        return;
    }
    m_orderNo = order.orderNo;
    m_power = order.power;
    m_unitPrice = order.unitPrice;
    buildReservedView(order);
}

void ChargePage::onStartCharge()
{
    m_reserveTimer->stop();
    ChargeService::instance().startCharge(m_orderNo);
    if (!ChargeService::instance().lastError().isEmpty()) {
        Toast::show(this, ChargeService::instance().lastError());
        return;
    }
    buildChargingView(ChargeService::instance().orderDetail(m_orderNo));
}

void ChargePage::onCountdown()
{
    --m_reserveRemain;
    if (m_reserveRemain <= 0) {
        m_reserveTimer->stop();
        ChargeService::instance().cancelReservation(m_orderNo);
        Toast::show(this, QStringLiteral("预约已超时取消"));
        emit backRequested();
        return;
    }
    m_countdownLabel->setText(QStringLiteral("预约保留 ")
                              + Utils::formatDuration(m_reserveRemain));
}

void ChargePage::onTick()
{
    if (m_orderNo.isEmpty())
        return;
    const LiveStat ls = ChargeService::instance().live(m_orderNo);
    if (!ls.ok) {
        if (m_liveFee)
            m_liveFee->setText(QStringLiteral("实时获取失败"));
        return;
    }
    if (m_liveDuration)
        m_liveDuration->setText(QStringLiteral("时长 ")
                                + Utils::formatDuration(ls.elapsedSec));
    if (m_livePower)
        m_livePower->setText(QStringLiteral("实时功率 ") + QString::number(ls.power, 'f', 0)
                             + QStringLiteral(" kW"));
    if (m_liveEnergy)
        m_liveEnergy->setText(QStringLiteral("累计电量 ") + QString::number(ls.energy, 'f', 2)
                              + QStringLiteral(" 度"));
    if (m_liveFee)
        m_liveFee->setText(QStringLiteral("当前费用 ¥ ") + Utils::formatMoney(ls.amount));
    if (m_liveSoc)
        m_liveSoc->setText(QStringLiteral("SoC(标准电池估算) ") + QString::number(ls.soc)
                           + QStringLiteral(" %"));
    if (ls.backendStatus == 2) {  // 已结束(别处结算)
        m_chargeTimer->stop();
        if (m_liveFee)
            m_liveFee->setText(QStringLiteral("充电已结束，可去订单中支付"));
    }
}

void ChargePage::onStopCharge()
{
    if (QMessageBox::question(this, QStringLiteral("结束充电"),
                              QStringLiteral("确定结束本次充电吗？"),
                              QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes)
        return;
    m_chargeTimer->stop();
    const Order after = ChargeService::instance().settle(m_orderNo, 0.0, 0.0, 0);
    if (after.orderNo.isEmpty()) {
        Toast::show(this, ChargeService::instance().lastError());
        return;
    }
    emit settleRequested(m_orderNo);
}
