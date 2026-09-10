<template>
  <div class="gauge-wrap">
    <ChartBox :option="option" />
    <div class="gauge-value">{{ Math.round(health) }}%</div>
  </div>
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
      center: ['50%', '50%'],
      progress: { show: true, width: 16, itemStyle: { color: '#2DD4A7' } },
      axisLine: { lineStyle: { width: 16, color: [[1, '#1B2740']] } },
      axisTick: { show: false },
      splitLine: { show: false },
      axisLabel: { show: false },
      pointer: { show: false },
      anchor: { show: false },
      detail: {
        show: false
      },
      data: [{ value: props.health }]
    }
  ]
}))
</script>

<style scoped>
.gauge-wrap {
  position: relative;
  width: 100%;
  height: 100%;
}
.gauge-value {
  position: absolute;
  left: 50%;
  top: 50%;
  transform: translate(-50%, -50%);
  font-size: 28px;
  font-weight: bold;
  color: #E6ECF5;
  pointer-events: none;
  z-index: 2;
}
</style>
