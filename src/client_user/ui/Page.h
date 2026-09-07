#pragma once

#include <QWidget>

// 可被压入导航栈的页面基类:统一提供「返回」信号 + 标题栏。
class Page : public QWidget
{
    Q_OBJECT
public:
    explicit Page(QWidget *parent = nullptr) : QWidget(parent) {}

signals:
    void backRequested();

protected:
    // 构建标准标题栏(返回按钮 + 居中标题),返回的控件直接 addWidget 到页面顶部。
    QWidget *makeHeader(const QString &title);
};
