#ifndef NCS_BACKEND_CORE_CHARGESERVICE_H
#define NCS_BACKEND_CORE_CHARGESERVICE_H

#include <functional>
#include <mutex>

#include <QString>

#include "entities.h"

namespace ncs {
namespace backend {

class Store;

// 充电主流程。业务方法包在 Store 事务里可回滚；自持互斥锁防并发交错。
// 语义：支持同一用户多桩并发；finish 只生成"待支付"账单(Completed)，
//       pay 才真正扣款(Paid)；存在未支付账单时不允许预约新桩。
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
    bool finish(int orderId, QString* err);   // 生成待支付账单(不扣款)
    bool pay(int orderId, QString* err);      // 扣款并标记已支付
    bool cancel(int orderId, QString* err);   // 仅 Reserved 可取消
    int sweepExpiredReservations(int olderThanSec);

private:
    bool releaseReservedLocked(int orderId);

    Store* store_;
    SendCmdFn sendCmd_;
    GetEnergyFn getEnergy_;
    std::mutex mu_;
};

}  // namespace backend
}  // namespace ncs

#endif  // NCS_BACKEND_CORE_CHARGESERVICE_H
