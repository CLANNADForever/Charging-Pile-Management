#pragma once

#include <QColor>
#include <QString>

// 通用格式化/脱敏工具。
namespace Utils {

// 手机号中间 4 位打码:13812345678 -> 138****5678 (NFR-S-02)
QString maskPhone(const QString &phone);

// 金额格式化:保留两位小数
QString formatMoney(double amount);

// 时长格式化:秒 -> HH:MM:SS
QString formatDuration(int totalSeconds);

// 订单状态文案:0 预约 1 充电中 2 已完成 3 已取消
QString orderStatusText(int status);

// 订单状态颜色
QColor orderStatusColor(int status);

} // namespace Utils
