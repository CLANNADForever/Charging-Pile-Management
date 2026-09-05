#include "views/MainWindow.h"

#include <QApplication>

#include "services/HttpStationService.h"
#include "services/HttpUserService.h"

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    // 默认真后端(需先启动 build/src/backend_server/ncs_server)。
    ncs::client::HttpUserService userService;
    ncs::client::HttpStationService stationService;
    ncs::client::MainWindow w(&userService, &stationService);
    w.show();
    return app.exec();
}
