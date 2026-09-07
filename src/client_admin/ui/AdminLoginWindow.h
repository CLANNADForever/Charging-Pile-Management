#pragma once

#include <QWidget>

class QLabel;
class QLineEdit;
class QPushButton;
class QTimer;

// 管理员登录页(UC-A-01)。
// 账号密码登录,连续失败 5 次锁定 30 秒;成功发出 loginSucceeded。
class AdminLoginWindow : public QWidget
{
    Q_OBJECT
public:
    explicit AdminLoginWindow(QWidget *parent = nullptr);

    // 清空输入、解除锁定(退出登录回到本页时调用)。
    void reset();

signals:
    void loginSucceeded();

private:
    void tryLogin();
    void lockAndCountdown();
    void updateLockHint();

    QLineEdit *m_accountEdit = nullptr;
    QLineEdit *m_passwordEdit = nullptr;
    QPushButton *m_togglePwdBtn = nullptr;
    QPushButton *m_loginBtn = nullptr;
    QLabel *m_errorLabel = nullptr;
    QLabel *m_lockHintLabel = nullptr;
    QTimer *m_lockTimer = nullptr;

    int m_failCount = 0;
    int m_lockSeconds = 0;
};
