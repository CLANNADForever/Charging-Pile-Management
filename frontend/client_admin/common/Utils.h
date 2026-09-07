#pragma once

#include <QColor>
#include <QString>

// 管理端通用格式化/状态文案工具。
namespace Utils {

// 手机号中间 4 位打码:13812345678 -> 138****5678 (NFR-S-02)
QString maskPhone(const QString &phone);

// 金额格式化:保留两位小数
QString formatMoney(double amount);

// 电桩状态:0 空闲 1 使用中 2 故障
QString chargerStatusText(int status);
QColor chargerStatusColor(int status);

// 订单状态:0 预约 1 充电中 2 已完成 3 已取消
QString orderStatusText(int status);
QColor orderStatusColor(int status);

} // namespace Utils
