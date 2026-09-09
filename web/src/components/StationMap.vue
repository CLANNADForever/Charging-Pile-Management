<template>
  <div ref="el" class="chart-box"></div>
</template>

<script setup>
import { ref, watch, onMounted, onBeforeUnmount, nextTick } from 'vue'
import * as echarts from 'echarts'
import beijingGeo from '../data/beijing.json'

// 充电站地图分布：北京行政区划 GeoJSON 底图(真实区界) + 涟漪散点标点。
// 三站分别落在 朝阳(望京)/海淀(中关村)/大兴(亦庄)。
const props = defineProps({
  stations: { type: Array, default: () => [] }
})

const el = ref(null)
let chart = null
let geoRegistered = false

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
    `可用率:${pct}　单价:${d.unitPrice ?? '--'} 元/度`,
    `今日营收:${d.todayRevenue ?? '--'} 元　今日充电量:${d.todayEnergy ?? '--'} kWh`,
    `累计充电量:${d.totalEnergy ?? '--'} kWh`
  ].join('<br/>')
}

function buildOption() {
  const data = props.stations
    .filter(s => s.longitude != null && s.latitude != null)
    .map(s => ({
      name: s.name,
      value: [s.longitude, s.latitude],
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
    }))

  return {
    tooltip: {
      trigger: 'item',
      backgroundColor: 'rgba(18,26,41,0.95)',
      borderColor: '#1B2740',
      textStyle: { color: '#E6ECF5', fontSize: 12 },
      formatter: detailFormatter
    },
    geo: {
      map: 'beijing',
      roam: true,
      zoom: 1.05,
      layoutCenter: ['50%', '50%'],
      label: {
        show: true,
        color: '#6B7A95',
        fontSize: 10
      },
      itemStyle: {
        areaColor: '#0E1830',
        borderColor: '#2C4A6E',
        borderWidth: 1.4,
        shadowColor: 'rgba(0,229,255,0.18)',
        shadowBlur: 12
      },
      emphasis: {
        label: { show: true, color: '#E6ECF5', fontSize: 11 },
        itemStyle: { areaColor: '#1B2A48' }
      }
    },
    series: [
      {
        type: 'effectScatter',
        coordinateSystem: 'geo',
        data,
        symbolSize: 15,
        rippleEffect: { brushType: 'stroke', scale: 3.2 },
        showEffectOn: 'render',
        itemStyle: {
          color: params => onlineRateColor(params.data.onlineRate),
          shadowBlur: 12,
          shadowColor: 'rgba(0,0,0,0.5)'
        },
        label: {
          show: true,
          position: 'bottom',
          color: '#E6ECF5',
          fontSize: 11,
          textShadowColor: 'rgba(0,0,0,0.6)',
          textShadowBlur: 3,
          formatter: '{b}'
        },
        zlevel: 2
      }
    ]
  }
}

function render() {
  if (chart) chart.setOption(buildOption(), true)
}

function resize() {
  if (chart) chart.resize()
}

onMounted(async () => {
  await nextTick()
  chart = echarts.init(el.value)
  if (!geoRegistered) {
    echarts.registerMap('beijing', beijingGeo)
    geoRegistered = true
  }
  window.addEventListener('resize', resize)
  render()
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
