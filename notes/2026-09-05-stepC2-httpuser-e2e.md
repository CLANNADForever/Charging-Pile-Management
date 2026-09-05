# 2026-09-05 · C2：HttpUserService(Qt 客户端) + 前后端端到端联测

## 本次做了什么
- 新增 `client_user/services/HttpUserService`：QNetworkAccessManager + QJsonDocument 实现 IUserService（同步阻塞、4s 超时、默认 http://127.0.0.1:8080）。UI 层无感知可切换(仍是纯虚接口)。
- `ncs_user_ui` 增加 Qt6::Network 依赖并编译 HttpUserService。
- 端到端测试 `tests/e2e`：测试进程里线程起真实后端(BackendApp/httplib) → HttpUserService 打它，覆盖 send-code/非法号/登录自动注册+幂等/错码。6/6 通过。
- 测试总数 3 套全绿：ncs_offscreen(14) + ncs_backend(Store/并发/HTTP) + ncs_e2e。

## 说明：后端并发修复(上一步, 已并入)
- QtSql 连接仅限创建线程 → Store 已改用**系统 SQLite C API + 一把 std::mutex 串行化**(每操作整段加锁，find+insert 原子)；不再依赖 QtSql/插件。
- MockUserService.users_ 加 QMutex。
- 新增并发测试：8 线程自动注册不同号 / 8 线程同号并发不重复。

## 运行(接真实后端)
```bash
build/src/backend_server/ncs_server /tmp/ncs.db 8080 &
# 把 main.cpp 里 MockUserService 换成 HttpUserService("http://127.0.0.1:8080") 即可切真后端
```

## 需要你过目
1. [ ] HttpUserService 同步阻塞 4s 超时可接受？(换异步版要动接口，暂没必要)
2. [ ] 默认后端地址 http://127.0.0.1:8080 是否要可配置(如启动参数/QSettings)。
3. [ ] C 端默认仍用 Mock(登录不依赖后端)；要切真后端需改 main.cpp 一行——你定默认用哪个。
