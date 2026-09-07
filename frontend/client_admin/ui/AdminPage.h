#pragma once

#include <QWidget>

// 管理端页面基类:统一提供 refresh(),供顶栏「刷新」按钮调用。
// 各子页面继承并重写 refresh() 拉取最新数据。
class AdminPage : public QWidget
{
    Q_OBJECT
public:
    explicit AdminPage(QWidget *parent = nullptr) : QWidget(parent) {}

    virtual void refresh() {}
};
