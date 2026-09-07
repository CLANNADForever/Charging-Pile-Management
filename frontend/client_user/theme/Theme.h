#pragma once

#include <QColor>
#include <QLinearGradient>
#include <QString>

class QApplication;

// 全局主题:品牌色板 + QSS + 品牌渐变工具。
// 色板源自 Logo「深蓝 → 电光蓝 → 青 → 绿」的流动能量环。
namespace Theme {

inline const QColor DeepNavy      { 0x0A, 0x11, 0x28 }; // 深蓝(标题/压暗)
inline const QColor ElectricBlue  { 0x2F, 0x80, 0xFF }; // 电光蓝(主色)
inline const QColor Cyan          { 0x00, 0xD4, 0xFF }; // 青(渐变过渡)
inline const QColor BrandGreen    { 0x00, 0xB3, 0x68 }; // 品牌绿(新能源)
inline const QColor Success       { 0x22, 0xC5, 0x5E }; // 成功
inline const QColor Error         { 0xEF, 0x44, 0x44 }; // 错误/危险
inline const QColor Warning       { 0xF5, 0x9E, 0x0B }; // 警示
inline const QColor BgLight       { 0xF5, 0xF6, 0xF8 }; // 页面底色
inline const QColor CardWhite     { 0xFF, 0xFF, 0xFF }; // 卡片白
inline const QColor TextPrimary   { 0x1A, 0x1D, 0x26 }; // 正文
inline const QColor TextSecondary { 0x8A, 0x8F, 0x99 }; // 次要文字
inline const QColor Divider       { 0xE9, 0xEB, 0xF0 }; // 分割线

// 全局 QSS 样式表
QString qss();

// 应用主题:设置 Fusion 基础风格 + QSS
void apply(QApplication &app);

// 品牌渐变(默认水平):电光蓝 → 青 → 品牌绿
QLinearGradient brandGradient(Qt::Orientation orientation = Qt::Horizontal);

} // namespace Theme
