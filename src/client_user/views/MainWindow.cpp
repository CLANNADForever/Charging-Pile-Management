#include "MainWindow.h"

#include <QStackedWidget>

#include "services/IChargeService.h"
#include "services/IStationService.h"
#include "services/IUserService.h"
#include "views/ChargePage.h"
#include "views/LoginPage.h"
#include "views/MySessionsPage.h"
#include "views/ProfilePage.h"
#include "views/StationDetailPage.h"
#include "views/StationListPage.h"

namespace ncs {
namespace client {

MainWindow::MainWindow(IUserService* userService,
                       IStationService* stationService,
                       IChargeService* chargeService, QWidget* parent)
    : QMainWindow(parent) {
    setWindowTitle(QStringLiteral("NCS 车主端"));
    setObjectName(QStringLiteral("ncsUserMainWindow"));
    setFixedSize(420, 760);

    stack_ = new QStackedWidget(this);
    auto* loginPage = new LoginPage(userService, stack_);
    profilePage_ = new ProfilePage(User{}, stack_);
    stationPage_ = new StationListPage(stationService, stack_);
    detailPage_ = new StationDetailPage(stationService, stack_);
    chargePage_ = new ChargePage(chargeService, stack_);
    sessionsPage_ = new MySessionsPage(chargeService, stack_);
    stack_->addWidget(loginPage);      // 0
    stack_->addWidget(profilePage_);   // 1
    stack_->addWidget(stationPage_);   // 2
    stack_->addWidget(detailPage_);    // 3
    stack_->addWidget(chargePage_);    // 4
    stack_->addWidget(sessionsPage_);  // 5
    setCentralWidget(stack_);

    connect(loginPage, &LoginPage::loginSucceeded, this,
            &MainWindow::onLoginSucceeded);
    connect(profilePage_, &ProfilePage::goFindStations, this,
            &MainWindow::onGoFindStations);
    connect(profilePage_, &ProfilePage::goSessions, this,
            &MainWindow::onGoSessions);
    connect(stationPage_, &StationListPage::stationChosen, this,
            &MainWindow::onStationChosen);
    connect(detailPage_, &StationDetailPage::backRequested, this,
            &MainWindow::onDetailBack);
    connect(detailPage_, &StationDetailPage::deviceChosen, this,
            &MainWindow::onDeviceChosen);
    connect(chargePage_, &ChargePage::backRequested, this,
            &MainWindow::onChargeBack);
    connect(sessionsPage_, &MySessionsPage::backRequested, this,
            [this] { stack_->setCurrentIndex(1); });
    connect(sessionsPage_, &MySessionsPage::sessionChosen, this,
            &MainWindow::onSessionChosen);
}

void MainWindow::onLoginSucceeded(const ncs::User& user) {
    userPhone_ = user.phone;
    profilePage_->setUser(user);
    stack_->setCurrentIndex(1);
}

void MainWindow::onGoFindStations() {
    stationPage_->refresh();
    stack_->setCurrentIndex(2);
}

void MainWindow::onGoSessions() {
    sessionsPage_->setPhone(userPhone_);
    sessionsPage_->refresh();
    stack_->setCurrentIndex(5);
}

void MainWindow::onStationChosen(int stationId) {
    lastStationId_ = stationId;
    detailPage_->load(stationId);
    stack_->setCurrentIndex(3);
}

void MainWindow::onDetailBack() {
    stationPage_->refresh();
    stack_->setCurrentIndex(2);
}

void MainWindow::onDeviceChosen(int deviceId) {
    chargeOrigin_ = 3;
    chargePage_->startSession(userPhone_, deviceId);
    stack_->setCurrentIndex(4);
}

void MainWindow::onSessionChosen(const ncs::Order& order) {
    chargeOrigin_ = 5;
    chargePage_->resumeSession(userPhone_, order);
    stack_->setCurrentIndex(4);
}

void MainWindow::onChargeBack() {
    if (chargeOrigin_ == 5) {
        sessionsPage_->refresh();
        stack_->setCurrentIndex(5);
    } else {
        if (lastStationId_ > 0)
            detailPage_->load(lastStationId_);
        stack_->setCurrentIndex(3);
    }
}

}  // namespace client
}  // namespace ncs
