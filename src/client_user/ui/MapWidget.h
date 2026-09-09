#pragma once

#include <QList>
#include <QWidget>

#include "core/service/StationService.h"

class QWebEngineView;
class QWebChannel;
class QPushButton;

// 地图浏览(UC-U-05):内嵌腾讯地图(QWebEngineView)展示电站 + 用户定位。
// 点击电站标记回传 stationClicked;无 Key/网络时页面自带兜底提示。
class MapWidget : public QWidget
{
    Q_OBJECT
public:
    explicit MapWidget(QWidget *parent = nullptr);

    void setData(const QList<Station> &stations, double userLat, double userLon);

signals:
    void stationClicked(int stationId);

protected:
    void resizeEvent(QResizeEvent *event) override;

private:
    void injectData();

    QWebEngineView *m_view = nullptr;
    QWebChannel *m_channel = nullptr;
    QPushButton *m_locateBtn = nullptr;
    QList<Station> m_stations;
    double m_userLat = 0.0;
    double m_userLon = 0.0;
    bool m_loaded = false;
};
