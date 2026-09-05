# 2026-09-05 · Task1：统一 API 信封 + 客户端异步化 + 点站看桩

## 统一响应信封(所有后端接口一致，之后新增接口也照此)
```
{ "code": 0, "message": "ok", "data": ... }
```
- code: 0=成功；1=业务失败(消息在 message)；2=请求解析错误(HTTP 400)
- data: 成功载荷(对象/数组/null)；列表类接口把数组放 data 里
- HTTP 状态：业务一律 200，解析/网关错误 400；客户端只按信封 code 判断。

## 异步化(去掉同步阻塞)
- 服务接口改回调风格：IUserService / IStationService 的 done 回调；HttpJsonClient(QNetworkAccessManager) 异步回调投递，不再 QEventLoop 阻塞。充电页将来实时刷新可直接复用。
- Mock 服务为同步回调(立即)。
- UI(Login/找桩/明细)全部回调更新，不再卡 UI。

## 点站看桩
- 找桩列表单击某站 → StationDetailPage 拉该站桩明细(状态/快慢/功率)；返回按钮回列表。

## 需要你过目
1. [ ] 信封 code 语义(code 0/1/2)够用？后续错误细分再加码。
2. [ ] 列表接口 data 为数组(不再裸数组)，客户端已对齐。
3. [ ] 站明细页信息够不够(未含实时遥测，等 Task2)。
