#include "views/MainWindow.h"

#include <QApplication>

#include "services/MockUserService.h"

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    ncs::client::MockUserService service;
    ncs::client::MainWindow w(&service);
    w.show();
    return app.exec();
}
