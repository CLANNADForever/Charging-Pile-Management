// NCS 共享实体：所有端(UI/后端)共用的纯数据结构，不含 UI 与网络逻辑。
// 建模原则：
//   - 只放"需要持久化 / 展示、且不能现场算出来"的字段；
//   - 可推导项(在线率/距离/累计充电次数=按订单聚合)不入实体，避免冗余漂移。
#ifndef NCS_COMMON_ENTITIES_H
#define NCS_COMMON_ENTITIES_H

#include <QDateTime>
#include <QString>
#include <QStringList>

#include "money.h"

namespace ncs {

const char* project_name();

// 用户账号状态(B 端风控)
enum class UserStatus : int {
    Normal = 0,  // 正常
    Frozen = 1,  // 冻结：禁止发起新交易
};

// 用户：11 位手机号为免密登录业务键；金额一律"分"
struct User {
    int id = 0;              // 运营侧数字主键
    QString phone;
    QString nickname;
    MoneyCents balanceCents = 0;
    UserStatus status = UserStatus::Normal;
    QDateTime registeredAt;  // 注册时间
};

// 充电桩运行状态(数值已落库/线上传输，追加请用新值勿改旧值)
enum class DeviceState : int {
    Idle = 0,      // 空闲
    Charging = 1,  // 充电中
    Fault = 2,     // 故障
    Reserved = 3,  // 已被预约占用(占桩)
    Rebooting = 4, // 重启中(远程重启流转，B2)
};

// 充电桩类型(快充/慢充)
enum class DeviceType : int {
    Fast = 0,  // 快充
    Slow = 1,  // 慢充
};

// 充电桩(状态/功率等为运行时快照；电压电流温度走模拟器遥测流，不在此持久化)
struct Device {
    int id = 0;
    int stationId = 0;       // 所属充电站
    DeviceType type = DeviceType::Fast;
    DeviceState state = DeviceState::Idle;
    double powerKw = 0.0;
    double energyKwh = 0.0;  // 本次会话累计电量
};

// 充电站(经纬度/空闲桩数；在线率=按桩现算；距离=按定位现算)
struct Station {
    int id = 0;
    QString name;
    QString address;
    double latitude = 0.0;
    double longitude = 0.0;
    int totalPiles = 0;
    int freePiles = 0;              // 由预约/结算维护
    MoneyCents pricePerKwhCents = 0;  // 充电单价 分/度(元/度 * 100) = 快充档(兼容别名 price_cents)

    // R2 分档单价：pricePerKwhCents 即快充档；慢/超档 0=未配置(计价回退快充档)
    MoneyCents priceSlowCents = 0;
    MoneyCents priceUltraCents = 0;

    // R1 运营属性
    int amenities = 0;        // 9 位 bitmask: 卫生间/休息室/餐饮/雨棚/便利店/自动售货机/饮用水/可洗车/有人值守
    int parking = 0;          // 0 无标注 / 1 停车减免 / 2 收费停车
    int location = 0;         // 0 地上 / 1 地下
    bool isPromo = false;     // 特惠站
    QString openHours;        // 营业时间(text，可空)
    MoneyCents minChargeCents = 0;  // 起充金额(分)，0=不强制
};

// R1：配套设施 bitmask <-> 名称列表(固定 9 项顺序)
QStringList stationAmenityNames(int mask);
int stationAmenityMask(const QStringList& names);

// R2：功率档位(慢 <30kW / 快 30–180 / 超 ≥180)与站内分档单价
enum class PowerTier : int { Slow = 0, Fast = 1, Ultra = 2 };
PowerTier power_tier(double powerKw);
// 取站内某桩功率对应档单价(分/度)；未配置档(<=0)回退快充档 pricePerKwhCents
MoneyCents stationTierPriceCents(const Station& s, double powerKw);

// 充电订单状态
enum class OrderStatus : int {
    Reserved = 0,   // 预约/待开始
    Charging = 1,   // 充电中
    Completed = 2,  // 充电结束、待支付
    Paid = 3,       // 已支付
    Canceled = 4,   // 已取消
};

// 充电订单：单价在开单时快照，费率后续调整不影响历史订单
struct Order {
    int id = 0;
    QString phone;          // 下单用户
    int stationId = 0;
    int deviceId = 0;
    QDateTime startedAt;    // 预约时间(=开单时间)
    QDateTime finishedAt;
    double energyKwh = 0.0;
    MoneyCents unitPriceCents = 0;  // 单价快照 分/度
    MoneyCents amountCents = 0;     // 应收金额(分)
    OrderStatus status = OrderStatus::Reserved;
};

}  // namespace ncs

#endif  // NCS_COMMON_ENTITIES_H
