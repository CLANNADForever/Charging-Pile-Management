# 2026-09-05 · C 端补全(4/5/7 完成, 6 有约束)

## A 后端(前一提交)
- POST /api/wallet/recharge、GET /api/orders/history(翻页)、PATCH /api/user/profile、
  POST /api/user/avatar(raw body → uploads/) + /uploads 静态挂载。

## B C 端
- 个人中心扩展：头像区 + 充值/历史/改昵称/头像入口(充值、昵称、头像走对话框; 历史页带翻页)。
- 头像：选图→转PNG→上传→Profile 下载显示(真实文件存后端 uploads/)；无摄像头(Qt Multimedia 未装)。
- 找桩距离：我的位置(经纬输入，默认39.9,116.32) + 每站 直线距离(haversine, 纯计算)。
- 导航：站详情"导航到站" → 路线信息页(直线距离+外部高德打开)。
  ⚠ 内嵌 QWebEngine 地图被 VM 依赖阻断：Qt6WebEngineCore cmake FOUND=FALSE，
  需 `sudo apt install qt6-webengine-dev`(无 sudo)。已降级为外部 uri 打开，等装好后可再嵌。

## 测试
- 3 套全绿(ncs_offscreen/backend/e2e)。后端增钱包/昵称/历史用例。
