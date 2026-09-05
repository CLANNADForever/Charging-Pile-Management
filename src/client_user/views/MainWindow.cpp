#include "MainWindow.h"

#include <QStackedWidget>

#include "services/IStationService.h"
#include "services/IUserService.h"
#include "views/LoginPage.h"
#include "views/ProfilePage.h"
#include "views/StationDetailPage.h"
#include "views/StationListPage.h"

namespace ncs {
namespace client {

MainWindow::MainWindow(IUserService* userService,
                       IStationService* stationService, QWidget* parent)
    : QMainWindow(parent) {
    setWindowTitle(QStringLiteral("NCS 车主端"));
    setObjectName(QStringLiteral("ncsUserMainWindow"));
    setFixedSize(420, 760);

    stack_ = new QStackedWidget(this);
    auto* loginPage = new LoginPage(userService, stack_);
    profilePage_ = new ProfilePage(User{}, stack_);
    stationPage_ = new StationListPage(stationService, stack_);
    detailPage_ = new StationDetailPage(stationService, stack_);
    stack_->addWidget(loginPage);     // 0
    stack_->addWidget(profilePage_);  // 1
    stack_->addWidget(stationPage_);  // 2
    stack_->addWidget(detailPage_);   // 3
    setCentralWidget(stack_);

    connect(loginPage, &LoginPage::loginSucceeded, this,
            &MainWindow::onLoginSucceeded);
    connect(profilePage_, &ProfilePage::goFindStations, this,
            &MainWindow::onGoFindStations);
    connect(stationPage_, &StationListPage::stationChosen, this,
            &MainWindow::onStationChosen);
    connect(detailPage_, &StationDetailPage::backRequested, this,
            &MainWindow::onDetailBack);
}

void MainWindow::onLoginSucceeded(const ncs::User& user) {
    profilePage_->setUser(user);
    stack_->setCurrentIndex(1);
}

void MainWindow::onGoFindStations() {
    stationPage_->refresh();
    stack_->setCurrentIndex(2);
}

void MainWindow::onStationChosen(int stationId) {
    detailPage_->load(stationId);
    stack_->setCurrentIndex(3);
}

void MainWindow::onDetailBack() {
    stack_->setCurrentIndex(2);
}

}  // namespace client
}  // namespace ncs
