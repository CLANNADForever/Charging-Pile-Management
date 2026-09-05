# 2026-09-05 · C 前建模三件事

1. **OrderStatus 预留 Reserved(预约/待开始)=0**：预约逻辑未实现，仅占位；默认 Order 状态改 Reserved。
2. **Order 单价快照 unitPriceCents(分/度)**：开单时快照，费率后续调整不影响历史订单。
3. **按 SRS 更新实体字段**(区分了哪些该是字段/哪些不是)：
   - 加：User{id,status(冻结),registeredAt}；Station{pricePerKwhCents}；Device{type(快/慢),stationId}；Order{unitPriceCents,Reserved}
   - 故意不加(可推导/运行时流)：在线率、距离、累计充电次数/时长(按订单聚合)、电压/电流/温度(遥测流)、营收排行(查询)
