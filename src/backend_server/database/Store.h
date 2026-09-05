#ifndef NCS_BACKEND_DATABASE_STORE_H
#define NCS_BACKEND_DATABASE_STORE_H

#include <mutex>
#include <QString>
#include <QVector>

#include "entities.h"

struct sqlite3;

namespace ncs {
namespace backend {

// SQLite 持久层：直接 SQLite C API(serialized) + 互斥锁串行化，可跨线程安全调用。
class Store {
public:
    Store();
    ~Store();
    Store(const Store&) = delete;
    Store& operator=(const Store&) = delete;

    bool open(const QString& dbPath);
    bool isOpen() const;
    void close();

    // 用户(免密登录)
    bool ensureUserByPhone(const QString& phone, ncs::User* out);
    bool findUserByPhone(const QString& phone, ncs::User* out) const;
    bool setBalanceCents(int userId, ncs::MoneyCents cents);
    qint64 countUsers() const;

    // 站点 / 桩列表
    QVector<ncs::Station> listStations() const;
    QVector<ncs::Device> listDevicesByStation(int stationId) const;

private:
    bool findLocked(const QString& phone, ncs::User* out) const;  // 调用方持锁
    bool insertUserLocked(const QString& phone);                  // 调用方持锁
    bool seedIfEmptyLocked();                                     // 调用方持锁

    sqlite3* db_ = nullptr;
    mutable std::mutex mu_;
};

}  // namespace backend
}  // namespace ncs

#endif  // NCS_BACKEND_DATABASE_STORE_H
