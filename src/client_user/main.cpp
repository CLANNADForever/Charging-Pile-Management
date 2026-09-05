#include "views/MainWindow.h"
#include "views/NavigationPage.h"

#include <QApplication>
#include <QObject>

#include "services/HttpChargeService.h"
#include "services/HttpStationService.h"
#include "services/HttpUserService.h"

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    ncs::client::HttpUserService userService;
    ncs::client::HttpStationService stationService;
    ncs::client::HttpChargeService chargeService;
    ncs::client::MainWindow w(&userService, &stationService, &chargeService);

    // 装配导航页(内嵌地图)到 MainWindow
    ncs::client::NavigationPage navPage;
    QObject::connect(
        &w, &ncs::client::MainWindow::routeRequested, &w,
        [&](double myLat, double myLng, double stLat, double stLng,
            const QString& stName) {
            navPage.openRoute(myLat, myLng, stLat, stLng, stName);
            w.pushPage(&navPage, [&] { /* 回到站内桩 */ });
        });
    QObject::connect(&navPage, &ncs::client::NavigationPage::backRequested, &w,
                     [&] { w.popPage(); });

    w.show();
    return app.exec();
}
