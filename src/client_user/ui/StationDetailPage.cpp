#include "StationDetailPage.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>

#include "common/Utils.h"
#include "core/service/StationService.h"

StationDetailPage::StationDetailPage(int stationId, QWidget *parent)
    : Page(parent)
    , m_station(StationService::instance().stationDetail(stationId))
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    root->addWidget(makeHeader(m_station.name.isEmpty() ? QStringLiteral("电站详情") : m_station.name));

    auto *body = new QVBoxLayout;
    body->setContentsMargins(16, 12, 16, 12);
    body->setSpacing(12);

    const auto loc = StationService::instance().currentLocation();
    const double distKm = StationService::haversineKm(loc.latitude, loc.longitude,
                                                      m_station.latitude, m_station.longitude);

    // 信息卡
    auto *infoCard = new QFrame(this);
    infoCard->setObjectName(QStringLiteral("card"));
    auto *infoLay = new QVBoxLayout(infoCard);
    infoLay->setContentsMargins(16, 14, 16, 14);
    infoLay->setSpacing(8);

    auto *addr = new QLabel(QStringLiteral("地址：") + m_station.address, infoCard);
    addr->setObjectName(QStringLiteral("valueLabel"));
    addr->setWordWrap(true);
    infoLay->addWidget(addr);

    auto *hours = new QLabel(QStringLiteral("开放时间：") + m_station.openHours, infoCard);
    hours->setObjectName(QStringLiteral("hintLabel"));
    infoLay->addWidget(hours);

    auto *fac = new QLabel(QStringLiteral("配套设施：") + m_station.facilities.join(QStringLiteral(" · ")), infoCard);
    fac->setObjectName(QStringLiteral("hintLabel"));
    fac->setWordWrap(true);
    infoLay->addWidget(fac);

    auto *price = new QLabel(QStringLiteral("充电价格：¥") + Utils::formatMoney(m_station.unitPrice)
                                 + QStringLiteral("/度"), infoCard);
    price->setStyleSheet(QStringLiteral("color:#2F80FF; font-size:15px; font-weight:bold;"));
    infoLay->addWidget(price);

    body->addWidget(infoCard);

    // 电桩列表
    auto *tableTitle = new QLabel(QStringLiteral("空闲情况"), this);
    tableTitle->setObjectName(QStringLiteral("sectionTitle"));
    body->addWidget(tableTitle);

    const QList<Charger> chargers = StationService::instance().chargersByStation(m_station.id);
    const QStringList statusText = { QStringLiteral("空闲"), QStringLiteral("使用中"),
        QStringLiteral("故障"), QStringLiteral("预约中"), QStringLiteral("重启中") };
    const QStringList statusColor = { QStringLiteral("#00B368"), QStringLiteral("#FF9500"),
        QStringLiteral("#EF4444"), QStringLiteral("#7C5CFF"), QStringLiteral("#00A6CF") };

    auto *scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    auto *chargerContainer = new QWidget(scrollArea);
    auto *chargerLay = new QVBoxLayout(chargerContainer);
    chargerLay->setContentsMargins(0, 0, 0, 0);
    chargerLay->setSpacing(10);
    scrollArea->setWidget(chargerContainer);

    for (const Charger &c : chargers) {
        auto *row = new QFrame(chargerContainer);
        row->setObjectName(QStringLiteral("card"));
        row->setFixedHeight(66);
        auto *rowLay = new QVBoxLayout(row);
        rowLay->setContentsMargins(14, 12, 14, 12);
        rowLay->setSpacing(6);

        auto *top = new QHBoxLayout;
        top->setSpacing(8);
        auto *dot = new QLabel(row);
        dot->setFixedSize(10, 10);
        dot->setStyleSheet(QStringLiteral("background:%1; border-radius:5px;")
                               .arg(statusColor.value(c.status, QStringLiteral("#8A8F99"))));
        auto *codeLabel = new QLabel(c.code, row);
        codeLabel->setObjectName(QStringLiteral("sectionTitle"));
        auto *statusLabel = new QLabel(statusText.value(c.status, QStringLiteral("未知")), row);
        statusLabel->setStyleSheet(QStringLiteral("color:%1; font-weight:bold;")
                                       .arg(statusColor.value(c.status, QStringLiteral("#8A8F99"))));
        top->addWidget(dot);
        top->addWidget(codeLabel);
        top->addStretch(1);
        top->addWidget(statusLabel);
        rowLay->addLayout(top);

        auto *meta = new QLabel(QStringLiteral("%1 · %2kW · 累计%3次")
                                    .arg(c.type)
                                    .arg(c.power, 0, 'f', 0)
                                    .arg(c.totalCount), row);
        meta->setObjectName(QStringLiteral("hintLabel"));
        rowLay->addWidget(meta);

        chargerLay->addWidget(row);
    }
    chargerLay->addStretch();

    body->addWidget(scrollArea, 1);

    // 底栏:最低价 + 选桩充电
    auto *bottom = new QHBoxLayout;
    bottom->setSpacing(12);
    auto *lowest = new QLabel(QStringLiteral("最低 ¥") + Utils::formatMoney(m_station.unitPrice)
                                  + QStringLiteral("/度"), this);
    lowest->setObjectName(QStringLiteral("valueLabel"));

    auto *navBtn = new QPushButton(QStringLiteral("一键导航"), this);
    navBtn->setObjectName(QStringLiteral("ghostButton"));
    connect(navBtn, &QPushButton::clicked, this, [this]() {
        emit navRequested(m_station.id);
    });

    auto *chargeBtn = new QPushButton(QStringLiteral("选桩充电"), this);
    chargeBtn->setObjectName(QStringLiteral("primaryButton"));
    connect(chargeBtn, &QPushButton::clicked, this, [this]() {
        emit chargeRequested(m_station.id);
    });
    bottom->addWidget(lowest, 1);
    bottom->addWidget(navBtn, 1);
    bottom->addWidget(chargeBtn, 2);
    body->addLayout(bottom);

    root->addLayout(body, 1);
}
