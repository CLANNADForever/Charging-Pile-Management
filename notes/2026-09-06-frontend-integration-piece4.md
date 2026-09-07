# 2026-09-06 · 新前端整合 片4(收口·阶段一)

## 完成
- frontend CMake 重构：`ncs_front_core`(core/service+BackendClient)静态库；可执行归位 `ncs_user`/`ncs_admin`。
- 旧 `src/client_user|client_admin` 退役删除；`src/CMakeLists`、顶层、tests CMake 收敛。
- offscreen 测试改造：实体/money + 新 core 冒烟(进程内后端：C 登录/站桩读、B admin 登录+读)；`ncs_backend` 保留；原 e2e 删除(覆盖并入 backend 测试，避免依赖已退役旧客户端)。
- B 端写全走后端：建/改/删站、增/删桩、冻结/解冻、手工标记故障/恢复(新增 `POST /api/admin/devices/{id}/fault`)。
- 片1–3 期间 ctest 偶发一次 backend Failed(直跑/复跑全绿，疑环境瞬时，记录观察)。

## 待做(片4 剩余 / 片5)
- 使用说明完整重写&更多联调；审计/运维日志读端到端验证。
- web 大屏入库 + 接 stats(片5)。
