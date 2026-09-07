#include "LoginWindow.h"

#include <QFrame>
#include <QGraphicsDropShadowEffect>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPixmap>
#include <QPushButton>
#include <QRandomGenerator>
#include <QTimer>
#include <QVBoxLayout>

#include "core/service/UserService.h"

LoginWindow::LoginWindow(QWidget *parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("loginRoot"));
    setWindowTitle(QStringLiteral("NCS · 充电"));
    setFixedSize(420, 760);

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    root->addStretch(2);

    // Logo 卡片(白底 logo 与白卡片无缝融合)
    auto *logoCard = new QFrame(this);
    logoCard->setObjectName(QStringLiteral("logoCard"));
    logoCard->setFixedSize(112, 112);
    auto *shadow = new QGraphicsDropShadowEffect(logoCard);
    shadow->setBlurRadius(28);
    shadow->setOffset(0, 8);
    shadow->setColor(QColor(0x2F, 0x80, 0xFF, 40));
    logoCard->setGraphicsEffect(shadow);

    auto *logoLay = new QVBoxLayout(logoCard);
    logoLay->setContentsMargins(14, 14, 14, 14);
    auto *logo = new QLabel(logoCard);
    logo->setPixmap(QPixmap(QStringLiteral(":/logo.png"))
                        .scaled(84, 84, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    logo->setAlignment(Qt::AlignCenter);
    logoLay->addWidget(logo);
    root->addWidget(logoCard, 0, Qt::AlignHCenter);

    root->addSpacing(20);

    auto *appTitle = new QLabel(QStringLiteral("东软电动汽车充电桩"), this);
    appTitle->setObjectName(QStringLiteral("appTitle"));
    appTitle->setAlignment(Qt::AlignHCenter);
    root->addWidget(appTitle);

    auto *appSub = new QLabel(QStringLiteral("充电桩应用管理平台"), this);
    appSub->setObjectName(QStringLiteral("appSubtitle"));
    appSub->setAlignment(Qt::AlignHCenter);
    root->addWidget(appSub);

    root->addSpacing(12);

    // 品牌渐变条(呼应 Logo 流动能量环)
    auto *brandBar = new QFrame(this);
    brandBar->setObjectName(QStringLiteral("brandBar"));
    brandBar->setFixedSize(56, 4);
    root->addWidget(brandBar, 0, Qt::AlignHCenter);

    root->addStretch(1);

    // 表单区
    auto *form = new QWidget(this);
    auto *formLay = new QVBoxLayout(form);
    formLay->setContentsMargins(32, 0, 32, 0);
    formLay->setSpacing(14);

    m_phoneEdit = new QLineEdit(form);
    m_phoneEdit->setPlaceholderText(QStringLiteral("请输入手机号"));
    m_phoneEdit->setMaxLength(11);
    formLay->addWidget(m_phoneEdit);

    auto *codeRow = new QHBoxLayout;
    codeRow->setSpacing(10);
    m_codeEdit = new QLineEdit(form);
    m_codeEdit->setPlaceholderText(QStringLiteral("请输入验证码"));
    m_codeEdit->setMaxLength(6);
    m_getCodeBtn = new QPushButton(QStringLiteral("获取验证码"), form);
    m_getCodeBtn->setObjectName(QStringLiteral("ghostButton"));
    m_getCodeBtn->setFixedWidth(112);
    codeRow->addWidget(m_codeEdit, 1);
    codeRow->addWidget(m_getCodeBtn);
    formLay->addLayout(codeRow);

    m_hintLabel = new QLabel(QStringLiteral("未注册的手机号将自动注册"), form);
    m_hintLabel->setObjectName(QStringLiteral("hintLabel"));
    m_hintLabel->setAlignment(Qt::AlignCenter);
    m_hintLabel->setWordWrap(true);
    formLay->addWidget(m_hintLabel);

    m_loginBtn = new QPushButton(QStringLiteral("一键登录 / 注册"), form);
    m_loginBtn->setObjectName(QStringLiteral("primaryButton"));
    formLay->addWidget(m_loginBtn);

    root->addWidget(form, 0);
    root->addStretch(2);

    m_countdown = new QTimer(this);
    m_countdown->setInterval(1000);

    connect(m_getCodeBtn, &QPushButton::clicked, this, &LoginWindow::onGetCode);
    connect(m_loginBtn, &QPushButton::clicked, this, &LoginWindow::onLogin);
    connect(m_countdown, &QTimer::timeout, this, &LoginWindow::tickCountdown);
}

void LoginWindow::reset()
{
    m_phoneEdit->clear();
    m_codeEdit->clear();
    m_expectedCode.clear();
    m_remaining = 0;
    m_countdown->stop();
    m_getCodeBtn->setEnabled(true);
    m_getCodeBtn->setText(QStringLiteral("获取验证码"));
    setHint(QStringLiteral("未注册的手机号将自动注册"), false);
}

void LoginWindow::setHint(const QString &text, bool error)
{
    m_hintLabel->setText(text);
    m_hintLabel->setStyleSheet(error ? QStringLiteral("color:#EF4444;")
                                     : QStringLiteral("color:#8A8F99;"));
}

void LoginWindow::onGetCode()
{
    const QString phone = m_phoneEdit->text().trimmed();
    if (!UserService::isValidPhone(phone)) {
        setHint(QStringLiteral("请输入正确的 11 位手机号"), true);
        return;
    }

    // 生成 6 位随机码并直接显示(模拟短信下发)
    m_expectedCode = QString::number(QRandomGenerator::global()->bounded(100000, 1000000));
    setHint(QStringLiteral("验证码已发送(模拟):") + m_expectedCode, false);

    m_remaining = 60;
    m_getCodeBtn->setEnabled(false);
    m_getCodeBtn->setText(QString::number(m_remaining) + QStringLiteral("s"));
    m_countdown->start();
}

void LoginWindow::tickCountdown()
{
    --m_remaining;
    if (m_remaining <= 0) {
        m_countdown->stop();
        m_getCodeBtn->setEnabled(true);
        m_getCodeBtn->setText(QStringLiteral("获取验证码"));
    } else {
        m_getCodeBtn->setText(QString::number(m_remaining) + QStringLiteral("s"));
    }
}

void LoginWindow::onLogin()
{
    const QString phone = m_phoneEdit->text().trimmed();
    if (!UserService::isValidPhone(phone)) {
        setHint(QStringLiteral("请输入正确的 11 位手机号"), true);
        return;
    }
    if (m_expectedCode.isEmpty()) {
        setHint(QStringLiteral("请先获取验证码"), true);
        return;
    }
    if (m_codeEdit->text().trimmed() != m_expectedCode) {
        setHint(QStringLiteral("验证码错误"), true);
        return;
    }

    const User user = UserService::instance().loginOrRegister(phone);
    if (user.frozen) {
        setHint(QStringLiteral("账号已被冻结，请联系客服"), true);
        return;
    }

    emit loginSucceeded();
}
