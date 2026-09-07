#pragma once

#include <QList>
#include <QString>

// 订单状态(前端口径):0 预约 1 充电中 2 已完成(含待支付/已支付) 3 已取消。
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

// 订单/充电服务(后端权威)。结算=finish(只出待支付账单)；支付=pay 单独(余额不足拒付)。
class ChargeService
{
public:
    static ChargeService &instance();

    // statusFilter: -1 全部;0/1/2/3 对应前端状态。
    QList<Order> listOrders(int statusFilter = -1) const;

    Order orderDetail(const QString &orderNo) const;

    // 某用户的订单(管理端 UC-A-07 双击查看)。
    QList<Order> listOrdersByPhone(const QString &phone) const;

    // 是否有"未结算(待支付)账单"——存在则不能再开新订单。
    bool hasUnsettledOrder() const;
    Order activeOrder() const;

    Order createReservation(int stationId, int chargerId);
    void startCharge(const QString &orderNo);
    void cancelReservation(const QString &orderNo);
    Order settle(const QString &orderNo, double energy, double amount, int durationSec);
    QString pay(const QString &orderNo);  // 立即支付；返回空=成功，否则错误提示
    bool isPendingPay(const QString &orderNo) const;  // 是否为待支付(未付)账单

    QString lastError() const { return m_lastError; }
    void clearLastError() { m_lastError.clear(); }

private:
    ChargeService();

    QList<Order> combinedOrders(const QString &phone) const;
    Order enrich(Order o) const;  // 补电站名/桩号/功率(列表只有 id 时)
    mutable QString m_lastError;
};
