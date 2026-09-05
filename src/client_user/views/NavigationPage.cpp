#include "NavigationPage.h"

#include <QDesktopServices>
#include <QLabel>
#include <QPushButton>
#include <QUrl>
#include <QVBoxLayout>

#include <QtWebEngineWidgets/QWebEngineView>

namespace ncs {
namespace client {

NavigationPage::NavigationPage(QWidget* parent) : QWidget(parent) {
    setObjectName(QStringLiteral("navigationPage"));
    layout_ = new QVBoxLayout(this);
    auto* heading = new QLabel(QStringLiteral("路线(腾讯地图 · 我的位置 → 电站)"), this);
    heading->setObjectName(QStringLiteral("navHeading"));
    heading->setAlignment(Qt::AlignCenter);
    info_ = new QLabel(this);
    info_->setObjectName(QStringLiteral("navInfo"));
    info_->setWordWrap(true);
    auto* openBtn = new QPushButton(QStringLiteral("用外部浏览器打开"), this);
    openBtn->setObjectName(QStringLiteral("btnNavExternal"));
    auto* back = new QPushButton(QStringLiteral("返回"), this);
    back->setObjectName(QStringLiteral("btnNavBack"));

    layout_->addWidget(heading);
    layout_->addWidget(info_);
    layout_->addStretch();
    layout_->addWidget(openBtn);
    layout_->addWidget(back);

    connect(openBtn, &QPushButton::clicked, this, [this] {
        QDesktopServices::openUrl(QUrl(routeUrl()));
    });
    connect(back, &QPushButton::clicked, this, &NavigationPage::backRequested);
}

QString NavigationPage::routeUrl() const {
    // 腾讯地图 URI 规划(免 key)；coord_type=gcj02
    return QStringLiteral(
               "https://apis.map.qq.com/uri/v1/routeplan"
               "?type=car&from=%1,%2,%3&to=%4,%5,%6"
               "&coord_type=gcj02&policy=1&referer=ncs-charge")
        .arg(myLat_, 0, 'f', 6)
        .arg(myLng_, 0, 'f', 6)
        .arg(QStringLiteral("我的位置"))
        .arg(stLat_, 0, 'f', 6)
        .arg(stLng_, 0, 'f', 6)
        .arg(stName_);
}

void NavigationPage::openRoute(double myLat, double myLng, double stLat,
                               double stLng, const QString& stName) {
    myLat_ = myLat; myLng_ = myLng; stLat_ = stLat; stLng_ = stLng; stName_ = stName;
    info_->setText(QStringLiteral("我的位置(%1,%2) → %3(%4,%5)")
                       .arg(myLat, 0, 'f', 4)
                       .arg(myLng, 0, 'f', 4)
                       .arg(stName)
                       .arg(stLat, 0, 'f', 4)
                       .arg(stLng, 0, 'f', 4));
    ensureView();
    view_->load(QUrl(routeUrl()));
}

void NavigationPage::ensureView() {
    if (view_)
        return;
    view_ = new QWebEngineView(this);
    view_->setObjectName(QStringLiteral("navMapView"));
    layout_->insertWidget(2, view_, 1);
}

}  // namespace client
}  // namespace ncs
