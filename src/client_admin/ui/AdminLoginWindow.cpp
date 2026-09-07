#include "AdminLoginWindow.h"

#include <QColor>
#include <QFrame>
#include <QGraphicsDropShadowEffect>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPixmap>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>

#include "core/service/AdminService.h"

namespace {
constexpr int kMaxFailCount = 5;
constexpr int kLockSeconds = 30;
}

AdminLoginWindow::AdminLoginWindow(QWidget *parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("loginRoot"));
    setWindowTitle(QStringLiteral("NCS 运营管理端 · 登录"));
    resize(1280, 800);
    setMinimumSize(900, 600);

    auto *outer = new QHBoxLayout(this);
    outer->addStretch();

    // 居中登录卡片
    auto *card = new QFrame;
    card->setObjectName(QStringLiteral("card"));
    card->setFixedWidth(480);
    auto *lay = new QVBoxLayout(card);
    lay->setContentsMargins(48, 48, 48, 44);
    lay->setSpacing(16);

    // 品牌区:圆形 Logo 徽章 + 标题 + 副标题
    auto *logoLabel = new QLabel(card);
    logoLabel->setFixedSize(132, 132);
    logoLabel->setAlignment(Qt::AlignCenter);
    logoLabel->setPixmap(QPixmap(QStringLiteral(":/logo.png"))
                             .scaled(104, 104, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    logoLabel->setStyleSheet(QStringLiteral("background:#FFFFFF; border-radius:66px;"));
    auto *logoShadow = new QGraphicsDropShadowEffect(logoLabel);
    logoShadow->setBlurRadius(28);
    logoShadow->setOffset(0, 4);
    logoShadow->setColor(QColor(0, 0, 0, 80));
    logoLabel->setGraphicsEffect(logoShadow);
    lay->addWidget(logoLabel, 0, Qt::AlignHCenter);
    lay->addSpacing(6);

    auto *title = new QLabel(QStringLiteral("东软电动汽车充电桩应用管理平台"), card);
    title->setObjectName(QStringLiteral("appTitle"));
    title->setWordWrap(true);
    title->setAlignment(Qt::AlignCenter);
    auto *sub = new QLabel(QStringLiteral("运营管理端"), card);
    sub->setObjectName(QStringLiteral("appSubtitle"));
    sub->setAlignment(Qt::AlignCenter);
    lay->addWidget(title);
    lay->addWidget(sub);
    lay->addSpacing(18);

    // 账号
    m_accountEdit = new QLineEdit(card);
    m_accountEdit->setObjectName(QStringLiteral("loginEdit"));
    m_accountEdit->setPlaceholderText(QStringLiteral("请输入管理员账号"));
    lay->addWidget(m_accountEdit);

    // 密码 + 显隐切换
    auto *pwdRow = new QHBoxLayout;
    pwdRow->setSpacing(6);
    m_passwordEdit = new QLineEdit(card);
    m_passwordEdit->setObjectName(QStringLiteral("loginEdit"));
    m_passwordEdit->setPlaceholderText(QStringLiteral("请输入密码"));
    m_passwordEdit->setEchoMode(QLineEdit::Password);
    m_togglePwdBtn = new QPushButton(QStringLiteral("显示"), card);
    m_togglePwdBtn->setObjectName(QStringLiteral("togglePwdButton"));
    m_togglePwdBtn->setCursor(Qt::PointingHandCursor);
    pwdRow->addWidget(m_passwordEdit, 1);
    pwdRow->addWidget(m_togglePwdBtn);
    lay->addLayout(pwdRow);

    connect(m_togglePwdBtn, &QPushButton::clicked, this, [this]() {
        const bool hidden = m_passwordEdit->echoMode() == QLineEdit::Password;
        m_passwordEdit->setEchoMode(hidden ? QLineEdit::Normal : QLineEdit::Password);
        m_togglePwdBtn->setText(hidden ? QStringLiteral("隐藏") : QStringLiteral("显示"));
    });

    // 错误 / 锁定提示
    m_errorLabel = new QLabel(card);
    m_errorLabel->setObjectName(QStringLiteral("errorLabel"));
    m_errorLabel->setAlignment(Qt::AlignCenter);
    m_errorLabel->hide();
    m_lockHintLabel = new QLabel(card);
    m_lockHintLabel->setObjectName(QStringLiteral("errorLabel"));
    m_lockHintLabel->setAlignment(Qt::AlignCenter);
    m_lockHintLabel->hide();
    lay->addWidget(m_errorLabel);
    lay->addWidget(m_lockHintLabel);

    // 登录按钮
    m_loginBtn = new QPushButton(QStringLiteral("登录"), card);
    m_loginBtn->setObjectName(QStringLiteral("primaryButton"));
    m_loginBtn->setCursor(Qt::PointingHandCursor);
    lay->addWidget(m_loginBtn);

    connect(m_loginBtn, &QPushButton::clicked, this, &AdminLoginWindow::tryLogin);
    connect(m_accountEdit, &QLineEdit::returnPressed, this, &AdminLoginWindow::tryLogin);
    connect(m_passwordEdit, &QLineEdit::returnPressed, this, &AdminLoginWindow::tryLogin);

    outer->addWidget(card);
    outer->addStretch();

    m_lockTimer = new QTimer(this);
    m_lockTimer->setInterval(1000);
    connect(m_lockTimer, &QTimer::timeout, this, &AdminLoginWindow::updateLockHint);
}

void AdminLoginWindow::tryLogin()
{
    if (m_lockSeconds > 0)
        return; // 锁定中

    const QString account = m_accountEdit->text().trimmed();
    const QString password = m_passwordEdit->text();

    if (account.isEmpty() || password.isEmpty()) {
        m_errorLabel->setText(QStringLiteral("请输入账号和密码"));
        m_errorLabel->show();
        return;
    }

    if (AdminService::instance().login(account, password)) {
        m_errorLabel->hide();
        m_failCount = 0;
        emit loginSucceeded();
        return;
    }

    ++m_failCount;
    m_errorLabel->setText(QStringLiteral("账号或密码错误"));
    m_errorLabel->show();

    if (m_failCount >= kMaxFailCount)
        lockAndCountdown();
}

void AdminLoginWindow::lockAndCountdown()
{
    m_lockSeconds = kLockSeconds;
    m_accountEdit->setEnabled(false);
    m_passwordEdit->setEnabled(false);
    m_togglePwdBtn->setEnabled(false);
    m_loginBtn->setEnabled(false);
    m_lockHintLabel->show();
    updateLockHint();
    m_lockTimer->start();
}

void AdminLoginWindow::updateLockHint()
{
    if (m_lockSeconds <= 0) {
        m_lockTimer->stop();
        m_lockHintLabel->hide();
        m_accountEdit->setEnabled(true);
        m_passwordEdit->setEnabled(true);
        m_togglePwdBtn->setEnabled(true);
        m_loginBtn->setEnabled(true);
        m_failCount = 0;
        return;
    }
    m_lockHintLabel->setText(QStringLiteral("已锁定,请 %1 秒后重试").arg(m_lockSeconds));
    --m_lockSeconds;
}

void AdminLoginWindow::reset()
{
    m_accountEdit->clear();
    m_passwordEdit->clear();
    m_passwordEdit->setEchoMode(QLineEdit::Password);
    m_togglePwdBtn->setText(QStringLiteral("显示"));
    m_errorLabel->hide();
    m_lockHintLabel->hide();
    m_failCount = 0;
    m_lockSeconds = 0;
    m_lockTimer->stop();
    m_accountEdit->setEnabled(true);
    m_passwordEdit->setEnabled(true);
    m_togglePwdBtn->setEnabled(true);
    m_loginBtn->setEnabled(true);
}
