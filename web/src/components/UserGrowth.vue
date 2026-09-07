<template>
  <ChartBox :option="option" />
</template>

<script setup>
import { computed } from 'vue'
import * as echarts from 'echarts'
import ChartBox from './ChartBox.vue'
import { categoryAxis, valueAxis } from '../echarts/theme'

// 用户增长趋势:折线 + 面积。
const props = defineProps({
  data: { type: Array, default: () => [] }
})

const option = computed(() => ({
  tooltip: {
    trigger: 'axis',
    backgroundColor: 'rgba(18,26,41,0.95)',
    borderColor: '#1B2740',
    textStyle: { color: '#E6ECF5', fontSize: 12 }
  },
  grid: { left: 40, right: 20, top: 20, bottom: 30 },
  xAxis: { ...categoryAxis(), type: 'category', data: props.data.map(d => d.date) },
  yAxis: { ...valueAxis(), type: 'value' },
  series: [
    {
      type: 'line',
      data: props.data.map(d => d.count),
      smooth: true,
      itemStyle: { color: '#A78BFA' },
      lineStyle: { width: 2 },
      areaStyle: {
        color: new echarts.graphic.LinearGradient(0, 0, 0, 1, [
          { offset: 0, color: 'rgba(167,139,250,0.28)' },
          { offset: 1, color: 'rgba(167,139,250,0)' }
        ])
      }
    }
  ]
}))
</script>
