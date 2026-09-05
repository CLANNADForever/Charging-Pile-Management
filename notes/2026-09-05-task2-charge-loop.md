# 2026-09-05 · Task2：后端充电最小闭环 + 模拟器真实状态机

## 决策(已与你确认)
- 后端权威状态机；预约/开始两段式；结算允许欠费(余额可为负)。
- 桩 DeviceState 增 Reserved；Order 用现有 Reserved/Charging/Completed。

## 接口(信封 {code,message,data})
```
POST /api/orders          {phone, device_id}     预约占桩 → 建 Order(Reserved,单价快照)、桩 Reserved、空闲-1
POST /api/orders/{id}/start                       开始 → 桩/Order→Charging、通知模拟器 start
POST /api/orders/{id}/finish                      结算 → 按能量×单价(half-up)计费扣余额(可负)、落 Completed、释放桩、空闲+1
```
- 并发：ChargeService 自持一把锁，把查→改整段串行(单进程单库)。
- 模拟器连接先 register(devices)，后端可下发 {"cmd":"start|stop","device_id"}；模拟器收到 start 后按功率逐心跳累计 energy_kwh 上报；结算取后端记录的最新上报能量。

## 文件
- 后端：Store(orders 表 + device/station/order 方法)、core/ChargeService、BackendApp(订单路由 + 模拟器命令/能量映射)
- 模拟器：v2 状态机(注册/听命令/累计电量)

## 测试(3 套全绿)
- ncs_backend 增：ChargeService 直测闭环(reserve→占桩不可重复→start→finish→amount=2.0kWh×200=400→余额-400→释放回2)
- sim：register + 心跳计数仍在。

## 你手动端到端看效果
```bash
build/src/backend_server/ncs_server /tmp/ncs.db 8080 18000 &
build/src/simulator/ncs_simulator 127.0.0.1 18000 2 1 9 &
# 后端 stdout 能看到 [sim] 心跳；再用 C 端或 curl 走 reserve/start/finish，
# 观察后端日志中收到 start/stop、结算金额随电量涨。
```

## 待办/后续候选
- C 端"选桩→开始→充电中实时电量/金额→结束"界面(实时刷新复用异步服务)。
- 预约超时释放、取消订单、冻结并发结算兜底。
