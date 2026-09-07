#pragma once

#include <QList>
#include <QWidget>

#include "core/service/StationService.h"

class QPushButton;
class QTimer;

// 简化地图(兜底方案):柔和浅灰底 + 网格 + 电站标记 + 用户定位。
// 支持点击电站标记进入详情;无腾讯地图 Key/网络时使用。
class MapWidget : public QWidget
{
    Q_OBJECT
public:
    explicit MapWidget(QWidget *parent = nullptr);

    void setData(const QList<Station> &stations, double userLat, double userLon);

signals:
    void stationClicked(int stationId);

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;

private:
    void repositionLocateButton();
    void updateBounds();
    QPointF project(double lat, double lon) const;

    QList<Station> m_stations;
    double m_userLat = 0.0;
    double m_userLon = 0.0;
    double m_minLat = 0.0, m_maxLat = 0.0;
    double m_minLon = 0.0, m_maxLon = 0.0;

    QPushButton *m_locateBtn = nullptr;
    QTimer *m_pulseTimer = nullptr;
    qreal m_pulse = 0.0;
};
