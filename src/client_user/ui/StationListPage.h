#pragma once

#include <QHash>
#include <QList>

#include "Page.h"
#include "core/service/StationService.h"

class QComboBox;
class QLabel;
class QPushButton;
class QVBoxLayout;
class QWidget;
class MapWidget;

// 主页面(三态):地图 + 列表,靠拖拽手柄切换比例(UC-U-02)。
class StationListPage : public Page
{
    Q_OBJECT
public:
    explicit StationListPage(QWidget *parent = nullptr);

    // 刷新(切回首页时更新空闲数等)
    void refresh();

signals:
    void openSearch();
    void openStation(int stationId);
    void openCoupons();
    void openNavigation(int stationId);

private:
    void setMapRatio(qreal ratio);
    void snapTo(qreal target);
    void applyFilters();
    void rebuildList();
    QWidget *buildSearchBar();
    void buildListContent();

    QVBoxLayout *m_rootLayout = nullptr;
    MapWidget *m_map = nullptr;
    QWidget *m_listWidget = nullptr;
    QComboBox *m_sortCombo = nullptr;
    QComboBox *m_distanceCombo = nullptr;
    QPushButton *m_filterBtn = nullptr;
    QVBoxLayout *m_cardsLayout = nullptr;

    qreal m_mapRatio = 0.58;
    QList<Station> m_allStations;
    QList<Station> m_filteredStations;
    QHash<int, double> m_predictedFree;  // stationId -> 预测空闲率
    double m_userLat = 0.0;
    double m_userLon = 0.0;
    double m_maxDistanceKm = -1.0; // -1 不限
    QStringList m_activeFacilities;
};
