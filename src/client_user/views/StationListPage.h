#ifndef NCS_CLIENT_VIEWS_STATIONLISTPAGE_H
#define NCS_CLIENT_VIEWS_STATIONLISTPAGE_H

#include <QWidget>

class QListWidget;
class QPushButton;
class QLabel;

namespace ncs {
namespace client {

class IStationService;

// 找桩页(占位)：拉站列表展示，美观交给后续同学前端壳。进入页面时 refresh()。
class StationListPage : public QWidget {
    Q_OBJECT
public:
    explicit StationListPage(IStationService* service, QWidget* parent = nullptr);

    void refresh();

private:
    IStationService* service_ = nullptr;
    QListWidget* list_ = nullptr;
    QPushButton* refreshBtn_ = nullptr;
    QLabel* status_ = nullptr;
};

}  // namespace client
}  // namespace ncs

#endif  // NCS_CLIENT_VIEWS_STATIONLISTPAGE_H
