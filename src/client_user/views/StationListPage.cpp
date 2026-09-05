#include "StationListPage.h"

#include <QLabel>
#include <QListWidget>
#include <QListWidgetItem>
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
    auto* heading = new QLabel(QStringLiteral("周边充电站(点站看桩)"), this);
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

    connect(refreshBtn_, &QPushButton::clicked, this, &StationListPage::refresh);
    connect(list_, &QListWidget::itemClicked, this,
            [this](QListWidgetItem* item) {
                emit stationChosen(item->data(Qt::UserRole).toInt());
            });
    refresh();
}

void StationListPage::refresh() {
    list_->clear();
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
            status_->setText(QStringLiteral("共 %1 个站点(单击看桩)").arg(stations.size()));
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
        });
}

}  // namespace client
}  // namespace ncs
