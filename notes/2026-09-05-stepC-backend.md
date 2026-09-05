# 2026-09-05 · C：后端 HTTP 服务起步(SQLite + AuthService + cpp-httplib)

## 关键决策(已与你确认)
- HTTP 用 cpp-httplib、JSON 用 nlohmann/json（Qt 客户端将来用 QNetworkAccessManager）；模拟器链路将来走 TCP 长连接 JSON-lines。
- 实体沿用 Qt 类型(common 不动)，后端链接 Qt Core/Sql（无 Widgets/Gui，可无头运行）。注意：严格"完全无 Qt"需把实体改 std，属另一改动，已记预备。
- 第三方头：github 直连不通，从 jsdelivr CDN 拉到并 vendor 到 third_party/（httplib.h、nlohmann/json.hpp），零运行时下载。

## 本次做了什么
- 建 ncs_server_lib(STATIC)：core/AuthService(复用 phone.h 校验+demo 码+冻结拦截) + database/Store(QtSql，系统自带 libqsqlite 驱动) + BackendApp(路由+实体<->JSON)
- ncs_server(薄 main)：`ncs_server [db路径] [端口]`，默认 ./ncs-backend.db:8080
- 路由：GET /health；POST /api/auth/send-code {phone}；POST /api/auth/login {phone,code}
- 测试：tests/server(纯 C++ httplib 客户端) 直测 Store/AuthService(自动注册) + 起真实服务打接口；ncs_backend Passed；总 2/2。
- 顺带合入另一窗口的共享校验抽取：common/phone + MockUserService 改用它。

## 运行
```bash
cmake -S . -B build && cmake --build build -j2
build/src/backend_server/ncs_server /tmp/ncs.db 8080
# 然后本机 GET /health、POST /api/auth/login 打 127.0.0.1:8080 验证
```

## 需要你过目
1. 后端落 Qt Core/Sql 可接受？(若必须完全无 Qt，把 common 实体改 std——影响现有 C 端，改动不小)
2. 路由/JSON 命名(phone/code/balance_cents/status 0=正常)合意？登录接口返回 200+ok:false(不搞 401/403)。
3. 冻结账号已拦截；"管理员手动冻结"接口留给管理端切片。

## 已记预备(下一步)
- HttpUserService(Qt QNetworkAccessManager 实现 IUserService) + 前后端端到端联测。
- 实体改"完全无 Qt std"的后备方案。
