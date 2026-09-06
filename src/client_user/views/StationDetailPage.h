#ifndef NCS_CLIENT_VIEWS_STATIONDETAILPAGE_H
#define NCS_CLIENT_VIEWS_STATIONDETAILPAGE_H

#include <QMap>
#include <QWidget>

#include "entities.h"

class QListWidget;
class QPushButton;
class QLabel;

namespace ncs {
namespace client {

class IStationService;

// 站内电桩明细页：标注"我的"活跃充电桩(device->该单状态)；点"返回"回站列表。
class StationDetailPage : public QWidget {
    Q_OBJECT
public:
    explicit StationDetailPage(IStationService* service, QWidget* parent = nullptr);

    void load(int stationId);
    void setStation(const ncs::Station& st);
    // 从"我的活跃订单"里按 device_id 交集标注(Reserved/Charging/Completed)
    void setMyActive(const QVector<ncs::Order>& activeOrders);

signals:
    void backRequested();
    void navRequested();
    void deviceChosen(int deviceId);

private:
    void repopulate();
    IStationService* service_ = nullptr;
    QString stationName_;
    double stationLat_ = 0, stationLng_ = 0;
    int currentStation_ = 0;
    QVector<ncs::Device> cachedDevices_;
    QMap<int, ncs::OrderStatus> myActive_;  // deviceId -> 我的活跃单状态
    class QLabel* info_ = nullptr;
    QListWidget* list_ = nullptr;
    QPushButton* backBtn_ = nullptr;
    QLabel* status_ = nullptr;
};

}  // namespace client
}  // namespace ncs

#endif  // NCS_CLIENT_VIEWS_STATIONDETAILPAGE_H
