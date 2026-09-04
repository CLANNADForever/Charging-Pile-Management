#include "MainWindow.h"

#include <QLabel>
#include <QVBoxLayout>
#include <QWidget>

#include "entities.h"

namespace ncs {
namespace client {

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowTitle(QStringLiteral("NCS 车主端"));
    setObjectName("ncsUserMainWindow");
    setFixedSize(420, 760);  // SRS：C 端强制竖屏 420x760

    auto* central = new QWidget(this);
    auto* layout = new QVBoxLayout(central);
    auto* title = new QLabel(QString::fromUtf8(project_name()), central);
    title->setObjectName("homeTitle");
    title->setAlignment(Qt::AlignCenter);
    auto* placeholder = new QLabel(
        QStringLiteral("骨架就绪\n选桩 / 我的 等页面由后续 change 加入"), central);
    placeholder->setObjectName("homePlaceholder");
    placeholder->setAlignment(Qt::AlignCenter);
    layout->addStretch();
    layout->addWidget(title);
    layout->addWidget(placeholder);
    layout->addStretch();
    setCentralWidget(central);
}

}  // namespace client
}  // namespace ncs
