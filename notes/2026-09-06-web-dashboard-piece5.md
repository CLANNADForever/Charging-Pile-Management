# 2026-09-06 · 片5：Web 数据大屏入库并接后端 stats
- `web/`(Vue3+ECharts+Vite) 源码入库；node_modules/dist 不入库(.gitignore)。
- 取数：useDashboard 启动用 admin/admin123 取 token → `/api/admin/stats/overview|daily` → 映射 KPI/近7日趋势/设备健康度；其余(排行/热力图/用户增长/负荷)保留静态。不可达回退静态防空白。
- vite dev 代理 `/api`→8080；后端统一回包加 CORS + OPTIONS 预检。
- VM 无 node：未能在本机 build 验证，语法经人工核对；构建/冒烟留在有 node 的环境。
