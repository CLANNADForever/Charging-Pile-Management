#pragma once

#include <QColor>
#include <QLinearGradient>
#include <QString>

class QApplication;
class QChart;

// 管理端全局主题:深色(非纯黑)+ 品牌蓝青渐变。
// 与用户端 Theme(浅色)相互独立,两个可执行程序各用各的 QSS。
namespace AdminTheme {

inline const QColor BgDeep    { 0x0B, 0x12, 0x20 }; // 主背景(深夜蓝)
inline const QColor BgSidebar { 0x0E, 0x16, 0x28 }; // 侧边栏
inline const QColor Card      { 0x15, 0x1E, 0x30 }; // 卡片
inline const QColor CardHover { 0x1B, 0x27, 0x40 }; // 卡片悬浮
inline const QColor Field     { 0x0F, 0x17, 0x26 }; // 输入框
inline const QColor Border    { 0x23, 0x2E, 0x44 }; // 边框/分隔线
inline const QColor Blue      { 0x4D, 0x9F, 0xFF }; // 品牌蓝
inline const QColor Cyan      { 0x22, 0xD3, 0xEE }; // 品牌青
inline const QColor Green     { 0x2D, 0xD4, 0xA7 }; // 成功/空闲
inline const QColor Amber     { 0xF5, 0x9E, 0x0B }; // 警示/使用中
inline const QColor Red       { 0xEF, 0x44, 0x44 }; // 危险/故障
inline const QColor Violet    { 0xA7, 0x8B, 0xFA }; // 图表辅助
inline const QColor TextMain  { 0xE6, 0xEC, 0xF5 }; // 正文
inline const QColor TextSub   { 0x8A, 0x94, 0xA8 }; // 次要
inline const QColor TextMute  { 0x5A, 0x64, 0x78 }; // 弱化

// 全局 QSS 样式表
QString qss();

// 应用主题:设置 Fusion 基础风格 + QSS
void apply(QApplication &app);

// 品牌渐变(默认水平):品牌蓝 → 品牌青
QLinearGradient brandGradient(Qt::Orientation orientation = Qt::Horizontal);

// 将 QChart 适配为深色风格:透明背景、图例/坐标轴/网格配色。
void styleChart(QChart *chart);

} // namespace AdminTheme
