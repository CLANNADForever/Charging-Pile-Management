<template>
  <ChartBox :option="option" />
</template>

<script setup>
import { computed } from 'vue'
import ChartBox from './ChartBox.vue'
import { categoryAxis, valueAxis } from '../echarts/theme'

// 未来 24 小时负荷预测:折线 + 高峰段红色 markArea 高亮。
const props = defineProps({
  data: { type: Array, default: () => [] }
})

// 把连续 peak 段转成 markArea 区间。
function peakAreas(data) {
  const areas = []
  let i = 0
  while (i < data.length) {
    if (data[i].peak) {
      let j = i
      while (j < data.length && data[j].peak) j++
      areas.push([{ xAxis: data[i].time }, { xAxis: data[j - 1].time }])
      i = j
    } else {
      i++
    }
  }
  return areas
}

const option = computed(() => ({
  tooltip: {
    trigger: 'axis',
    backgroundColor: 'rgba(18,26,41,0.95)',
    borderColor: '#1B2740',
    textStyle: { color: '#E6ECF5', fontSize: 12 }
  },
  grid: { left: 44, right: 20, top: 24, bottom: 30 },
  xAxis: { ...categoryAxis(), type: 'category', data: props.data.map(d => d.time) },
  yAxis: { ...valueAxis(), type: 'value', name: 'kWh/h', nameTextStyle: { color: '#8A94A8' } },
  series: [
    {
      type: 'line',
      data: props.data.map(d => d.load),
      smooth: true,
      itemStyle: { color: '#A78BFA' },
      lineStyle: { width: 2.5, shadowBlur: 10, shadowColor: 'rgba(167,139,250,0.5)' },
      markArea: {
        silent: true,
        itemStyle: { color: 'rgba(239,68,68,0.14)' },
        label: { show: true, color: '#EF4444', fontSize: 11, formatter: '高峰' },
        data: peakAreas(props.data)
      }
    }
  ]
}))
</script>
