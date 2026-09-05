#ifndef NCS_BACKEND_DATABASE_STORE_H
#define NCS_BACKEND_DATABASE_STORE_H

#include <mutex>
#include <QString>

#include "entities.h"

struct sqlite3;

namespace ncs {
namespace backend {

// SQLite 持久层：直接使用 SQLite C API(serialized 编译)，单连接 + 互斥锁串行化，
// 可从任意工作线程安全调用(QtSql 连接受"仅创建线程可用"限制，故不用 QtSql)。
class Store {
public:
    Store();
    ~Store();
    Store(const Store&) = delete;
    Store& operator=(const Store&) = delete;

    bool open(const QString& dbPath);
    bool isOpen() const;
    void close();

    // 免密登录：号码不存在则注册(默认昵称/0 余额)。整体原子(锁内 find+insert)。
    bool ensureUserByPhone(const QString& phone, ncs::User* out);
    bool findUserByPhone(const QString& phone, ncs::User* out) const;
    bool setBalanceCents(int userId, ncs::MoneyCents cents);
    qint64 countUsers() const;

private:
    bool findLocked(const QString& phone, ncs::User* out) const;  // 调用方须已持锁
    bool insertUserLocked(const QString& phone);                  // 调用方须已持锁

    sqlite3* db_ = nullptr;
    mutable std::mutex mu_;
};

}  // namespace backend
}  // namespace ncs

#endif  // NCS_BACKEND_DATABASE_STORE_H
