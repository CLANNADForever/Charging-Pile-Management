#include "ProfilePage.h"

#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

#include "money.h"

namespace ncs {
namespace client {

ProfilePage::ProfilePage(const ncs::User& user, QWidget* parent)
    : QWidget(parent) {
    setObjectName(QStringLiteral("profilePage"));
    auto* layout = new QVBoxLayout(this);

    auto* heading = new QLabel(QStringLiteral("个人中心"), this);
    heading->setObjectName(QStringLiteral("profileHeading"));
    heading->setAlignment(Qt::AlignCenter);

    nickname_ = new QLabel(this);
    nickname_->setObjectName(QStringLiteral("profileNickname"));
    phone_ = new QLabel(this);
    phone_->setObjectName(QStringLiteral("profilePhone"));
    balance_ = new QLabel(this);
    balance_->setObjectName(QStringLiteral("profileBalance"));

    auto* goStation = new QPushButton(QStringLiteral("周边找桩"), this);
    goStation->setObjectName(QStringLiteral("btnGoStations"));

    auto* note = new QLabel(QStringLiteral("占位：钱包 / 历史订单 / 设置 由后续加入"), this);
    note->setObjectName(QStringLiteral("profileNote"));
    note->setWordWrap(true);

    layout->addStretch();
    layout->addWidget(heading);
    layout->addSpacing(16);
    layout->addWidget(nickname_);
    layout->addWidget(phone_);
    layout->addWidget(balance_);
    layout->addSpacing(16);
    layout->addWidget(goStation);
    layout->addSpacing(16);
    layout->addWidget(note);
    layout->addStretch();

    connect(goStation, &QPushButton::clicked, this, &ProfilePage::goFindStations);
    setUser(user);
}

void ProfilePage::setUser(const ncs::User& user) {
    nickname_->setText(QStringLiteral("昵称：") + user.nickname);
    phone_->setText(QStringLiteral("手机号：") + user.phone);
    balance_->setText(QStringLiteral("余额：") + format_cents(user.balanceCents) +
                      QStringLiteral(" 元"));
}

}  // namespace client
}  // namespace ncs
