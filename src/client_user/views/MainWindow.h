#ifndef NCS_CLIENT_VIEWS_MAINWINDOW_H
#define NCS_CLIENT_VIEWS_MAINWINDOW_H

#include <QMainWindow>

#include "entities.h"

class QStackedWidget;

namespace ncs {
namespace client {

class IUserService;
class IStationService;
class IChargeService;
class LoginPage;
class ProfilePage;
class StationListPage;
class StationDetailPage;
class ChargePage;

// C 端主窗口：页面堆栈 0登录 1个人 2找桩 3站内桩 4充电会话。
class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(IUserService* userService,
                        IStationService* stationService = nullptr,
                        IChargeService* chargeService = nullptr,
                        QWidget* parent = nullptr);

private slots:
    void onLoginSucceeded(const ncs::User& user);
    void onGoFindStations();
    void onStationChosen(int stationId);
    void onDetailBack();
    void onDeviceChosen(int deviceId);
    void onChargeBack();

private:
    QString userPhone_;
    int lastStationId_ = 0;
    QStackedWidget* stack_ = nullptr;
    ProfilePage* profilePage_ = nullptr;
    StationListPage* stationPage_ = nullptr;
    StationDetailPage* detailPage_ = nullptr;
    ChargePage* chargePage_ = nullptr;
};

}  // namespace client
}  // namespace ncs

#endif  // NCS_CLIENT_VIEWS_MAINWINDOW_H
