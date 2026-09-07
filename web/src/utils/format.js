// 数字/金额格式化工具。

export function formatMoney(v) {
  if (v === null || v === undefined) return '--'
  return Number(v).toLocaleString('zh-CN', {
    minimumFractionDigits: 2,
    maximumFractionDigits: 2
  })
}

export function formatNumber(v) {
  if (v === null || v === undefined) return '--'
  return Number(v).toLocaleString('zh-CN')
}
