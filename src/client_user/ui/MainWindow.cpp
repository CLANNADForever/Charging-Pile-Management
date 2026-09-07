#include "MainWindow.h"

#include <QHBoxLayout>
#include <QStackedWidget>
#include <QVBoxLayout>

#include "ChargeEntryPage.h"
#include "ChargePage.h"
#include "CouponPage.h"
#include "MapPage.h"
#include "NavButton.h"
#include "OrderDetailPage.h"
#include "OrderListPage.h"
#include "OrderSettlePage.h"
#include "SearchPage.h"
#include "StationDetailPage.h"
#include "StationListPage.h"
#include "UserCenterPage.h"
#include "common/Toast.h"
#include "theme/Theme.h"

MainWindow::MainWindow(QWidget *parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("root"));
    setWindowTitle(QStringLiteral("NCS · 充电"));
    setFixedSize(420, 760);

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // 页面栈:0 首页 / 1 充电 / 2 我的(根页),子页面动态压栈
    m_stack = new QStackedWidget(this);
    root->addWidget(m_stack, 1);

    m_stationList = new StationListPage(this);
    m_stack->addWidget(m_stationList);
    m_chargeEntry = new ChargeEntryPage(this);
    m_stack->addWidget(m_chargeEntry);
    m_userCenter = new UserCenterPage(this);
    m_stack->addWidget(m_userCenter);

    // 底部导航栏
    auto *navBar = new QWidget(this);
    navBar->setObjectName(QStringLiteral("navBar"));
    navBar->setFixedHeight(64);
    auto *nav = new QHBoxLayout(navBar);
    nav->setContentsMargins(0, 0, 0, 0);
    nav->setSpacing(0);

    struct NavItem
    {
        NavButton::Icon icon;
        QString label;
    };
    const NavItem items[] = {
        { NavButton::Home,    QStringLiteral("首页") },
        { NavButton::Charge,  QStringLiteral("充电") },
        { NavButton::Profile, QStringLiteral("我的") },
    };
    for (int i = 0; i < 3; ++i) {
        auto *btn = new NavButton(items[i].icon, items[i].label, navBar);
        connect(btn, &NavButton::clicked, this, [this, i]() { switchTab(i); });
        nav->addWidget(btn, 1);
        m_navs.append(btn);
    }
    root->addWidget(navBar, 0);

    // 用户中心信号接线
    connect(m_userCenter, &UserCenterPage::logoutRequested,
            this, &MainWindow::logoutRequested);
    connect(m_userCenter, &UserCenterPage::openCoupons, this, [this]() {
        pushPage(new CouponPage(this));
    });
    connect(m_userCenter, &UserCenterPage::openOrders, this, [this]() {
        auto *list = new OrderListPage(this);
        connect(list, &OrderListPage::orderClicked, this, [this](const QString &orderNo) {
            auto *detail = new OrderDetailPage(orderNo, this);
            connect(detail, &OrderDetailPage::settleRequested, this, [this](const QString &no) {
                auto *settle = new OrderSettlePage(no, this);
                connect(settle, &OrderSettlePage::doneRequested, this, [this]() {
                    switchTab(0);
                });
                pushPage(settle);
            });
            pushPage(detail);
        });
        pushPage(list);
    });

    // 主页面(电站列表)信号接线
    auto openStationDetail = [this](int stationId) {
        auto *detail = new StationDetailPage(stationId, this);
        connect(detail, &StationDetailPage::chargeRequested, this, [this](int id) {
            auto *charge = new ChargePage(id, this);
            connect(charge, &ChargePage::settleRequested, this, [this](const QString &orderNo) {
                auto *st = new OrderSettlePage(orderNo, this);
                connect(st, &OrderSettlePage::doneRequested, this, [this]() { switchTab(0); });
                pushPage(st);
            });
            pushPage(charge);
        });
        connect(detail, &StationDetailPage::navRequested, this, [this](int id) {
            pushPage(new MapPage(id, this));
        });
        pushPage(detail);
    };

    connect(m_stationList, &StationListPage::openSearch, this, [this, openStationDetail]() {
        auto *search = new SearchPage(this);
        connect(search, &SearchPage::openStation, this, [openStationDetail](int id) {
            openStationDetail(id);
        });
        pushPage(search);
    });
    connect(m_stationList, &StationListPage::openStation, this, [openStationDetail](int id) {
        openStationDetail(id);
    });
    connect(m_stationList, &StationListPage::openCoupons, this, [this]() {
        pushPage(new CouponPage(this));
    });
    connect(m_stationList, &StationListPage::openNavigation, this, [this](int id) {
        pushPage(new MapPage(id, this));
    });

    // 充电入口(底部「充电」tab)：去支付 / 去订单管理 / 去首页
    connect(m_chargeEntry, &ChargeEntryPage::openOrders, this, [this]() {
        auto *list = new OrderListPage(this);
        connect(list, &OrderListPage::orderClicked, this, [this](const QString &orderNo) {
            auto *detail = new OrderDetailPage(orderNo, this);
            connect(detail, &OrderDetailPage::settleRequested, this, [this](const QString &no) {
                auto *st = new OrderSettlePage(no, this);
                connect(st, &OrderSettlePage::doneRequested, this, [this]() { switchTab(0); });
                pushPage(st);
            });
            pushPage(detail);
        });
        pushPage(list);
    });
    connect(m_chargeEntry, &ChargeEntryPage::settleRequested, this, [this](const QString &orderNo) {
        auto *st = new OrderSettlePage(orderNo, this);
        connect(st, &OrderSettlePage::doneRequested, this, [this]() { switchTab(0); });
        pushPage(st);
    });
    connect(m_chargeEntry, &ChargeEntryPage::goHome, this, [this]() { switchTab(0); });

    switchTab(0);
}

void MainWindow::switchTab(int index)
{
    if (index < 0 || index >= m_navs.size())
        return;

    // 清空子页面
    while (m_stack->count() > kRootCount) {
        QWidget *w = m_stack->widget(m_stack->count() - 1);
        m_stack->removeWidget(w);
        w->deleteLater();
    }

    for (int i = 0; i < m_navs.size(); ++i)
        m_navs[i]->setActive(i == index);

    m_current = index;
    m_stack->setCurrentIndex(index);

    if (index == 0)
        m_stationList->refresh();
    if (index == 1)
        m_chargeEntry->refresh();
    if (index == 2)
        m_userCenter->refresh();
}

void MainWindow::pushPage(Page *page)
{
    connect(page, &Page::backRequested, this, &MainWindow::popPage);
    m_stack->addWidget(page);
    m_stack->setCurrentWidget(page);
}

void MainWindow::popPage()
{
    if (m_stack->count() <= kRootCount)
        return;
    QWidget *w = m_stack->currentWidget();
    m_stack->removeWidget(w);
    w->deleteLater();
    m_stack->setCurrentIndex(m_current);
}
