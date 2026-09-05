#ifndef NCS_CLIENT_VIEWS_STATIONLISTPAGE_H
#define NCS_CLIENT_VIEWS_STATIONLISTPAGE_H

#include <QVector>
#include <QWidget>
#include "entities.h"

class QListWidget;
class QPushButton;
class QLabel;
class QLineEdit;

namespace ncs {
namespace client {

class IStationService;

// 找桩页：列出站点(带"我的位置"直线距离)，单击某站看桩。
class StationListPage : public QWidget {
    Q_OBJECT
public:
    explicit StationListPage(IStationService* service, QWidget* parent = nullptr);

    void refresh();
    const ncs::Station* stationById(int id) const;
    double myLat() const { return myLat_; }
    double myLng() const { return myLng_; }

signals:
    void stationChosen(int stationId);

private:
    void readMyPosition();

    IStationService* service_ = nullptr;
    double myLat_ = 39.90;
    double myLng_ = 116.32;
    QLineEdit* latEdit_ = nullptr;
    QLineEdit* lngEdit_ = nullptr;
    QVector<ncs::Station> last_;
    QListWidget* list_ = nullptr;
    QPushButton* refreshBtn_ = nullptr;
    QLabel* status_ = nullptr;
};

}  // namespace client
}  // namespace ncs

#endif  // NCS_CLIENT_VIEWS_STATIONLISTPAGE_H
