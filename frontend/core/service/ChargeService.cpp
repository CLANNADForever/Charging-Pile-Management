#include "ChargeService.h"

#include <QDateTime>

#include "StationService.h"
#include "UserService.h"

ChargeService &ChargeService::instance()
{
    static ChargeService s;
    return s;
}

ChargeService::ChargeService()
{
    // 桩数据:几条不同状态的示例订单,便于界面演示。
    Order o1;
    o1.userPhone = QStringLiteral("13800001111");
    o1.orderNo = QStringLiteral("NO202609060001");
    o1.stationName = QStringLiteral("未来科技城超级充电站");
    o1.chargerNo = QStringLiteral("GG-03");
    o1.startTime = QStringLiteral("2026-09-06 10:00:00");
    o1.endTime = QStringLiteral("2026-09-06 10:42:00");
    o1.durationSec = 42 * 60;
    o1.energy = 21.0;
    o1.unitPrice = 1.28;
    o1.amount = 26.88;
    o1.balanceAfter = 23.12;
    o1.status = 2; // 已完成
    m_orders.append(o1);

    Order o2;
    o2.userPhone = QStringLiteral("13800002222");
    o2.orderNo = QStringLiteral("NO202609060002");
    o2.stationName = QStringLiteral("滨江公园慢充站");
    o2.chargerNo = QStringLiteral("BJ-05");
    o2.startTime = QStringLiteral("2026-09-06 14:20:00");
    o2.endTime = QStringLiteral("2026-09-06 16:05:00");
    o2.durationSec = 105 * 60;
    o2.energy = 12.25;
    o2.unitPrice = 0.98;
    o2.amount = 12.01;
    o2.balanceAfter = 38.00;
    o2.status = 2; // 已完成
    m_orders.append(o2);

    Order o3;
    o3.userPhone = QStringLiteral("13800003333");
    o3.orderNo = QStringLiteral("NO202609050010");
    o3.stationName = QStringLiteral("高铁站东广场充电站");
    o3.chargerNo = QStringLiteral("GT-01");
    o3.startTime = QStringLiteral("2026-09-05 09:10:00");
    o3.endTime.clear();
    o3.unitPrice = 1.50;
    o3.status = 3; // 已取消
    m_orders.append(o3);

    Order o4;
    o4.userPhone = QStringLiteral("13911114444");
    o4.orderNo = QStringLiteral("NO202609040007");
    o4.stationName = QStringLiteral("软件园一期充电站");
    o4.chargerNo = QStringLiteral("RJ-08");
    o4.startTime = QStringLiteral("2026-09-04 18:05:00");
    o4.endTime = QStringLiteral("2026-09-04 18:35:00");
    o4.durationSec = 30 * 60;
    o4.energy = 15.0;
    o4.unitPrice = 1.20;
    o4.amount = 18.00;
    o4.balanceAfter = 32.00;
    o4.status = 2; // 已完成
    m_orders.append(o4);
}

QList<Order> ChargeService::listOrders(int statusFilter) const
{
    if (statusFilter < 0)
        return m_orders;

    QList<Order> result;
    for (const Order &o : m_orders) {
        if (o.status == statusFilter)
            result.append(o);
    }
    return result;
}

Order ChargeService::orderDetail(const QString &orderNo) const
{
    for (const Order &o : m_orders) {
        if (o.orderNo == orderNo)
            return o;
    }
    return Order();
}

QList<Order> ChargeService::listOrdersByPhone(const QString &phone) const
{
    QList<Order> result;
    for (const Order &o : m_orders) {
        if (o.userPhone == phone)
            result.append(o);
    }
    return result;
}

bool ChargeService::hasUnsettledOrder() const
{
    for (const Order &o : m_orders) {
        if (o.status == 0 || o.status == 1)
            return true;
    }
    return false;
}

Order ChargeService::activeOrder() const
{
    for (const Order &o : m_orders) {
        if (o.status == 0 || o.status == 1)
            return o;
    }
    Order empty;
    empty.status = -1;
    return empty;
}

Order ChargeService::createReservation(int stationId, int chargerId)
{
    const Station station = StationService::instance().stationDetail(stationId);
    const Charger charger = StationService::instance().chargerById(chargerId);

    Order o;
    o.userPhone = UserService::instance().current().phone;
    o.orderNo = QStringLiteral("NO")
        + QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMddHHmmss"));
    o.stationName = station.name;
    o.chargerNo = charger.code;
    o.stationId = stationId;
    o.chargerId = chargerId;
    o.power = charger.power;
    o.unitPrice = station.unitPrice;
    o.status = 0; // 预约

    StationService::instance().setChargerStatus(chargerId, 1); // 电桩使用中
    m_orders.prepend(o);
    return o;
}

void ChargeService::startCharge(const QString &orderNo)
{
    for (Order &o : m_orders) {
        if (o.orderNo == orderNo) {
            o.status = 1;
            o.startTime = QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
            return;
        }
    }
}

void ChargeService::cancelReservation(const QString &orderNo)
{
    for (Order &o : m_orders) {
        if (o.orderNo == orderNo) {
            o.status = 3;
            StationService::instance().setChargerStatus(o.chargerId, 0);
            return;
        }
    }
}

Order ChargeService::settle(const QString &orderNo, double energy, double amount, int durationSec)
{
    for (Order &o : m_orders) {
        if (o.orderNo == orderNo) {
            o.status = 2;
            o.endTime = QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
            o.durationSec = durationSec;
            o.energy = energy;
            o.amount = amount;
            o.balanceAfter = qMax(0.0, UserService::instance().current().balance - amount);
            UserService::instance().deduct(amount);
            StationService::instance().setChargerStatus(o.chargerId, 0);
            StationService::instance().incrementChargerCount(o.chargerId);
            return o;
        }
    }
    return Order();
}
