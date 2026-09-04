#ifndef NCS_CLIENT_VIEWS_MAINWINDOW_H
#define NCS_CLIENT_VIEWS_MAINWINDOW_H

#include <QMainWindow>

#include "entities.h"

class QStackedWidget;

namespace ncs {
namespace client {

class IUserService;
class LoginPage;
class ProfilePage;

// C 端主窗口：竖屏壳窗，中央为页面堆栈。登录成功后切到个人中心占位页。
class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(IUserService* service, QWidget* parent = nullptr);

private slots:
    void onLoginSucceeded(const ncs::User& user);

private:
    QStackedWidget* stack_ = nullptr;
    ProfilePage* profilePage_ = nullptr;
};

}  // namespace client
}  // namespace ncs

#endif  // NCS_CLIENT_VIEWS_MAINWINDOW_H
