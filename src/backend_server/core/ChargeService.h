#ifndef NCS_BACKEND_CORE_CHARGESERVICE_H
#define NCS_BACKEND_CORE_CHARGESERVICE_H

#include <functional>
#include <mutex>

#include <QString>

#include "entities.h"

namespace ncs {
namespace backend {

class Store;

// 充电主流程(预约/开始/结算/取消/超时清扫)。每个业务方法整体包在 Store 事务里，
// 任一步失败回滚；ChargeService 自持互斥锁避免并发业务交错。
class ChargeService {
public:
    using SendCmdFn = std::function<void(int deviceId, bool start)>;
    using GetEnergyFn = std::function<double(int deviceId)>;

    ChargeService(Store* store, SendCmdFn sendCmd, GetEnergyFn getEnergy)
        : store_(store), sendCmd_(std::move(sendCmd)),
          getEnergy_(std::move(getEnergy)) {}

    bool reserve(const QString& phone, int deviceId, ncs::Order* out,
                 QString* err);
    bool start(int orderId, QString* err);
    bool finish(int orderId, QString* err);
    bool cancel(int orderId, QString* err);  // 仅 Reserved 可取消(释放桩)
    int sweepExpiredReservations(int olderThanSec);  // 释放超时未开始的预约

private:
    bool releaseReservedLocked(int orderId);  // 调用方已持锁+事务

    Store* store_;
    SendCmdFn sendCmd_;
    GetEnergyFn getEnergy_;
    std::mutex mu_;
};

}  // namespace backend
}  // namespace ncs

#endif  // NCS_BACKEND_CORE_CHARGESERVICE_H
