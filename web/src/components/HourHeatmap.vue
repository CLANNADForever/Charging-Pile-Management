<template>
  <ChartBox :option="option" />
</template>

<script setup>
import { computed } from 'vue'
import ChartBox from './ChartBox.vue'

// 充电时段热力分布:7 天 × 24 小时热力图。
const props = defineProps({
  data: { type: Array, default: () => [] }
})

const DAYS = ['周一', '周二', '周三', '周四', '周五', '周六', '周日']

const option = computed(() => {
  const dayIndex = {}
  DAYS.forEach((d, i) => { dayIndex[d] = i })

  const heatData = props.data.map(d => [d.hour, dayIndex[d.day] ?? 0, d.value])
  const maxVal = props.data.reduce((m, d) => Math.max(m, d.value || 0), 0) || 1

  return {
    tooltip: {
      position: 'top',
      backgroundColor: 'rgba(18,26,41,0.95)',
      borderColor: '#1B2740',
      textStyle: { color: '#E6ECF5', fontSize: 12 },
      formatter: p => `${p.value[1] >= 0 ? DAYS[p.value[1]] : ''} ${p.value[0]} 时:${p.value[2]} kWh`
    },
    grid: { left: 60, right: 24, top: 10, bottom: 56 },
    xAxis: {
      type: 'category',
      data: Array.from({ length: 24 }, (_, h) => h + '时'),
      axisLabel: { color: '#8A94A8', fontSize: 10 },
      axisLine: { lineStyle: { color: '#1B2740' } },
      splitArea: { show: false }
    },
    yAxis: {
      type: 'category',
      data: DAYS,
      axisLabel: { color: '#8A94A8', fontSize: 11 },
      axisLine: { lineStyle: { color: '#1B2740' } },
      splitArea: { show: false }
    },
    visualMap: {
      min: 0,
      max: maxVal,
      calculable: false,
      orient: 'horizontal',
      left: 'center',
      bottom: 0,
      itemWidth: 10,
      itemHeight: 90,
      textStyle: { color: '#8A94A8', fontSize: 10 },
      inRange: { color: ['#0E1628', '#4D9FFF', '#22D3EE', '#F59E0B'] }
    },
    series: [
      {
        type: 'heatmap',
        data: heatData,
        itemStyle: { borderColor: '#0B1220', borderWidth: 2 },
        label: { show: false }
      }
    ]
  }
})
</script>
