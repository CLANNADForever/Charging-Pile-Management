#ifndef NCS_CLIENT_VIEWS_STATIONLISTPAGE_H
#define NCS_CLIENT_VIEWS_STATIONLISTPAGE_H

#include <QWidget>

class QListWidget;
class QPushButton;
class QLabel;

namespace ncs {
namespace client {

class IStationService;

// 找桩页：列出站点；单击某站发 stationChosen，由 MainWindow 切到站内桩明细。
class StationListPage : public QWidget {
    Q_OBJECT
public:
    explicit StationListPage(IStationService* service, QWidget* parent = nullptr);

    void refresh();

signals:
    void stationChosen(int stationId);

private:
    IStationService* service_ = nullptr;
    QListWidget* list_ = nullptr;
    QPushButton* refreshBtn_ = nullptr;
    QLabel* status_ = nullptr;
};

}  // namespace client
}  // namespace ncs

#endif  // NCS_CLIENT_VIEWS_STATIONLISTPAGE_H
