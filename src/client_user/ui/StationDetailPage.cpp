#include "StationDetailPage.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QTableWidget>
#include <QHeaderView>
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
    auto *table = new QTableWidget(chargers.size(), 5, this);
    table->setHorizontalHeaderLabels({ QStringLiteral("编号"), QStringLiteral("类型"),
        QStringLiteral("功率"), QStringLiteral("状态"), QStringLiteral("累计次数") });
    table->verticalHeader()->setVisible(false);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setSelectionMode(QAbstractItemView::NoSelection);
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);

    const QStringList statusText = { QStringLiteral("空闲"), QStringLiteral("使用中"),
        QStringLiteral("故障"), QStringLiteral("预约中"), QStringLiteral("重启中") };
    const QStringList statusColor = { QStringLiteral("#00B368"), QStringLiteral("#FF9500"),
        QStringLiteral("#EF4444"), QStringLiteral("#7C5CFF"), QStringLiteral("#00A6CF") };

    for (int i = 0; i < chargers.size(); ++i) {
        const Charger &c = chargers.at(i);
        auto *cell0 = new QTableWidgetItem(c.code);
        auto *cell1 = new QTableWidgetItem(c.type);
        auto *cell2 = new QTableWidgetItem(QStringLiteral("%1kW").arg(c.power, 0, 'f', 0));
        auto *cell3 = new QTableWidgetItem(statusText.value(c.status, QStringLiteral("未知")));
        cell3->setForeground(QColor(statusColor.value(c.status, QStringLiteral("#8A8F99"))));
        auto *cell4 = new QTableWidgetItem(QString::number(c.totalCount));
        table->setItem(i, 0, cell0);
        table->setItem(i, 1, cell1);
        table->setItem(i, 2, cell2);
        table->setItem(i, 3, cell3);
        table->setItem(i, 4, cell4);
    }
    body->addWidget(table, 1);

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
