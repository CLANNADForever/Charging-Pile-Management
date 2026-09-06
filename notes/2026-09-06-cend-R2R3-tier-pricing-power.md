# 2026-09-06 · C 端后端增量 · 波1（R2 分档定价 + R3 功率口径统一）

> 工作节奏（用户确认）：固定长命分支 `feature/c-end-v2-backend`；一轮=一个依赖波；波内 R 小步提交；波末全量 ctest + notes + 喊 review。R1 此前已 review 通过并已并入本分支。

## R2 分档定价（50688c3）
- `PowerTier{Slow,Fast,Ultra}` + `ncs::power_tier(powerKw)`（慢<30 / 快 30–180 / 超 ≥180）。
- Station 三档价：`pricePerKwhCents`(=快充档，DB `price_cents`，兼容别名) + `priceSlowCents/priceUltraCents`(DB `price_slow_cents/price_ultra_cents`，0=未配置)。
- Store：建表带列、老库 ALTER 迁移、seed 三站分档价(望京 200/140/280、中关村 180/160/300、亦庄 240/180/320)、建/改站 StationFields 扩展。
- JSON：站输出 `price_cents`(别名)+`price_slow_cents/price_fast_cents/price_ultra_cents`。
- 计价：`reserve` 快照改用 `ncs::stationTierPriceCents(station, device.powerKw)`(未配置档回退快充档)。

## R3 功率口径统一（407668c）
- seed 桩功率/类型按 `id%3`：0→180(超,type0) / 1→120(快,type0) / 2→7(慢,type1)；9 台覆盖三档各 3 台。
- `ncs_simulator::devicePowerKw` 同式，桩 DB 功率与 sim 上报一致。既有 e2e 依赖 device1=快充 200，未受影响。

## 验收
- 新增测试：power_tier 边界、seed 覆盖三档、站分档价持久化、未配置档回退、**同站不同档出不同账单**(fast 200x2=400 vs slow 140x2=280)、seed JSON 三档价。ctest 3/3 绿。

## 需用户过目项
- 快充档用 `price_cents` 兼任，避免 C 端/旧接口回归；若后续想彻底三列独立可再迁移一次。
- 桩 `type`(0 快/1 慢) 语义保留：功率<30→慢，其余→快(含超充展示为"快充")。
- R4/R5(Order 充电时刻+SoC)可与下波继续。
