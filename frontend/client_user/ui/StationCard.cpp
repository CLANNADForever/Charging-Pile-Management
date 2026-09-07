#include "StationCard.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPushButton>
#include <QVBoxLayout>

#include "common/Utils.h"

namespace {

QLabel *makeTag(const QString &text, const QString &bg, const QString &fg)
{
    auto *tag = new QLabel(text);
    tag->setStyleSheet(QStringLiteral("background:%1; color:%2; border-radius:4px;"
                                      " padding:2px 6px; font-size:11px;")
                           .arg(bg, fg));
    return tag;
}

} // namespace

StationCard::StationCard(const Station &station, double distanceKm,
                         const QStringList &powerTypes, int freeCount, int totalCount,
                         QWidget *parent)
    : QFrame(parent)
    , m_stationId(station.id)
{
    setObjectName(QStringLiteral("stationCard"));
    setCursor(Qt::PointingHandCursor);

    auto *lay = new QVBoxLayout(this);
    lay->setContentsMargins(16, 14, 16, 14);
    lay->setSpacing(8);

    // 名称 + 优惠
    auto *top = new QHBoxLayout;
    top->setSpacing(8);
    auto *name = new QLabel(station.name, this);
    name->setObjectName(QStringLiteral("sectionTitle"));
    top->addWidget(name, 1);
    if (station.hasCoupon)
        top->addWidget(makeTag(QStringLiteral("领券"), QStringLiteral("#FF3B30"), QStringLiteral("#FFFFFF")));
    lay->addLayout(top);

    // 功率标签 + 停车减免
    auto *chipRow = new QHBoxLayout;
    chipRow->setSpacing(6);
    for (const QString &t : powerTypes)
        chipRow->addWidget(makeTag(t, QStringLiteral("#EAF3FF"), QStringLiteral("#2F80FF")));
    if (station.parkingFree)
        chipRow->addWidget(makeTag(QStringLiteral("停车减免"), QStringLiteral("#E6F7EE"), QStringLiteral("#00B368")));
    chipRow->addStretch();
    lay->addLayout(chipRow);

    // 配套设施
    auto *fac = new QLabel(station.facilities.join(QStringLiteral(" · ")), this);
    fac->setObjectName(QStringLiteral("hintLabel"));
    lay->addWidget(fac);

    // 底部:价格 + 空闲 + 距离
    auto *bottom = new QHBoxLayout;
    bottom->setSpacing(10);
    auto *price = new QLabel(QStringLiteral("¥ ") + Utils::formatMoney(station.unitPrice)
                                 + QStringLiteral("/度"), this);
    price->setStyleSheet(QStringLiteral("color:#2F80FF; font-size:15px; font-weight:bold;"));
    bottom->addWidget(price);

    auto *free = new QLabel(QStringLiteral("空闲 %1/%2").arg(freeCount).arg(totalCount), this);
    free->setObjectName(QStringLiteral("hintLabel"));
    bottom->addWidget(free);
    bottom->addStretch();

    auto *dist = new QPushButton(QStringLiteral("%1 km").arg(distanceKm, 0, 'f', 1), this);
    dist->setObjectName(QStringLiteral("textButton"));
    dist->setCursor(Qt::PointingHandCursor);
    connect(dist, &QPushButton::clicked, this, [this]() { emit navClicked(m_stationId); });
    bottom->addWidget(dist);
    lay->addLayout(bottom);
}

void StationCard::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
        emit clicked(m_stationId);
    QFrame::mousePressEvent(event);
}
