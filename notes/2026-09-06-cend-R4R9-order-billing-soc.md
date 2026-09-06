# 2026-09-06 · C 端后端增量 · 波2（R4+R5 → R6–R9）

> 长命分支 `feature/c-end-v2-backend`；波末全量 ctest + notes + review。

## R4+R5（Order 充电时刻 / 标准电池）
- `Order.charge_started_at`：预约不设、`start` 打点；时长/起止一律按 它→`finished_at`。
- `Order.battery_cap_kwh`(默认60) + `start_soc_pct`(默认20) 快照；`ncs::soc_pct`=clamp(start+energy/cap*100,0..100)。
- orders 表建表带列 + 老库 ALTER 迁移；三处读路径(SQL)补列。

## R6–R7（余额门禁 / 预约截止）
- **推翻早前"预约不查余额"决定**(记录在案)：reserve 需 `balance>=station.min_charge_cents`(不足 code1)。seed 三站 min 置 0(不打断零余额演示充电流)；门禁用新建 min=500 站测试。
- 预约超时默认 15 分钟(main arg)。`POST /api/orders` 返回 `expires_at`=started+timeout。

## R8–R9（live / 详情小票）
- `/live` 增 `power_kw`(sim 实时)、`soc_pct`、`elapsed_sec`(自 charge_started)。
- `GET /api/orders/{id}`：订单全字段+站名+device+功率档(slow/fast/ultra)+charge_started/时长+SoC+当前余额。
- `BackendApp::simPowerKw` 支持实时功率。

## 验收
- 新增：r45 打点+SoC(70/clamp100)；HTTP：低余额预约拒、充值后成功且带 expires_at、live 四字段、detail 字段。ctest 3/3 绿。

## 需用户过目项
- `reserve` 响应 data 顶层多一个 `expires_at`；C 端旧解析不受影响(忽略多余字段)。
- 超时秒由 main 第4参控制且写进 BackendApp 供路由算 expires；若前后端想统一改可配置化。
- 桩类型仍沿用 0快/1慢。
