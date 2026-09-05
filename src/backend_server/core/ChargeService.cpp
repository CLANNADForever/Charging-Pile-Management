#include "ChargeService.h"

#include <QDateTime>

#include "billing.h"
#include "database/Store.h"

namespace ncs {
namespace backend {

bool ChargeService::reserve(const QString& phone, int deviceId,
                            ncs::Order* out, QString* err) {
    std::lock_guard<std::mutex> lk(mu_);
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

    ncs::Order o;
    o.phone = phone;
    o.stationId = d.stationId;
    o.deviceId = deviceId;
    o.unitPriceCents = s.pricePerKwhCents;  // 单价快照
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
    o.id = orderId;
    if (out)
        *out = o;
    return true;
}

bool ChargeService::start(int orderId, QString* err) {
    std::lock_guard<std::mutex> lk(mu_);
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
    if (!store_->setDeviceState(o.deviceId,
                                static_cast<int>(ncs::DeviceState::Charging)) ||
        !store_->updateOrderStatus(orderId,
                                   static_cast<int>(ncs::OrderStatus::Charging))) {
        if (err)
            *err = QStringLiteral("开始充电失败");
        return false;
    }
    if (sendCmd_)
        sendCmd_(o.deviceId, true);
    return true;
}

bool ChargeService::finish(int orderId, QString* err) {
    std::lock_guard<std::mutex> lk(mu_);
    ncs::Order o;
    if (!store_->getOrderById(orderId, &o)) {
        if (err)
            *err = QStringLiteral("订单不存在");
        return false;
    }
    if (o.status != ncs::OrderStatus::Charging) {
        if (err)
            *err = QStringLiteral("订单非充电中，无法结算");
        return false;
    }

    double energy = getEnergy_ ? getEnergy_(o.deviceId) : 0.0;
    if (energy < 0.0)
        energy = 0.0;
    const ncs::MoneyCents amount =
        ncs::charging_amount_cents(energy, o.unitPriceCents);

    ncs::User u;
    if (!store_->findUserByPhone(o.phone, &u)) {
        if (err)
            *err = QStringLiteral("用户不存在");
        return false;
    }
    const ncs::MoneyCents newBalance = u.balanceCents - amount;  // 允许欠费
    if (!store_->setBalanceCents(u.id, newBalance) ||
        !store_->setDeviceState(o.deviceId,
                                static_cast<int>(ncs::DeviceState::Idle)) ||
        !store_->adjustStationFree(o.stationId, 1) ||
        !store_->updateOrderSettled(orderId, energy, amount)) {
        if (err)
            *err = QStringLiteral("结算落库失败");
        return false;
    }
    if (sendCmd_)
        sendCmd_(o.deviceId, false);

    o.status = ncs::OrderStatus::Completed;
    o.energyKwh = energy;
    o.amountCents = amount;
    if (err)
        err->clear();
    return true;
}

}  // namespace backend
}  // namespace ncs
