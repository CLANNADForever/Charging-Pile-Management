#pragma once

#include <QFrame>

#include "core/service/StationService.h"

// 电站卡片:展示站名/功率/设施/价格/优惠/距离/占用情况。
// 点击卡片进入详情,点击「距离」一键导航。
class StationCard : public QFrame
{
    Q_OBJECT
public:
    StationCard(const Station &station, double distanceKm,
                const QStringList &powerTypes, int freeCount, int totalCount,
                QWidget *parent = nullptr);

signals:
    void clicked(int stationId);
    void navClicked(int stationId);

protected:
    void mousePressEvent(QMouseEvent *event) override;

private:
    int m_stationId = 0;
};
