# 代码导览（入口 / 模块连接 / Qt 用法 / 错误处理）

> 读者：项目开发者自己。所有"链接 → 行号"点击可在编辑器中打开对应文件（GitHub 上 #L 可精确跳行）；行号基于当前 master。
> 模块线：`client_user`(C) · `client_admin`(B) · `backend_server` · `simulator` · `common`/`shared/net`(共享) · `tests`。

## 0. 快速入口汇总

| 入口 | 位置 |
|---|---|
| 后端 `ncs_server` | [src/backend_server/main.cpp:10](../src/backend_server/main.cpp#L10) |
| C 端 `ncs_user` | [src/client_user/main.cpp:11](../src/client_user/main.cpp#L11) |
| B 端 `ncs_admin` | [src/client_admin/main.cpp:751](../src/client_admin/main.cpp#L751) |
| 模拟器 `ncs_simulator` | [src/simulator/main.cpp:37](../src/simulator/main.cpp#L37) |
| 测试 offscreen | [tests/tst_common.cpp:196](../tests/tst_common.cpp#L196)（QTEST_MAIN） |
| 测试 e2e | [tests/e2e/tst_e2e.cpp:247](../tests/e2e/tst_e2e.cpp#L247)（QTEST_MAIN） |
| 测试 backend(纯 main) | [tests/server/test_server.cpp](../tests/server/test_server.cpp)（非 Qt 的 `main()`） |
| ctest 注册 | [tests/CMakeLists.txt:12](../tests/CMakeLists.txt#L12) / [tests/server/CMakeLists.txt:6](../tests/server/CMakeLists.txt#L6) / [tests/e2e/CMakeLists.txt:11](../tests/e2e/CMakeLists.txt#L11) |

## 1. 主要模块入口（按启动顺序读）

- **后端（最先起）**：[main.cpp:10](../src/backend_server/main.cpp#L10) → `init()`(→[BackendApp.cpp:235](../src/backend_server/BackendApp.cpp#L235) 打开 DB + 注册路由) → [启动预约超时清扫](../src/backend_server/main.cpp#L31) → [起模拟器 TCP 监听](../src/backend_server/main.cpp#L33) → [阻塞监听 HTTP](../src/backend_server/main.cpp#L41)。
- **C 端**：[main.cpp:11](../src/client_user/main.cpp#L11)：装配三个 HttpService(Mock/Http 二选一注入)→ [MainWindow 构造函数](../src/client_user/views/MainWindow.cpp#L28) 里建页面堆栈与导航连接。
- **B 端**：[main.cpp:751](../src/client_admin/main.cpp#L751)：单文件装配登录页 + 各管理页 + thin `AdminApi`。
- **模拟器**：[main.cpp:37](../src/simulator/main.cpp#L37)：`connect → register → poll 等命令 → 周期心跳`。
- **测试入口**：offscreen/e2e 用 QtTest 框架自动跑 `private slots`；backend 测试是裸 `main()`（无 Qt），方便直连 service/HTTP。跑法：`ctest --test-dir build --output-on-failure`。

## 2. 模块之间的连接处（改动/调试重点）

### (a) 前端 ↔ 后端：HTTP/JSON
- **协议信封**：后端所有响应经 [reply()/replyOk()/replyBizErr()](../src/backend_server/BackendApp.cpp#L58) 统一成 `{code,message,data}`；业务失败也是 HTTP 200 + code≠0。
- **路由注册**：[registerRoutes()](../src/backend_server/BackendApp.cpp#L239)，例：[login](../src/backend_server/BackendApp.cpp#L261) / [站富查询](../src/backend_server/BackendApp.cpp#L282) / [下单](../src/backend_server/BackendApp.cpp#L441)。
- **客户端发起**：所有业务服务收口到一个 [HttpJsonClient::send()](../src/shared/net/HttpJsonClient.cpp#L15)（async + 超时 + 解析），业务层如 [HttpUserService::login](../src/client_user/services/HttpUserService.cpp#L52)、[HttpChargeService::reserve](../src/client_user/services/HttpChargeService.cpp#L62)。
- **UI↔服务**：View 不直接碰网络，走 `I*Service` 异步回调（如 ChargePage 的 [onReserve/onStart/onPoll](../src/client_user/views/ChargePage.cpp#L176)）。想换 Mock 就改注入即可。

### (b) 后端 ↔ 数据库：SQLite（C API）
- 入口：[Store::open()](../src/backend_server/database/Store.cpp#L35)（建表 + 老库 PRAGMA/ALTER 迁移 + 回填 + seed）。
- 读：如 [listStations()](../src/backend_server/database/Store.cpp#L336)；写：如 [createStation()](../src/backend_server/database/Store.cpp#L961)。
- **跨多步事务**：用 [beginTx()](../src/backend_server/database/Store.cpp#L626)/commitTx/rollbackTx，业务侧经 RAII [TxGuard](../src/backend_server/core/ChargeService.cpp#L12) 包裹，如 [reserve()](../src/backend_server/core/ChargeService.cpp#L44)/[finish()](../src/backend_server/core/ChargeService.cpp#L161)/[pay()](../src/backend_server/core/ChargeService.cpp#L201)。

### (c) 后端 ↔ 模拟器：TCP JSON-lines
- 后端监听：[startSimListener()](../src/backend_server/BackendApp.cpp#L1273) → [accept 循环](../src/backend_server/BackendApp.cpp#L1305) → [每连接读行处理](../src/backend_server/BackendApp.cpp#L1317)（register/heartbeat→energy 缓存；[register 分支](../src/backend_server/BackendApp.cpp#L1334)）。
- 下发指令：[sendSimCommand()](../src/backend_server/BackendApp.cpp#L1393)（ChargeService 通过回调触发）。
- 模拟器侧：收到 [start/stop 命令](../src/simulator/main.cpp#L105) 切换 `charging[]` 并按功率累加能量随心跳上报。
- 遥测→DB 状态：[applySimState()](../src/backend_server/BackendApp.cpp#L1503)（每次心跳按 DB 现态独立重判，无去重缓存——上轮修过的"隐形故障"）。

### (d) 共享数据契约
- 实体/枚举/金额/计费/手机号：[src/common/entities.h](../src/common/entities.h)（金额=整数分 MoneyCents，[money.h](../src/common/money.h)）。

## 3. Qt 特性使用（示例）

| 特性 | 例 |
|---|---|
| 信号槽解耦页面 | [MainWindow connect loginSucceeded](../src/client_user/views/MainWindow.cpp#L50)；ChargePage `backRequested` 信号 → MainWindow 切页 |
| QStackedWidget 单例页 + 进入/返回显式刷新 | [MainWindow.cpp:33](../src/client_user/views/MainWindow.cpp#L33)（约定见 使用说明 §3） |
| QTimer 每秒轮询实时计费 | [ChargePage.cpp:74](../src/client_user/views/ChargePage.cpp#L74) + [onPoll()](../src/client_user/views/ChargePage.cpp#L176) |
| 异步网络 QNetworkAccessManager + finished | [HttpJsonClient.cpp:36](../src/shared/net/HttpJsonClient.cpp#L36) |
| 对象树父对象自动释放 / setObjectName 供测试与前端壳定位 | [MainWindow.cpp:30](../src/client_user/views/MainWindow.cpp#L30)、[ChargePage.cpp:28](../src/client_user/views/ChargePage.cpp#L28) |
| 内嵌浏览器 QWebEngineView（懒创建，只在 ncs_user） | [NavigationPage.cpp:70](../src/client_user/views/NavigationPage.cpp#L70) |
| QCryptographicHash 口令哈希(sha256) | [Store.cpp:885](../src/backend_server/database/Store.cpp#L885) |

## 4. 异常 / 错误处理（示例）

| 层 | 策略与例 |
|---|---|
| 后端业务错误 | 不进 5xx：路由内判 `code` 回 [replyBizErr](../src/backend_server/BackendApp.cpp#L58)；参数解析失败 try/catch → 400（[login 路由](../src/backend_server/BackendApp.cpp#L261)） |
| SQLite | 每个方法检查 `rc==SQLITE_DONE` + `sqlite3_changes()>0` 判影响行（如 [createStation](../src/backend_server/database/Store.cpp#L961)）；失败时 TxGuard 析构自动回滚 |
| 坏行容错 | 模拟器 TCP 行解析 try/catch 忽略非法行，继续收 [handleSimConnection](../src/backend_server/BackendApp.cpp#L1317) |
| 客户端网络错误 | [HttpJsonClient 把"传输失败/非 JSON"与"业务 code"分开](../src/shared/net/HttpJsonClient.cpp#L39)，业务层透传 message（如 [HttpUserService.cpp:122](../src/client_user/services/HttpUserService.cpp#L122)） |
| UI 错误呈现 | 服务回调里把 err 写到状态文本，用户可见（ChargePage onReserve/onFinish/onPay） |

---

> 配套：整体功能/架构/接口见 [汇报材料-功能架构接口.md](汇报材料-功能架构接口.md)；工程约定与运行见 [使用说明.md](../使用说明.md)。
