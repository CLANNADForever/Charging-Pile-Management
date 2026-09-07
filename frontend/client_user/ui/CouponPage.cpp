#include "CouponPage.h"

#include <QFrame>
#include <QLabel>
#include <QVBoxLayout>

CouponPage::CouponPage(QWidget *parent)
    : Page(parent)
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    root->addWidget(makeHeader(QStringLiteral("优惠券")));

    auto *body = new QVBoxLayout;
    body->setContentsMargins(20, 0, 20, 0);
    body->addStretch(1);

    auto *card = new QFrame(this);
    card->setObjectName(QStringLiteral("card"));
    auto *lay = new QVBoxLayout(card);
    lay->setContentsMargins(24, 32, 24, 32);
    lay->setSpacing(8);

    auto *emoji = new QLabel(QStringLiteral("🎟"), card);
    emoji->setObjectName(QStringLiteral("couponEmoji"));
    emoji->setAlignment(Qt::AlignCenter);

    auto *title = new QLabel(QStringLiteral("优惠券功能暂未开发"), card);
    title->setObjectName(QStringLiteral("sectionTitle"));
    title->setAlignment(Qt::AlignCenter);

    auto *hint = new QLabel(QStringLiteral("敬请期待"), card);
    hint->setObjectName(QStringLiteral("hintLabel"));
    hint->setAlignment(Qt::AlignCenter);

    lay->addWidget(emoji);
    lay->addWidget(title);
    lay->addWidget(hint);

    body->addWidget(card);
    body->addStretch(1);
    root->addLayout(body, 1);
}
