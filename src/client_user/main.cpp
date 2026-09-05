#include "views/MainWindow.h"

#include <QApplication>

#include "services/HttpChargeService.h"
#include "services/HttpStationService.h"
#include "services/HttpUserService.h"

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    // 默认真后端(先启动 build/src/backend_server/ncs_server)。
    ncs::client::HttpUserService userService;
    ncs::client::HttpStationService stationService;
    ncs::client::HttpChargeService chargeService;
    ncs::client::MainWindow w(&userService, &stationService, &chargeService);
    w.show();
    return app.exec();
}
