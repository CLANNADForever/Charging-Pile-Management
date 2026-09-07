<template>
  <ChartBox :option="option" />
</template>

<script setup>
import { computed } from 'vue'
import * as echarts from 'echarts'
import ChartBox from './ChartBox.vue'
import { categoryAxis } from '../echarts/theme'

// 近 30 日营收趋势:折线(营收,左轴)+ 柱状(订单量,右轴)。
const props = defineProps({
  data: { type: Array, default: () => [] }
})

const axisLabel = { color: '#8A94A8', fontSize: 11 }
const splitLine = { lineStyle: { color: '#1B2740', type: 'dashed' } }

const option = computed(() => ({
  tooltip: {
    trigger: 'axis',
    backgroundColor: 'rgba(18,26,41,0.95)',
    borderColor: '#1B2740',
    textStyle: { color: '#E6ECF5', fontSize: 12 }
  },
  legend: {
    data: ['营收', '订单量'],
    top: 0,
    textStyle: { color: '#8A94A8', fontSize: 12 },
    itemWidth: 14,
    itemHeight: 10
  },
  grid: { left: 56, right: 56, top: 36, bottom: 28 },
  xAxis: { ...categoryAxis(), type: 'category', data: props.data.map(d => d.date) },
  yAxis: [
    {
      type: 'value', name: '营收(元)', nameTextStyle: { color: '#8A94A8' },
      axisLine: { show: false }, axisTick: { show: false },
      axisLabel, splitLine
    },
    {
      type: 'value', name: '订单量', nameTextStyle: { color: '#8A94A8' },
      axisLine: { show: false }, axisTick: { show: false },
      axisLabel, splitLine: { show: false }
    }
  ],
  series: [
    {
      name: '营收',
      type: 'line',
      data: props.data.map(d => d.revenue),
      smooth: true,
      itemStyle: { color: '#4D9FFF' },
      lineStyle: { width: 2 },
      areaStyle: {
        color: new echarts.graphic.LinearGradient(0, 0, 0, 1, [
          { offset: 0, color: 'rgba(77,159,255,0.28)' },
          { offset: 1, color: 'rgba(77,159,255,0)' }
        ])
      }
    },
    {
      name: '订单量',
      type: 'bar',
      data: props.data.map(d => d.orders),
      yAxisIndex: 1,
      barWidth: 10,
      itemStyle: { color: 'rgba(34,211,238,0.55)', borderRadius: [3, 3, 0, 0] }
    }
  ]
}))
</script>
