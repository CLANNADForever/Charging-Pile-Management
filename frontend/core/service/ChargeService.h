#pragma once

#include <QList>
#include <QString>

// 订单状态:0 预约 1 充电中 2 已完成 3 已取消
struct Order
{
    QString userPhone; // 所属用户手机号
    QString orderNo;
    QString stationName;
    QString chargerNo;
    int     stationId = 0;
    int     chargerId = 0;
    double  power = 0.0;       // kW
    QString startTime;
    QString endTime;
    int     durationSec = 0;
    double  energy = 0.0;       // 度(kWh)
    double  unitPrice = 0.0;    // 元/度
    double  amount = 0.0;       // 总金额(元)
    double  balanceAfter = 0.0; // 扣款后余额(元)
    int     status = 0;
};

// 订单/充电服务。
// 阶段一:桩实现(内存假数据);后续阶段接入数据库与充电流程后替换内部实现。
class ChargeService
{
public:
    static ChargeService &instance();

    // statusFilter: -1 全部;0/1/2/3 对应状态。
    QList<Order> listOrders(int statusFilter = -1) const;

    Order orderDetail(const QString &orderNo) const;

    // 某用户的订单(管理端 UC-A-07 双击查看)。
    QList<Order> listOrdersByPhone(const QString &phone) const;

    // 充电流程(前端演示用桩逻辑)
    bool hasUnsettledOrder() const;
    Order activeOrder() const;
    Order createReservation(int stationId, int chargerId);
    void startCharge(const QString &orderNo);
    void cancelReservation(const QString &orderNo);
    Order settle(const QString &orderNo, double energy, double amount, int durationSec);

private:
    ChargeService();

    QList<Order> m_orders;
};
