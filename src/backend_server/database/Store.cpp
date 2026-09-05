#include "Store.h"

#include <sqlite3.h>

#include <QDateTime>
#include <QCryptographicHash>

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
    auto lk = lockGuard();
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

    const char* kTables[] = {
        "CREATE TABLE IF NOT EXISTS users ("
        " id INTEGER PRIMARY KEY AUTOINCREMENT, phone TEXT NOT NULL UNIQUE,"
        " nickname TEXT NOT NULL, balance_cents INTEGER NOT NULL DEFAULT 0,"
        " status INTEGER NOT NULL DEFAULT 0, registered_at TEXT NOT NULL)",
        "CREATE TABLE IF NOT EXISTS stations ("
        " id INTEGER PRIMARY KEY AUTOINCREMENT, name TEXT NOT NULL,"
        " address TEXT NOT NULL, latitude REAL NOT NULL DEFAULT 0,"
        " longitude REAL NOT NULL DEFAULT 0, total_piles INTEGER NOT NULL DEFAULT 0,"
        " price_cents INTEGER NOT NULL DEFAULT 0, free_piles INTEGER NOT NULL DEFAULT 0)",
        "CREATE TABLE IF NOT EXISTS devices ("
        " id INTEGER PRIMARY KEY AUTOINCREMENT, station_id INTEGER NOT NULL,"
        " type INTEGER NOT NULL DEFAULT 0, state INTEGER NOT NULL DEFAULT 0,"
        " power_kw REAL NOT NULL DEFAULT 0, energy_kwh REAL NOT NULL DEFAULT 0)",
        "CREATE TABLE IF NOT EXISTS orders ("
        " id INTEGER PRIMARY KEY AUTOINCREMENT, phone TEXT NOT NULL,"
        " station_id INTEGER NOT NULL, device_id INTEGER NOT NULL,"
        " unit_price_cents INTEGER NOT NULL DEFAULT 0,"
        " amount_cents INTEGER NOT NULL DEFAULT 0,"
        " energy_kwh REAL NOT NULL DEFAULT 0, status INTEGER NOT NULL DEFAULT 0,"
        " started_at TEXT NOT NULL, finished_at TEXT)",
        "CREATE TABLE IF NOT EXISTS admins ("
        " username TEXT PRIMARY KEY, password TEXT NOT NULL)",
    };
    for (const char* sql : kTables) {
        char* err = nullptr;
        if (sqlite3_exec(db_, sql, nullptr, nullptr, &err) != SQLITE_OK) {
            sqlite3_free(err);
            return false;
        }
    }
    if (!seedIfEmptyLocked())
        return false;
    return seedAdminsLocked();
}

bool Store::isOpen() const {
    auto lk = lockGuard();
    return db_ != nullptr;
}

void Store::close() {
    auto lk = lockGuard();
    if (db_) {
        sqlite3_close(db_);
        db_ = nullptr;
    }
}

bool Store::seedIfEmptyLocked() {
    sqlite3_stmt* st = nullptr;
    if (sqlite3_prepare_v2(db_, "SELECT COUNT(*) FROM stations", -1, &st,
                           nullptr) != SQLITE_OK)
        return false;
    const bool hasRows = sqlite3_step(st) == SQLITE_ROW &&
                         sqlite3_column_int64(st, 0) > 0;
    sqlite3_finalize(st);
    if (hasRows)
        return true;

    const char* stations =
        "INSERT INTO stations(name,address,latitude,longitude,total_piles,price_cents,free_piles) VALUES"
        " ('望京充电站','北京市朝阳区望京街道',39.996,116.481,3,200,2),"
        " ('中关村充电站','北京市海淀区中关村大街',39.984,116.316,4,180,3),"
        " ('亦庄超充站','北京市大兴区荣华中路',39.795,116.506,2,240,2)";
    char* err = nullptr;
    if (sqlite3_exec(db_, stations, nullptr, nullptr, &err) != SQLITE_OK) {
        sqlite3_free(err);
        return false;
    }
    const char* devices =
        "INSERT INTO devices(station_id,type,state,power_kw,energy_kwh) VALUES"
        " (1,0,0,120.0,0.0),(1,0,0,120.0,0.0),(1,0,1,120.0,12.5),"
        " (2,1,0,7.0,0.0),(2,1,0,7.0,0.0),(2,1,2,0.0,3.2),(2,1,0,7.0,0.0),"
        " (3,0,0,180.0,0.0),(3,0,0,180.0,0.0)";
    if (sqlite3_exec(db_, devices, nullptr, nullptr, &err) != SQLITE_OK) {
        sqlite3_free(err);
        return false;
    }
    return true;
}

// ---------- helpers / users ----------

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
    auto lk = lockGuard();
    return findLocked(phone, out);
}

bool Store::ensureUserByPhone(const QString& phone, ncs::User* out) {
    if (!out)
        return false;
    if (findUserByPhone(phone, out))
        return true;
    auto lk = lockGuard();
    if (!db_)
        return false;
    if (!findLocked(phone, out) && !insertUserLocked(phone))
        return false;
    return findLocked(phone, out);
}

bool Store::setBalanceCents(int userId, ncs::MoneyCents cents) {
    auto lk = lockGuard();
    if (!db_)
        return false;
    sqlite3_stmt* st = nullptr;
    if (sqlite3_prepare_v2(db_, "UPDATE users SET balance_cents = ? WHERE id = ?",
                           -1, &st, nullptr) != SQLITE_OK)
        return false;
    sqlite3_bind_int64(st, 1, static_cast<sqlite3_int64>(cents));
    sqlite3_bind_int(st, 2, userId);
    const int rc = sqlite3_step(st);
    sqlite3_finalize(st);
    return rc == SQLITE_DONE;
}

qint64 Store::countUsers() const {
    auto lk = lockGuard();
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

// ---------- 站 / 桩 ----------

QVector<ncs::Station> Store::listStations() const {
    QVector<ncs::Station> out;
    auto lk = lockGuard();
    if (!db_)
        return out;
    sqlite3_stmt* st = nullptr;
    if (sqlite3_prepare_v2(db_,
                           "SELECT id,name,address,latitude,longitude,total_piles,"
                           "price_cents,free_piles FROM stations ORDER BY id",
                           -1, &st, nullptr) != SQLITE_OK)
        return out;
    while (sqlite3_step(st) == SQLITE_ROW) {
        ncs::Station s;
        s.id = sqlite3_column_int(st, 0);
        s.name = columnText(st, 1);
        s.address = columnText(st, 2);
        s.latitude = sqlite3_column_double(st, 3);
        s.longitude = sqlite3_column_double(st, 4);
        s.totalPiles = sqlite3_column_int(st, 5);
        s.pricePerKwhCents = sqlite3_column_int64(st, 6);
        s.freePiles = sqlite3_column_int(st, 7);
        out.push_back(s);
    }
    sqlite3_finalize(st);
    return out;
}

QVector<ncs::Device> Store::listDevicesByStation(int stationId) const {
    QVector<ncs::Device> out;
    auto lk = lockGuard();
    if (!db_)
        return out;
    sqlite3_stmt* st = nullptr;
    if (sqlite3_prepare_v2(db_,
                           "SELECT id,station_id,type,state,power_kw,energy_kwh "
                           "FROM devices WHERE station_id=? ORDER BY id",
                           -1, &st, nullptr) != SQLITE_OK)
        return out;
    sqlite3_bind_int(st, 1, stationId);
    while (sqlite3_step(st) == SQLITE_ROW) {
        ncs::Device d;
        d.id = sqlite3_column_int(st, 0);
        d.stationId = sqlite3_column_int(st, 1);
        d.type = static_cast<ncs::DeviceType>(sqlite3_column_int(st, 2));
        d.state = static_cast<ncs::DeviceState>(sqlite3_column_int(st, 3));
        d.powerKw = sqlite3_column_double(st, 4);
        d.energyKwh = sqlite3_column_double(st, 5);
        out.push_back(d);
    }
    sqlite3_finalize(st);
    return out;
}

bool Store::getStationById(int id, ncs::Station* out) const {
    auto lk = lockGuard();
    if (!db_ || !out)
        return false;
    sqlite3_stmt* st = nullptr;
    if (sqlite3_prepare_v2(db_,
                           "SELECT id,name,address,latitude,longitude,total_piles,"
                           "price_cents,free_piles FROM stations WHERE id=?",
                           -1, &st, nullptr) != SQLITE_OK)
        return false;
    sqlite3_bind_int(st, 1, id);
    const bool found = sqlite3_step(st) == SQLITE_ROW;
    if (found) {
        out->id = sqlite3_column_int(st, 0);
        out->name = columnText(st, 1);
        out->address = columnText(st, 2);
        out->latitude = sqlite3_column_double(st, 3);
        out->longitude = sqlite3_column_double(st, 4);
        out->totalPiles = sqlite3_column_int(st, 5);
        out->pricePerKwhCents = sqlite3_column_int64(st, 6);
        out->freePiles = sqlite3_column_int(st, 7);
    }
    sqlite3_finalize(st);
    return found;
}

bool Store::getDeviceById(int id, ncs::Device* out) const {
    auto lk = lockGuard();
    if (!db_ || !out)
        return false;
    sqlite3_stmt* st = nullptr;
    if (sqlite3_prepare_v2(db_,
                           "SELECT id,station_id,type,state,power_kw,energy_kwh "
                           "FROM devices WHERE id=?",
                           -1, &st, nullptr) != SQLITE_OK)
        return false;
    sqlite3_bind_int(st, 1, id);
    const bool found = sqlite3_step(st) == SQLITE_ROW;
    if (found) {
        out->id = sqlite3_column_int(st, 0);
        out->stationId = sqlite3_column_int(st, 1);
        out->type = static_cast<ncs::DeviceType>(sqlite3_column_int(st, 2));
        out->state = static_cast<ncs::DeviceState>(sqlite3_column_int(st, 3));
        out->powerKw = sqlite3_column_double(st, 4);
        out->energyKwh = sqlite3_column_double(st, 5);
    }
    sqlite3_finalize(st);
    return found;
}

bool Store::setDeviceState(int deviceId, int state) {
    auto lk = lockGuard();
    if (!db_)
        return false;
    sqlite3_stmt* st = nullptr;
    if (sqlite3_prepare_v2(db_, "UPDATE devices SET state=? WHERE id=?", -1,
                           &st, nullptr) != SQLITE_OK)
        return false;
    sqlite3_bind_int(st, 1, state);
    sqlite3_bind_int(st, 2, deviceId);
    const int rc = sqlite3_step(st);
    sqlite3_finalize(st);
    return rc == SQLITE_DONE;
}

bool Store::adjustStationFree(int stationId, int delta) {
    auto lk = lockGuard();
    if (!db_)
        return false;
    sqlite3_stmt* st = nullptr;
    if (sqlite3_prepare_v2(db_, "UPDATE stations SET free_piles = free_piles + ? WHERE id=?",
                           -1, &st, nullptr) != SQLITE_OK)
        return false;
    sqlite3_bind_int(st, 1, delta);
    sqlite3_bind_int(st, 2, stationId);
    const int rc = sqlite3_step(st);
    sqlite3_finalize(st);
    return rc == SQLITE_DONE;
}

// ---------- 订单 ----------

bool Store::createOrder(const ncs::Order& o, int* newId) {
    auto lk = lockGuard();
    if (!db_)
        return false;
    const char* sql = "INSERT INTO orders (phone,station_id,device_id,unit_price_cents,"
                      "amount_cents,energy_kwh,status,started_at,finished_at) "
                      "VALUES (?,?,?,?,0,0,?,?,NULL)";
    sqlite3_stmt* st = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &st, nullptr) != SQLITE_OK)
        return false;
    const QByteArray ph = o.phone.toUtf8();
    const QByteArray iso = o.startedAt.isValid()
                               ? o.startedAt.toUTC().toString(Qt::ISODate).toUtf8()
                               : isoNowUtc().toUtf8();
    sqlite3_bind_text(st, 1, ph.constData(), ph.size(), SQLITE_TRANSIENT);
    sqlite3_bind_int(st, 2, o.stationId);
    sqlite3_bind_int(st, 3, o.deviceId);
    sqlite3_bind_int64(st, 4, static_cast<sqlite3_int64>(o.unitPriceCents));
    sqlite3_bind_int(st, 5, static_cast<int>(o.status));
    sqlite3_bind_text(st, 6, iso.constData(), iso.size(), SQLITE_TRANSIENT);
    const int rc = sqlite3_step(st);
    sqlite3_finalize(st);
    if (rc != SQLITE_DONE)
        return false;
    if (newId)
        *newId = static_cast<int>(sqlite3_last_insert_rowid(db_));
    return true;
}

bool Store::getOrderById(int id, ncs::Order* out) const {
    auto lk = lockGuard();
    if (!db_ || !out)
        return false;
    sqlite3_stmt* st = nullptr;
    if (sqlite3_prepare_v2(db_,
                           "SELECT id,phone,station_id,device_id,unit_price_cents,"
                           "amount_cents,energy_kwh,status,started_at,finished_at "
                           "FROM orders WHERE id=?",
                           -1, &st, nullptr) != SQLITE_OK)
        return false;
    sqlite3_bind_int(st, 1, id);
    const bool found = sqlite3_step(st) == SQLITE_ROW;
    if (found) {
        out->id = sqlite3_column_int(st, 0);
        out->phone = columnText(st, 1);
        out->stationId = sqlite3_column_int(st, 2);
        out->deviceId = sqlite3_column_int(st, 3);
        out->unitPriceCents = sqlite3_column_int64(st, 4);
        out->amountCents = sqlite3_column_int64(st, 5);
        out->energyKwh = sqlite3_column_double(st, 6);
        out->status = static_cast<ncs::OrderStatus>(sqlite3_column_int(st, 7));
        out->startedAt = fromDbIso(columnText(st, 8));
        const QString f = columnText(st, 9);
        if (!f.isEmpty())
            out->finishedAt = fromDbIso(f);
    }
    sqlite3_finalize(st);
    return found;
}

bool Store::updateOrderStatus(int id, int status) {
    auto lk = lockGuard();
    if (!db_)
        return false;
    sqlite3_stmt* st = nullptr;
    if (sqlite3_prepare_v2(db_, "UPDATE orders SET status=? WHERE id=?", -1,
                           &st, nullptr) != SQLITE_OK)
        return false;
    sqlite3_bind_int(st, 1, status);
    sqlite3_bind_int(st, 2, id);
    const int rc = sqlite3_step(st);
    sqlite3_finalize(st);
    return rc == SQLITE_DONE;
}

bool Store::updateOrderSettled(int id, double energyKwh,
                               ncs::MoneyCents amountCents) {
    auto lk = lockGuard();
    if (!db_)
        return false;
    sqlite3_stmt* st = nullptr;
    if (sqlite3_prepare_v2(db_,
                           "UPDATE orders SET status=2, energy_kwh=?, amount_cents=?, "
                           "finished_at=? WHERE id=?",
                           -1, &st, nullptr) != SQLITE_OK)
        return false;
    sqlite3_bind_double(st, 1, energyKwh);
    sqlite3_bind_int64(st, 2, static_cast<sqlite3_int64>(amountCents));
    const QByteArray iso = isoNowUtc().toUtf8();
    sqlite3_bind_text(st, 3, iso.constData(), iso.size(), SQLITE_TRANSIENT);
    sqlite3_bind_int(st, 4, id);
    const int rc = sqlite3_step(st);
    sqlite3_finalize(st);
    return rc == SQLITE_DONE;
}

qint64 Store::countOrders() const {
    auto lk = lockGuard();
    if (!db_)
        return -1;
    sqlite3_stmt* st = nullptr;
    if (sqlite3_prepare_v2(db_, "SELECT COUNT(*) FROM orders", -1, &st, nullptr) != SQLITE_OK)
        return -1;
    qint64 n = -1;
    if (sqlite3_step(st) == SQLITE_ROW)
        n = sqlite3_column_int64(st, 0);
    sqlite3_finalize(st);
    return n;
}

std::unique_lock<std::mutex> Store::lockGuard() const {
    if (txActive_ && txOwner_ == std::this_thread::get_id())
        return std::unique_lock<std::mutex>();  // 已是本线程事务锁内
    return std::unique_lock<std::mutex>(mu_);
}

bool Store::beginTx() {
    if (txActive_)
        return false;
    mu_.lock();
    txActive_ = true;
    txOwner_ = std::this_thread::get_id();
    char* err = nullptr;
    if (sqlite3_exec(db_, "BEGIN", nullptr, nullptr, &err) != SQLITE_OK) {
        sqlite3_free(err);
        txActive_ = false;
        mu_.unlock();
        return false;
    }
    return true;
}

bool Store::commitTx() {
    if (!txActive_ || txOwner_ != std::this_thread::get_id())
        return false;
    char* err = nullptr;
    const bool ok = sqlite3_exec(db_, "COMMIT", nullptr, nullptr, &err) == SQLITE_OK;
    if (!ok)
        sqlite3_free(err);
    txActive_ = false;
    mu_.unlock();
    return ok;
}

bool Store::rollbackTx() {
    if (!txActive_ || txOwner_ != std::this_thread::get_id())
        return false;
    char* err = nullptr;
    const bool ok = sqlite3_exec(db_, "ROLLBACK", nullptr, nullptr, &err) == SQLITE_OK;
    if (!ok)
        sqlite3_free(err);
    txActive_ = false;
    mu_.unlock();
    return ok;
}

QVector<int> Store::listExpiredReservedOrderIds(int olderThanSec) const {
    QVector<int> out;
    auto lk = lockGuard();
    if (!db_)
        return out;
    sqlite3_stmt* st = nullptr;
    if (sqlite3_prepare_v2(db_,
                           "SELECT id FROM orders WHERE status=0 AND started_at < ?",
                           -1, &st, nullptr) != SQLITE_OK)
        return out;
    const QDateTime cutoff = QDateTime::currentDateTimeUtc().addSecs(-olderThanSec);
    const QByteArray iso = cutoff.toString(Qt::ISODate).toUtf8();
    sqlite3_bind_text(st, 1, iso.constData(), iso.size(), SQLITE_TRANSIENT);
    while (sqlite3_step(st) == SQLITE_ROW)
        out.push_back(sqlite3_column_int(st, 0));
    sqlite3_finalize(st);
    return out;
}

bool Store::updateOrderPaid(int id) {
    auto lk = lockGuard();
    if (!db_)
        return false;
    sqlite3_stmt* st = nullptr;
    if (sqlite3_prepare_v2(db_, "UPDATE orders SET status=3 WHERE id=?", -1,
                           &st, nullptr) != SQLITE_OK)
        return false;
    sqlite3_bind_int(st, 1, id);
    const int rc = sqlite3_step(st);
    sqlite3_finalize(st);
    return rc == SQLITE_DONE;
}

qint64 Store::countUnpaidByPhone(const QString& phone) const {
    auto lk = lockGuard();
    if (!db_)
        return -1;
    sqlite3_stmt* st = nullptr;
    if (sqlite3_prepare_v2(db_,
                           "SELECT COUNT(*) FROM orders WHERE phone=? AND status=2",
                           -1, &st, nullptr) != SQLITE_OK)
        return -1;
    const QByteArray p = phone.toUtf8();
    sqlite3_bind_text(st, 1, p.constData(), p.size(), SQLITE_TRANSIENT);
    qint64 n = 0;
    if (sqlite3_step(st) == SQLITE_ROW)
        n = sqlite3_column_int64(st, 0);
    sqlite3_finalize(st);
    return n;
}

QVector<ncs::Order> Store::listActiveOrdersByPhone(const QString& phone) const {
    QVector<ncs::Order> out;
    auto lk = lockGuard();
    if (!db_)
        return out;
    sqlite3_stmt* st = nullptr;
    if (sqlite3_prepare_v2(db_,
                           "SELECT id,phone,station_id,device_id,unit_price_cents,"
                           "amount_cents,energy_kwh,status,started_at,finished_at "
                           "FROM orders WHERE phone=? AND status IN (0,1,2) ORDER BY id DESC",
                           -1, &st, nullptr) != SQLITE_OK)
        return out;
    const QByteArray p = phone.toUtf8();
    sqlite3_bind_text(st, 1, p.constData(), p.size(), SQLITE_TRANSIENT);
    while (sqlite3_step(st) == SQLITE_ROW) {
        ncs::Order o;
        o.id = sqlite3_column_int(st, 0);
        o.phone = columnText(st, 1);
        o.stationId = sqlite3_column_int(st, 2);
        o.deviceId = sqlite3_column_int(st, 3);
        o.unitPriceCents = sqlite3_column_int64(st, 4);
        o.amountCents = sqlite3_column_int64(st, 5);
        o.energyKwh = sqlite3_column_double(st, 6);
        o.status = static_cast<ncs::OrderStatus>(sqlite3_column_int(st, 7));
        o.startedAt = fromDbIso(columnText(st, 8));
        const QString f = columnText(st, 9);
        if (!f.isEmpty())
            o.finishedAt = fromDbIso(f);
        out.push_back(o);
    }
    sqlite3_finalize(st);
    return out;
}

bool Store::addBalanceByPhone(const QString& phone, ncs::MoneyCents deltaCents) {
    auto lk = lockGuard();
    if (!db_)
        return false;
    sqlite3_stmt* st = nullptr;
    if (sqlite3_prepare_v2(db_,
                           "UPDATE users SET balance_cents = balance_cents + ? WHERE phone=?",
                           -1, &st, nullptr) != SQLITE_OK)
        return false;
    sqlite3_bind_int64(st, 1, static_cast<sqlite3_int64>(deltaCents));
    const QByteArray p = phone.toUtf8();
    sqlite3_bind_text(st, 2, p.constData(), p.size(), SQLITE_TRANSIENT);
    const int rc = sqlite3_step(st);
    const bool changed = rc == SQLITE_DONE && sqlite3_changes(db_) > 0;
    sqlite3_finalize(st);
    return changed;
}

bool Store::setNickname(const QString& phone, const QString& nickname) {
    auto lk = lockGuard();
    if (!db_)
        return false;
    sqlite3_stmt* st = nullptr;
    if (sqlite3_prepare_v2(db_, "UPDATE users SET nickname=? WHERE phone=?", -1,
                           &st, nullptr) != SQLITE_OK)
        return false;
    const QByteArray n = nickname.toUtf8();
    const QByteArray p = phone.toUtf8();
    sqlite3_bind_text(st, 1, n.constData(), n.size(), SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 2, p.constData(), p.size(), SQLITE_TRANSIENT);
    const int rc = sqlite3_step(st);
    const bool changed = rc == SQLITE_DONE && sqlite3_changes(db_) > 0;
    sqlite3_finalize(st);
    return changed;
}

QVector<ncs::Order> Store::listHistoryByPhone(const QString& phone, int limit,
                                              int offset) const {
    QVector<ncs::Order> out;
    auto lk = lockGuard();
    if (!db_)
        return out;
    sqlite3_stmt* st = nullptr;
    if (sqlite3_prepare_v2(db_,
                           "SELECT id,phone,station_id,device_id,unit_price_cents,"
                           "amount_cents,energy_kwh,status,started_at,finished_at "
                           "FROM orders WHERE phone=? AND status=3 "
                           "ORDER BY id DESC LIMIT ? OFFSET ?",
                           -1, &st, nullptr) != SQLITE_OK)
        return out;
    const QByteArray p = phone.toUtf8();
    sqlite3_bind_text(st, 1, p.constData(), p.size(), SQLITE_TRANSIENT);
    sqlite3_bind_int(st, 2, limit);
    sqlite3_bind_int(st, 3, offset);
    while (sqlite3_step(st) == SQLITE_ROW) {
        ncs::Order o;
        o.id = sqlite3_column_int(st, 0);
        o.phone = columnText(st, 1);
        o.stationId = sqlite3_column_int(st, 2);
        o.deviceId = sqlite3_column_int(st, 3);
        o.unitPriceCents = sqlite3_column_int64(st, 4);
        o.amountCents = sqlite3_column_int64(st, 5);
        o.energyKwh = sqlite3_column_double(st, 6);
        o.status = static_cast<ncs::OrderStatus>(sqlite3_column_int(st, 7));
        o.startedAt = fromDbIso(columnText(st, 8));
        const QString f = columnText(st, 9);
        if (!f.isEmpty())
            o.finishedAt = fromDbIso(f);
        out.push_back(o);
    }
    sqlite3_finalize(st);
    return out;
}

qint64 Store::countHistoryByPhone(const QString& phone) const {
    auto lk = lockGuard();
    if (!db_)
        return -1;
    sqlite3_stmt* st = nullptr;
    if (sqlite3_prepare_v2(db_, "SELECT COUNT(*) FROM orders WHERE phone=? AND status=3",
                           -1, &st, nullptr) != SQLITE_OK)
        return -1;
    const QByteArray p = phone.toUtf8();
    sqlite3_bind_text(st, 1, p.constData(), p.size(), SQLITE_TRANSIENT);
    qint64 n = 0;
    if (sqlite3_step(st) == SQLITE_ROW)
        n = sqlite3_column_int64(st, 0);
    sqlite3_finalize(st);
    return n;
}


bool Store::seedAdminsLocked() {
    sqlite3_stmt* st = nullptr;
    if (sqlite3_prepare_v2(db_, "SELECT COUNT(*) FROM admins", -1, &st,
                           nullptr) != SQLITE_OK)
        return false;
    const bool has = sqlite3_step(st) == SQLITE_ROW &&
                     sqlite3_column_int64(st, 0) > 0;
    sqlite3_finalize(st);
    if (has)
        return true;
    const auto h = [](const QString& s) {
        return QCryptographicHash::hash(s.toUtf8(), QCryptographicHash::Sha256)
            .toHex();
    };
    const QString sql =
        QStringLiteral("INSERT INTO admins(username,password) VALUES('%1','%2')")
            .arg(QStringLiteral("admin"), QString::fromLatin1(h(QStringLiteral("admin123"))));
    char* err = nullptr;
    const int rc =
        sqlite3_exec(db_, sql.toUtf8().constData(), nullptr, nullptr, &err);
    if (rc != SQLITE_OK) {
        sqlite3_free(err);
        return false;
    }
    return true;
}

bool Store::authenticateAdmin(const QString& username,
                              const QString& password) const {
    auto lk = lockGuard();
    if (!db_)
        return false;
    const QByteArray h = QCryptographicHash::hash(
                             password.toUtf8(), QCryptographicHash::Sha256)
                             .toHex();
    sqlite3_stmt* st = nullptr;
    if (sqlite3_prepare_v2(db_, "SELECT password FROM admins WHERE username=?",
                           -1, &st, nullptr) != SQLITE_OK)
        return false;
    const QByteArray un = username.toUtf8();
    sqlite3_bind_text(st, 1, un.constData(), un.size(), SQLITE_TRANSIENT);
    bool ok = false;
    if (sqlite3_step(st) == SQLITE_ROW)
        ok = columnText(st, 0) == QString::fromLatin1(h);
    sqlite3_finalize(st);
    return ok;
}

QVector<ncs::User> Store::searchUsers(const QString& phone) const {
    QVector<ncs::User> out;
    auto lk = lockGuard();
    if (!db_)
        return out;
    sqlite3_stmt* st = nullptr;
    if (sqlite3_prepare_v2(db_,
                           "SELECT id,phone,nickname,balance_cents,status,registered_at "
                           "FROM users WHERE phone LIKE ? ORDER BY id LIMIT 50",
                           -1, &st, nullptr) != SQLITE_OK)
        return out;
    const QByteArray like = ("%" + phone + "%").toUtf8();
    sqlite3_bind_text(st, 1, like.constData(), like.size(), SQLITE_TRANSIENT);
    while (sqlite3_step(st) == SQLITE_ROW) {
        ncs::User u;
        u.id = sqlite3_column_int(st, 0);
        u.phone = columnText(st, 1);
        u.nickname = columnText(st, 2);
        u.balanceCents = sqlite3_column_int64(st, 3);
        u.status = static_cast<ncs::UserStatus>(sqlite3_column_int(st, 4));
        u.registeredAt = fromDbIso(columnText(st, 5));
        out.push_back(u);
    }
    sqlite3_finalize(st);
    return out;
}

bool Store::setUserStatus(int userId, int status) {
    auto lk = lockGuard();
    if (!db_)
        return false;
    sqlite3_stmt* st = nullptr;
    if (sqlite3_prepare_v2(db_, "UPDATE users SET status=? WHERE id=?", -1,
                           &st, nullptr) != SQLITE_OK)
        return false;
    sqlite3_bind_int(st, 1, status);
    sqlite3_bind_int(st, 2, userId);
    const int rc = sqlite3_step(st);
    const bool changed = rc == SQLITE_DONE && sqlite3_changes(db_) > 0;
    sqlite3_finalize(st);
    return changed;
}

int Store::createStation(const QString& name, const QString& address, double lat,
                         double lng, ncs::MoneyCents priceCents) {
    auto lk = lockGuard();
    if (!db_)
        return -1;
    const char* sql =
        "INSERT INTO stations(name,address,latitude,longitude,total_piles,price_cents,free_piles) "
        "VALUES (?,?,?,?,0,?,0)";
    sqlite3_stmt* st = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &st, nullptr) != SQLITE_OK)
        return -1;
    const QByteArray n = name.toUtf8(), a = address.toUtf8();
    sqlite3_bind_text(st, 1, n.constData(), n.size(), SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 2, a.constData(), a.size(), SQLITE_TRANSIENT);
    sqlite3_bind_double(st, 3, lat);
    sqlite3_bind_double(st, 4, lng);
    sqlite3_bind_int64(st, 5, static_cast<sqlite3_int64>(priceCents));
    const int rc = sqlite3_step(st);
    sqlite3_finalize(st);
    return rc == SQLITE_DONE ? static_cast<int>(sqlite3_last_insert_rowid(db_))
                             : -1;
}

bool Store::updateStation(int id, const QString& name, const QString& address,
                          double lat, double lng, ncs::MoneyCents priceCents) {
    auto lk = lockGuard();
    if (!db_)
        return false;
    const char* sql = "UPDATE stations SET name=?,address=?,latitude=?,longitude=?,"
                      "price_cents=? WHERE id=?";
    sqlite3_stmt* st = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &st, nullptr) != SQLITE_OK)
        return false;
    const QByteArray n = name.toUtf8(), a = address.toUtf8();
    sqlite3_bind_text(st, 1, n.constData(), n.size(), SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 2, a.constData(), a.size(), SQLITE_TRANSIENT);
    sqlite3_bind_double(st, 3, lat);
    sqlite3_bind_double(st, 4, lng);
    sqlite3_bind_int64(st, 5, static_cast<sqlite3_int64>(priceCents));
    sqlite3_bind_int(st, 6, id);
    const int rc = sqlite3_step(st);
    const bool changed = rc == SQLITE_DONE && sqlite3_changes(db_) > 0;
    sqlite3_finalize(st);
    return changed;
}

qint64 Store::countDevicesByStation(int stationId) const {
    auto lk = lockGuard();
    if (!db_)
        return -1;
    sqlite3_stmt* st = nullptr;
    if (sqlite3_prepare_v2(db_, "SELECT COUNT(*) FROM devices WHERE station_id=?",
                           -1, &st, nullptr) != SQLITE_OK)
        return -1;
    sqlite3_bind_int(st, 1, stationId);
    qint64 n = 0;
    if (sqlite3_step(st) == SQLITE_ROW)
        n = sqlite3_column_int64(st, 0);
    sqlite3_finalize(st);
    return n;
}

bool Store::deleteStationById(int id) {
    auto lk = lockGuard();
    if (!db_)
        return false;
    sqlite3_stmt* st = nullptr;
    if (sqlite3_prepare_v2(db_, "DELETE FROM stations WHERE id=?", -1, &st,
                           nullptr) != SQLITE_OK)
        return false;
    sqlite3_bind_int(st, 1, id);
    const int rc = sqlite3_step(st);
    const bool changed = rc == SQLITE_DONE && sqlite3_changes(db_) > 0;
    sqlite3_finalize(st);
    return changed;
}

bool Store::createDevices(int stationId, int type, int count, double powerKw) {
    auto lk = lockGuard();
    if (!db_ || count <= 0)
        return false;
    const char* sql =
        "INSERT INTO devices(station_id,type,state,power_kw,energy_kwh) VALUES (?,?,0,?,0)";
    sqlite3_stmt* st = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &st, nullptr) != SQLITE_OK)
        return false;
    sqlite3_bind_int(st, 1, stationId);
    sqlite3_bind_int(st, 2, type);
    sqlite3_bind_double(st, 3, powerKw);
    bool all = true;
    for (int i = 0; i < count; ++i) {
        sqlite3_reset(st);
        if (sqlite3_step(st) != SQLITE_DONE) {
            all = false;
            break;
        }
    }
    sqlite3_finalize(st);
    if (!all)
        return false;
    const QByteArray up =
        QByteArrayLiteral("UPDATE stations SET total_piles=total_piles+%1, "
                          "free_piles=free_piles+%2 WHERE id=%3")
            .replace("%1", QByteArray::number(count))
            .replace("%2", QByteArray::number(count))
            .replace("%3", QByteArray::number(stationId));
    char* err = nullptr;
    const int rc = sqlite3_exec(db_, up.constData(), nullptr, nullptr, &err);
    if (rc != SQLITE_OK)
        sqlite3_free(err);
    return rc == SQLITE_OK;
}

int Store::deleteDeviceIfIdle(int id) {
    auto lk = lockGuard();
    if (!db_)
        return -1;
    sqlite3_stmt* st = nullptr;
    if (sqlite3_prepare_v2(db_, "SELECT state,station_id FROM devices WHERE id=?",
                           -1, &st, nullptr) != SQLITE_OK)
        return -1;
    sqlite3_bind_int(st, 1, id);
    int state = -1, station = -1;
    if (sqlite3_step(st) == SQLITE_ROW) {
        state = sqlite3_column_int(st, 0);
        station = sqlite3_column_int(st, 1);
    }
    sqlite3_finalize(st);
    if (state == -1)
        return -1;
    if (state != 0)
        return 0;
    sqlite3_stmt* del = nullptr;
    if (sqlite3_prepare_v2(db_, "DELETE FROM devices WHERE id=?", -1, &del,
                           nullptr) != SQLITE_OK)
        return -1;
    sqlite3_bind_int(del, 1, id);
    const int rc = sqlite3_step(del);
    sqlite3_finalize(del);
    if (rc != SQLITE_DONE)
        return -1;
    char* err = nullptr;
    const QByteArray up =
        QByteArrayLiteral(
            "UPDATE stations SET total_piles=MAX(0,total_piles-1), "
            "free_piles=MAX(0,free_piles-1) WHERE id=") +
        QByteArray::number(station);
    sqlite3_exec(db_, up.constData(), nullptr, nullptr, &err);
    return 1;
}
}  // namespace backend
}  // namespace ncs
