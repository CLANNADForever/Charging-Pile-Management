#include "StationDetailPage.h"

#include <QLabel>
#include <QListWidget>
#include <QListWidgetItem>
#include <QPushButton>
#include <QVBoxLayout>

#include "entities.h"
#include "services/IStationService.h"

namespace ncs {
namespace client {

namespace {
QString stateText(ncs::DeviceState s) {
    switch (s) {
        case ncs::DeviceState::Idle:
            return QStringLiteral("空闲");
        case ncs::DeviceState::Charging:
            return QStringLiteral("充电中");
        case ncs::DeviceState::Fault:
            return QStringLiteral("故障");
    }
    return QStringLiteral("未知");
}
QString typeText(ncs::DeviceType t) {
    return t == ncs::DeviceType::Fast ? QStringLiteral("快充")
                                      : QStringLiteral("慢充");
}
}  // namespace

StationDetailPage::StationDetailPage(IStationService* service, QWidget* parent)
    : QWidget(parent), service_(service) {
    setObjectName(QStringLiteral("stationDetailPage"));
    auto* layout = new QVBoxLayout(this);
    auto* heading = new QLabel(QStringLiteral("站内电桩"), this);
    heading->setObjectName(QStringLiteral("deviceHeading"));
    heading->setAlignment(Qt::AlignCenter);

    list_ = new QListWidget(this);
    list_->setObjectName(QStringLiteral("deviceList"));
    backBtn_ = new QPushButton(QStringLiteral("返回"), this);
    backBtn_->setObjectName(QStringLiteral("btnDeviceBack"));
    status_ = new QLabel(this);
    status_->setObjectName(QStringLiteral("deviceStatus"));
    status_->setWordWrap(true);

    layout->addWidget(heading);
    layout->addWidget(list_, 1);
    layout->addWidget(backBtn_);
    layout->addWidget(status_);

    connect(backBtn_, &QPushButton::clicked, this,
            &StationDetailPage::backRequested);
    connect(list_, &QListWidget::itemClicked, this,
            [this](QListWidgetItem* item) {
                emit deviceChosen(item->data(Qt::UserRole).toInt());
            });
}

void StationDetailPage::load(int stationId) {
    currentStation_ = stationId;
    list_->clear();
    if (!service_) {
        status_->setText(QStringLiteral("未配置站服务"));
        return;
    }
    status_->setText(QStringLiteral("加载中…"));
    service_->listDevices(
        stationId, [this](const QVector<ncs::Device>& devices, const QString& err) {
            list_->clear();
            if (!err.isEmpty()) {
                status_->setText(QStringLiteral("加载失败：") + err);
                return;
            }
            if (devices.isEmpty()) {
                status_->setText(QStringLiteral("该站暂无电桩"));
                return;
            }
            status_->setText(QStringLiteral("共 %1 个电桩").arg(devices.size()));
            for (const auto& d : devices) {
                const QString line =
                    QStringLiteral("桩 #%1 · %2 · %3 · %4 kW")
                        .arg(d.id)
                        .arg(typeText(d.type))
                        .arg(stateText(d.state))
                        .arg(d.powerKw, 0, 'f', 1);
                {
                auto* item = new QListWidgetItem(line, list_);
                item->setData(Qt::UserRole, d.id);
                list_->addItem(item);
            }
            }
        });
}

}  // namespace client
}  // namespace ncs
