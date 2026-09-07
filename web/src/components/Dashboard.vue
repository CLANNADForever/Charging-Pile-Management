<template>
  <div class="screen-wrap">
    <div class="screen" :style="screenStyle">
      <TitleBar :data-through="meta?.dataThrough" />

      <!-- 核心指标带(6 张 KPI 卡) -->
      <div class="kpi-band">
        <KpiCard
          v-for="(c, i) in kpiCards"
          :key="i"
          :label="c.label"
          :value="c.value"
          :sub="c.sub"
          :color="c.color"
        />
      </div>

      <!-- 三列 -->
      <div class="columns">
        <div class="column">
          <Panel title="电桩状态占比" unit="台">
            <DonutPie :data="chargerStatusData" :center-label="chargerTotal" center-sub="总电桩" />
          </Panel>
          <Panel title="电站充电量排行" unit="kWh">
            <StationRankBar :data="stationRank" />
          </Panel>
          <Panel title="用户增长趋势" unit="人">
            <UserGrowth :data="userGrowth" />
          </Panel>
        </div>

        <div class="column">
          <Panel title="充电站地图分布" :grow="2">
            <StationMap :stations="stations" />
          </Panel>
          <Panel title="近 30 日营收趋势" unit="元">
            <RevenueTrend :data="revenueTrend" />
          </Panel>
          <Panel title="各站今日营收排行" unit="元">
            <StationRevenueBar :data="todayRevenueByStation" />
          </Panel>
        </div>

        <div class="column">
          <Panel title="充电时段热力分布">
            <HourHeatmap :data="hourHeatmap" />
          </Panel>
          <Panel title="快慢充占比">
            <DonutPie :data="powerTypeData" :center-label="powerTotal" center-sub="总电桩" />
          </Panel>
          <Panel title="未来 24 小时负荷预测" unit="kW">
            <LoadForecast :data="loadForecast" />
          </Panel>
          <Panel title="设备健康度与预警">
            <div class="health-warn">
              <HealthGauge :health="health" />
              <PeakWarnings :items="peakWarnings" />
            </div>
          </Panel>
        </div>
      </div>
    </div>
  </div>
</template>

<script setup>
import { ref, computed, onMounted, onBeforeUnmount } from 'vue'
import TitleBar from './TitleBar.vue'
import Panel from './Panel.vue'
import KpiCard from './KpiCard.vue'
import DonutPie from './DonutPie.vue'
import StationMap from './StationMap.vue'
import StationRankBar from './StationRankBar.vue'
import StationRevenueBar from './StationRevenueBar.vue'
import RevenueTrend from './RevenueTrend.vue'
import UserGrowth from './UserGrowth.vue'
import HourHeatmap from './HourHeatmap.vue'
import LoadForecast from './LoadForecast.vue'
import HealthGauge from './HealthGauge.vue'
import PeakWarnings from './PeakWarnings.vue'
import { useDashboard } from '../composables/useDashboard'
import { formatMoney, formatNumber } from '../utils/format'

const { data } = useDashboard()
const meta = computed(() => (data.value && data.value.meta) || null)

// ===== 核心指标带 =====
const kpiCards = computed(() => {
  const k = data.value?.kpi
  if (!k) return []
  const rate = k.totalChargers
    ? Math.round((k.onlineChargers / k.totalChargers) * 100) + '%'
    : '--'
  return [
    { label: '总充电次数', value: formatNumber(k.totalOrders), sub: '累计', color: '#4D9FFF' },
    { label: '总营收(元)', value: formatMoney(k.totalRevenue), sub: '累计', color: '#22D3EE' },
    { label: '在线电桩', value: `${k.onlineChargers}/${k.totalChargers}`, sub: '在线率 ' + rate, color: '#2DD4A7' },
    { label: '注册用户', value: formatNumber(k.registeredUsers), sub: '累计注册', color: '#A78BFA' },
    { label: '今日营收(元)', value: formatMoney(k.todayRevenue), sub: '今日', color: '#F59E0B' },
    { label: '本月营收(元)', value: formatMoney(k.monthRevenue), sub: '本月', color: '#4D9FFF' }
  ]
})

// ===== 电桩状态环图 =====
const chargerStatusData = computed(() => {
  const s = data.value?.chargerStatus
  if (!s) return []
  return [
    { name: '空闲', value: s.idle, color: '#2DD4A7' },
    { name: '使用中', value: s.using, color: '#F59E0B' },
    { name: '故障', value: s.fault, color: '#EF4444' }
  ]
})
const chargerTotal = computed(() => {
  const t = data.value?.chargerStatus?.total
  return t == null ? '--' : String(t)
})

// ===== 快慢充占比环图 =====
const powerTypeData = computed(() => {
  const p = data.value?.powerType
  if (!p) return []
  return [
    { name: '快充', value: p.fast, color: '#22D3EE' },
    { name: '慢充', value: p.slow, color: '#4D9FFF' },
    { name: '超充', value: p.ultra, color: '#A78BFA' }
  ]
})
const powerTotal = computed(() => {
  const p = data.value?.powerType
  if (!p) return '--'
  return String(p.fast + p.slow + p.ultra)
})

// ===== 地图与排行 =====
const stations = computed(() => data.value?.stations || [])
const stationRank = computed(() => data.value?.stationRank || [])
const todayRevenueByStation = computed(() => data.value?.todayRevenueByStation || [])
const revenueTrend = computed(() => data.value?.revenueTrend || [])
const userGrowth = computed(() => data.value?.userGrowth || [])
const hourHeatmap = computed(() => data.value?.hourHeatmap || [])
const loadForecast = computed(() => data.value?.loadForecast || [])
const health = computed(() => data.value?.health ?? 0)
const peakWarnings = computed(() => data.value?.peakWarnings || [])

// ===== 1920×1080 等比缩放居中 =====
const scale = ref(1)
const offsetX = ref(0)
const offsetY = ref(0)

function updateScale() {
  const s = Math.min(window.innerWidth / 1920, window.innerHeight / 1080)
  scale.value = s
  offsetX.value = (window.innerWidth - 1920 * s) / 2
  offsetY.value = (window.innerHeight - 1080 * s) / 2
}

const screenStyle = computed(() => ({
  transform: `translate(${offsetX.value}px, ${offsetY.value}px) scale(${scale.value})`
}))

onMounted(() => {
  updateScale()
  window.addEventListener('resize', updateScale)
})

onBeforeUnmount(() => {
  window.removeEventListener('resize', updateScale)
})
</script>
