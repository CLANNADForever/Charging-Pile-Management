#include "NavigationPage.h"

#include <QDesktopServices>
#include <QLabel>
#include <QPushButton>
#include <QUrl>
#include <QVBoxLayout>
#include <cmath>

namespace ncs {
namespace client {

namespace {
double kmBetween(double lat1, double lng1, double lat2, double lng2) {
    constexpr double kR = 6371.0;
    const double p = 3.14159265358979323846 / 180.0;
    const double dLat = (lat2 - lat1) * p;
    const double dLng = (lng2 - lng1) * p;
    const double a = std::sin(dLat / 2) * std::sin(dLat / 2) +
                     std::cos(lat1 * p) * std::cos(lat2 * p) *
                         std::sin(dLng / 2) * std::sin(dLng / 2);
    return kR * 2.0 * std::atan2(std::sqrt(a), std::sqrt(1.0 - a));
}
}  // namespace

NavigationPage::NavigationPage(QWidget* parent) : QWidget(parent) {
    setObjectName(QStringLiteral("navigationPage"));
    auto* layout = new QVBoxLayout(this);
    auto* heading = new QLabel(QStringLiteral("路线(我的位置 → 电站)"), this);
    heading->setObjectName(QStringLiteral("navHeading"));
    heading->setAlignment(Qt::AlignCenter);
    info_ = new QLabel(this);
    info_->setObjectName(QStringLiteral("navInfo"));
    info_->setWordWrap(true);
    info_->setAlignment(Qt::AlignCenter);

    auto* openBtn = new QPushButton(QStringLiteral("用外部地图打开(高德)"), this);
    openBtn->setObjectName(QStringLiteral("btnNavExternal"));
    auto* back = new QPushButton(QStringLiteral("返回"), this);
    back->setObjectName(QStringLiteral("btnNavBack"));

    layout->addStretch();
    layout->addWidget(heading);
    layout->addSpacing(12);
    layout->addWidget(info_);
    layout->addStretch();
    layout->addWidget(openBtn);
    layout->addWidget(back);

    connect(openBtn, &QPushButton::clicked, this, [this] {
        const QString url =
            QStringLiteral("https://uri.amap.com/navigation?from=%1,%2,我的位置"
                           "&to=%3,%4,%5&mode=car")
                .arg(myLat_, 0, 'f', 6)
                .arg(myLng_, 0, 'f', 6)
                .arg(stLat_, 0, 'f', 6)
                .arg(stLng_, 0, 'f', 6)
                .arg(stName_);
        QDesktopServices::openUrl(QUrl(url));
    });
    connect(back, &QPushButton::clicked, this, &NavigationPage::backRequested);
}

void NavigationPage::openRoute(double myLat, double myLng, double stLat,
                               double stLng, const QString& stName) {
    myLat_ = myLat; myLng_ = myLng; stLat_ = stLat; stLng_ = stLng; stName_ = stName;
    const double km = kmBetween(myLat, myLng, stLat, stLng);
    info_->setText(
        QStringLiteral("起点(我的位置)：%1, %2\n终点：%3(%4, %5)\n\n直线距离约 %6 km\n"
                       "(演示导航：点下方按钮用外部地图打开)")
            .arg(myLat, 0, 'f', 4)
            .arg(myLng, 0, 'f', 4)
            .arg(stName)
            .arg(stLat, 0, 'f', 4)
            .arg(stLng, 0, 'f', 4)
            .arg(km, 0, 'f', 1));
}

}  // namespace client
}  // namespace ncs
