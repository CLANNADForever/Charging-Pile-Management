# 2026-09-06 · 覆盖决定：余额非负 + start 返回订单 + 站内"我的活跃桩"

## ① 余额不允许为负(推翻"允许欠费")
- `ChargeService::pay`：`amountCents > 当前余额` → 拒付(不扣款、不标 Paid)，提示"余额不足，请先充值"。
- `ChargeService::reserve`：`balance<0` → 拒绝(兜底历史遗留负数)。
- 同步清理旧"允许欠费/余额-400"测试断言(改为预充值)与 使用说明 语义。
- 覆盖旧 notes(2026-09-05-issue123/ task2)里的"允许欠费沿用"。

## ② start 把客户端订单清成 0
- 根因：后端 `/orders/{id}/start` 成功返回 `data:null`；ChargePage::onStart 无条件 applyOrder(空订单) → cur_.id=0 → finish(0)/onPoll(0) 全废。
- 修：后端 start 成功返回更新后订单 JSON(同 finish)；客户端 applyOrder 仅在 `o.id>0` 时回填。
- 回归：e2e 断言 start 响应 id==预约id 且状态 Charging；服务层 e2e 覆盖 start→live→finish。

## ③ 站内列表标注我的充电桩，点击直接恢复
- 接线：进站详情拉一次 `listActive(phone)` → MainWindow 维护 `deviceId->order`；StationDetailPage 标注"我的·充电中/预约/待支付"；onDeviceChosen 若有我的活跃单 → `resumeSession`(实时/结算页)，否则 → `startSession`。
- 只标"我的"(需求所述)；"他人占用桩"语义留待前端回合定。

## 验收
- 后端/客户端代码 + 测试已按模块 3 个小提交；ctest 3/3 绿。手动路径待用户过：0 余额支付被拒、负余额不可约、start 后立刻结束成功、点自己充电中的桩进实时页。
