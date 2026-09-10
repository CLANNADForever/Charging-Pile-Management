#include "MainWindow.h"

#include <QHBoxLayout>
#include <QIcon>
#include <QPainter>
#include <QPixmap>
#include <QPushButton>
#include <QStackedWidget>
#include <QVBoxLayout>

#include "ChargePage.h"
#include "CouponPage.h"
#include "MapPage.h"
#include "NavButton.h"
#include "OrderDetailPage.h"
#include "OrderListPage.h"
#include "OrderSettlePage.h"
#include "ScanPage.h"
#include "SearchPage.h"
#include "StationDetailPage.h"
#include "StationListPage.h"
#include "UserCenterPage.h"
#include "common/Toast.h"
#include "theme/Theme.h"

namespace {
// 生成二维码图标(三个定位角 + 点阵)
QIcon makeQrIcon()
{
    QPixmap pm(48, 48);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(0x2A, 0x32, 0x40));
    p.drawRect(2, 2, 14, 14);
    p.drawRect(32, 2, 14, 14);
    p.drawRect(2, 32, 14, 14);
    p.setBrush(Qt::white);
    p.drawRect(6, 6, 6, 6);
    p.drawRect(36, 6, 6, 6);
    p.drawRect(6, 36, 6, 6);
    p.setBrush(QColor(0x2A, 0x32, 0x40));
    for (int r = 0; r < 5; ++r)
        for (int c = 0; c < 5; ++c)
            if ((r * 7 + c * 3) % 2 == 0)
                p.drawRect(20 + c * 4, 20 + r * 4, 2, 2);
    return QIcon(pm);
}
} // namespace

MainWindow::MainWindow(QWidget *parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("root"));
    setWindowTitle(QStringLiteral("熠熠ee · 充电"));
    setFixedSize(420, 760);

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // 页面栈:0 首页 / 1 充电 / 2 我的(根页),子页面动态压栈
    m_stack = new QStackedWidget(this);
    root->addWidget(m_stack, 1);

    m_stationList = new StationListPage(this);
    m_stack->addWidget(m_stationList);
    m_userCenter = new UserCenterPage(this);
    m_stack->addWidget(m_userCenter);

    // 底部导航栏
    auto *navBar = new QWidget(this);
    navBar->setObjectName(QStringLiteral("navBar"));
    navBar->setFixedHeight(64);
    auto *nav = new QHBoxLayout(navBar);
    nav->setContentsMargins(0, 0, 0, 0);
    nav->setSpacing(0);

    auto *homeBtn = new NavButton(NavButton::Home, QStringLiteral("首页"), navBar);
    connect(homeBtn, &NavButton::clicked, this, [this]() { switchTab(0); });
    nav->addWidget(homeBtn, 1);
    m_navs.append(homeBtn);

    nav->addSpacing(120);  // 中间给凸起扫码按钮留位

    auto *profileBtn = new NavButton(NavButton::Profile, QStringLiteral("我的"), navBar);
    connect(profileBtn, &NavButton::clicked, this, [this]() { switchTab(1); });
    nav->addWidget(profileBtn, 1);
    m_navs.append(profileBtn);
    root->addWidget(navBar, 0);

    // 中间凸起「扫码充电」圆形按钮:半嵌入底部导航,与底栏平滑过渡
    auto *scanBtn = new QPushButton(this);
    scanBtn->setObjectName(QStringLiteral("scanChargeButton"));
    scanBtn->setIcon(QIcon(makeQrIcon()));
    scanBtn->setIconSize(QSize(34, 34));
    scanBtn->setFixedSize(68, 68);
    scanBtn->setCursor(Qt::PointingHandCursor);
    connect(scanBtn, &QPushButton::clicked, this, [this]() {
        auto *scan = new ScanPage(this);
        connect(scan, &ScanPage::scanned, this, [this](const QString &text) {
            bool ok = false;
            const int stationId = text.trimmed().toInt(&ok);
            if (ok && stationId > 0) {
                popPage();  // 关闭扫码页
                openChargePage(stationId);
            } else {
                Toast::show(this, QStringLiteral("二维码无效:") + text);
            }
        });
        pushPage(scan);
    });
    scanBtn->move((420 - scanBtn->width()) / 2, 760 - 64 - 14);
    scanBtn->raise();

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
        connect(detail, &StationDetailPage::chargeRequested, this, &MainWindow::openChargePage);
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
    Theme::fadeIn(m_stack->currentWidget());

    if (index == 0)
        m_stationList->refresh();
    if (index == 1)
        m_userCenter->refresh();
}

void MainWindow::pushPage(Page *page)
{
    connect(page, &Page::backRequested, this, &MainWindow::popPage);
    m_stack->addWidget(page);
    m_stack->setCurrentWidget(page);
    Theme::fadeIn(page);
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

void MainWindow::openChargePage(int stationId)
{
    auto *charge = new ChargePage(stationId, this);
    connect(charge, &ChargePage::settleRequested, this, [this](const QString &orderNo) {
        auto *st = new OrderSettlePage(orderNo, this);
        connect(st, &OrderSettlePage::doneRequested, this, [this]() { switchTab(0); });
        pushPage(st);
    });
    connect(charge, &ChargePage::openOrderDetail, this, [this](const QString &orderNo) {
        popPage();  // 移除选桩页
        auto *od = new OrderDetailPage(orderNo, this);
        connect(od, &OrderDetailPage::settleRequested, this, [this](const QString &no) {
            auto *st = new OrderSettlePage(no, this);
            connect(st, &OrderSettlePage::doneRequested, this, [this]() { switchTab(0); });
            pushPage(st);
        });
        pushPage(od);
    });
    pushPage(charge);
}
