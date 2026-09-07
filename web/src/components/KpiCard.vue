<template>
  <div class="kpi-card">
    <div class="kpi-label">{{ label }}</div>
    <div class="kpi-value" :style="{ color }">{{ value }}</div>
    <div v-if="sub" class="kpi-sub">{{ sub }}</div>
  </div>
</template>

<script setup>
defineProps({
  label: { type: String, required: true },
  value: { type: [String, Number], default: '--' },
  sub: { type: String, default: '' },
  color: { type: String, default: '#4D9FFF' }
})
</script>

<style scoped>
.kpi-card {
  flex: 1;
  min-width: 0;
  background: #121A29;
  border: 1px solid #1B2740;
  border-radius: 12px;
  display: flex;
  flex-direction: column;
  justify-content: center;
  padding: 0 20px;
  transition: background 0.3s;
}
.kpi-card:hover {
  background: #1B2740;
}
.kpi-label {
  font-size: 14px;
  color: #8A94A8;
  white-space: nowrap;
}
.kpi-value {
  font-size: 30px;
  font-weight: bold;
  margin: 6px 0 2px;
  font-variant-numeric: tabular-nums;
  white-space: nowrap;
  overflow: hidden;
  text-overflow: ellipsis;
}
.kpi-sub {
  font-size: 12px;
  color: #5A6478;
}
</style>
