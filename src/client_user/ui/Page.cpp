#include "Page.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSpacerItem>

QWidget *Page::makeHeader(const QString &title)
{
    auto *header = new QWidget(this);
    header->setFixedHeight(52);
    auto *lay = new QHBoxLayout(header);
    lay->setContentsMargins(4, 0, 4, 0);
    lay->setSpacing(0);

    auto *back = new QPushButton(QStringLiteral("‹"), header);
    back->setObjectName(QStringLiteral("backButton"));
    back->setFixedSize(40, 40);
    back->setCursor(Qt::PointingHandCursor);
    connect(back, &QPushButton::clicked, this, &Page::backRequested);

    auto *titleLabel = new QLabel(title, header);
    titleLabel->setObjectName(QStringLiteral("pageTitle"));
    titleLabel->setAlignment(Qt::AlignCenter);

    lay->addWidget(back);
    lay->addStretch(1);
    lay->addWidget(titleLabel);
    lay->addStretch(1);
    // 平衡返回按钮宽度,使标题真正居中
    lay->addSpacerItem(new QSpacerItem(40, 0, QSizePolicy::Fixed, QSizePolicy::Minimum));
    return header;
}
