<template>
  <div class="kpi-card" :style="{ '--accent': color }">
    <div class="kpi-top">
      <span class="kpi-dot"></span>
      <span class="kpi-label">{{ label }}</span>
    </div>
    <div class="kpi-value">{{ display }}</div>
    <div v-if="sub" class="kpi-sub">{{ sub }}</div>
  </div>
</template>

<script setup>
import { ref, computed, watch, onMounted } from 'vue'

const props = defineProps({
  label: { type: String, required: true },
  value: { type: [String, Number], default: '--' },
  sub: { type: String, default: '' },
  color: { type: String, default: '#4D9FFF' }
})

// 数字滚动动画:仅对纯数值做 count-up,其余(如 "12/30")直接显示
const animated = ref(props.value)
const display = computed(() => animated.value)

function isNumeric(v) {
  return typeof v === 'number' || (typeof v === 'string' && /^[\d.]+$/.test(v))
}

function animateTo(target) {
  if (!isNumeric(target)) {
    animated.value = target
    return
  }
  const end = parseFloat(target)
  const start = parseFloat(animated.value) || 0
  const dur = 900
  const t0 = performance.now()
  const decimals = String(target).includes('.') ? 2 : 0
  function step(now) {
    const p = Math.min((now - t0) / dur, 1)
    const eased = 1 - Math.pow(1 - p, 3) // ease-out cubic
    const val = start + (end - start) * eased
    animated.value = decimals
      ? val.toFixed(decimals)
      : Math.round(val).toLocaleString('zh-CN')
    if (p < 1) requestAnimationFrame(step)
  }
  requestAnimationFrame(step)
}

onMounted(() => animateTo(props.value))
watch(() => props.value, v => animateTo(v))
</script>

<style scoped>
.kpi-card {
  flex: 1;
  min-width: 0;
  position: relative;
  background: linear-gradient(160deg, rgba(20, 34, 58, 0.6) 0%, rgba(12, 22, 40, 0.5) 100%);
  backdrop-filter: blur(14px);
  -webkit-backdrop-filter: blur(14px);
  border: 1px solid rgba(99, 179, 255, 0.16);
  border-radius: 14px;
  display: flex;
  flex-direction: column;
  justify-content: center;
  padding: 0 20px;
  overflow: hidden;
  box-shadow: 0 6px 22px rgba(0, 0, 0, 0.3);
  transition: transform 0.3s, box-shadow 0.3s, border-color 0.3s;
}
.kpi-card::before {
  content: '';
  position: absolute;
  top: 0;
  left: 0;
  right: 0;
  height: 3px;
  background: linear-gradient(90deg, var(--accent), transparent);
  opacity: 0.9;
}
.kpi-card::after {
  content: '';
  position: absolute;
  right: -24px;
  top: -24px;
  width: 90px;
  height: 90px;
  border-radius: 50%;
  background: radial-gradient(circle, color-mix(in srgb, var(--accent) 22%, transparent) 0%, transparent 70%);
  pointer-events: none;
}
.kpi-card:hover {
  transform: translateY(-3px);
  border-color: color-mix(in srgb, var(--accent) 45%, transparent);
  box-shadow: 0 10px 30px rgba(0, 0, 0, 0.4), 0 0 22px color-mix(in srgb, var(--accent) 35%, transparent);
}

.kpi-top {
  display: flex;
  align-items: center;
  gap: 7px;
}
.kpi-dot {
  width: 7px;
  height: 7px;
  border-radius: 50%;
  background: var(--accent);
  box-shadow: 0 0 8px var(--accent);
}
.kpi-label {
  font-size: 14px;
  color: #8A94A8;
  letter-spacing: 1px;
  white-space: nowrap;
}
.kpi-value {
  font-size: 31px;
  font-weight: 700;
  margin: 8px 0 3px;
  color: var(--accent);
  font-variant-numeric: tabular-nums;
  white-space: nowrap;
  overflow: hidden;
  text-overflow: ellipsis;
  text-shadow: 0 0 16px color-mix(in srgb, var(--accent) 55%, transparent);
}
.kpi-sub {
  font-size: 12px;
  color: #55617A;
  letter-spacing: 0.5px;
}
</style>
