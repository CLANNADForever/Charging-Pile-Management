// 充电订单服务接口(异步)：预约→开始→充电中查询→结算/取消。
#ifndef NCS_CLIENT_SERVICES_ICHARGESERVICE_H
#define NCS_CLIENT_SERVICES_ICHARGESERVICE_H

#include <functional>

#include <QString>

#include "entities.h"
#include "money.h"

namespace ncs {
namespace client {

struct OrderResult {
    bool ok = false;
    QString message;
    ncs::Order order;
};
struct LiveInfo {
    bool ok = false;
    QString message;
    ncs::OrderStatus status = ncs::OrderStatus::Reserved;
    double energyKwh = 0.0;
    MoneyCents amountCents = 0;
};
using OrderCallback = std::function<void(const OrderResult&)>;
using LiveCallback = std::function<void(const LiveInfo&)>;
using OrdersCallback =
    std::function<void(const QVector<ncs::Order>&, const QString& error)>;
struct HistoryResult {
    bool ok = false;
    QString message;
    QVector<ncs::Order> items;
    qint64 total = 0;
};
using HistoryCallback = std::function<void(const HistoryResult&)>;

class IChargeService {
public:
    virtual ~IChargeService() = default;
    virtual void reserve(const QString& phone, int deviceId,
                         OrderCallback done) = 0;
    virtual void start(int orderId, OrderCallback done) = 0;
    virtual void finish(int orderId, OrderCallback done) = 0;
    virtual void pay(int orderId, OrderCallback done) = 0;
    virtual void cancel(int orderId, OrderCallback done) = 0;
    virtual void live(int orderId, LiveCallback done) = 0;
    virtual void listActive(const QString& phone, OrdersCallback done) = 0;
    virtual void listHistory(const QString& phone, int limit, int offset,
                            HistoryCallback done) = 0;
};

}  // namespace client
}  // namespace ncs

#endif  // NCS_CLIENT_SERVICES_ICHARGESERVICE_H
