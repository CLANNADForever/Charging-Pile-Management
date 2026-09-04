// NCS 共享实体：所有端(UI/后端)共用的纯数据结构，不含 UI 与网络逻辑。
#ifndef NCS_COMMON_ENTITIES_H
#define NCS_COMMON_ENTITIES_H

#include <QString>

namespace ncs {

// 供测试与界面使用的工程标识
const char* project_name();

// 用户：11 位手机号为免密登录主键
struct User {
    QString phone;
    QString nickname;
    double balance = 0.0;
};

// 充电桩运行状态
enum class DeviceState : int {
    Idle = 0,      // 空闲
    Charging = 1,  // 充电中
    Fault = 2,     // 故障
};

// 充电桩(本片起步字段，运营端字段后续 change 再加)
struct Device {
    int id = 0;
    DeviceState state = DeviceState::Idle;
    double powerKw = 0.0;
    double energyKwh = 0.0;
};

}  // namespace ncs

#endif  // NCS_COMMON_ENTITIES_H
