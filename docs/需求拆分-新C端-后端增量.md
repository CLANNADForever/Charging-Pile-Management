# 新 C 端（手机地图版）· 后端/实体/模拟器增量需求拆分

> 来源：`docs/待拆分.md`（前端同学提示词，2026-09-06）。本文件**只取背后业务逻辑**，页面/排版要求一律忽略。
> 定位：给后端在独立 git branch 上实施的小需求清单，按依赖排序。
> 日期：2026-09-06

## 已定决策（实施前必须遵守）

1. **电价**：Station 内按功率档分档定价 —— 超充 ≥180kW / 快充 30–180kW / 慢充 <30kW。
2. **推荐**："推荐排序/热门推荐" = 按订单量自动（近 7 日付费单数降序），**不建**预置推荐组。
3. **SoC**：标准电池模型，后端计算（能量仍由模拟器上报，协议不变）。

## 明确不做（仅前端占位，不需后端）

- 优惠券：页面点进入提示"功能未开发"即可。
- 品牌：占位"招商中"。
- 现场照片：前端放占位，可后补。
- 分组推荐预置组：已被"按订单量自动推荐"覆盖。
- C 端 token/登出：现 C 端无会话（登录自报 phone）；"退出登录"如需真语义，另起需求。

## 需求清单

### R1 [实体+数据] Station 运营属性扩充
- 新增字段：`amenities`（9 项配套设施：卫生间/休息室/餐饮/雨棚/便利店/自动售货机/饮用水/可洗车/有人值守，建议整数 bitmask）、`parking`（0 无标注 / 1 停车减免 / 2 收费停车）、`location`（0 地上 / 1 地下）、`is_promo`（特惠站，bool）、`open_hours`（text，可空）、`min_charge_cents`（起充金额，分，默认 0=不强制）。
- 建表 / seed / JSON 序列化（to_json 把 bitmask 转名字数组）。
- 验收：建站接口可存上述字段，站 JSON 含全套。

### R2 [实体] 分档定价
- 功率档 helper `ncs::power_tier(powerKw)`（慢/快/超 三档）。
- `Station` 单价改 `price_slow/fast/ultra_cents` 三列（替代或兼容单 `price_cents`，若替代需给 C 端留别名或同步改其展示，避免回归）。
- 结算取"该桩 power 对应档"的站内单价快照进 `Order.unitPriceCents`（保留既有快照字段）。
- 验收：同站不同档价能出不同账单；power_tier 有单测。

### R3 [数据+模拟器] 功率口径统一
- 现状 `Device.type/power_kw` 与模拟器 even/odd=120/7 错配（7kW 却标快充）。按 R2 三档重排：seed 覆盖三档且功率落在对应带宽；`ncs_simulator` 的 devicePowerKw 改为按三档分布。
- 类型字段去留由实现者定，但**存量 int 不得静默漂移**（纯加档或写迁移说明）。
- 验收：每档至少有一台桩在跑；模拟器上报功率与该桩 DB 档一致。

### R4 [实体] Order 加"充电开始时刻"
- `Order` 新增 `charge_started_at`（nullable；预约时不设，`start` 时写入）。`started_at` 保持=预约时间。
- 时长 / 起止时间一律以 `charge_started_at → finished_at` 计。
- 验收：预约等待不累计时长；live 时长从充电开始算。

### R5 [实体+helper] SoC 标准电池
- `Order` 存快照 `battery_cap_kwh`（默认 60）+ `start_soc_pct`（默认 20，`start` 时定）。
- helper `ncs::soc_pct(order)` = clamp(start + energy/cap×100, 0..100)。
- 验收：可算得 0..100 的 soc。

### R6 [后端] 预约余额门禁 + 超时改 15 分钟
- `reserve` 增加 `balance >= station.min_charge_cents` 校验（不足 code1 提示）；结算仍允许欠费、未付账单禁新桩保留。
- 预约超时默认改 15 分钟（main 默认参数）。
- 注意：这**推翻**了之前"预约不查余额"的决定，写进 notes。
- 验收：低余额预约被拒，HTTP 测试覆盖。

### R7 [后端] 预约响应带截止时间
- `POST /api/orders` 返回 `expires_at`（按超时配置算），供前端倒计时。
- 验收：字段存在且约等于 now+timeout。

### R8 [后端] live 扩字段
- `GET /orders/{id}/live` 增加 `power_kw`（最近心跳实时功率）、`soc_pct`、`elapsed_sec`；保留 energy/amount/status。
- 验收：充电中每秒轮询可拿到四项。

### R9 [后端] 订单详情/小票端点
- `GET /api/orders/{id}`：订单全字段 + `station_name` + `device_id` + 起止时间 + 时长 + energy + 单价/档 + amount + 结算后余额。
- 订单管理 / 详情 / 小票三页共用。
- 验收：能拼出完整一张小票字段。

### R10 [后端] 站列表富查询
- `GET /api/stations` 支持：`lat/lng`（distance 与 radiusKm 过滤、distance 排序）、`sort=distance|price|recommend`（price=最低档单价升序；recommend=近 7 日付费单数降序）、facet：`q`（名称/地址关键字）、`amenities`、`power_tier`、`parking`、`location`、`is_promo`；分页；无参调用兼容返回全部。
- 验收：每个 facet / 排序 / 搜索有 HTTP 用例。

### R11 [后端] 热门推荐
- 空搜索时返回热门前 N（与 R10 的 sort=recommend&limit 同一实现）。
- 验收：造单后热门榜变化有测试。

### R12 [测试] 关键路径 HTTP 覆盖
- 为新端点补 HTTP 自动化：余额不足拒、分档计价、档位/设施筛选、排序、搜索、live 新字段、charge_started 时长、soc 计算。
- 3 套测试全绿；**C 端现有编译与行为不回归**。

## 依赖与推进顺序

- 第一波：R1 → R2 → R3（实体+数据+模拟器口径，地基）；R4/R5（Order）可与 R1 并行。
- 第二波：R6–R9（订单计价 / 余额 / SoC / 详情）。
- 第三波：R10–R11（搜索 / 推荐 / 富查询）。
- 每波后：全量测试 + 写 notes。

## 待定（可留到页面回合，别让 agent 猜）

1. C 端真 token/登出是否做（现在"退出登录"只能清前端本地状态）。
2. 验证码：演示继续固定 123456，还是后端真随机生成并明文返回（仅演示可接受）。
