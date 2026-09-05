# 2026-09-05 · 预约取消/超时 + C 端充电交互页 + e2e 闭环

## 预约取消与超时
- `POST /api/orders/{id}/cancel`：仅 Reserved 可取消(释放桩 Idle、空闲+1、Order→Canceled)。
- 超时释放：后端 `startReserveSweeper(timeoutSec)` 后台线程周期性把超时未开始的 Reserved 订单自动取消。
  默认 300s(可第 4 个启动参数/直接 `ChargeService::sweepExpiredReservations`)。测试用 -1 秒验证。

## Store 事务化
- Store 加 `beginTx/commitTx/rollbackTx`；普通方法在"本线程事务内"不再重复加锁(lockGuard)。
- ChargeService 的 reserve/start/finish/cancel/sweep 全部包在事务里：任一步失败整体回滚
  (createOrder→setDeviceState→adjustStationFree 不会再半途丢状态)。

## C 端充电交互页(异步服务，不阻塞)
- IChargeService/HttpChargeService：reserve/start/finish/cancel + `GET /api/orders/{id}/live`(实时能量/金额估算)。
- ChargePage(站内点桩进入)：预约→开始→每秒轮询 live 显示电量/金额→结束结算 / 取消预约→返回。
- MainWindow 新增页面 4；main 注入 HttpChargeService。

## e2e 真闭环(tests/e2e)
- 起真实后端(HTTP+模拟器TCP)；Http 客户端登录→预约(单/双)→start→模拟器上报 2.5kWh→
  live=500分→finish 落 Completed amount500→另桩 cancel 释放。全绿。

## 你过目
1. [ ] ssh -X 跑 C 端：登录→找桩→点站→点桩→预约/开始/看金额涨/结束。
2. [ ] 预约超时默认 300s 合不合适(演示可把启动第4参调小)。
3. [ ] C 端充电页布局是占位，等同学前端壳。
