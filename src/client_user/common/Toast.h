#pragma once

#include <QString>

class QWidget;

// 轻量非模态提示:底部居中悬浮,淡入淡出自动消失。
namespace Toast {

void show(QWidget *parent, const QString &message, int durationMs = 2000);

} // namespace Toast
