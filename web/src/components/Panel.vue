<template>
  <div class="panel" :style="{ flexGrow: grow }">
    <div class="panel-header">
      <span class="panel-bar"></span>
      <span class="panel-title">{{ title }}</span>
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
