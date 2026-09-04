// NCS 共享实体：所有端(UI/后端)共用的纯数据结构，不含 UI 与网络逻辑。
#ifndef NCS_COMMON_ENTITIES_H
#define NCS_COMMON_ENTITIES_H

#include <QDateTime>
#include <QString>

#include "money.h"

namespace ncs {

// 供测试与界面使用的工程标识
const char* project_name();

// 用户：11 位手机号为免密登录主键；金额一律为"分"
struct User {
    QString phone;
    QString nickname;
    MoneyCents balanceCents = 0;
};

// 充电桩运行状态
enum class DeviceState : int {
    Idle = 0,      // 空闲
    Charging = 1,  // 充电中
    Fault = 2,     // 故障
};

// 充电桩(起步字段，运营端字段后续 change 再加)
struct Device {
    int id = 0;
    DeviceState state = DeviceState::Idle;
    double powerKw = 0.0;
    double energyKwh = 0.0;
};

// 充电站(找桩/导航用字段起步；freePiles 需随预约/结束实时维护)
struct Station {
    int id = 0;
    QString name;
    QString address;
    double latitude = 0.0;
    double longitude = 0.0;
    int totalPiles = 0;
    int freePiles = 0;
};

// 充电订单状态
enum class OrderStatus : int {
    Charging = 0,   // 充电中(未结算)
    Completed = 1,  // 已结算完成
    Canceled = 2,   // 已取消
};

// 充电订单：金额一律 MoneyCents(分)；energyKwh 由设备计量累计
struct Order {
    int id = 0;
    QString phone;         // 下单用户
    int stationId = 0;
    int deviceId = 0;
    QDateTime startedAt;
    QDateTime finishedAt;
    double energyKwh = 0.0;
    MoneyCents amountCents = 0;
    OrderStatus status = OrderStatus::Charging;
};

}  // namespace ncs

#endif  // NCS_COMMON_ENTITIES_H
