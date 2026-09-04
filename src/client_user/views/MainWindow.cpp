#include "MainWindow.h"

#include <QStackedWidget>

#include "services/IUserService.h"
#include "views/LoginPage.h"
#include "views/ProfilePage.h"

namespace ncs {
namespace client {

MainWindow::MainWindow(IUserService* service, QWidget* parent)
    : QMainWindow(parent) {
    setWindowTitle(QStringLiteral("NCS 车主端"));
    setObjectName(QStringLiteral("ncsUserMainWindow"));
    setFixedSize(420, 760);  // SRS：C 端强制竖屏 420x760

    stack_ = new QStackedWidget(this);
    auto* loginPage = new LoginPage(service, stack_);
    profilePage_ = new ProfilePage(User{}, stack_);  // 登录成功后 setUser 填充
    stack_->addWidget(loginPage);                    // index 0
    stack_->addWidget(profilePage_);                 // index 1
    setCentralWidget(stack_);

    connect(loginPage, &LoginPage::loginSucceeded,
            this, &MainWindow::onLoginSucceeded);
}

void MainWindow::onLoginSucceeded(const ncs::User& user) {
    profilePage_->setUser(user);
    stack_->setCurrentIndex(1);
}

}  // namespace client
}  // namespace ncs
