#pragma once

#include <QColor>
#include <QLinearGradient>
#include <QString>

class QApplication;
class QWidget;

// 全局主题:品牌色板 + QSS + 品牌渐变工具 + 通用动画。
// 色板源自 Logo「深蓝 → 电光蓝 → 青 → 绿」的流动能量环,已柔化降饱和。
namespace Theme {

inline const QColor DeepNavy      { 0x0A, 0x11, 0x28 }; // 深蓝(标题/压暗)
inline const QColor ElectricBlue  { 0x5E, 0xA8, 0xFF }; // 柔和电光蓝(主色)
inline const QColor Cyan          { 0x3E, 0xC9, 0xC0 }; // 柔和青(渐变过渡)
inline const QColor BrandGreen    { 0x45, 0xD0, 0x94 }; // 柔和品牌绿(新能源)
inline const QColor Success       { 0x34, 0xC7, 0x6B }; // 柔和成功
inline const QColor Error         { 0xF0, 0x6A, 0x6A }; // 柔和错误
inline const QColor Warning       { 0xF5, 0xA6, 0x2E }; // 柔和警示
inline const QColor BgLight       { 0xF2, 0xF6, 0xFB }; // 页面底色(柔和)
inline const QColor CardWhite     { 0xFD, 0xFE, 0xFF }; // 暖白卡片
inline const QColor TextPrimary   { 0x2A, 0x32, 0x40 }; // 正文(柔和深灰)
inline const QColor TextSecondary { 0x8B, 0x93, 0xA1 }; // 次要文字
inline const QColor Divider       { 0xEE, 0xF1, 0xF6 }; // 分割线

// 全局 QSS 样式表
QString qss();

// 应用主题:设置 Fusion 基础风格 + QSS
void apply(QApplication &app);

// 品牌渐变(默认水平):柔和电光蓝 → 青 → 品牌绿
QLinearGradient brandGradient(Qt::Orientation orientation = Qt::Horizontal);

// 页面/控件淡入动画(220ms,ease-out);动画结束自动移除离屏特效,避免切页重影。
void fadeIn(QWidget *w);

} // namespace Theme
