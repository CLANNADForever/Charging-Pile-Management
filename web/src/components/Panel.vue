<template>
  <div class="panel" :style="{ flexGrow: grow }">
    <!-- 科技 HUD 四角装饰 -->
    <i class="corner tl"></i>
    <i class="corner tr"></i>
    <i class="corner bl"></i>
    <i class="corner br"></i>

    <div class="panel-header">
      <span class="panel-bar"></span>
      <span class="panel-title">{{ title }}</span>
      <span class="header-line"></span>
      <span v-if="unit" class="panel-unit">{{ unit }}</span>
    </div>
    <div class="panel-body">
      <slot />
    </div>
  </div>
</template>

<script setup>
defineProps({
  title: { type: String, required: true },
  unit: { type: String, default: '' },
  grow: { type: Number, default: 1 }
})
</script>

<style scoped>
.panel-header {
  position: relative;
}
.header-line {
  flex: 1;
  height: 1px;
  margin-left: 12px;
  background: linear-gradient(90deg, rgba(0, 229, 255, 0.25), transparent);
}

/* 四角 L 形光角 */
.corner {
  position: absolute;
  width: 14px;
  height: 14px;
  pointer-events: none;
  opacity: 0.85;
}
.corner.tl { top: -1px; left: -1px; border-top: 2px solid var(--cyan); border-left: 2px solid var(--cyan); border-top-left-radius: 6px; }
.corner.tr { top: -1px; right: -1px; border-top: 2px solid var(--cyan); border-right: 2px solid var(--cyan); border-top-right-radius: 6px; }
.corner.bl { bottom: -1px; left: -1px; border-bottom: 2px solid var(--cyan); border-left: 2px solid var(--cyan); border-bottom-left-radius: 6px; }
.corner.br { bottom: -1px; right: -1px; border-bottom: 2px solid var(--cyan); border-right: 2px solid var(--cyan); border-bottom-right-radius: 6px; }
</style>
