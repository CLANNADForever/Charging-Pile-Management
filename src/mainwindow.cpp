#include "mainwindow.h"

#include <QLabel>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle(QStringLiteral("充电桩监控"));
    auto *status = new QLabel(QStringLiteral("系统就绪"), this);
    setCentralWidget(status);
}
