#pragma once

#include <QString>

#include "Page.h"
#include "core/service/StationService.h"

class QWebEngineView;

// 一键导航 (UC-U-04):出行方式 + 内嵌地图实时路线 + 系统浏览器兜底。
class MapPage : public Page
{
    Q_OBJECT
public:
    explicit MapPage(int stationId, QWidget *parent = nullptr);

private:
    void openInBrowser();
    void refreshRoute();

    Station m_station;
    QString m_mode = QStringLiteral("drive"); // drive / walk / bus
    QWebEngineView *m_view = nullptr;
};
