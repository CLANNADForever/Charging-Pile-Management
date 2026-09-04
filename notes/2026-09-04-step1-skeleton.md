# 2026-09-04 · 第一片：多模块骨架 + C 端壳窗 + QtTest

## 本次做了什么
- 移除 AutoAI-Harness 治理层（负担过重，时间紧，留待宽裕时再评估）；`docs/ncs-srs.md` 已保留为需求文档。
- 按 SRS 把 src/ 重组为多模块 CMake：
  - `src/common/` → 静态库 `ncs_common`：`entities.h`（起步：User / DeviceState / Device）
  - `src/client_user/` → 可执行 `ncs_user`：420×760 竖屏壳窗（仅占位，无业务页）
  - `tests/` → 可执行 `ncs_tests`：QtTest，offscreen 跑（实体 + 壳窗断言）
- 构建/测试均通过（ctest 1/1 Passed）。

## 常用命令
```bash
cmake -S . -B build && cmake --build build -j2     # 构建全部
ctest --test-dir build --output-on-failure         # 无头测试(自动 offscreen)
build/src/client_user/ncs_user                     # 运行 C 端(有 X11 时窗口会弹出)
```

## 需要你过目 / 决定
1. [ ] **实际弹窗看一次**：`ssh -X` 下跑 `build/src/client_user/ncs_user`，确认 420×760 竖屏、标题"NCS 车主端"、占位文案可接受。
2. [ ] SRS（`docs/ncs-srs.md`）确认无损、仍是你那份完整需求。
3. [ ] 命名约定确认：可执行 `ncs_user`、命名空间 `ncs`、窗口 objectName `ncsUserMainWindow`（后续测试要按它查找）。C 端/管理端/后端是否也接受 `ncs_admin`/`ncs_server`？
4. [ ] entities 起步只放了 User/Device，**Station / Order / 计费状态机字段还没建模**——建议在"登录"切片之前先补实体建模。

## 我的建议 · 下一个切片
按 SRS 的依赖顺序，建议第二片 = **"手机号免密登录(纯 UI + MockService)"**：补 IUserService 纯虚接口 +
MockUserService + 登录页/个人中心骨架，并把注册的 11 位手机号校验规则 + 验证码模拟用 QtTest 覆盖。
（理由：SRS 规定 View 不碰业务、靠 Mock 起步，登录是最小且可无后端测试的闭环。）
