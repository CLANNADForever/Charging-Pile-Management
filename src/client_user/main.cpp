#include "views/MainWindow.h"

#include <QApplication>

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    ncs::client::MainWindow w;
    w.show();
    return app.exec();
}
