#include "MapPage.h"

#include <QDesktopServices>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QRadioButton>
#include <QUrl>
#include <QVBoxLayout>

#include "core/service/StationService.h"

MapPage::MapPage(int stationId, QWidget *parent)
    : Page(parent)
    , m_station(StationService::instance().stationDetail(stationId))
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);
    root->addWidget(makeHeader(QStringLiteral("导航")));

    auto *body = new QVBoxLayout;
    body->setContentsMargins(16, 16, 16, 16);
    body->setSpacing(16);

    // 出行方式
    auto *modeRow = new QHBoxLayout;
    modeRow->setSpacing(12);
    auto *drive = new QRadioButton(QStringLiteral("驾车"), this);
    auto *walk = new QRadioButton(QStringLiteral("步行"), this);
    auto *bus = new QRadioButton(QStringLiteral("公交"), this);
    drive->setChecked(true);
    connect(drive, &QRadioButton::toggled, this, [this](bool c) { if (c) m_mode = QStringLiteral("drive"); });
    connect(walk, &QRadioButton::toggled, this, [this](bool c) { if (c) m_mode = QStringLiteral("walk"); });
    connect(bus, &QRadioButton::toggled, this, [this](bool c) { if (c) m_mode = QStringLiteral("bus"); });
    modeRow->addWidget(drive);
    modeRow->addWidget(walk);
    modeRow->addWidget(bus);
    modeRow->addStretch();
    body->addLayout(modeRow);

    // 起终点信息卡(方案 B:无内嵌地图时的兜底)
    const auto loc = StationService::instance().currentLocation();
    const double distKm = StationService::haversineKm(loc.latitude, loc.longitude,
                                                      m_station.latitude, m_station.longitude);

    auto *card = new QFrame(this);
    card->setObjectName(QStringLiteral("card"));
    auto *cardLay = new QVBoxLayout(card);
    cardLay->setContentsMargins(16, 16, 16, 16);
    cardLay->setSpacing(10);

    auto addRow = [cardLay](const QString &k, const QString &v) {
        auto *row = new QHBoxLayout;
        auto *kl = new QLabel(k);
        kl->setObjectName(QStringLiteral("hintLabel"));
        auto *vl = new QLabel(v);
        vl->setObjectName(QStringLiteral("valueLabel"));
        vl->setWordWrap(true);
        row->addWidget(kl);
        row->addWidget(vl, 1);
        cardLay->addLayout(row);
    };
    addRow(QStringLiteral("起点"), loc.label);
    addRow(QStringLiteral("终点"), m_station.name);
    addRow(QStringLiteral("直线距离"), QString::number(distKm, 'f', 1) + QStringLiteral(" km"));
    addRow(QStringLiteral("终点坐标"), QStringLiteral("%1, %2")
                                         .arg(m_station.latitude, 0, 'f', 6)
                                         .arg(m_station.longitude, 0, 'f', 6));

    body->addWidget(card);
    body->addStretch(1);

    auto *openBtn = new QPushButton(QStringLiteral("在系统浏览器中打开"), this);
    openBtn->setObjectName(QStringLiteral("primaryButton"));
    connect(openBtn, &QPushButton::clicked, this, &MapPage::openInBrowser);
    body->addWidget(openBtn);

    root->addLayout(body, 1);
}

void MapPage::openInBrowser()
{
    const QString type = (m_mode == QStringLiteral("walk")) ? QStringLiteral("walk")
        : (m_mode == QStringLiteral("bus")) ? QStringLiteral("bus") : QStringLiteral("drive");
    const QString url = QStringLiteral(
        "https://apis.map.qq.com/uri/v1/routeplan?type=%1&tocoord=%2,%3&to=%4")
        .arg(type)
        .arg(m_station.latitude, 0, 'f', 6)
        .arg(m_station.longitude, 0, 'f', 6)
        .arg(m_station.name);
    QDesktopServices::openUrl(QUrl(url));
}
