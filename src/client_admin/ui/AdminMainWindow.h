#pragma once

#include <QMainWindow>

class QLabel;
class QListWidget;
class QStackedWidget;

// 管理后台主界面框架(UC-A-02)。
// 左侧导航栏 + 右侧 QStackedWidget 承载 6 个子页面;顶栏 + 底部状态栏。
class AdminMainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit AdminMainWindow(QWidget *parent = nullptr);

signals:
    void logoutRequested();

private:
    QWidget *buildSidebar();
    QWidget *buildTopBar();
    void buildStatusBar();
    void refreshStatusBar();
    void refreshCurrentPage();
    void confirmLogout();

    QListWidget *m_navList = nullptr;
    QStackedWidget *m_pageStack = nullptr;
    QLabel *m_timeLabel = nullptr;
    QLabel *m_onlineLabel = nullptr;
};
