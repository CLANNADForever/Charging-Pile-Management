# 2026-09-06 · 新 C 端后端增量 R1：Station 运营属性扩充（第一波第一项）

## 来源
`docs/需求拆分-新C端-后端增量.md`（前端同学 2026-09-06 提示词拆出，只取业务）。在独立分支 `feature/cend-R1-station-ops` 上实施，按文档依赖推进；本次只做 R1。

## R1 内容（已交付）
- **Station 新字段**：`amenities`(9 位 bitmask：卫生间/休息室/餐饮/雨棚/便利店/自动售货机/饮用水/可洗车/有人值守)、`parking`(0/1/2)、`location`(0/1)、`is_promo`(bool)、`open_hours`(text 可空)、`min_charge_cents`(分,0=不强制)。
- **共享 helper**（common/entities）`stationAmenityNames(mask)` / `stationAmenityMask(names)`。
- **Store**：建表带新列；老库 `PRAGMA` 探测后逐列 `ALTER` 迁移；seed 三站带演示属性；`create/updateStation` 增 `StationFields` 参数(缺省兼容旧调用)。
- **后端**：`stationToJson` 输出 `amenities_mask`+`amenities`(名字数组)+全套；admin 建站/改站接收新字段（`amenities` 既支持数值 mask 也支持名字数组）。
- **验收测试**：seed JSON 全套、建站持久化往返、名字数组→mask、Store 往返/迁移；ctest 3/3 绿。

## 关键点 / 需用户过目项
- 既有单 `price_cents` 保留（R1 不改价）；**R2 分档定价**会改 `price_slow/fast/ultra_cents`，届时需给 C 端留兼容（文档 R2 已注明）。
- amenities 名字顺序即 bit 位顺序，若前端要展示需按同一数组（已放 common 共享，双端可用）。
- seed 运营属性值仅为演示（望京 mask339/is_promo、中关村 mask107/地下、亦庄 mask405/parking2）。
- C 端 / 管理端 UI 尚未消费新字段（属"页面回合"），仅保证 JSON 全。

## 推进
- 已 4 个小提交：R1a(实体+Store) / R1b(JSON+路由) / R1c(测试) / R1d(文档)。下一步 R2 分档定价（同分支或新分支按文档第一波）。
