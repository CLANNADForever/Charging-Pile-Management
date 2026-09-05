#include "MainWindow.h"

#include <QStackedWidget>

#include "services/IStationService.h"
#include "services/IUserService.h"
#include "views/LoginPage.h"
#include "views/ProfilePage.h"
#include "views/StationListPage.h"

namespace ncs {
namespace client {

MainWindow::MainWindow(IUserService* userService,
                       IStationService* stationService, QWidget* parent)
    : QMainWindow(parent) {
    setWindowTitle(QStringLiteral("NCS 车主端"));
    setObjectName(QStringLiteral("ncsUserMainWindow"));
    setFixedSize(420, 760);  // SRS：C 端强制竖屏 420x760

    stack_ = new QStackedWidget(this);
    auto* loginPage = new LoginPage(userService, stack_);
    profilePage_ = new ProfilePage(User{}, stack_);  // 登录成功后 setUser
    stationPage_ = new StationListPage(stationService, stack_);
    stack_->addWidget(loginPage);    // index 0
    stack_->addWidget(profilePage_); // index 1
    stack_->addWidget(stationPage_); // index 2
    setCentralWidget(stack_);

    connect(loginPage, &LoginPage::loginSucceeded, this,
            &MainWindow::onLoginSucceeded);
    connect(profilePage_, &ProfilePage::goFindStations, this,
            &MainWindow::onGoFindStations);
}

void MainWindow::onLoginSucceeded(const ncs::User& user) {
    profilePage_->setUser(user);
    stack_->setCurrentIndex(1);
}

void MainWindow::onGoFindStations() {
    stationPage_->refresh();  // 进入页面前拉一次
    stack_->setCurrentIndex(2);
}

}  // namespace client
}  // namespace ncs
