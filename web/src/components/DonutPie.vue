<template>
  <ChartBox :option="option" />
</template>

<script setup>
import { computed } from 'vue'
import ChartBox from './ChartBox.vue'

// 通用环形图:data = [{ name, value, color }],中心显示主/副文字。
const props = defineProps({
  data: { type: Array, default: () => [] },
  centerLabel: { type: String, default: '--' },
  centerSub: { type: String, default: '' }
})

const option = computed(() => {
  const hasData = props.data.length > 0
  return {
    tooltip: {
      trigger: 'item',
      backgroundColor: 'rgba(18,26,41,0.95)',
      borderColor: '#1B2740',
      textStyle: { color: '#E6ECF5', fontSize: 12 },
      formatter: '{b}: {c} ({d}%)'
    },
    legend: {
      bottom: 0,
      icon: 'circle',
      textStyle: { color: '#8A94A8', fontSize: 12 },
      itemWidth: 10,
      itemHeight: 10
    },
    series: [
      {
        type: 'pie',
        radius: ['52%', '72%'],
        center: ['50%', '44%'],
        avoidLabelOverlap: true,
        itemStyle: { borderColor: '#121A29', borderWidth: 2 },
        label: { show: false },
        emphasis: { label: { show: true, fontSize: 16, fontWeight: 'bold', color: '#E6ECF5' } },
        data: hasData
          ? props.data.map(d => ({ name: d.name, value: d.value, itemStyle: { color: d.color } }))
          : []
      }
    ],
    graphic: [
      {
        type: 'text',
        left: 'center',
        top: '36%',
        style: {
          text: hasData ? props.centerLabel : '暂无数据',
          fontSize: hasData ? 22 : 14,
          fontWeight: 'bold',
          fill: hasData ? '#E6ECF5' : '#8A94A8',
          textAlign: 'center'
        }
      },
      {
        type: 'text',
        left: 'center',
        top: '48%',
        style: {
          text: props.centerSub,
          fontSize: 12,
          fill: '#8A94A8',
          textAlign: 'center'
        }
      }
    ]
  }
})
</script>
