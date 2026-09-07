<template>
  <div ref="el" class="chart-box"></div>
</template>

<script setup>
import { ref, onMounted, onBeforeUnmount, watch, nextTick } from 'vue'
import * as echarts from 'echarts'

// ECharts 通用容器:负责 init / setOption / resize / dispose。
// 用 nextTick 确保父级 flex 布局完成、容器有高度后再 init;
// 用 ResizeObserver 监听容器尺寸变化(避免嵌套 flex 下 init 时高度为 0 导致空白)。
const props = defineProps({
  option: { type: Object, default: null }
})

const el = ref(null)
let chart = null
let observer = null

function render() {
  if (chart && props.option) {
    chart.setOption(props.option, true)
  }
}

function resize() {
  if (chart) chart.resize()
}

onMounted(() => {
  nextTick(() => {
    chart = echarts.init(el.value)
    render()
    if (window.ResizeObserver) {
      observer = new ResizeObserver(() => resize())
      observer.observe(el.value)
    }
  })
  window.addEventListener('resize', resize)
})

onBeforeUnmount(() => {
  window.removeEventListener('resize', resize)
  if (observer) {
    observer.disconnect()
    observer = null
  }
  if (chart) {
    chart.dispose()
    chart = null
  }
})

watch(() => props.option, () => render(), { deep: true })
</script>

<style scoped>
.chart-box {
  width: 100%;
  height: 100%;
  min-height: 0;
}
</style>
