#include "ProfilePage.h"

#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QPixmap>

#include "money.h"

namespace ncs {
namespace client {

ProfilePage::ProfilePage(const ncs::User& user, QWidget* parent)
    : QWidget(parent) {
    setObjectName(QStringLiteral("profilePage"));
    auto* layout = new QVBoxLayout(this);

    avatar_ = new QLabel(this);
    avatar_->setObjectName(QStringLiteral("profileAvatar"));
    avatar_->setFixedSize(72, 72);
    avatar_->setScaledContents(true);
    avatar_->setAlignment(Qt::AlignCenter);
    avatar_->setText(QStringLiteral("头像"));
    avatar_->setStyleSheet(QStringLiteral("border:1px solid #ccc; border-radius:36px;"));

    auto* heading = new QLabel(QStringLiteral("个人中心"), this);
    heading->setObjectName(QStringLiteral("profileHeading"));
    heading->setAlignment(Qt::AlignCenter);
    nickname_ = new QLabel(this);
    nickname_->setObjectName(QStringLiteral("profileNickname"));
    phone_ = new QLabel(this);
    phone_->setObjectName(QStringLiteral("profilePhone"));
    balance_ = new QLabel(this);
    balance_->setObjectName(QStringLiteral("profileBalance"));


    layout->addWidget(heading);
    layout->addWidget(avatar_, 0, Qt::AlignHCenter);
    layout->addWidget(nickname_);
    layout->addWidget(phone_);
    layout->addWidget(balance_);
    layout->addSpacing(8);
    auto* g1 = new QPushButton(QStringLiteral("周边找桩"), this);
    g1->setObjectName(QStringLiteral("btnGoStations"));
    connect(g1, &QPushButton::clicked, this, &ProfilePage::goFindStations);
    auto* g2 = new QPushButton(QStringLiteral("我的充电会话"), this);
    g2->setObjectName(QStringLiteral("btnMySessions"));
    connect(g2, &QPushButton::clicked, this, &ProfilePage::goSessions);
    auto* r1 = new QPushButton(QStringLiteral("充值"), this);
    r1->setObjectName(QStringLiteral("btnRecharge"));
    connect(r1, &QPushButton::clicked, this, &ProfilePage::rechargeRequested);
    auto* r2 = new QPushButton(QStringLiteral("历史订单"), this);
    r2->setObjectName(QStringLiteral("btnHistory"));
    connect(r2, &QPushButton::clicked, this, &ProfilePage::historyRequested);
    auto* r3 = new QPushButton(QStringLiteral("改昵称"), this);
    r3->setObjectName(QStringLiteral("btnNickname"));
    connect(r3, &QPushButton::clicked, this, &ProfilePage::nicknameRequested);
    auto* r4 = new QPushButton(QStringLiteral("设置头像"), this);
    r4->setObjectName(QStringLiteral("btnAvatar"));
    connect(r4, &QPushButton::clicked, this, &ProfilePage::avatarRequested);
    layout->addWidget(g1);
    layout->addWidget(g2);
    layout->addWidget(r1);
    layout->addWidget(r2);
    layout->addWidget(r3);
    layout->addWidget(r4);
    layout->addStretch();

    setUser(user);
}

void ProfilePage::setUser(const ncs::User& user) {
    nickname_->setText(QStringLiteral("昵称：") + user.nickname);
    phone_->setText(QStringLiteral("手机号：") + user.phone);
    balance_->setText(QStringLiteral("余额：") + format_cents(user.balanceCents) +
                      QStringLiteral(" 元"));
}

void ProfilePage::setAvatarPixmap(const QPixmap& pm) {
    avatar_->setPixmap(pm.scaled(avatar_->size(), Qt::KeepAspectRatio,
                                 Qt::SmoothTransformation));
}

}  // namespace client
}  // namespace ncs
