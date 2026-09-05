#ifndef NCS_BACKEND_DATABASE_STORE_H
#define NCS_BACKEND_DATABASE_STORE_H

#include <mutex>
#include <thread>
#include <QString>
#include <QVector>

#include "entities.h"

struct sqlite3;

namespace ncs {
namespace backend {

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
    bool authenticateAdmin(const QString& username,
                           const QString& password) const;
    QVector<ncs::User> searchUsers(const QString& phone) const;
    bool setUserStatus(int userId, int status);
    int createStation(const QString& name, const QString& address,
                      double lat, double lng, ncs::MoneyCents priceCents);
    bool updateStation(int id, const QString& name, const QString& address,
                       double lat, double lng, ncs::MoneyCents priceCents);
    qint64 countDevicesByStation(int stationId) const;
    bool deleteStationById(int id);
    bool createDevices(int stationId, int type, int count, double powerKw);
    int deleteDeviceIfIdle(int id);  // 1=已删 0=占用 拒绝 -1=不存在
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
