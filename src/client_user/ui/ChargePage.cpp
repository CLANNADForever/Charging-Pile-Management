#include "ChargePage.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QProgressBar>
#include <QPushButton>
#include <QStackedWidget>
#include <QTimer>
#include <QVBoxLayout>

#include "common/Toast.h"
#include "common/Utils.h"
#include "core/service/StationService.h"
#include "core/service/UserService.h"

namespace {
constexpr double kMinChargeAmount = 5.0; // 最低起充金额(BR-04)
constexpr int kReserveSeconds = 15 * 60;
constexpr int kTimeScale = 60;           // 1 真实秒 = 60 模拟秒
} // namespace

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

    // 拦截:已有未结算订单则先去结算,不进入选桩
    if (ChargeService::instance().hasUnsettledOrder()) {
        m_orderNo = ChargeService::instance().activeOrder().orderNo;
        QTimer::singleShot(0, this, [this]() {
            QMessageBox box(this);
            box.setWindowTitle(QStringLiteral("提示"));
            box.setText(QStringLiteral("您有未完成的充电订单，请先结算"));
            box.setIcon(QMessageBox::Warning);
            box.setStandardButtons(QMessageBox::NoButton);
            box.addButton(QStringLiteral("去结算"), QMessageBox::AcceptRole);
            box.exec();
            emit settleRequested(m_orderNo);
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
    m_chargerList->setObjectName(QStringLiteral("chargerList"));
    m_chargerList->setFrameShape(QFrame::NoFrame);
    m_chargerList->setSpacing(8);
    const QList<Charger> chargers = StationService::instance().chargersByStation(m_stationId);
    for (const Charger &c : chargers) {
        if (c.status != 0)
            continue;
        auto *item = new QListWidgetItem(m_chargerList);
        item->setText(QStringLiteral("%1    %2 · %3kW")
                          .arg(c.code, c.type)
                          .arg(c.power, 0, 'f', 0));
        item->setData(Qt::UserRole, c.id);
        item->setSizeHint(QSize(0, 52));
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
    m_countdownLabel->setText(QStringLiteral("预约保留 ") + Utils::formatDuration(m_reserveRemain));
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

    m_durationLabel = new QLabel(view);
    m_durationLabel->setStyleSheet(QStringLiteral("font-size:22px; font-weight:bold; color:#1A1D26;"));
    lay->addWidget(m_durationLabel);

    m_powerLabel = new QLabel(view);
    m_powerLabel->setObjectName(QStringLiteral("valueLabel"));
    lay->addWidget(m_powerLabel);

    m_energyLabel = new QLabel(view);
    m_energyLabel->setObjectName(QStringLiteral("valueLabel"));
    lay->addWidget(m_energyLabel);

    m_feeLabel = new QLabel(view);
    m_feeLabel->setStyleSheet(QStringLiteral("color:#2F80FF; font-size:15px; font-weight:bold;"));
    lay->addWidget(m_feeLabel);

    auto *socHint = new QLabel(QStringLiteral("电池荷电状态(模拟 SoC)"), view);
    socHint->setObjectName(QStringLiteral("hintLabel"));
    lay->addWidget(socHint);

    m_socBar = new QProgressBar(view);
    m_socBar->setRange(0, 100);
    m_socBar->setValue(20);
    m_socBar->setTextVisible(true);
    lay->addWidget(m_socBar);

    lay->addStretch(1);

    auto *stopBtn = new QPushButton(QStringLiteral("结束充电"), view);
    stopBtn->setObjectName(QStringLiteral("primaryButton"));
    connect(stopBtn, &QPushButton::clicked, this, &ChargePage::onStopCharge);
    lay->addWidget(stopBtn);

    m_viewStack->addWidget(view);
    m_viewStack->setCurrentWidget(view);
}

void ChargePage::onReserve()
{
    auto *item = m_chargerList->currentItem();
    if (!item) {
        Toast::show(this, QStringLiteral("请先选择电桩"));
        return;
    }
    const double balance = UserService::instance().current().balance;
    if (balance < kMinChargeAmount) {
        Toast::show(this, QStringLiteral("余额不足，请先充值"));
        return;
    }

    const int chargerId = item->data(Qt::UserRole).toInt();
    const Order order = ChargeService::instance().createReservation(m_stationId, chargerId);
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
    // 充电会话统一收敛到“订单详情”数据页(该页会实时刷新)
    emit openOrderDetail(m_orderNo);
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
    m_countdownLabel->setText(QStringLiteral("预约保留 ") + Utils::formatDuration(m_reserveRemain));
}

void ChargePage::onTick()
{
    m_simSeconds += kTimeScale;
    const double energy = m_power * m_simSeconds / 3600.0;
    const double fee = energy * m_unitPrice;

    m_durationLabel->setText(QStringLiteral("时长 ") + Utils::formatDuration(m_simSeconds));
    m_powerLabel->setText(QStringLiteral("实时功率 ") + QString::number(m_power, 'f', 0)
                          + QStringLiteral(" kW"));
    m_energyLabel->setText(QStringLiteral("累计电量 ") + QString::number(energy, 'f', 1)
                           + QStringLiteral(" 度"));
    m_feeLabel->setText(QStringLiteral("当前费用 ¥") + Utils::formatMoney(fee));
    m_socBar->setValue(qMin(100, 20 + int(energy)));
}

void ChargePage::onStopCharge()
{
    if (QMessageBox::question(this, QStringLiteral("结束充电"),
                              QStringLiteral("确定结束本次充电吗？"),
                              QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes)
        return;

    m_chargeTimer->stop();
    const double energy = m_power * m_simSeconds / 3600.0;
    const double amount = energy * m_unitPrice;
    const Order after =
        ChargeService::instance().settle(m_orderNo, energy, amount, m_simSeconds);
    if (after.orderNo.isEmpty()) {
        Toast::show(this, ChargeService::instance().lastError());
        return;
    }
    emit settleRequested(m_orderNo);
}
