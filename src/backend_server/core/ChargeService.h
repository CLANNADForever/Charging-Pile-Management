#ifndef NCS_BACKEND_CORE_CHARGESERVICE_H
#define NCS_BACKEND_CORE_CHARGESERVICE_H

#include <functional>
#include <mutex>

#include <QString>

#include "entities.h"

namespace ncs {
namespace backend {

class Store;

// 充电主流程(预约→开始→结算)。自持一把互斥锁把"查→改"整段串行，
// 保证同桩并发预约/结算、余额扣减原子(单进程单库足够)。
class ChargeService {
public:
    using SendCmdFn = std::function<void(int deviceId, bool start)>;
    using GetEnergyFn = std::function<double(int deviceId)>;

    ChargeService(Store* store, SendCmdFn sendCmd, GetEnergyFn getEnergy)
        : store_(store), sendCmd_(std::move(sendCmd)),
          getEnergy_(std::move(getEnergy)) {}

    // 预约占桩：device 必须 Idle；落 Order(Reserved,单价快照)、桩→Reserved、站空闲-1
    bool reserve(const QString& phone, int deviceId, ncs::Order* out,
                 QString* err);

    // 开始充电：Order Reserved→Charging、桩→Charging、通知模拟器 start
    bool start(int orderId, QString* err);

    // 结算：停桩、按能量计费(half-up)、扣余额(允许欠费)、落 Completed、释放桩
    bool finish(int orderId, QString* err);

private:
    Store* store_;
    SendCmdFn sendCmd_;
    GetEnergyFn getEnergy_;
    std::mutex mu_;
};

}  // namespace backend
}  // namespace ncs

#endif  // NCS_BACKEND_CORE_CHARGESERVICE_H
