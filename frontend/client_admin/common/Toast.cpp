#include "Toast.h"

#include <QFontMetrics>
#include <QLabel>
#include <QTimer>
#include <QWidget>

namespace Toast {

namespace {
constexpr int kBottomGap = 60;
constexpr int kHorizontalPadding = 48;
constexpr int kVerticalPadding = 24;
}

// 轻量非模态提示:底部居中悬浮,定时自动消失。
// 不使用 QGraphicsOpacityEffect,避免离屏渲染在切换页面时产生重影。
void show(QWidget *parent, const QString &message, int durationMs)
{
    if (!parent)
        return;

    auto *toast = new QLabel(message, parent);
    toast->setObjectName(QStringLiteral("toast"));
    toast->setAlignment(Qt::AlignCenter);
    toast->setWordWrap(false);
    toast->setAttribute(Qt::WA_TransparentForMouseEvents);
    toast->setAttribute(Qt::WA_DeleteOnClose);

    const QFontMetrics fm(toast->font());
    const int w = fm.horizontalAdvance(message) + kHorizontalPadding;
    const int h = fm.height() + kVerticalPadding;
    toast->setFixedSize(w, h);
    toast->move((parent->width() - w) / 2, parent->height() - h - kBottomGap);

    toast->show();
    QTimer::singleShot(durationMs, toast, &QWidget::close);
}

} // namespace Toast
