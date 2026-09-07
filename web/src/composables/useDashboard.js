import { ref, onMounted, onBeforeUnmount } from 'vue'
import fallbackData from '../data/dashboard.json'

// 大屏数据源：尽力从本仓库后端拉 stats/overview|daily(需 admin 登录拿 token)，
// 后端不可达/CORS 被拦/构建后 file:// 打开时回退到静态数据，保证不空白。
// 布局保持队友交付不动；只改“取数”与字段映射。
let adminToken = ''

async function apiJson(path) {
  if (!adminToken) {
    const lr = await fetch('/api/admin/login', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ username: 'admin', password: 'admin123' })
    })
    const j = await lr.json()
    adminToken = j && j.data && j.data.token
  }
  const res = await fetch(path, {
    cache: 'no-store',
    headers: { Authorization: 'Bearer ' + adminToken }
  })
  const body = await res.json()
  if (!body || body.code !== 0) throw new Error('后端业务错误')
  return body.data
}

function yuan(cents) {
  return Math.round((cents || 0) / 100 * 100) / 100
}

async function loadBackend() {
  const overview = await apiJson('/api/admin/stats/overview')
  const daily = await apiJson('/api/admin/stats/daily?days=7')

  const kpi = {
    ...fallbackData.kpi,
    todayRevenue: yuan(overview.today && overview.today.revenue_cents),
    monthRevenue: yuan(overview.month && overview.month.revenue_cents),
    totalRevenue: yuan(overview.total && overview.total.revenue_cents),
    totalOrders: (overview.total && overview.total.orders) || 0,
    onlineChargers: overview.devices_online || 0,
    totalChargers: overview.devices_total || 0
  }
  const h = (overview.device_health) || {}
  const chargerStatus = {
    idle: h.idle || 0,
    using: (h.charging || 0) + (h.reserved || 0),
    fault: h.fault || 0,
    total: overview.devices_total || 0
  }
  const health = chargerStatus.total
    ? Math.round((chargerStatus.idle + chargerStatus.using) / chargerStatus.total * 1000) / 10
    : 0
  const revenueTrend = (daily || []).map(d => ({
    date: String(d.day || '').slice(5), // yyyy-MM-dd -> MM-dd
    revenue: yuan(d.revenue_cents),
    orders: d.orders || 0
  }))
  return {
    ...fallbackData,
    kpi,
    chargerStatus,
    health,
    revenueTrend
  }
}

export function useDashboard(interval = 30000) {
  const data = ref(fallbackData)
  const error = ref(false)
  let timer = null

  async function fetchData() {
    try {
      data.value = await loadBackend()
      error.value = false
    } catch (e) {
      error.value = true
      // 保留 fallbackData(静态数据),不覆盖
    }
  }

  onMounted(() => {
    fetchData()
    timer = setInterval(fetchData, interval)
  })

  onBeforeUnmount(() => {
    if (timer) clearInterval(timer)
  })

  return { data, error, refresh: fetchData }
}
