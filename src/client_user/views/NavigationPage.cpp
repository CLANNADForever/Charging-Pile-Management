#include "NavigationPage.h"

#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QDesktopServices>
#include <QUrl>

#include <QtWebEngineWidgets/QWebEngineView>

namespace ncs {
namespace client {

NavigationPage::NavigationPage(QWidget* parent) : QWidget(parent) {
    setObjectName(QStringLiteral("navigationPage"));
    layout_ = new QVBoxLayout(this);
    auto* heading = new QLabel(QStringLiteral("路线(我的位置 → 电站)"), this);
    heading->setObjectName(QStringLiteral("navHeading"));
    heading->setAlignment(Qt::AlignCenter);
    info_ = new QLabel(this);
    info_->setObjectName(QStringLiteral("navInfo"));
    info_->setWordWrap(true);
    openBtn_ = new QPushButton(QStringLiteral("用外部地图打开"), this);
    openBtn_->setObjectName(QStringLiteral("btnNavExternal"));
    auto* back = new QPushButton(QStringLiteral("返回"), this);
    back->setObjectName(QStringLiteral("btnNavBack"));

    layout_->addWidget(heading);
    layout_->addWidget(info_);
    layout_->addStretch();
    layout_->addWidget(openBtn_);
    layout_->addWidget(back);

    connect(openBtn_, &QPushButton::clicked, this, [this] {
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
    ensureView();
    info_->setText(QStringLiteral("%1 → %2")
                       .arg(QStringLiteral("我的位置"))
                       .arg(stName));
    const QString html = QStringLiteral(R"HTML(
<!doctype html><html><head><meta charset="utf-8">
<link rel="stylesheet" href="https://cdn.jsdelivr.net/npm/leaflet@1.9.4/dist/leaflet.css"/>
<script src="https://cdn.jsdelivr.net/npm/leaflet@1.9.4/dist/leaflet.js"></script>
<style>html,body,#map{height:100%;margin:0}</style></head><body>
<div id="map"></div><script>
var a=[%1,%2], b=[%3,%4];
var map=L.map('map').setView([(a[0]+b[0])/2,(a[1]+b[1])/2],12);
L.tileLayer('https://tile.openstreetmap.org/{z}/{x}/{y}.png',{maxZoom:18}).addTo(map);
L.marker(a).addTo(map).bindPopup('我的位置').openPopup();
L.marker(b).addTo(map).bindPopup('%5');
L.polyline([a,b],{color:'#1a73e8'}).addTo(map);
</script></body></html>
)HTML")
        .arg(myLat_, 0, 'f', 6)
        .arg(myLng_, 0, 'f', 6)
        .arg(stLat_, 0, 'f', 6)
        .arg(stLng_, 0, 'f', 6)
        .arg(stName_.toHtmlEscaped());
    view_->setHtml(html, QUrl(QStringLiteral("https://local.map/")));
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
