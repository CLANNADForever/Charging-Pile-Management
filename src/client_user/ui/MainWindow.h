#pragma once

#include <QList>
#include <QWidget>

class QStackedWidget;
class NavButton;
class Page;
class UserCenterPage;
class StationListPage;
class ChargeEntryPage;

// 用户端主窗口:底部导航(首页 / 充电 / 我的) + 页面栈。
// 固定手机竖屏比例 420×760 (NFR-U-02)。
class MainWindow : public QWidget
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);

signals:
    void logoutRequested();

private:
    void switchTab(int index);
    void pushPage(Page *page);
    void popPage();

    QStackedWidget *m_stack = nullptr;
    QList<NavButton *> m_navs;
    StationListPage *m_stationList = nullptr;
    ChargeEntryPage *m_chargeEntry = nullptr;
    UserCenterPage *m_userCenter = nullptr;
    int m_current = -1;

    static constexpr int kRootCount = 3;
};
