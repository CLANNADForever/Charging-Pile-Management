#ifndef NCS_BACKEND_DATABASE_STORE_H
#define NCS_BACKEND_DATABASE_STORE_H

#include <QSqlDatabase>
#include <QString>

#include "entities.h"

namespace ncs {
namespace backend {

// SQLite 持久层。当前只建 users 表；orders/stations/devices 由后续切片建表。
// 每个 Store 实例使用独立连接名，便于测试打开临时库。
class Store {
public:
    Store();
    ~Store();
    Store(const Store&) = delete;
    Store& operator=(const Store&) = delete;

    bool open(const QString& dbPath);
    bool isOpen() const;
    void close();

    // 免密登录：号码不存在则注册(默认昵称/0 余额)，返回当前记录
    bool ensureUserByPhone(const QString& phone, ncs::User* out);
    bool findUserByPhone(const QString& phone, ncs::User* out) const;
    bool setBalanceCents(int userId, ncs::MoneyCents cents);
    qint64 countUsers() const;

private:
    bool ensureSchema();
    static ncs::User rowToUser(const QSqlQuery& q);

    QString connName_;
    QSqlDatabase db_;
};

}  // namespace backend
}  // namespace ncs

#endif  // NCS_BACKEND_DATABASE_STORE_H
