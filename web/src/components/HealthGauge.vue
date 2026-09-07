<template>
  <ChartBox :option="option" />
</template>

<script setup>
import { computed } from 'vue'
import ChartBox from './ChartBox.vue'

// 设备健康度仪表盘:(空闲+使用中)/总数 ×100%。
const props = defineProps({
  health: { type: Number, default: 0 }
})

const option = computed(() => ({
  series: [
    {
      type: 'gauge',
      min: 0,
      max: 100,
      startAngle: 200,
      endAngle: -20,
      radius: '95%',
      center: ['50%', '58%'],
      progress: { show: true, width: 12, itemStyle: { color: '#2DD4A7' } },
      axisLine: { lineStyle: { width: 12, color: [[1, '#1B2740']] } },
      axisTick: { show: false },
      splitLine: { show: false },
      axisLabel: { show: false },
      pointer: { show: false },
      anchor: { show: false },
      detail: {
        valueAnimation: true,
        formatter: '{value}%',
        fontSize: 26,
        fontWeight: 'bold',
        color: '#E6ECF5',
        offsetCenter: [0, 0]
      },
      data: [{ value: props.health }]
    }
  ]
}))
</script>
