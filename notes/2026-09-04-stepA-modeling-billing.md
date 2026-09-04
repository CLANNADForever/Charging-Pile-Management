# 2026-09-04 · 重构(ncs_user_ui 库) + A切片(实体建模 & 四舍五入计费)

## 本次做了什么
- **库化重构**（采纳建议）：`src/client_user/services/+views/` 编成静态库 **ncs_user_ui**；
  `ncs_user` 只留薄 main（装配 + 启动）；`tests` 链接同一个库，不再重复编译 C 端源码。
- **MockUserService 去全局态**：账本由 static 全局改为**实例成员**，每个 service 自持，测试互不串状态；加 `clear()` 便于重置。
- **A：实体建模补齐**（`src/common/entities.h`）：
  - `Station`（站：经纬度/总桩数/空闲桩数）
  - `Order` + `OrderStatus`（Charging/Completed/Canceled，金额 `amountCents` 单位"分"，`energyKwh` 由设备累计）
- **计费规则定为四舍五入(half-up)到分**（`src/common/billing.h/.cpp`）：
  - `yuan_to_cents(yuan)`：元→分
  - `charging_amount_cents(kwh, 单价分/kWh)`：如 0.5kWh×3分=1.5→**2** 分
- 测试扩到 **14 个全过**（新增 billingHalfUp/chargeAmounts/stationDefaults/orderFields）。

## 需要你过目
1. [ ] 计费四舍五入的语义 OK？(half-up：0.5 分进位)
2. [ ] 计费**单价单位是 分/kWh**（如 200 分 = 2 元/度）。若你们单价想按时长(分/分钟)计费，告诉我，加第二种计费方式。
3. [ ] Order 里字段够不够起步？(device/station/phone/起止时间/电量/金额/状态)

## 已记预备（本步未做）
- **下一步预备 = C：后端起步（HTTP + SQLite + 复用 common 实体），届时把 IUserService 换 HttpUserService**。等 A/找桩页推进后再说。
