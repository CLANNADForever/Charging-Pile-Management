#pragma once

#include "Page.h"
#include "core/service/StationService.h"

// 电站详情页 (UC-U-03)。
class StationDetailPage : public Page
{
    Q_OBJECT
public:
    explicit StationDetailPage(int stationId, QWidget *parent = nullptr);

signals:
    void chargeRequested(int stationId);
    void navRequested(int stationId);

private:
    Station m_station;
};
