#pragma once

#include <QList>

#include "Page.h"

class QButtonGroup;
class QListWidget;
class QPushButton;
struct Order;

// 我的订单列表 (UC-U-10)。
class OrderListPage : public Page
{
    Q_OBJECT
public:
    explicit OrderListPage(QWidget *parent = nullptr);

signals:
    void orderClicked(const QString &orderNo);

private:
    // status: -1 全部;-2 进行中(0/1);2 已完成;3 已取消
    void setFilter(int status);
    void rebuildList();
    QWidget *buildCard(const Order &order);

    int m_filter = -1;
    QButtonGroup *m_group = nullptr;
    QList<QPushButton *> m_tabs;
    QListWidget *m_list = nullptr;
};
