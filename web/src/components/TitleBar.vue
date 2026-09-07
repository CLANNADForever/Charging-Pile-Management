<template>
  <header class="titlebar">
    <div class="titlebar-left">
      <div class="logo">
        <img :src="logoUrl" alt="NCS" />
      </div>
      <div>
        <div class="main-title">NCS 充电管理平台</div>
        <div class="sub-title">大数据可视化分析大屏</div>
      </div>
    </div>
    <div class="titlebar-right">
      <div class="clock">{{ time }}</div>
      <div class="data-through">数据截止:{{ dataThrough }}</div>
    </div>
  </header>
</template>

<script setup>
import { ref, computed, onMounted, onBeforeUnmount } from 'vue'
import logoUrl from '../assets/logo.png'

const props = defineProps({
  dataThrough: { type: String, default: '--' }
})

const now = ref(new Date())
let timer = null

const time = computed(() => {
  const d = now.value
  const p = n => String(n).padStart(2, '0')
  return `${d.getFullYear()}-${p(d.getMonth() + 1)}-${p(d.getDate())} ` +
         `${p(d.getHours())}:${p(d.getMinutes())}:${p(d.getSeconds())}`
})

onMounted(() => {
  timer = setInterval(() => { now.value = new Date() }, 1000)
})

onBeforeUnmount(() => {
  if (timer) clearInterval(timer)
})
</script>
