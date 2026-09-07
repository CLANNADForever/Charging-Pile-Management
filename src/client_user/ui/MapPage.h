#pragma once

#include <QString>

#include "Page.h"
#include "core/service/StationService.h"

// 一键导航 (UC-U-04):出行方式 + 起终点信息 + 系统浏览器打开(兜底方案 B)。
class MapPage : public Page
{
    Q_OBJECT
public:
    explicit MapPage(int stationId, QWidget *parent = nullptr);

private:
    void openInBrowser();

    Station m_station;
    QString m_mode = QStringLiteral("drive"); // drive / walk / bus
};
