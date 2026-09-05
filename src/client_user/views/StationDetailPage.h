#ifndef NCS_CLIENT_VIEWS_STATIONDETAILPAGE_H
#define NCS_CLIENT_VIEWS_STATIONDETAILPAGE_H

#include <QWidget>

#include "entities.h"

class QListWidget;
class QPushButton;
class QLabel;

namespace ncs {
namespace client {

class IStationService;

// 站内电桩明细页：点"返回"回站列表。
class StationDetailPage : public QWidget {
    Q_OBJECT
public:
    explicit StationDetailPage(IStationService* service, QWidget* parent = nullptr);

    void load(int stationId);
    void setStation(const ncs::Station& st);

signals:
    void backRequested();
    void navRequested();
    void deviceChosen(int deviceId);

private:
    IStationService* service_ = nullptr;
    QString stationName_;
    double stationLat_ = 0, stationLng_ = 0;
    int currentStation_ = 0;
    class QLabel* info_ = nullptr;
    QListWidget* list_ = nullptr;
    QPushButton* backBtn_ = nullptr;
    QLabel* status_ = nullptr;
};

}  // namespace client
}  // namespace ncs

#endif  // NCS_CLIENT_VIEWS_STATIONDETAILPAGE_H
