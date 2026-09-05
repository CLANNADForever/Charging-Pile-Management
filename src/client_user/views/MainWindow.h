#ifndef NCS_CLIENT_VIEWS_MAINWINDOW_H
#define NCS_CLIENT_VIEWS_MAINWINDOW_H

#include <QMainWindow>

#include <functional>

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
class MySessionsPage;
class HistoryPage;

// C 端主窗口: 0登录 1个人 2找桩 3站内桩 4充电 5我的会话 6历史 7导航。
class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(IUserService* userService,
                        IStationService* stationService = nullptr,
                        IChargeService* chargeService = nullptr,
                        QWidget* parent = nullptr);

signals:
    void routeRequested(double myLat, double myLng, double stLat,
                       double stLng, const QString& stName);

public slots:
    void pushPage(QWidget* page, std::function<void()> onBack);
    void popPage();

private slots:
    void onLoginSucceeded(const ncs::User& user);
    void onGoFindStations();
    void onGoSessions();
    void onStationChosen(int stationId);
    void onDetailBack();
    void onDeviceChosen(int deviceId);
    void onChargeBack();
    void onSessionChosen(const ncs::Order& order);
    void onDetailNav();
    void onRecharge();
    void onNickname();
    void onHistory();
    void onAvatar();

private:
    void setCurrentUser(const ncs::User& u);
    void refreshAvatar();

    IUserService* userService_ = nullptr;
    QString userPhone_;
    ncs::User user_;
    int lastStationId_ = 0;
    double lastStLat_ = 0, lastStLng_ = 0;
    QString lastStName_;
    int chargeOrigin_ = 3;
    QStackedWidget* stack_ = nullptr;
    QWidget* pushedPage_ = nullptr;
    std::function<void()> pushedBack_;
    int prevIndex_ = 0;
    ProfilePage* profilePage_ = nullptr;
    StationListPage* stationPage_ = nullptr;
    StationDetailPage* detailPage_ = nullptr;
    ChargePage* chargePage_ = nullptr;
    MySessionsPage* sessionsPage_ = nullptr;
    HistoryPage* historyPage_ = nullptr;
};

}  // namespace client
}  // namespace ncs

#endif  // NCS_CLIENT_VIEWS_MAINWINDOW_H
