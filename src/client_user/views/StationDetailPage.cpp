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
        case ncs::DeviceState::Reserved:
            return QStringLiteral("预约");
        case ncs::DeviceState::Rebooting:
            return QStringLiteral("重启中");
    }
    return QStringLiteral("未知");
}
QString typeText(ncs::DeviceType t) {
    return t == ncs::DeviceType::Fast ? QStringLiteral("快充")
                                      : QStringLiteral("慢充");
}
QString activeMark(ncs::OrderStatus st) {
    switch (st) {
        case ncs::OrderStatus::Reserved:
            return QStringLiteral("我的·预约中");
        case ncs::OrderStatus::Charging:
            return QStringLiteral("我的·充电中");
        case ncs::OrderStatus::Completed:
            return QStringLiteral("我的·待支付");
        default:
            return QString();
    }
}
}  // namespace

StationDetailPage::StationDetailPage(IStationService* service, QWidget* parent)
    : QWidget(parent), service_(service) {
    setObjectName(QStringLiteral("stationDetailPage"));
    auto* layout = new QVBoxLayout(this);
    auto* heading = new QLabel(QStringLiteral("站内电桩"), this);
    info_ = new QLabel(QStringLiteral(""), this);
    info_->setObjectName(QStringLiteral("deviceStationInfo"));
    info_->setWordWrap(true);
    heading->setObjectName(QStringLiteral("deviceHeading"));
    heading->setAlignment(Qt::AlignCenter);

    list_ = new QListWidget(this);
    list_->setObjectName(QStringLiteral("deviceList"));
    auto* navBtn = new QPushButton(QStringLiteral("导航到站"), this);
    navBtn->setObjectName(QStringLiteral("btnNavigate"));
    backBtn_ = new QPushButton(QStringLiteral("返回"), this);
    backBtn_->setObjectName(QStringLiteral("btnDeviceBack"));
    status_ = new QLabel(this);
    status_->setObjectName(QStringLiteral("deviceStatus"));
    status_->setWordWrap(true);

    layout->addWidget(heading);
    layout->addWidget(info_);
    layout->addWidget(list_, 1);
    layout->addWidget(navBtn);
    layout->addWidget(backBtn_);
    layout->addWidget(status_);

    connect(backBtn_, &QPushButton::clicked, this,
            &StationDetailPage::backRequested);
    connect(navBtn, &QPushButton::clicked, this,
            &StationDetailPage::navRequested);
    connect(list_, &QListWidget::itemClicked, this,
            [this](QListWidgetItem* item) {
                emit deviceChosen(item->data(Qt::UserRole).toInt());
            });
}

void StationDetailPage::setStation(const ncs::Station& st) {
    stationName_ = st.name;
    stationLat_ = st.latitude;
    stationLng_ = st.longitude;
    info_->setText(QStringLiteral("%1 · %2").arg(st.name).arg(st.address));
}

void StationDetailPage::load(int stationId) {
    currentStation_ = stationId;
    list_->clear();
    cachedDevices_.clear();
    if (!service_) {
        status_->setText(QStringLiteral("未配置站服务"));
        return;
    }
    status_->setText(QStringLiteral("加载中…"));
    service_->listDevices(
        stationId,
        [this](const QVector<ncs::Device>& devices, const QString& err) {
            if (!err.isEmpty()) {
                cachedDevices_.clear();
                list_->clear();
                status_->setText(QStringLiteral("加载失败：") + err);
                return;
            }
            cachedDevices_ = devices;
            repopulate();
        });
}

void StationDetailPage::setMyActive(const QVector<ncs::Order>& activeOrders) {
    myActive_.clear();
    for (const auto& o : activeOrders)
        if (o.id > 0 && o.deviceId > 0 &&
            (o.status == ncs::OrderStatus::Reserved ||
             o.status == ncs::OrderStatus::Charging ||
             o.status == ncs::OrderStatus::Completed))
            myActive_.insert(o.deviceId, o.status);
    repopulate();
}

void StationDetailPage::repopulate() {
    list_->clear();
    if (cachedDevices_.isEmpty()) {
        status_->setText(currentStation_ > 0 ? QStringLiteral("该站暂无电桩")
                                             : QString());
        return;
    }
    status_->setText(QStringLiteral("共 %1 个电桩(带“我的”可直接点进实时)").arg(cachedDevices_.size()));
    for (const auto& d : cachedDevices_) {
        QString line = QStringLiteral("桩 #%1 · %2 · %3 · %4 kW")
                           .arg(d.id)
                           .arg(typeText(d.type))
                           .arg(stateText(d.state))
                           .arg(d.powerKw, 0, 'f', 1);
        const auto it = myActive_.constFind(d.id);
        if (it != myActive_.constEnd()) {
            const QString mark = activeMark(it.value());
            if (!mark.isEmpty())
                line += QStringLiteral(" · ") + mark;
        }
        auto* item = new QListWidgetItem(line, list_);
        item->setData(Qt::UserRole, d.id);
        list_->addItem(item);
    }
}

}  // namespace client
}  // namespace ncs
