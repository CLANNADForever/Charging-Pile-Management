import { ref, onMounted, onBeforeUnmount } from 'vue'
import fallbackData from '../data/dashboard.json'

// 大屏数据源：从本仓库后端拉取真实数据(需 admin 登录拿 token)；
// ML 负荷预测读 ml_data 离线产物(已同步到 web/public/data/)。
// 后端不可达 / CORS 被拦 / file:// 双击打开时回退到 fallbackData(真实种子数据)。
let adminToken = ''

async function ensureToken() {
  if (adminToken) return adminToken
  const lr = await fetch('/api/admin/login', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ username: 'admin', password: 'admin123' })
  })
  const j = await lr.json()
  adminToken = (j && j.data && j.data.token) || ''
  return adminToken
}

async function apiJson(path, { auth = true } = {}) {
  const res = await fetch(path, {
    cache: 'no-store',
    headers: auth ? { Authorization: 'Bearer ' + adminToken } : {}
  })
  const body = await res.json()
  if (!body || body.code !== 0) throw new Error('后端业务错误')
  return body.data
}

function yuan(cents) {
  return Math.round((cents || 0) / 100 * 100) / 100
}
function round2(x) {
  return Math.round((x || 0) * 100) / 100
}

// 功率档位(与后端 power_tier 一致)：慢 <30 / 快 30–180 / 超 ≥180 kW
function powerTier(kw) {
  if (kw >= 180) return 'ultra'
  if (kw >= 30) return 'fast'
  return 'slow'
}

const DAYS = ['周一', '周二', '周三', '周四', '周五', '周六', '周日']

// 近 30 日逐日注册用户数(registered_at 为 UTC ISO)
function buildUserGrowth(users) {
  const days = 30
  const counts = {}
  for (const u of users || []) {
    if (!u.registered_at) continue
    const d = String(u.registered_at).slice(0, 10) // yyyy-MM-dd(UTC)
    counts[d] = (counts[d] || 0) + 1
  }
  const out = []
  for (let i = days - 1; i >= 0; i--) {
    const d = new Date(Date.now() - i * 86400000)
    const key = d.toISOString().slice(0, 10)
    out.push({ date: key.slice(5), count: counts[key] || 0 })
  }
  return out
}

async function loadBackend() {
  await ensureToken()

  const [overview, daily, stations, users, stStats, hourly] = await Promise.all([
    apiJson('/api/admin/stats/overview'),
    apiJson('/api/admin/stats/daily?days=30'),
    apiJson('/api/stations', { auth: false }),
    apiJson('/api/admin/users').catch(() => []),
    apiJson('/api/admin/stats/stations').catch(() => []),
    apiJson('/api/admin/stats/hourly').catch(() => [])
  ])

  // 逐站经营聚合(按 station_id 索引)
  const statsMap = {}
  for (const s of stStats || []) statsMap[s.station_id] = s

  // 逐站设备(拿真实功率档位 + 逐站状态)；失败不影响主流程
  const devicesByStation = {}
  await Promise.all((stations || []).map(async st => {
    try {
      devicesByStation[st.id] = await apiJson(`/api/stations/${st.id}/devices`, { auth: false })
    } catch (e) {
      devicesByStation[st.id] = []
    }
  }))
  const allDevices = Object.values(devicesByStation).flat()

  // 快/慢/超充占比(按功率档)
  const powerType = { fast: 0, slow: 0, ultra: 0 }
  for (const d of allDevices) {
    const t = powerTier(d.power_kw || 0)
    if (t === 'ultra') powerType.ultra++
    else if (t === 'fast') powerType.fast++
    else powerType.slow++
  }

  // KPI(全部取真实值)
  const kpi = {
    totalOrders: (overview.total && overview.total.orders) || 0,
    totalRevenue: yuan(overview.total && overview.total.revenue_cents),
    onlineChargers: overview.devices_online || 0,
    totalChargers: overview.devices_total || 0,
    registeredUsers: overview.registered_users || (users || []).length,
    todayRevenue: yuan(overview.today && overview.today.revenue_cents),
    monthRevenue: yuan(overview.month && overview.month.revenue_cents)
  }

  // 电桩状态
  const h = overview.device_health || {}
  const chargerStatus = {
    idle: h.idle || 0,
    using: (h.charging || 0) + (h.reserved || 0),
    fault: (h.fault || 0) + (h.rebooting || 0),
    total: overview.devices_total || 0
  }
  const health = chargerStatus.total
    ? Math.round((chargerStatus.idle + chargerStatus.using) / chargerStatus.total * 1000) / 10
    : 0

  // 站点(地图)：真实经纬度 + 逐站状态 + 逐站经营数据
  const stStates = {}
  for (const st of stations || []) stStates[st.id] = { total: 0, idle: 0, using: 0, fault: 0 }
  for (const d of allDevices) {
    const s = stStates[d.station_id]
    if (!s) continue
    s.total++
    if (d.state === 0) s.idle++
    else if (d.state === 1 || d.state === 3) s.using++
    else s.fault++ // 2 故障 / 4 重启中
  }
  const stationsMapped = (stations || []).map(st => {
    const s = stStates[st.id] || { total: 0, idle: 0, using: 0, fault: 0 }
    const g = statsMap[st.id] || {}
    const total = st.total_piles || s.total || 0
    return {
      id: st.id,
      name: st.name,
      address: st.address || '',
      longitude: st.longitude,
      latitude: st.latitude,
      totalChargers: total,
      idle: s.idle,
      using: s.using,
      fault: s.fault,
      onlineRate: total ? Math.round((total - s.fault) / total * 100) / 100 : 0,
      unitPrice: yuan(st.price_cents),
      todayRevenue: yuan(g.today_revenue_cents),
      todayEnergy: round2(g.today_energy_kwh),
      todayOrders: g.today_orders || 0,
      totalRevenue: yuan(g.total_revenue_cents),
      totalEnergy: round2(g.total_energy_kwh),
      totalOrders: g.total_orders || 0
    }
  })

  // 排行：累计充电量 / 今日营收(真实逐站聚合)
  const stationRank = stationsMapped
    .map(s => ({ name: s.name, energy: s.totalEnergy, revenue: s.totalRevenue }))
    .sort((a, b) => b.energy - a.energy)
  const todayRevenueByStation = stationsMapped
    .map(s => ({ name: s.name, revenue: s.todayRevenue }))
    .sort((a, b) => b.revenue - a.revenue)

  // 营收趋势(近 30 日)
  const revenueTrend = (daily || []).map(d => ({
    date: String(d.day || '').slice(5),
    revenue: yuan(d.revenue_cents),
    orders: d.orders || 0
  }))

  // 用户增长(按注册日聚合)
  const userGrowth = buildUserGrowth(users || [])

  // 充电时段热力(星期×小时，值=电量 kWh)
  const hourHeatmap = (hourly || []).map(c => ({
    day: DAYS[(c.dow - 1 + 7) % 7] || DAYS[0],
    hour: c.hour,
    value: round2(c.energy_kwh)
  }))

  // ML 负荷预测 + 高峰预警(读 ml_data 离线产物，已同步到 public/data)
  let loadForecast = []
  let peakWarnings = []
  try {
    const lf = await (await fetch('data/web_load_forecast.json', { cache: 'no-store' })).json()
    loadForecast = (lf.points || []).map(p => ({
      time: String(p.time || '').slice(11, 16),
      load: round2(p.load),
      peak: !!p.peak
    }))
  } catch (e) {
    loadForecast = []
  }
  try {
    const pw = await (await fetch('data/web_peak_warnings.json', { cache: 'no-store' })).json()
    peakWarnings = (pw.warnings || []).map(w => ({
      time: String(w.start || '').slice(11, 16),
      load: round2(w.peak_kwh),
      stations: w.stations || 0
    }))
  } catch (e) {
    peakWarnings = []
  }

  // 真实数据为空时回退到原静态数据，避免组件空白/为 0。
  const hasAny = (arr, key) => Array.isArray(arr) && arr.some(x => Number(x && x[key]) > 0)
  return {
    meta: { dataThrough: '实时' },
    kpi,
    chargerStatus,
    health,
    stations: stationsMapped.length ? stationsMapped : fallbackData.stations,
    stationRank: hasAny(stationRank, 'energy') ? stationRank : fallbackData.stationRank,
    todayRevenueByStation: hasAny(todayRevenueByStation, 'revenue')
      ? todayRevenueByStation : fallbackData.todayRevenueByStation,
    revenueTrend: hasAny(revenueTrend, 'revenue') ? revenueTrend : fallbackData.revenueTrend,
    userGrowth: hasAny(userGrowth, 'count') ? userGrowth : fallbackData.userGrowth,
    hourHeatmap: hasAny(hourHeatmap, 'value') ? hourHeatmap : fallbackData.hourHeatmap,
    powerType: (powerType.fast + powerType.slow + powerType.ultra) > 0
      ? powerType : fallbackData.powerType,
    loadForecast: loadForecast.length ? loadForecast : fallbackData.loadForecast,
    peakWarnings: peakWarnings.length ? peakWarnings : fallbackData.peakWarnings
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
      // 保留 fallbackData(真实种子数据)，不覆盖
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
