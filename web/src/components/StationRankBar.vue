<template>
  <ChartBox :option="option" />
</template>

<script setup>
import { computed } from 'vue'
import * as echarts from 'echarts'
import ChartBox from './ChartBox.vue'
import { valueAxis, categoryAxis } from '../echarts/theme'

// 电站累计充电量排行(横向柱状图)。
const props = defineProps({
  data: { type: Array, default: () => [] }
})

const option = computed(() => {
  const sorted = [...props.data].sort((a, b) => (b.energy || 0) - (a.energy || 0))
  const names = sorted.map(d => d.name)
  const values = sorted.map(d => d.energy || 0)

  return {
    tooltip: {
      trigger: 'axis',
      axisPointer: { type: 'shadow' },
      backgroundColor: 'rgba(18,26,41,0.95)',
      borderColor: '#1B2740',
      textStyle: { color: '#E6ECF5', fontSize: 12 },
      formatter: params => {
        const i = params[0].dataIndex
        const d = sorted[i]
        return `${d.name}<br/>累计充电量:${d.energy} kWh<br/>累计营收:${d.revenue ?? '--'} 元`
      }
    },
    grid: { left: 120, right: 40, top: 10, bottom: 24 },
    xAxis: { ...valueAxis(), type: 'value' },
    yAxis: { ...categoryAxis(), type: 'category', data: names, inverse: true },
    series: [
      {
        type: 'bar',
        data: values,
        barWidth: 12,
        itemStyle: {
          borderRadius: [0, 6, 6, 0],
          color: new echarts.graphic.LinearGradient(0, 0, 1, 0, [
            { offset: 0, color: '#22D3EE' },
            { offset: 1, color: '#4D9FFF' }
          ])
        },
        label: { show: true, position: 'right', color: '#8A94A8', fontSize: 12, formatter: '{c}' }
      }
    ]
  }
})
</script>
