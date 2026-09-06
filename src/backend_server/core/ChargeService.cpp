#include "ChargeService.h"

#include <QDateTime>

#include "billing.h"
#include "database/Store.h"

namespace ncs {
namespace backend {

namespace {
class TxGuard {
public:
    explicit TxGuard(Store* s) : store_(s), ok_(store_ && store_->beginTx()) {}
    ~TxGuard() {
        if (ok_ && !committed_)
            store_->rollbackTx();
    }
    bool ok() const { return ok_; }
    void commit() {
        if (ok_ && !committed_)
            committed_ = store_->commitTx();
    }
private:
    Store* store_;
    bool ok_;
    bool committed_ = false;
};
}  // namespace

bool ChargeService::releaseReservedLocked(int orderId) {
    ncs::Order o;
    if (!store_->getOrderById(orderId, &o))
        return false;
    if (o.status != ncs::OrderStatus::Reserved)
        return false;
    return store_->setDeviceState(
               o.deviceId, static_cast<int>(ncs::DeviceState::Idle)) &&
           store_->adjustStationFree(o.stationId, 1) &&
           store_->updateOrderStatus(
               orderId, static_cast<int>(ncs::OrderStatus::Canceled));
}

bool ChargeService::reserve(const QString& phone, int deviceId, ncs::Order* out,
                            QString* err) {
    std::lock_guard<std::mutex> ck(mu_);
    TxGuard tx(store_);
    if (!tx.ok()) {
        if (err)
            *err = QStringLiteral("事务开启失败");
        return false;
    }
    ncs::User u;
    if (!store_->findUserByPhone(phone, &u)) {
        if (err)
            *err = QStringLiteral("用户未注册，请先登录");
        return false;
    }
    if (u.status == ncs::UserStatus::Frozen) {
        if (err)
            *err = QStringLiteral("账号已冻结");
        return false;
    }
    if (u.balanceCents < 0) {  // 余额不允许为负(兜底历史遗留负数)
        if (err)
            *err = QStringLiteral("余额异常为负，请先充值");
        return false;
    }
    if (store_->countUnpaidByPhone(phone) > 0) {
        if (err)
            *err = QStringLiteral("存在未支付账单，请先在我的充电中支付");
        return false;
    }
    ncs::Device d;
    if (!store_->getDeviceById(deviceId, &d)) {
        if (err)
            *err = QStringLiteral("桩不存在");
        return false;
    }
    if (d.state != ncs::DeviceState::Idle) {
        if (err)
            *err = QStringLiteral("该桩当前不可用(非空闲)");
        return false;
    }
    ncs::Station s;
    if (!store_->getStationById(d.stationId, &s)) {
        if (err)
            *err = QStringLiteral("所属站点不存在");
        return false;
    }
    if (s.minChargeCents > 0 && u.balanceCents < s.minChargeCents) {
        if (err)
            *err = QStringLiteral("余额不足：该站起充需至少 %1 分")
                       .arg(s.minChargeCents);
        return false;
    }
    ncs::Order o;
    o.phone = phone;
    o.stationId = d.stationId;
    o.deviceId = deviceId;
    // R2：按该桩功率对应档的站内单价快照(未配置档回退快充档)
    o.unitPriceCents = ncs::stationTierPriceCents(s, d.powerKw);
    o.status = ncs::OrderStatus::Reserved;
    o.startedAt = QDateTime::currentDateTimeUtc();
    int orderId = 0;
    if (!store_->createOrder(o, &orderId)) {
        if (err)
            *err = QStringLiteral("创建订单失败");
        return false;
    }
    if (!store_->setDeviceState(deviceId,
                                static_cast<int>(ncs::DeviceState::Reserved)) ||
        !store_->adjustStationFree(d.stationId, -1)) {
        if (err)
            *err = QStringLiteral("占桩失败");
        return false;
    }
    tx.commit();
    o.id = orderId;
    if (out)
        *out = o;
    return true;
}

bool ChargeService::start(int orderId, QString* err) {
    std::lock_guard<std::mutex> ck(mu_);
    TxGuard tx(store_);
    if (!tx.ok()) {
        if (err)
            *err = QStringLiteral("事务开启失败");
        return false;
    }
    ncs::Order o;
    if (!store_->getOrderById(orderId, &o)) {
        if (err)
            *err = QStringLiteral("订单不存在");
        return false;
    }
    if (o.status != ncs::OrderStatus::Reserved) {
        if (err)
            *err = QStringLiteral("订单状态非预约，无法开始");
        return false;
    }
    const QDateTime nowUtc = QDateTime::currentDateTimeUtc();
    if (!store_->setDeviceState(o.deviceId,
                                static_cast<int>(ncs::DeviceState::Charging)) ||
        !store_->updateOrderStatus(orderId,
                                   static_cast<int>(ncs::OrderStatus::Charging)) ||
        !store_->setOrderChargeStarted(orderId,
                                       nowUtc.toString(Qt::ISODate))) {
        if (err)
            *err = QStringLiteral("开始充电失败");
        return false;
    }
    tx.commit();
    if (sendCmd_)
        sendCmd_(o.deviceId, true);
    return true;
}

bool ChargeService::finish(int orderId, QString* err) {
    std::lock_guard<std::mutex> ck(mu_);
    TxGuard tx(store_);
    if (!tx.ok()) {
        if (err)
            *err = QStringLiteral("事务开启失败");
        return false;
    }
    ncs::Order o;
    if (!store_->getOrderById(orderId, &o)) {
        if (err)
            *err = QStringLiteral("订单不存在");
        return false;
    }
    if (o.status != ncs::OrderStatus::Charging) {
        if (err)
            *err = QStringLiteral("订单非充电中，无法结束");
        return false;
    }
    double energy = getEnergy_ ? getEnergy_(o.deviceId) : 0.0;
    if (energy < 0.0)
        energy = 0.0;
    const ncs::MoneyCents amount = ncs::charging_amount_cents(energy, o.unitPriceCents);
    // 结束充电：释放桩，生成"待支付"账单(不扣款)
    if (!store_->setDeviceState(o.deviceId,
                                static_cast<int>(ncs::DeviceState::Idle)) ||
        !store_->adjustStationFree(o.stationId, 1) ||
        !store_->updateOrderSettled(orderId, energy, amount)) {
        if (err)
            *err = QStringLiteral("结束充电落库失败");
        return false;
    }
    tx.commit();
    if (sendCmd_)
        sendCmd_(o.deviceId, false);
    if (err)
        err->clear();
    return true;
}

bool ChargeService::pay(int orderId, QString* err) {
    std::lock_guard<std::mutex> ck(mu_);
    TxGuard tx(store_);
    if (!tx.ok()) {
        if (err)
            *err = QStringLiteral("事务开启失败");
        return false;
    }
    ncs::Order o;
    if (!store_->getOrderById(orderId, &o)) {
        if (err)
            *err = QStringLiteral("订单不存在");
        return false;
    }
    if (o.status != ncs::OrderStatus::Completed) {
        if (err)
            *err = QStringLiteral("该订单不是待支付状态");
        return false;
    }
    ncs::User u;
    if (!store_->findUserByPhone(o.phone, &u)) {
        if (err)
            *err = QStringLiteral("用户不存在");
        return false;
    }
    if (o.amountCents > u.balanceCents) {  // 余额不允许为负：不足则拒绝支付
        if (err)
            *err = QStringLiteral("余额不足，请先充值");
        return false;
    }
    const ncs::MoneyCents newBalance = u.balanceCents - o.amountCents;
    if (!store_->setBalanceCents(u.id, newBalance) ||
        !store_->updateOrderPaid(orderId)) {
        if (err)
            *err = QStringLiteral("支付落库失败");
        return false;
    }
    tx.commit();
    return true;
}

bool ChargeService::cancel(int orderId, QString* err) {
    std::lock_guard<std::mutex> ck(mu_);
    TxGuard tx(store_);
    if (!tx.ok()) {
        if (err)
            *err = QStringLiteral("事务开启失败");
        return false;
    }
    if (!releaseReservedLocked(orderId)) {
        if (err)
            *err = QStringLiteral("仅预约中(待开始)的订单可取消");
        return false;
    }
    tx.commit();
    return true;
}

int ChargeService::sweepExpiredReservations(int olderThanSec) {
    std::lock_guard<std::mutex> ck(mu_);
    const auto ids = store_->listExpiredReservedOrderIds(olderThanSec);
    if (ids.isEmpty())
        return 0;
    TxGuard tx(store_);
    if (!tx.ok())
        return 0;
    int n = 0;
    for (const int id : ids) {
        if (releaseReservedLocked(id))
            ++n;
    }
    tx.commit();
    return n;
}

}  // namespace backend
}  // namespace ncs
