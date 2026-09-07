#include "OrderListPage.h"

#include <QButtonGroup>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>

#include "common/Utils.h"
#include "core/service/ChargeService.h"
#include "theme/Theme.h"

OrderListPage::OrderListPage(QWidget *parent)
    : Page(parent)
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    root->addWidget(makeHeader(QStringLiteral("我的订单")));

    // 分类 tab
    auto *tabBar = new QHBoxLayout;
    tabBar->setContentsMargins(16, 8, 16, 8);
    tabBar->setSpacing(8);

    m_group = new QButtonGroup(this);
    m_group->setExclusive(true);

    struct Tab { QString text; int status; };
    const Tab tabs[] = {
        { QStringLiteral("全部"),   -1 },
        { QStringLiteral("进行中"), -2 },
        { QStringLiteral("已完成"), 2 },
        { QStringLiteral("已取消"), 3 },
    };
    for (int i = 0; i < 4; ++i) {
        auto *b = new QPushButton(tabs[i].text, this);
        b->setObjectName(QStringLiteral("tabButton"));
        b->setCheckable(true);
        b->setCursor(Qt::PointingHandCursor);
        m_group->addButton(b);
        m_tabs.append(b);
        const int status = tabs[i].status;
        connect(b, &QPushButton::clicked, this, [this, status]() { setFilter(status); });
        tabBar->addWidget(b, 1);
    }
    root->addLayout(tabBar);

    m_list = new QListWidget(this);
    m_list->setFrameShape(QFrame::NoFrame);
    m_list->setSelectionMode(QAbstractItemView::NoSelection);
    m_list->setFocusPolicy(Qt::NoFocus);
    m_list->setSpacing(10);
    m_list->setContentsMargins(16, 0, 16, 16);
    root->addWidget(m_list, 1);

    m_tabs[0]->setChecked(true);
    rebuildList();
}

void OrderListPage::setFilter(int status)
{
    m_filter = status;
    rebuildList();
}

void OrderListPage::rebuildList()
{
    m_list->clear();

    QList<Order> orders;
    if (m_filter == -2) {
        const QList<Order> all = ChargeService::instance().listOrders(-1);
        for (const Order &o : all) {
            if (o.status == 0 || o.status == 1)
                orders.append(o);
        }
    } else {
        orders = ChargeService::instance().listOrders(m_filter);
    }

    for (const Order &o : orders) {
        QWidget *card = buildCard(o);
        auto *item = new QListWidgetItem(m_list);
        item->setSizeHint(card->sizeHint());
        m_list->addItem(item);
        m_list->setItemWidget(item, card);
    }
}

QWidget *OrderListPage::buildCard(const Order &o)
{
    auto *card = new QFrame;
    card->setObjectName(QStringLiteral("card"));
    auto *lay = new QVBoxLayout(card);
    lay->setContentsMargins(16, 14, 16, 14);
    lay->setSpacing(6);

    auto *top = new QHBoxLayout;
    auto *no = new QLabel(o.orderNo, card);
    no->setObjectName(QStringLiteral("hintLabel"));
    const bool pending = (o.status == 2 && !o.paid);
    auto *status = new QLabel(pending ? QStringLiteral("待支付")
                                      : Utils::orderStatusText(o.status), card);
    const QColor sc = pending ? Theme::Warning : Utils::orderStatusColor(o.status);
    status->setStyleSheet(QStringLiteral("color:%1; font-size:13px; font-weight:bold;")
                              .arg(sc.name()));
    top->addWidget(no);
    top->addStretch();
    top->addWidget(status);
    lay->addLayout(top);

    auto *station = new QLabel(o.stationName, card);
    station->setObjectName(QStringLiteral("sectionTitle"));
    lay->addWidget(station);

    auto *meta = new QLabel(
        QStringLiteral("开始 %1 · 时长 %2 · %3 度 · ¥%4")
            .arg(o.startTime)
            .arg(Utils::formatDuration(o.durationSec))
            .arg(QString::number(o.energy, 'f', 1))
            .arg(Utils::formatMoney(o.amount)),
        card);
    meta->setObjectName(QStringLiteral("hintLabel"));
    lay->addWidget(meta);

    auto *detailBtn = new QPushButton(QStringLiteral("查看详情 ›"), card);
    detailBtn->setObjectName(QStringLiteral("textButton"));
    detailBtn->setCursor(Qt::PointingHandCursor);
    connect(detailBtn, &QPushButton::clicked, this, [this, orderNo = o.orderNo]() {
        emit orderClicked(orderNo);
    });
    lay->addWidget(detailBtn, 0, Qt::AlignRight);

    return card;
}
