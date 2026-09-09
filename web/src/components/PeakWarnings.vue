<template>
  <div class="warnings">
    <div v-if="!items.length" class="no-warn">近期无高峰预警</div>
    <div v-else class="warn-list">
      <div v-for="(w, i) in items" :key="i" class="warn-item">
        <span class="dot"></span>
        <span>{{ w.time }} 起进入高峰,预计负荷 {{ w.load }} kWh/h,影响 {{ w.stations }} 站</span>
      </div>
    </div>
  </div>
</template>

<script setup>
defineProps({
  items: { type: Array, default: () => [] }
})
</script>

<style scoped>
.warnings {
  flex: 1;
  min-height: 0;
  display: flex;
  flex-direction: column;
  justify-content: center;
  gap: 10px;
  padding: 0 4px;
  overflow: hidden;
}
.no-warn {
  color: #8A94A8;
  font-size: 13px;
  text-align: center;
}
.warn-list {
  display: flex;
  flex-direction: column;
  gap: 10px;
}
.warn-item {
  display: flex;
  align-items: center;
  gap: 8px;
  font-size: 12px;
  color: #E6ECF5;
  line-height: 1.5;
}
.dot {
  width: 8px;
  height: 8px;
  border-radius: 50%;
  background: #EF4444;
  flex-shrink: 0;
  animation: blink 1.5s infinite;
}
@keyframes blink {
  0%, 100% { opacity: 1; }
  50% { opacity: 0.3; }
}
</style>
