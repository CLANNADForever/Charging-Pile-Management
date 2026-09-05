#include "StationListPage.h"

#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>

#include "services/IStationService.h"
#include "money.h"

namespace ncs {
namespace client {

StationListPage::StationListPage(IStationService* service, QWidget* parent)
    : QWidget(parent), service_(service) {
    setObjectName(QStringLiteral("stationPage"));

    auto* layout = new QVBoxLayout(this);
    auto* heading = new QLabel(QStringLiteral("周边充电站"), this);
    heading->setObjectName(QStringLiteral("stationHeading"));
    heading->setAlignment(Qt::AlignCenter);

    list_ = new QListWidget(this);
    list_->setObjectName(QStringLiteral("stationList"));

    refreshBtn_ = new QPushButton(QStringLiteral("刷新"), this);
    refreshBtn_->setObjectName(QStringLiteral("btnStationRefresh"));

    status_ = new QLabel(this);
    status_->setObjectName(QStringLiteral("stationStatus"));
    status_->setWordWrap(true);

    layout->addWidget(heading);
    layout->addWidget(list_, 1);
    layout->addWidget(refreshBtn_);
    layout->addWidget(status_);

    connect(refreshBtn_, &QPushButton::clicked, this,
            &StationListPage::refresh);
    refresh();
}

void StationListPage::refresh() {
    list_->clear();
    if (!service_) {
        status_->setText(QStringLiteral("未配置站服务"));
        return;
    }
    const auto stations = service_->listStations();
    if (stations.isEmpty()) {
        status_->setText(QStringLiteral("暂无站点或加载失败(请确认后端已启动)"));
        return;
    }
    status_->setText(QStringLiteral("共 %1 个站点").arg(stations.size()));
    for (const auto& s : stations) {
        const QString line =
            QStringLiteral("%1 · %2 · 空闲 %3/%4 · %5 元/度")
                .arg(s.name)
                .arg(s.address)
                .arg(s.freePiles)
                .arg(s.totalPiles)
                .arg(format_cents(s.pricePerKwhCents));
        auto* item = new QListWidgetItem(line, list_);
        item->setData(Qt::UserRole, s.id);
        list_->addItem(item);
    }
}

}  // namespace client
}  // namespace ncs
