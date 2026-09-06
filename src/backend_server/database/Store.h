#ifndef NCS_BACKEND_DATABASE_STORE_H
#define NCS_BACKEND_DATABASE_STORE_H

#include <mutex>
#include <thread>
#include <utility>
#include <QString>
#include <QVector>

#include "entities.h"

struct sqlite3;

namespace ncs {
namespace backend {

// ---- B2 管理端查询行/参数结构 ----
// 设备列表聚合行(累计充电次数/时长 = 按订单聚合，不冗余落库)
struct DeviceRow {
    ncs::Device dev;
    QString stationName;
    qint64 sessions = 0;       // 累计充电次数(已结束/已支付订单)
    double chargeSec = 0.0;    // 累计充电时长(秒)
};
struct DeviceFilter {
    int stationId = -1;        // -1=全部
    int type = -1;             // -1=全部
    int state = -1;            // -1=全部
    QString q;                 // 桩号关键字(空=全部)
};
struct AuditRow {
    int id = 0;
    QString username;
    QString action;
    QString detail;
    QString result;
    QDateTime at;
};
struct DeviceOpRow {
    int id = 0;
    int deviceId = 0;
    QString opType;   // fault/restart/recover
    QString opBy;     // 执行人(设备自主为空)
    QString detail;
    QDateTime at;
};
struct RevenueAgg {
    ncs::MoneyCents cents = 0;  // 应收合计(分)
    qint64 orders = 0;          // 已支付订单数
    double energyKwh = 0.0;     // 电量合计
};
struct DailyRevenue {
    QString day;                // YYYY-MM-DD(UTC)
    ncs::MoneyCents cents = 0;
    qint64 orders = 0;
    double energyKwh = 0.0;
};

// R1 建/改站扩展属性(缺省全默认，不破坏旧调用)
struct StationFields {
    int amenities = 0;
    int parking = 0;
    int location = 0;
    bool isPromo = false;
    QString openHours;
    ncs::MoneyCents minChargeCents = 0;
    // R2 分档单价(0=未配置，计价回退快充档 price_cents)
    ncs::MoneyCents priceSlowCents = 0;
    ncs::MoneyCents priceUltraCents = 0;
};

// SQLite 持久层：SQLite C API + 互斥锁串行化。单方法原子；
// 跨方法业务原子(预约/结算)由上层(ChargeService)再包一把锁。
class Store {
public:
    Store();
    ~Store();
    Store(const Store&) = delete;
    Store& operator=(const Store&) = delete;

    bool open(const QString& dbPath);
    bool isOpen() const;
    void close();

    // 事务(整段原子，可回滚)；单实例连接 + 同一写线程使用
    bool beginTx();
    bool commitTx();
    bool rollbackTx();

    // 用户
    bool ensureUserByPhone(const QString& phone, ncs::User* out);
    bool findUserByPhone(const QString& phone, ncs::User* out) const;
    bool setBalanceCents(int userId, ncs::MoneyCents cents);
    qint64 countUsers() const;

    // 站 / 桩 / 订单
    QVector<ncs::Station> listStations() const;
    QVector<ncs::Device> listDevicesByStation(int stationId) const;
    bool getStationById(int id, ncs::Station* out) const;
    bool getDeviceById(int id, ncs::Device* out) const;
    bool setDeviceState(int deviceId, int state);
    bool adjustStationFree(int stationId, int delta);

    bool createOrder(const ncs::Order& o, int* newId);
    bool getOrderById(int id, ncs::Order* out) const;
    bool updateOrderStatus(int id, int status);
    bool setOrderChargeStarted(int id, const QString& isoUtc);  // R4 start 打点
    bool updateOrderSettled(int id, double energyKwh,
                            ncs::MoneyCents amountCents);
    bool updateOrderPaid(int id);
    qint64 countUnpaidByPhone(const QString& phone) const;
    QVector<ncs::Order> listActiveOrdersByPhone(const QString& phone) const;
    qint64 countOrders() const;
    QVector<int> listExpiredReservedOrderIds(int olderThanSec) const;

    // 钱包/资料/历史
    bool addBalanceByPhone(const QString& phone, ncs::MoneyCents deltaCents);
    bool setNickname(const QString& phone, const QString& nickname);
    QVector<ncs::Order> listHistoryByPhone(const QString& phone, int limit,
                                           int offset) const;
    qint64 countHistoryByPhone(const QString& phone) const;
    // 管理端：认证 / 风控 / 资产
    bool authenticateAdmin(const QString& username, const QString& password,
                           QString* roleOut = nullptr) const;
    QVector<ncs::User> searchUsers(const QString& phone,
                                   int statusFilter = 0) const;  // 0=全部 1=正常 2=冻结
    bool setUserStatus(int userId, int status);
    int createStation(const QString& name, const QString& address,
                      double lat, double lng, ncs::MoneyCents priceCents,
                      const StationFields& f = StationFields());
    bool updateStation(int id, const QString& name, const QString& address,
                       double lat, double lng, ncs::MoneyCents priceCents,
                       const StationFields& f = StationFields());
    qint64 countDevicesByStation(int stationId) const;
    bool deleteStationById(int id);
    // 批量建桩：1=成功 -1=站不存在 -2=插桩失败 -3=计数更新失败(整批事务)
    int createDevices(int stationId, int type, int count, double powerKw);
    int deleteDeviceIfIdle(int id);  // 1=已删 0=占用 拒绝 -1=不存在(删桩+计数回退同一事务)

    // B2：审计 / 运维日志
    bool appendAudit(const QString& username, const QString& action,
                     const QString& detail, bool ok);
    QVector<AuditRow> listAudit(int limit, int offset) const;
    qint64 countAudit() const;
    bool appendDeviceOp(int deviceId, const QString& opType,
                        const QString& opBy, const QString& detail);
    QVector<DeviceOpRow> listDeviceOps(int limit, int offset) const;
    qint64 countDeviceOps() const;

    // B2：管理端列表聚合 / 统计
    bool listDevicesAdmin(const DeviceFilter& f, int limit, int offset,
                          QVector<DeviceRow>* rows) const;
    qint64 countDevicesAdmin(const DeviceFilter& f) const;
    RevenueAgg revenueWindow(const QString& fromIso,
                             const QString& toIso) const;  // 已支付订单; 空串=不设界
    QVector<DailyRevenue> dailyRevenue(int days) const;    // 含今天, 缺日补 0
    QVector<int> deviceStateCounts() const;                // 下标=DeviceState 值
    QVector<std::pair<int,int>> listDeviceStations() const; // (device_id, station_id)
private:
    bool findLocked(const QString& phone, ncs::User* out) const;  // 调用方持锁
    bool insertUserLocked(const QString& phone);                  // 调用方持锁
    bool seedIfEmptyLocked();                                     // 调用方持锁
    bool seedAdminsLocked();                                     // 调用方持锁

    // 事务内(本线程)不再重复加锁，否则照常加锁
    std::unique_lock<std::mutex> lockGuard() const;

    sqlite3* db_ = nullptr;
    mutable std::mutex mu_;
    mutable bool txActive_ = false;
    mutable std::thread::id txOwner_;
};

}  // namespace backend
}  // namespace ncs

#endif  // NCS_BACKEND_DATABASE_STORE_H
