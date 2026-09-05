#include "StationListPage.h"

#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <cmath>

#include "services/IStationService.h"
#include "money.h"

namespace ncs {
namespace client {

namespace {
double haversineKm(double lat1, double lng1, double lat2, double lng2) {
    constexpr double kR = 6371.0;
    const double p = 3.14159265358979323846 / 180.0;
    const double dLat = (lat2 - lat1) * p;
    const double dLng = (lng2 - lng1) * p;
    const double a = std::sin(dLat / 2) * std::sin(dLat / 2) +
                     std::cos(lat1 * p) * std::cos(lat2 * p) *
                         std::sin(dLng / 2) * std::sin(dLng / 2);
    const double c = 2.0 * std::atan2(std::sqrt(a), std::sqrt(1.0 - a));
    return kR * c;
}
}  // namespace

StationListPage::StationListPage(IStationService* service, QWidget* parent)
    : QWidget(parent), service_(service) {
    setObjectName(QStringLiteral("stationPage"));
    auto* layout = new QVBoxLayout(this);

    auto* heading = new QLabel(QStringLiteral("周边充电站(点站看桩)"), this);
    heading->setObjectName(QStringLiteral("stationHeading"));
    heading->setAlignment(Qt::AlignCenter);

    auto* posRow = new QHBoxLayout;
    posRow->addWidget(new QLabel(QStringLiteral("我的位置"), this));
    latEdit_ = new QLineEdit(this);
    latEdit_->setObjectName(QStringLiteral("myLatEdit"));
    latEdit_->setPlaceholderText(QStringLiteral("纬度"));
    lngEdit_ = new QLineEdit(this);
    lngEdit_->setObjectName(QStringLiteral("myLngEdit"));
    lngEdit_->setPlaceholderText(QStringLiteral("经度"));
    latEdit_->setText(QStringLiteral("39.90"));
    lngEdit_->setText(QStringLiteral("116.32"));
    posRow->addWidget(latEdit_);
    posRow->addWidget(lngEdit_);
    connect(latEdit_, &QLineEdit::editingFinished, this,
            &StationListPage::refresh);
    connect(lngEdit_, &QLineEdit::editingFinished, this,
            &StationListPage::refresh);

    list_ = new QListWidget(this);
    list_->setObjectName(QStringLiteral("stationList"));
    refreshBtn_ = new QPushButton(QStringLiteral("刷新"), this);
    refreshBtn_->setObjectName(QStringLiteral("btnStationRefresh"));
    status_ = new QLabel(this);
    status_->setObjectName(QStringLiteral("stationStatus"));
    status_->setWordWrap(true);

    layout->addWidget(heading);
    layout->addLayout(posRow);
    layout->addWidget(list_, 1);
    layout->addWidget(refreshBtn_);
    layout->addWidget(status_);

    connect(refreshBtn_, &QPushButton::clicked, this, &StationListPage::refresh);
    connect(list_, &QListWidget::itemClicked, this,
            [this](QListWidgetItem* item) {
                emit stationChosen(item->data(Qt::UserRole).toInt());
            });
    refresh();
}

const ncs::Station* StationListPage::stationById(int id) const {
    for (const auto& st : last_)
        if (st.id == id)
            return &st;
    return nullptr;
}

void StationListPage::readMyPosition() {
    myLat_ = latEdit_->text().toDouble();
    myLng_ = lngEdit_->text().toDouble();
}

void StationListPage::refresh() {
    list_->clear();
    readMyPosition();
    if (!service_) {
        status_->setText(QStringLiteral("未配置站服务"));
        return;
    }
    status_->setText(QStringLiteral("加载中…"));
    service_->listStations(
        [this](const QVector<ncs::Station>& stations, const QString& err) {
            list_->clear();
            if (!err.isEmpty()) {
                status_->setText(QStringLiteral("加载失败：") + err);
                return;
            }
            if (stations.isEmpty()) {
                status_->setText(QStringLiteral("暂无站点"));
                return;
            }
            last_ = stations;
            status_->setText(
                QStringLiteral("共 %1 个站点(单击看桩)").arg(stations.size()));
            for (const auto& s : stations) {
                const double km = haversineKm(myLat_, myLng_, s.latitude,
                                              s.longitude);
                const QString line =
                    QStringLiteral("%1 · 约 %2 km\n%3 · 空闲 %4/%5 · %6 元/度")
                        .arg(s.name)
                        .arg(km, 0, 'f', 1)
                        .arg(s.address)
                        .arg(s.freePiles)
                        .arg(s.totalPiles)
                        .arg(format_cents(s.pricePerKwhCents));
                auto* item = new QListWidgetItem(line, list_);
                item->setData(Qt::UserRole, s.id);
                list_->addItem(item);
            }
        });
}

}  // namespace client
}  // namespace ncs
