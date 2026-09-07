#pragma once

#include <QWidget>

class QLabel;
class QLineEdit;
class QPushButton;
class QTimer;

// 用户登录窗口:手机号免密登录 / 自动注册 (UC-U-01)。
class LoginWindow : public QWidget
{
    Q_OBJECT
public:
    explicit LoginWindow(QWidget *parent = nullptr);

    // 退出登录后重新显示前重置状态
    void reset();

signals:
    void loginSucceeded();

private slots:
    void onGetCode();
    void onLogin();
    void tickCountdown();

private:
    void setHint(const QString &text, bool error);

    QLineEdit *m_phoneEdit = nullptr;
    QLineEdit *m_codeEdit = nullptr;
    QPushButton *m_getCodeBtn = nullptr;
    QPushButton *m_loginBtn = nullptr;
    QLabel *m_hintLabel = nullptr;
    QTimer *m_countdown = nullptr;
    int m_remaining = 0;
    QString m_expectedCode;
};
