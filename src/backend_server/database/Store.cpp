#include "Store.h"

#include <sqlite3.h>

#include <QDateTime>

namespace ncs {
namespace backend {

namespace {

QString isoNowUtc() {
    return QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
}
QDateTime fromDbIso(const QString& s) {
    return QDateTime::fromString(s, Qt::ISODate).toUTC();
}
QString columnText(sqlite3_stmt* st, int col) {
    const auto* p = sqlite3_column_text(st, col);
    return p ? QString::fromUtf8(reinterpret_cast<const char*>(p)) : QString();
}

}  // namespace

Store::Store() = default;

Store::~Store() {
    close();
}

bool Store::open(const QString& dbPath) {
    std::lock_guard<std::mutex> lk(mu_);
    if (db_)
        return true;
    const QByteArray path = dbPath.toUtf8();
    if (sqlite3_open(path.constData(), &db_) != SQLITE_OK) {
        if (db_) {
            sqlite3_close(db_);
            db_ = nullptr;
        }
        return false;
    }
    sqlite3_busy_timeout(db_, 3000);

    const char* sql = "CREATE TABLE IF NOT EXISTS users ("
                      "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
                      "  phone TEXT NOT NULL UNIQUE,"
                      "  nickname TEXT NOT NULL,"
                      "  balance_cents INTEGER NOT NULL DEFAULT 0,"
                      "  status INTEGER NOT NULL DEFAULT 0,"
                      "  registered_at TEXT NOT NULL"
                      ")";
    char* err = nullptr;
    const int rc = sqlite3_exec(db_, sql, nullptr, nullptr, &err);
    if (rc != SQLITE_OK) {
        sqlite3_free(err);
        return false;
    }
    return true;
}

bool Store::isOpen() const {
    std::lock_guard<std::mutex> lk(mu_);
    return db_ != nullptr;
}

void Store::close() {
    std::lock_guard<std::mutex> lk(mu_);
    if (db_) {
        sqlite3_close(db_);
        db_ = nullptr;
    }
}

bool Store::findLocked(const QString& phone, ncs::User* out) const {
    if (!db_ || !out)
        return false;
    const char* sql = "SELECT id,phone,nickname,balance_cents,status,registered_at "
                      "FROM users WHERE phone = ?";
    sqlite3_stmt* st = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &st, nullptr) != SQLITE_OK)
        return false;
    const QByteArray p = phone.toUtf8();
    sqlite3_bind_text(st, 1, p.constData(), p.size(), SQLITE_TRANSIENT);

    bool found = false;
    if (sqlite3_step(st) == SQLITE_ROW) {
        ncs::User u;
        u.id = sqlite3_column_int(st, 0);
        u.phone = columnText(st, 1);
        u.nickname = columnText(st, 2);
        u.balanceCents = sqlite3_column_int64(st, 3);
        u.status = static_cast<ncs::UserStatus>(sqlite3_column_int(st, 4));
        u.registeredAt = fromDbIso(columnText(st, 5));
        *out = u;
        found = true;
    }
    sqlite3_finalize(st);
    return found;
}

bool Store::insertUserLocked(const QString& phone) {
    const char* sql = "INSERT INTO users (phone, nickname, balance_cents, status, registered_at) "
                      "VALUES (?, ?, 0, 0, ?)";
    sqlite3_stmt* st = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &st, nullptr) != SQLITE_OK)
        return false;
    const QByteArray p = phone.toUtf8();
    const QByteArray nick = (QStringLiteral("充电用户") + phone.right(4)).toUtf8();
    const QByteArray iso = isoNowUtc().toUtf8();
    sqlite3_bind_text(st, 1, p.constData(), p.size(), SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 2, nick.constData(), nick.size(), SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 3, iso.constData(), iso.size(), SQLITE_TRANSIENT);
    const int rc = sqlite3_step(st);
    sqlite3_finalize(st);
    return rc == SQLITE_DONE;
}

bool Store::findUserByPhone(const QString& phone, ncs::User* out) const {
    std::lock_guard<std::mutex> lk(mu_);
    return findLocked(phone, out);
}

bool Store::ensureUserByPhone(const QString& phone, ncs::User* out) {
    if (!out)
        return false;
    if (findUserByPhone(phone, out))  // 已存在则直接返回(带锁)
        return true;

    std::lock_guard<std::mutex> lk(mu_);  // 锁内复查 + 插入，避免并发同号双插
    if (!db_)
        return false;
    if (!findLocked(phone, out) && !insertUserLocked(phone))
        return false;
    return findLocked(phone, out);  // 填充 out
}

bool Store::setBalanceCents(int userId, ncs::MoneyCents cents) {
    std::lock_guard<std::mutex> lk(mu_);
    if (!db_)
        return false;
    const char* sql = "UPDATE users SET balance_cents = ? WHERE id = ?";
    sqlite3_stmt* st = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &st, nullptr) != SQLITE_OK)
        return false;
    sqlite3_bind_int64(st, 1, static_cast<sqlite3_int64>(cents));
    sqlite3_bind_int(st, 2, userId);
    const int rc = sqlite3_step(st);
    sqlite3_finalize(st);
    return rc == SQLITE_DONE;
}

qint64 Store::countUsers() const {
    std::lock_guard<std::mutex> lk(mu_);
    if (!db_)
        return -1;
    sqlite3_stmt* st = nullptr;
    if (sqlite3_prepare_v2(db_, "SELECT COUNT(*) FROM users", -1, &st, nullptr) != SQLITE_OK)
        return -1;
    qint64 n = -1;
    if (sqlite3_step(st) == SQLITE_ROW)
        n = sqlite3_column_int64(st, 0);
    sqlite3_finalize(st);
    return n;
}

}  // namespace backend
}  // namespace ncs
