// ===== ECharts 深色通用配置(全端复用) =====

// 图表系列色盘(多序列依次取用)
export const palette = ['#4D9FFF', '#22D3EE', '#2DD4A7', '#A78BFA', '#F59E0B', '#EF4444']

// 状态语义色
export const statusColor = {
  idle: '#2DD4A7',   // 空闲 / 正常 / 正增长
  using: '#F59E0B',  // 使用中 / 预警
  fault: '#EF4444',  // 故障 / 危险
  peak: '#EF4444'    // 高峰
}

// 深色悬浮提示
export function tooltip(trigger = 'axis') {
  return {
    trigger,
    backgroundColor: 'rgba(18,26,41,0.95)',
    borderColor: '#1B2740',
    textStyle: { color: '#E6ECF5', fontSize: 12 },
    axisPointer: { lineStyle: { color: '#4D9FFF' } }
  }
}

// 数值轴(透明轴 + 虚线网格)
export function valueAxis() {
  return {
    axisLine: { show: false },
    axisTick: { show: false },
    axisLabel: { color: '#8A94A8', fontSize: 12 },
    splitLine: { lineStyle: { color: '#1B2740', type: 'dashed' } }
  }
}

// 类目轴
export function categoryAxis() {
  return {
    axisLine: { lineStyle: { color: '#1B2740' } },
    axisTick: { show: false },
    axisLabel: { color: '#8A94A8', fontSize: 12 }
  }
}
