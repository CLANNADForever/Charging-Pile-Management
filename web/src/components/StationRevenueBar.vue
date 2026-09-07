<template>
  <ChartBox :option="option" />
</template>

<script setup>
import { computed } from 'vue'
import * as echarts from 'echarts'
import ChartBox from './ChartBox.vue'
import { valueAxis, categoryAxis } from '../echarts/theme'

// 各站今日营收排行(竖向柱状图)。
const props = defineProps({
  data: { type: Array, default: () => [] }
})

const option = computed(() => {
  const sorted = [...props.data].sort((a, b) => (b.revenue || 0) - (a.revenue || 0))
  const names = sorted.map(d => d.name)
  const values = sorted.map(d => d.revenue || 0)

  return {
    tooltip: {
      trigger: 'axis',
      axisPointer: { type: 'shadow' },
      backgroundColor: 'rgba(18,26,41,0.95)',
      borderColor: '#1B2740',
      textStyle: { color: '#E6ECF5', fontSize: 12 },
      formatter: params => {
        const i = params[0].dataIndex
        return `${sorted[i].name}<br/>今日营收:${sorted[i].revenue} 元`
      }
    },
    grid: { left: 48, right: 20, top: 30, bottom: 60 },
    xAxis: {
      ...categoryAxis(),
      type: 'category',
      data: names,
      axisLabel: { color: '#8A94A8', fontSize: 11, rotate: 28, interval: 0 }
    },
    yAxis: { ...valueAxis(), type: 'value' },
    series: [
      {
        type: 'bar',
        data: values,
        barWidth: 18,
        itemStyle: {
          borderRadius: [6, 6, 0, 0],
          color: new echarts.graphic.LinearGradient(0, 0, 0, 1, [
            { offset: 0, color: '#4D9FFF' },
            { offset: 1, color: '#22D3EE' }
          ])
        },
        label: { show: true, position: 'top', color: '#8A94A8', fontSize: 11, formatter: '{c}' }
      }
    ]
  }
})
</script>
