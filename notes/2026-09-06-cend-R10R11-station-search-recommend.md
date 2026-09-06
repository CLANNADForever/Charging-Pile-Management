# 2026-09-06 · C 端后端增量 · 波3（R10 站富查询 + R11 热门推荐）

> 长命分支 `feature/c-end-v2-backend`；波末 ctest 3/3 + notes + review。至此 R1–R11 全部落地，剩余 R12 属贯穿性测试(已分散在各波 HTTP 用例中)。

## R10 站富查询
- `GET /api/stations`：**无参数仍返回数组(兼容旧 C 端)**；带任一参数返回 `{items,total,page,page_size}`。
- facet：`q`(名/址关键字)、`amenities`(位掩码，需全含)、`power_tier`(slow/fast/ultra，按站内有该档桩)、`parking`、`location`、`is_promo`。
- `sort=distance`(需 lat/lng)、`price`(最低档价升)、`recommend`(近7日已支付单数降)；`lat/lng+radiusKm` 距离过滤，逐站带 `distance_km`。
- 依赖：`ncs::haversine_km`(common)、`Store::paidCount7dByStation`(orders status=3 近7日)。

## R11 热门推荐
- `GET /api/stations/hot?limit=N`：近7日付费单数 TopN(默认 10，与 `sort=recommend` 同源)。

## 验收(HTTP)
- 无参兼容数组；q/amenities/power_tier/parking facet、distance+radius、sort=price 首站为最低档价站、hot Top1=近7日有已支付单的站。ctest 3/3。

## 需用户过目项
- 富查询在"服务端内存过滤+排序"(站量小)；若未来站上千，建议下推 SQL(设施/档位需设备表 join)。
- C 端旧 `/api/stations` 无参解析未变；新地图列表建议前端直接带参数或走 `/hot`。
- 期间一次 ctest 偶发 Failed(直跑 8 连绿、复跑全绿)——疑似机器瞬时抖动，未定位到确定性用例；后续观察。
