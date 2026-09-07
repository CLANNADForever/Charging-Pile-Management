<template>
  <div ref="el" class="chart-box"></div>
</template>

<script setup>
import { ref, watch, onMounted, onBeforeUnmount, nextTick } from 'vue'
import * as echarts from 'echarts'
import fallbackGeo from '../data/hangzhou.json'

// 充电站地图分布:杭州 GeoJSON 底图 + 涟漪散点。
// 散点大小 ∝ 累计充电量,颜色 ∝ 在线率;悬停 tooltip 显示完整站详情。
const props = defineProps({
  stations: { type: Array, default: () => [] }
})

const el = ref(null)
let chart = null
let geoLoaded = false

function onlineRateColor(rate) {
  if (rate == null) return '#8A94A8'
  if (rate >= 0.9) return '#2DD4A7'
  if (rate >= 0.6) return '#F59E0B'
  return '#EF4444'
}

function detailFormatter(params) {
  const d = params.data || {}
  const pct = d.onlineRate != null ? (d.onlineRate * 100).toFixed(0) + '%' : '--'
  return [
    `<div style="font-weight:bold;margin-bottom:6px;">${d.name || '--'}</div>`,
    `地址:${d.address || '--'}`,
    `电桩:总 ${d.total ?? '--'} / 空闲 ${d.idle ?? '--'} / 使用中 ${d.using ?? '--'} / 故障 ${d.fault ?? '--'}`,
    `在线率:${pct}　单价:${d.unitPrice ?? '--'} 元/度`,
    `今日营收:${d.todayRevenue ?? '--'} 元　今日充电量:${d.todayEnergy ?? '--'} kWh`,
    `累计充电量:${d.totalEnergy ?? '--'} kWh`
  ].join('<br/>')
}

function mapData() {
  return props.stations.map(s => {
    const energy = s.totalEnergy || 0
    return {
      name: s.name,
      value: [s.longitude, s.latitude, energy],
      symbolSize: 10 + Math.min(12, energy / 4000),
      itemStyle: { color: onlineRateColor(s.onlineRate) },
      address: s.address,
      total: s.totalChargers,
      idle: s.idle,
      using: s.using,
      fault: s.fault,
      onlineRate: s.onlineRate,
      unitPrice: s.unitPrice,
      todayRevenue: s.todayRevenue,
      todayEnergy: s.todayEnergy,
      totalEnergy: s.totalEnergy
    }
  })
}

function buildOption() {
  const tooltip = {
    trigger: 'item',
    backgroundColor: 'rgba(18,26,41,0.95)',
    borderColor: '#1B2740',
    textStyle: { color: '#E6ECF5', fontSize: 12 },
    formatter: detailFormatter
  }

  const data = mapData()

  if (geoLoaded) {
    return {
      tooltip,
      geo: {
        map: 'hangzhou',
        roam: true,
        zoom: 1.25,
        itemStyle: {
          areaColor: '#121A29',
          borderColor: '#2A3548',
          borderWidth: 1.5
        },
        emphasis: {
          itemStyle: { areaColor: '#1B2740' },
          label: { show: true, color: '#E6ECF5' }
        }
      },
      series: [
        {
          type: 'effectScatter',
          coordinateSystem: 'geo',
          data,
          rippleEffect: { brushType: 'stroke', scale: 3 },
          showEffectOn: 'render',
          itemStyle: { shadowBlur: 8, shadowColor: 'rgba(0,0,0,0.4)' },
          label: { show: true, position: 'bottom', color: '#E6ECF5', fontSize: 10, formatter: '{b}' }
        }
      ]
    }
  }

  // 兜底:无 GeoJSON 时,经纬度线性投影到平面散点。
  const lngs = props.stations.map(s => s.longitude)
  const lats = props.stations.map(s => s.latitude)
  const minLng = Math.min(...lngs, 0), maxLng = Math.max(...lngs, 1)
  const minLat = Math.min(...lats, 0), maxLat = Math.max(...lats, 1)
  return {
    tooltip,
    xAxis: {
      type: 'value', min: minLng, max: maxLng,
      axisLabel: { color: '#8A94A8', fontSize: 10 }, splitLine: { show: false }
    },
    yAxis: {
      type: 'value', min: minLat, max: maxLat,
      axisLabel: { color: '#8A94A8', fontSize: 10 }, splitLine: { show: false }
    },
    series: [
      {
        type: 'scatter',
        data: props.stations.map(s => ({
          name: s.name,
          value: [s.longitude, s.latitude],
          symbolSize: 16,
          itemStyle: { color: onlineRateColor(s.onlineRate) },
          address: s.address, total: s.totalChargers, idle: s.idle, using: s.using,
          fault: s.fault, onlineRate: s.onlineRate, unitPrice: s.unitPrice,
          todayRevenue: s.todayRevenue, todayEnergy: s.todayEnergy, totalEnergy: s.totalEnergy
        })),
        label: { show: true, position: 'bottom', color: '#E6ECF5', fontSize: 10 }
      }
    ]
  }
}

function render() {
  if (chart) chart.setOption(buildOption(), true)
}

async function loadGeo() {
  try {
    const res = await fetch('data/hangzhou.json', { cache: 'no-store' })
    if (!res.ok) throw new Error('HTTP ' + res.status)
    const geo = await res.json()
    echarts.registerMap('hangzhou', geo)
  } catch (e) {
    // fetch 失败(如 file:// 双击)时回退到 import 的静态 GeoJSON
    echarts.registerMap('hangzhou', fallbackGeo)
  }
  geoLoaded = true
  render()
}

function resize() {
  if (chart) chart.resize()
}

onMounted(async () => {
  await nextTick()
  chart = echarts.init(el.value)
  window.addEventListener('resize', resize)
  await loadGeo()
})

onBeforeUnmount(() => {
  window.removeEventListener('resize', resize)
  if (chart) {
    chart.dispose()
    chart = null
  }
})

watch(() => props.stations, () => render(), { deep: true })
</script>

<style scoped>
.chart-box {
  width: 100%;
  height: 100%;
  min-height: 0;
}
</style>
