#include "AdminMainWindow.h"

#include <QDateTime>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QStackedWidget>
#include <QStatusBar>
#include <QTimer>
#include <QVBoxLayout>

#include "common/Toast.h"
#include "core/service/AdminService.h"
#include "core/service/StationService.h"
#include "ui/AdminPage.h"
#include "ui/ChargerPage.h"
#include "ui/ChargerStatusPage.h"
#include "ui/PredictPage.h"
#include "ui/RevenuePage.h"
#include "ui/StationPage.h"
#include "ui/UserPage.h"

AdminMainWindow::AdminMainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle(QStringLiteral("熠熠ee 运营管理端"));
    resize(1440, 900);
    setMinimumSize(1280, 800);

    auto *root = new QWidget;
    root->setObjectName(QStringLiteral("appRoot"));
    auto *h = new QHBoxLayout(root);
    h->setContentsMargins(0, 0, 0, 0);
    h->setSpacing(0);

    h->addWidget(buildSidebar());

    // 右侧:顶栏 + 内容区
    auto *right = new QWidget;
    auto *rv = new QVBoxLayout(right);
    rv->setContentsMargins(0, 0, 0, 0);
    rv->setSpacing(0);
    rv->addWidget(buildTopBar());

    m_pageStack = new QStackedWidget;
    m_pageStack->addWidget(new RevenuePage);       // 0 营收分析
    m_pageStack->addWidget(new ChargerStatusPage); // 1 电桩状态
    m_pageStack->addWidget(new ChargerPage);       // 2 充电桩管理
    m_pageStack->addWidget(new StationPage);       // 3 充电站管理
    m_pageStack->addWidget(new UserPage);          // 4 用户管理
    m_pageStack->addWidget(new PredictPage);       // 5 智能预测
    rv->addWidget(m_pageStack, 1);

    h->addWidget(right, 1);

    setCentralWidget(root);
    buildStatusBar();

    connect(m_navList, &QListWidget::currentRowChanged, this, [this](int row) {
        if (row >= 0 && row < m_pageStack->count())
            m_pageStack->setCurrentIndex(row);
    });
    m_navList->setCurrentRow(0);

    auto *timer = new QTimer(this);
    timer->setInterval(1000);
    connect(timer, &QTimer::timeout, this, &AdminMainWindow::refreshStatusBar);
    timer->start();
}

QWidget *AdminMainWindow::buildSidebar()
{
    auto *bar = new QWidget;
    bar->setObjectName(QStringLiteral("sideBar"));
    bar->setFixedWidth(220);
    auto *lay = new QVBoxLayout(bar);
    lay->setContentsMargins(0, 24, 0, 16);
    lay->setSpacing(0);

    auto *brand = new QLabel(QStringLiteral("熠熠ee 运营管理端"), bar);
    brand->setObjectName(QStringLiteral("sideBrand"));
    brand->setAlignment(Qt::AlignCenter);
    auto *sub = new QLabel(QStringLiteral("充电桩管理平台"), bar);
    sub->setObjectName(QStringLiteral("sideSub"));
    sub->setAlignment(Qt::AlignCenter);
    lay->addWidget(brand);
    lay->addWidget(sub);
    lay->addSpacing(24);

    m_navList = new QListWidget(bar);
    m_navList->setObjectName(QStringLiteral("navList"));
    m_navList->setFocusPolicy(Qt::NoFocus);
    m_navList->addItems({
        QStringLiteral("营收分析"),
        QStringLiteral("电桩状态"),
        QStringLiteral("充电桩管理"),
        QStringLiteral("充电站管理"),
        QStringLiteral("用户管理"),
        QStringLiteral("智能预测"),
    });
    lay->addWidget(m_navList, 1);

    auto *logout = new QPushButton(QStringLiteral("退出登录"), bar);
    logout->setObjectName(QStringLiteral("secondaryButton"));
    logout->setCursor(Qt::PointingHandCursor);
    connect(logout, &QPushButton::clicked, this, &AdminMainWindow::confirmLogout);
    lay->addWidget(logout);

    return bar;
}

QWidget *AdminMainWindow::buildTopBar()
{
    auto *bar = new QWidget;
    bar->setObjectName(QStringLiteral("topBar"));
    bar->setFixedHeight(56);
    auto *lay = new QHBoxLayout(bar);
    lay->setContentsMargins(24, 0, 24, 0);
    lay->setSpacing(12);

    auto *admin = new QLabel(
        QStringLiteral("当前账号:%1").arg(AdminService::instance().currentAccount()), bar);
    admin->setObjectName(QStringLiteral("adminLabel"));
    lay->addWidget(admin);
    lay->addStretch();

    auto *refresh = new QPushButton(QStringLiteral("刷新"), bar);
    refresh->setObjectName(QStringLiteral("secondaryButton"));
    refresh->setCursor(Qt::PointingHandCursor);
    connect(refresh, &QPushButton::clicked, this, &AdminMainWindow::refreshCurrentPage);
    lay->addWidget(refresh);

    auto *logout = new QPushButton(QStringLiteral("退出登录"), bar);
    logout->setObjectName(QStringLiteral("secondaryButton"));
    logout->setCursor(Qt::PointingHandCursor);
    connect(logout, &QPushButton::clicked, this, &AdminMainWindow::confirmLogout);
    lay->addWidget(logout);

    return bar;
}

void AdminMainWindow::buildStatusBar()
{
    auto *sb = statusBar();
    sb->setSizeGripEnabled(false);

    auto *db = new QLabel(QStringLiteral(" SQLite: charge_platform.db(内存桩模式) "));
    m_onlineLabel = new QLabel;
    m_timeLabel = new QLabel;

    sb->addWidget(db, 1);
    sb->addPermanentWidget(m_onlineLabel);
    sb->addPermanentWidget(m_timeLabel);

    refreshStatusBar();
}

void AdminMainWindow::refreshStatusBar()
{
    // 在线电桩 = 空闲 + 使用中
    int online = 0;
    int total = 0;
    const auto stations = StationService::instance().listStations();
    for (const Station &s : stations) {
        const auto chargers = StationService::instance().chargersByStation(s.id);
        for (const Charger &c : chargers) {
            ++total;
            if (c.status == 0 || c.status == 1)
                ++online;
        }
    }
    m_onlineLabel->setText(QStringLiteral("在线电桩 %1 / %2 ").arg(online).arg(total));
    m_timeLabel->setText(QStringLiteral(" %1 ")
                             .arg(QDateTime::currentDateTime().toString(
                                 QStringLiteral("yyyy-MM-dd HH:mm:ss"))));
}

void AdminMainWindow::refreshCurrentPage()
{
    if (auto *page = qobject_cast<AdminPage *>(m_pageStack->currentWidget()))
        page->refresh();
    Toast::show(this, QStringLiteral("已刷新"));
}

void AdminMainWindow::confirmLogout()
{
    const auto ret = QMessageBox::question(this, QStringLiteral("退出登录"),
                                           QStringLiteral("确定要退出登录吗?"),
                                           QMessageBox::Yes | QMessageBox::No);
    if (ret == QMessageBox::Yes)
        emit logoutRequested();
}
