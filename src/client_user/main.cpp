#include "views/MainWindow.h"

#include <QApplication>

#include "services/HttpUserService.h"

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    // 默认真后端；如需离线/演示可换 MockUserService。
    ncs::client::HttpUserService service;
    ncs::client::MainWindow w(&service);
    w.show();
    return app.exec();
}
