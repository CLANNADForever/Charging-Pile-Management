#pragma once

#include <QHash>
#include <QList>
#include <QString>
#include <QStringList>

// 电站
struct Station
{
    int id = 0;
    QString name;
    QString address;
    double latitude = 0.0;
    double longitude = 0.0;
    double unitPrice = 0.0;   // 元/度
    QString openHours;        // 开放时间
    QStringList facilities;   // 配套设施
    bool hasCoupon = false;   // 优惠情况
    bool parkingFree = false; // 停车减免
};

// 电桩
struct Charger
{
    int id = 0;
    int stationId = 0;
    QString code;    // 编号,如 GG-01
    QString type;    // 超充/快充/慢充
    double power = 0.0; // kW
    int status = 0;       // 0 空闲 1 使用中 2 故障
    int totalCount = 0;   // 累计充电次数
    int totalMinutes = 0; // 累计充电时长(分钟)
};

// 电站/电桩服务。
// 阶段一/二:桩实现(内存假数据);后续接入数据库后替换内部实现。
class StationService
{
public:
    struct Location
    {
        double latitude = 0.0;
        double longitude = 0.0;
        QString label;
    };

    static StationService &instance();

    QList<Station> listStations() const;
    // 低拥堵推荐: stationId -> 预测空闲率[0,1]; 接口不可用时返回空表。
    QHash<int, double> predictedFreeRatio() const;
    Station stationDetail(int id) const;
    QList<Charger> chargersByStation(int stationId) const;
    Charger chargerById(int id) const;
    // 运维：状态=2 标记故障(on)，状态=0 恢复正常；失败返回 false 并写 err
    bool setChargerStatus(int id, int status, QString *err = nullptr);
    bool rebootCharger(int id, QString *err = nullptr);  // 调后端远程重启(故障→重启→恢复)
    void incrementChargerCount(int id);

    // —— 管理端操作(UC-A-05 / UC-A-06)——
    QList<Charger> allChargers() const;
    void addCharger(int stationId, const QString &code, const QString &type, double power);
    bool deleteCharger(int chargerId); // 使用中禁止删除
    int addStation(const QString &name, const QString &address, double lat, double lon,
                   double price, int chargerCount, double defaultPower); // 批量建桩
    bool updateStation(int id, const QString &name, const QString &address,
                       double lat, double lon, double price);
    bool deleteStation(int id); // 有电桩禁止删除(BR-10)

    // 当前定位(模拟:固定坐标)
    Location currentLocation() const;

    // 球面两点距离(公里)
    static double haversineKm(double lat1, double lon1, double lat2, double lon2);

private:
    StationService();

    QList<Station> m_stations;
    QList<Charger> m_chargers;
};
