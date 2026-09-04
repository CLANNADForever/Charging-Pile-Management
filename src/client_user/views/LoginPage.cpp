#include "LoginPage.h"

#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

#include "services/IUserService.h"

namespace ncs {
namespace client {

LoginPage::LoginPage(IUserService* service, QWidget* parent)
    : QWidget(parent), service_(service) {
    setObjectName(QStringLiteral("loginPage"));

    auto* layout = new QVBoxLayout(this);

    auto* heading = new QLabel(QStringLiteral("手机号免密登录"), this);
    heading->setObjectName(QStringLiteral("loginHeading"));
    heading->setAlignment(Qt::AlignCenter);

    auto* phoneLabel = new QLabel(QStringLiteral("手机号"), this);
    phoneEdit_ = new QLineEdit(this);
    phoneEdit_->setObjectName(QStringLiteral("phoneEdit"));
    phoneEdit_->setPlaceholderText(QStringLiteral("11 位手机号"));
    phoneEdit_->setMaxLength(11);

    auto* codeLabel = new QLabel(QStringLiteral("验证码"), this);
    codeEdit_ = new QLineEdit(this);
    codeEdit_->setObjectName(QStringLiteral("codeEdit"));
    codeEdit_->setPlaceholderText(QStringLiteral("6 位验证码"));
    codeEdit_->setMaxLength(6);

    btnSendCode_ = new QPushButton(QStringLiteral("获取验证码"), this);
    btnSendCode_->setObjectName(QStringLiteral("btnSendCode"));
    btnLogin_ = new QPushButton(QStringLiteral("登录"), this);
    btnLogin_->setObjectName(QStringLiteral("btnLogin"));

    status_ = new QLabel(this);
    status_->setObjectName(QStringLiteral("loginStatus"));
    status_->setWordWrap(true);

    layout->addStretch();
    layout->addWidget(heading);
    layout->addSpacing(16);
    layout->addWidget(phoneLabel);
    layout->addWidget(phoneEdit_);
    layout->addWidget(btnSendCode_);
    layout->addSpacing(8);
    layout->addWidget(codeLabel);
    layout->addWidget(codeEdit_);
    layout->addSpacing(16);
    layout->addWidget(btnLogin_);
    layout->addSpacing(8);
    layout->addWidget(status_);
    layout->addStretch();

    connect(btnSendCode_, &QPushButton::clicked, this, &LoginPage::onSendCode);
    connect(btnLogin_, &QPushButton::clicked, this, &LoginPage::onLogin);
}

void LoginPage::onSendCode() {
    const QString phone = phoneEdit_->text().trimmed();
    if (phone.isEmpty()) {
        status_->setText(QStringLiteral("请先输入手机号"));
        return;
    }
    const LoginResult r = service_->requestCode(phone);
    status_->setText(r.ok ? r.message : QStringLiteral("提示：") + r.message);
}

void LoginPage::onLogin() {
    const QString phone = phoneEdit_->text().trimmed();
    const QString code = codeEdit_->text().trimmed();
    if (phone.isEmpty() || code.isEmpty()) {
        status_->setText(QStringLiteral("请填写手机号与验证码"));
        return;
    }
    const LoginResult r = service_->login(phone, code);
    if (r.ok) {
        emit loginSucceeded(r.user);
        status_->setText(QStringLiteral("登录成功"));
    } else {
        status_->setText(QStringLiteral("登录失败：") + r.message);
    }
}

}  // namespace client
}  // namespace ncs
