#ifndef NCS_CLIENT_VIEWS_LOGINPAGE_H
#define NCS_CLIENT_VIEWS_LOGINPAGE_H

#include <QWidget>

#include "entities.h"

class QLineEdit;
class QPushButton;
class QLabel;

namespace ncs {
namespace client {

class IUserService;

// 登录页：只做渲染与输入转发，业务规则/校验在 IUserService 实现里。
class LoginPage : public QWidget {
    Q_OBJECT
public:
    explicit LoginPage(IUserService* service, QWidget* parent = nullptr);

signals:
    void loginSucceeded(const ncs::User& user);

private slots:
    void onSendCode();
    void onLogin();

private:
    IUserService* service_ = nullptr;
    QLineEdit* phoneEdit_ = nullptr;
    QLineEdit* codeEdit_ = nullptr;
    QPushButton* btnSendCode_ = nullptr;
    QPushButton* btnLogin_ = nullptr;
    QLabel* status_ = nullptr;
};

}  // namespace client
}  // namespace ncs

#endif  // NCS_CLIENT_VIEWS_LOGINPAGE_H
