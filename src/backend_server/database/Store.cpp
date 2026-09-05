#include "Store.h"

#include <QAtomicInt>
#include <QDateTime>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

namespace ncs {
namespace backend {

namespace {
QString nextConnName() {
    static QAtomicInt counter{0};
    return QStringLiteral("ncs_store_%1").arg(counter.fetchAndAddRelaxed(1));
}
QString toDbIso(const QDateTime& dt) {
    return dt.toUTC().toString(Qt::ISODate);
}
QDateTime fromDbIso(const QString& s) {
    return QDateTime::fromString(s, Qt::ISODate).toUTC();
}
}  // namespace

Store::Store() : connName_(nextConnName()) {}

Store::~Store() {
    close();
}

bool Store::open(const QString& dbPath) {
    db_ = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connName_);
    db_.setDatabaseName(dbPath);
    if (!db_.open())
        return false;
    return ensureSchema();
}

bool Store::isOpen() const {
    return db_.isOpen();
}

void Store::close() {
    if (db_.isValid())
        db_.close();
    db_ = QSqlDatabase();
    QSqlDatabase::removeDatabase(connName_);
}

bool Store::ensureSchema() {
    QSqlQuery q(db_);
    return q.exec(QStringLiteral(
        "CREATE TABLE IF NOT EXISTS users ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  phone TEXT NOT NULL UNIQUE,"
        "  nickname TEXT NOT NULL,"
        "  balance_cents INTEGER NOT NULL DEFAULT 0,"
        "  status INTEGER NOT NULL DEFAULT 0,"
        "  registered_at TEXT NOT NULL"
        ")"));
}

ncs::User Store::rowToUser(const QSqlQuery& q) {
    ncs::User u;
    u.id = q.value(0).toInt();
    u.phone = q.value(1).toString();
    u.nickname = q.value(2).toString();
    u.balanceCents = q.value(3).toLongLong();
    u.status = static_cast<ncs::UserStatus>(q.value(4).toInt());
    u.registeredAt = fromDbIso(q.value(5).toString());
    return u;
}

bool Store::ensureUserByPhone(const QString& phone, ncs::User* out) {
    if (!isOpen() || !out)
        return false;
    if (!findUserByPhone(phone, out)) {
        // 不存在 → 注册
        QSqlQuery ins(db_);
        ins.prepare(QStringLiteral(
            "INSERT INTO users (phone, nickname, balance_cents, status, registered_at) "
            "VALUES (?, ?, 0, 0, ?)"));
        ins.addBindValue(phone);
        ins.addBindValue(QStringLiteral("充电用户") + phone.right(4));
        ins.addBindValue(toDbIso(QDateTime::currentDateTimeUtc()));
        if (!ins.exec())
            return false;
        return findUserByPhone(phone, out);
    }
    return true;
}

bool Store::findUserByPhone(const QString& phone, ncs::User* out) const {
    if (!isOpen() || !out)
        return false;
    QSqlQuery q(db_);
    q.prepare(QStringLiteral(
        "SELECT id,phone,nickname,balance_cents,status,registered_at "
        "FROM users WHERE phone = ?"));
    q.addBindValue(phone);
    if (!q.exec() || !q.next())
        return false;
    *out = rowToUser(q);
    return true;
}

bool Store::setBalanceCents(int userId, ncs::MoneyCents cents) {
    if (!isOpen())
        return false;
    QSqlQuery q(db_);
    q.prepare(QStringLiteral("UPDATE users SET balance_cents = ? WHERE id = ?"));
    q.addBindValue(qint64(cents));
    q.addBindValue(userId);
    return q.exec();
}

qint64 Store::countUsers() const {
    if (!isOpen())
        return -1;
    QSqlQuery q(db_);
    if (!q.exec(QStringLiteral("SELECT COUNT(*) FROM users")) || !q.next())
        return -1;
    return q.value(0).toLongLong();
}

}  // namespace backend
}  // namespace ncs
