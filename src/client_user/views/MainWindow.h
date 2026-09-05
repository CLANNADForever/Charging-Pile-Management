#ifndef NCS_CLIENT_VIEWS_MAINWINDOW_H
#define NCS_CLIENT_VIEWS_MAINWINDOW_H

#include <QMainWindow>

#include "entities.h"

class QStackedWidget;

namespace ncs {
namespace client {

class IUserService;
class IStationService;
class LoginPage;
class ProfilePage;
class StationListPage;
class StationDetailPage;

// C 端主窗口：竖屏壳窗，页面堆栈(0=登录 1=个人中心 2=找桩 3=站内桩)。
class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(IUserService* userService,
                        IStationService* stationService = nullptr,
                        QWidget* parent = nullptr);

private slots:
    void onLoginSucceeded(const ncs::User& user);
    void onGoFindStations();
    void onStationChosen(int stationId);
    void onDetailBack();

private:
    QStackedWidget* stack_ = nullptr;
    ProfilePage* profilePage_ = nullptr;
    StationListPage* stationPage_ = nullptr;
    StationDetailPage* detailPage_ = nullptr;
};

}  // namespace client
}  // namespace ncs

#endif  // NCS_CLIENT_VIEWS_MAINWINDOW_H
