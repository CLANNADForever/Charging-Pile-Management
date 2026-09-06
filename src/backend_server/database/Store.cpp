#include "Store.h"

#include <sqlite3.h>

#include <QDate>
#include <QDateTime>
#include <QCryptographicHash>
#include <QMap>
#include <QStringList>

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
        " price_cents INTEGER NOT NULL DEFAULT 0, free_piles INTEGER NOT NULL DEFAULT 0,"
        " price_slow_cents INTEGER NOT NULL DEFAULT 0, price_ultra_cents INTEGER NOT NULL DEFAULT 0,"
        " amenities INTEGER NOT NULL DEFAULT 0, parking INTEGER NOT NULL DEFAULT 0,"
        " location INTEGER NOT NULL DEFAULT 0, is_promo INTEGER NOT NULL DEFAULT 0,"
        " open_hours TEXT, min_charge_cents INTEGER NOT NULL DEFAULT 0)",
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
        " started_at TEXT NOT NULL, finished_at TEXT,"
        " charge_started_at TEXT, battery_cap_kwh REAL NOT NULL DEFAULT 60,"
        " start_soc_pct INTEGER NOT NULL DEFAULT 20)",
        "CREATE TABLE IF NOT EXISTS admins ("
        " username TEXT PRIMARY KEY, password TEXT NOT NULL,"
        " role TEXT NOT NULL DEFAULT 'super')",
        "CREATE TABLE IF NOT EXISTS admin_audit ("
        " id INTEGER PRIMARY KEY AUTOINCREMENT, username TEXT NOT NULL,"
        " action TEXT NOT NULL, detail TEXT NOT NULL DEFAULT '',"
        " result TEXT NOT NULL DEFAULT 'ok', created_at TEXT NOT NULL)",
        "CREATE TABLE IF NOT EXISTS device_ops ("
        " id INTEGER PRIMARY KEY AUTOINCREMENT, device_id INTEGER NOT NULL,"
        " op_type TEXT NOT NULL, op_by TEXT NOT NULL DEFAULT '',"
        " detail TEXT NOT NULL DEFAULT '', created_at TEXT NOT NULL)",
    };
    for (const char* sql : kTables) {
        char* err = nullptr;
        if (sqlite3_exec(db_, sql, nullptr, nullptr, &err) != SQLITE_OK) {
            sqlite3_free(err);
            return false;
        }
    }
    // 老库补 admins.role 列(新库建表已带)
    {
        sqlite3_stmt* st = nullptr;
        bool hasRole = false;
        if (sqlite3_prepare_v2(db_, "PRAGMA table_info(admins)", -1, &st,
                               nullptr) == SQLITE_OK) {
            while (sqlite3_step(st) == SQLITE_ROW) {
                if (columnText(st, 1) == QLatin1String("role")) {
                    hasRole = true;
                    break;
                }
            }
        }
        sqlite3_finalize(st);
        if (!hasRole) {
            char* err = nullptr;
            if (sqlite3_exec(db_,
                             "ALTER TABLE admins ADD COLUMN role TEXT NOT NULL DEFAULT 'super'",
                             nullptr, nullptr, &err) != SQLITE_OK)
                sqlite3_free(err);
        }
    }
    // 老库补 stations 运营属性列(新库建表已带)
    {
        sqlite3_stmt* st = nullptr;
        QStringList cols;
        if (sqlite3_prepare_v2(db_, "PRAGMA table_info(stations)", -1, &st,
                               nullptr) == SQLITE_OK) {
            while (sqlite3_step(st) == SQLITE_ROW)
                cols << columnText(st, 1);
        }
        sqlite3_finalize(st);
        const struct {
            const char* col;
            const char* ddl;
        } need[] = {
            {"amenities", "ALTER TABLE stations ADD COLUMN amenities INTEGER NOT NULL DEFAULT 0"},
            {"parking", "ALTER TABLE stations ADD COLUMN parking INTEGER NOT NULL DEFAULT 0"},
            {"location", "ALTER TABLE stations ADD COLUMN location INTEGER NOT NULL DEFAULT 0"},
            {"is_promo", "ALTER TABLE stations ADD COLUMN is_promo INTEGER NOT NULL DEFAULT 0"},
            {"open_hours", "ALTER TABLE stations ADD COLUMN open_hours TEXT"},
            {"min_charge_cents", "ALTER TABLE stations ADD COLUMN min_charge_cents INTEGER NOT NULL DEFAULT 0"},
            {"price_slow_cents", "ALTER TABLE stations ADD COLUMN price_slow_cents INTEGER NOT NULL DEFAULT 0"},
            {"price_ultra_cents", "ALTER TABLE stations ADD COLUMN price_ultra_cents INTEGER NOT NULL DEFAULT 0"},
        };
        bool addedTierCol = false;
        for (const auto& x : need) {
            if (cols.contains(QLatin1String(x.col)))
                continue;
            char* err = nullptr;
            if (sqlite3_exec(db_, x.ddl, nullptr, nullptr, &err) != SQLITE_OK) {
                sqlite3_free(err);
            } else if (QLatin1String(x.col) == QLatin1String("price_slow_cents") ||
                       QLatin1String(x.col) == QLatin1String("price_ultra_cents")) {
                addedTierCol = true;
            }
        }
        // 老库升级：分档价列是这次新增的，旧站只有 price_cents。
        // 回填 slow/ultra=price_cents(即快充档)，避免慢/超静默走快充兜底、API 暴露 0 元。
        if (addedTierCol) {
            char* err = nullptr;
            if (sqlite3_exec(db_,
                             "UPDATE stations SET price_slow_cents=price_cents, "
                             "price_ultra_cents=price_cents "
                             "WHERE price_slow_cents<=0 AND price_ultra_cents<=0",
                             nullptr, nullptr, &err) != SQLITE_OK)
                sqlite3_free(err);
        }
    }
    // 老库补 orders 充电时刻/电池快照列(R4/R5)
    {
        sqlite3_stmt* st = nullptr;
        QStringList ocols;
        if (sqlite3_prepare_v2(db_, "PRAGMA table_info(orders)", -1, &st,
                               nullptr) == SQLITE_OK) {
            while (sqlite3_step(st) == SQLITE_ROW)
                ocols << columnText(st, 1);
        }
        sqlite3_finalize(st);
        const struct {
            const char* col;
            const char* ddl;
        } need[] = {
            {"charge_started_at", "ALTER TABLE orders ADD COLUMN charge_started_at TEXT"},
            {"battery_cap_kwh", "ALTER TABLE orders ADD COLUMN battery_cap_kwh REAL NOT NULL DEFAULT 60"},
            {"start_soc_pct", "ALTER TABLE orders ADD COLUMN start_soc_pct INTEGER NOT NULL DEFAULT 20"},
        };
        for (const auto& x : need) {
            if (ocols.contains(QLatin1String(x.col)))
                continue;
            char* err = nullptr;
            if (sqlite3_exec(db_, x.ddl, nullptr, nullptr, &err) != SQLITE_OK)
                sqlite3_free(err);
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
        "INSERT INTO stations(name,address,latitude,longitude,total_piles,price_cents,free_piles,"
        " price_slow_cents,price_ultra_cents,amenities,parking,location,is_promo,open_hours,min_charge_cents) VALUES"
        " ('望京充电站','北京市朝阳区望京街道',39.996,116.481,3,200,2,140,280,339,1,0,1,'00:00-24:00',0),"
        " ('中关村充电站','北京市海淀区中关村大街',39.984,116.316,4,180,3,160,300,107,0,1,0,'06:00-24:00',0),"
        " ('亦庄超充站','北京市大兴区荣华中路',39.795,116.506,2,240,2,180,320,405,2,0,1,'00:00-24:00',0)";
    char* err = nullptr;
    if (sqlite3_exec(db_, stations, nullptr, nullptr, &err) != SQLITE_OK) {
        sqlite3_free(err);
        return false;
    }
    const char* devices =
        "INSERT INTO devices(station_id,type,state,power_kw,energy_kwh) VALUES"
        " (1,0,0,120.0,0.0),(1,1,0,7.0,0.0),(1,0,1,180.0,12.5),"
        " (2,0,0,120.0,0.0),(2,1,0,7.0,0.0),(2,0,2,180.0,3.2),(2,0,0,120.0,0.0),"
        " (3,1,0,7.0,0.0),(3,0,0,180.0,0.0)";
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
                           "price_cents,free_piles,amenities,parking,location,is_promo,"
                           "open_hours,min_charge_cents,price_slow_cents,price_ultra_cents "
                           "FROM stations ORDER BY id",
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
        s.amenities = sqlite3_column_int(st, 8);
        s.parking = sqlite3_column_int(st, 9);
        s.location = sqlite3_column_int(st, 10);
        s.isPromo = sqlite3_column_int(st, 11) != 0;
        s.openHours = columnText(st, 12);
        s.minChargeCents = sqlite3_column_int64(st, 13);
        s.priceSlowCents = sqlite3_column_int64(st, 14);
        s.priceUltraCents = sqlite3_column_int64(st, 15);
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
                           "price_cents,free_piles,amenities,parking,location,is_promo,"
                           "open_hours,min_charge_cents,price_slow_cents,price_ultra_cents "
                           "FROM stations WHERE id=?",
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
        out->amenities = sqlite3_column_int(st, 8);
        out->parking = sqlite3_column_int(st, 9);
        out->location = sqlite3_column_int(st, 10);
        out->isPromo = sqlite3_column_int(st, 11) != 0;
        out->openHours = columnText(st, 12);
        out->minChargeCents = sqlite3_column_int64(st, 13);
        out->priceSlowCents = sqlite3_column_int64(st, 14);
        out->priceUltraCents = sqlite3_column_int64(st, 15);
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
                           "amount_cents,energy_kwh,status,started_at,finished_at,charge_started_at,battery_cap_kwh,start_soc_pct "
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
        out->chargeStartedAt = fromDbIso(columnText(st, 10));
        out->batteryCapKwh = sqlite3_column_double(st, 11);
        out->startSocPct = sqlite3_column_int(st, 12);
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

bool Store::setOrderChargeStarted(int id, const QString& isoUtc) {
    auto lk = lockGuard();
    if (!db_)
        return false;
    sqlite3_stmt* st = nullptr;
    if (sqlite3_prepare_v2(db_, "UPDATE orders SET charge_started_at=? WHERE id=?",
                           -1, &st, nullptr) != SQLITE_OK)
        return false;
    const QByteArray iso = isoUtc.toUtf8();
    sqlite3_bind_text(st, 1, iso.constData(), iso.size(), SQLITE_TRANSIENT);
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
                           "amount_cents,energy_kwh,status,started_at,finished_at,charge_started_at,battery_cap_kwh,start_soc_pct "
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
        o.chargeStartedAt = fromDbIso(columnText(st, 10));
        o.batteryCapKwh = sqlite3_column_double(st, 11);
        o.startSocPct = sqlite3_column_int(st, 12);
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
                           "amount_cents,energy_kwh,status,started_at,finished_at,charge_started_at,battery_cap_kwh,start_soc_pct "
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
        o.chargeStartedAt = fromDbIso(columnText(st, 10));
        o.batteryCapKwh = sqlite3_column_double(st, 11);
        o.startSocPct = sqlite3_column_int(st, 12);
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
    const auto h = [](const QString& s) {
        return QCryptographicHash::hash(s.toUtf8(), QCryptographicHash::Sha256)
            .toHex();
    };
    const auto ins = [this, &h](const QString& u, const QString& pass,
                            const QString& role) {
        sqlite3_stmt* st = nullptr;
        const char* sql =
            "INSERT OR IGNORE INTO admins(username,password,role) VALUES(?,?,?)";
        if (sqlite3_prepare_v2(db_, sql, -1, &st, nullptr) != SQLITE_OK)
            return false;
        const QByteArray un = u.toUtf8();
        const QByteArray pw = h(pass);
        const QByteArray ro = role.toUtf8();
        sqlite3_bind_text(st, 1, un.constData(), un.size(), SQLITE_TRANSIENT);
        sqlite3_bind_text(st, 2, pw.constData(), pw.size(), SQLITE_TRANSIENT);
        sqlite3_bind_text(st, 3, ro.constData(), ro.size(), SQLITE_TRANSIENT);
        const int rc = sqlite3_step(st);
        sqlite3_finalize(st);
        return rc == SQLITE_DONE;
    };
    return ins(QStringLiteral("admin"), QStringLiteral("admin123"),
               QStringLiteral("super")) &&
           ins(QStringLiteral("operator"), QStringLiteral("operator123"),
               QStringLiteral("operator")) &&
           ins(QStringLiteral("viewer"), QStringLiteral("viewer123"),
               QStringLiteral("viewer"));
}

bool Store::authenticateAdmin(const QString& username,
                              const QString& password,
                              QString* roleOut) const {
    auto lk = lockGuard();
    if (!db_)
        return false;
    const QByteArray h = QCryptographicHash::hash(
                             password.toUtf8(), QCryptographicHash::Sha256)
                             .toHex();
    sqlite3_stmt* st = nullptr;
    if (sqlite3_prepare_v2(db_, "SELECT password,role FROM admins WHERE username=?",
                           -1, &st, nullptr) != SQLITE_OK)
        return false;
    const QByteArray un = username.toUtf8();
    sqlite3_bind_text(st, 1, un.constData(), un.size(), SQLITE_TRANSIENT);
    bool ok = false;
    if (sqlite3_step(st) == SQLITE_ROW) {
        ok = columnText(st, 0) == QString::fromLatin1(h);
        if (ok && roleOut)
            *roleOut = columnText(st, 1);
    }
    sqlite3_finalize(st);
    return ok;
}

QVector<ncs::User> Store::searchUsers(const QString& phone,
                                      int statusFilter) const {
    QVector<ncs::User> out;
    auto lk = lockGuard();
    if (!db_)
        return out;
    QString sql = QStringLiteral(
        "SELECT id,phone,nickname,balance_cents,status,registered_at "
        "FROM users WHERE 1=1");
    if (!phone.isEmpty())
        sql += QStringLiteral(" AND phone LIKE ?");
    if (statusFilter == 1)
        sql += QStringLiteral(" AND status=0");
    else if (statusFilter == 2)
        sql += QStringLiteral(" AND status=1");
    sql += QStringLiteral(" ORDER BY id LIMIT 50");
    sqlite3_stmt* st = nullptr;
    if (sqlite3_prepare_v2(db_, sql.toUtf8().constData(), -1, &st, nullptr) !=
        SQLITE_OK)
        return out;
    int bi = 1;
    QByteArray like;
    if (!phone.isEmpty()) {
        like = ("%" + phone + "%").toUtf8();
        sqlite3_bind_text(st, bi++, like.constData(), like.size(),
                          SQLITE_TRANSIENT);
    }
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
                         double lng, ncs::MoneyCents priceCents,
                         const StationFields& f) {
    auto lk = lockGuard();
    if (!db_)
        return -1;
    const char* sql =
        "INSERT INTO stations(name,address,latitude,longitude,total_piles,price_cents,"
        "free_piles,price_slow_cents,price_ultra_cents,amenities,parking,location,is_promo,"
        "open_hours,min_charge_cents) VALUES (?,?,?,?,0,?,0,?,?,?,?,?,?,?,?)";
    sqlite3_stmt* st = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &st, nullptr) != SQLITE_OK)
        return -1;
    const QByteArray n = name.toUtf8(), a = address.toUtf8();
    const QByteArray oh = f.openHours.toUtf8();
    sqlite3_bind_text(st, 1, n.constData(), n.size(), SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 2, a.constData(), a.size(), SQLITE_TRANSIENT);
    sqlite3_bind_double(st, 3, lat);
    sqlite3_bind_double(st, 4, lng);
    sqlite3_bind_int64(st, 5, static_cast<sqlite3_int64>(priceCents));
    sqlite3_bind_int64(st, 6, static_cast<sqlite3_int64>(f.priceSlowCents));
    sqlite3_bind_int64(st, 7, static_cast<sqlite3_int64>(f.priceUltraCents));
    sqlite3_bind_int(st, 8, f.amenities);
    sqlite3_bind_int(st, 9, f.parking);
    sqlite3_bind_int(st, 10, f.location);
    sqlite3_bind_int(st, 11, f.isPromo ? 1 : 0);
    sqlite3_bind_text(st, 12, oh.constData(), oh.size(), SQLITE_TRANSIENT);
    sqlite3_bind_int64(st, 13, static_cast<sqlite3_int64>(f.minChargeCents));
    const int rc = sqlite3_step(st);
    sqlite3_finalize(st);
    return rc == SQLITE_DONE ? static_cast<int>(sqlite3_last_insert_rowid(db_))
                             : -1;
}

bool Store::updateStation(int id, const QString& name, const QString& address,
                          double lat, double lng, ncs::MoneyCents priceCents,
                          const StationFields& f) {
    auto lk = lockGuard();
    if (!db_)
        return false;
    const char* sql = "UPDATE stations SET name=?,address=?,latitude=?,longitude=?,"
                      "price_cents=?,price_slow_cents=?,price_ultra_cents=?,amenities=?,"
                      "parking=?,location=?,is_promo=?,open_hours=?,min_charge_cents=? "
                      "WHERE id=?";
    sqlite3_stmt* st = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &st, nullptr) != SQLITE_OK)
        return false;
    const QByteArray n = name.toUtf8(), a = address.toUtf8();
    const QByteArray oh = f.openHours.toUtf8();
    sqlite3_bind_text(st, 1, n.constData(), n.size(), SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 2, a.constData(), a.size(), SQLITE_TRANSIENT);
    sqlite3_bind_double(st, 3, lat);
    sqlite3_bind_double(st, 4, lng);
    sqlite3_bind_int64(st, 5, static_cast<sqlite3_int64>(priceCents));
    sqlite3_bind_int64(st, 6, static_cast<sqlite3_int64>(f.priceSlowCents));
    sqlite3_bind_int64(st, 7, static_cast<sqlite3_int64>(f.priceUltraCents));
    sqlite3_bind_int(st, 8, f.amenities);
    sqlite3_bind_int(st, 9, f.parking);
    sqlite3_bind_int(st, 10, f.location);
    sqlite3_bind_int(st, 11, f.isPromo ? 1 : 0);
    sqlite3_bind_text(st, 12, oh.constData(), oh.size(), SQLITE_TRANSIENT);
    sqlite3_bind_int64(st, 13, static_cast<sqlite3_int64>(f.minChargeCents));
    sqlite3_bind_int(st, 14, id);
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

int Store::createDevices(int stationId, int type, int count, double powerKw) {
    if (!db_ || count <= 0)
        return -2;
    if (!beginTx())
        return -2;
    // 校验站存在，杜绝孤儿桩
    {
        sqlite3_stmt* st = nullptr;
        if (sqlite3_prepare_v2(db_, "SELECT 1 FROM stations WHERE id=?", -1,
                               &st, nullptr) != SQLITE_OK) {
            rollbackTx();
            return -2;
        }
        sqlite3_bind_int(st, 1, stationId);
        const bool exists = sqlite3_step(st) == SQLITE_ROW;
        sqlite3_finalize(st);
        if (!exists) {
            rollbackTx();
            return -1;
        }
    }
    const char* sql =
        "INSERT INTO devices(station_id,type,state,power_kw,energy_kwh) VALUES (?,?,0,?,0)";
    sqlite3_stmt* st = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &st, nullptr) != SQLITE_OK) {
        rollbackTx();
        return -2;
    }
    sqlite3_bind_int(st, 1, stationId);
    sqlite3_bind_int(st, 2, type);
    sqlite3_bind_double(st, 3, powerKw);
    for (int i = 0; i < count; ++i) {
        sqlite3_reset(st);
        if (sqlite3_step(st) != SQLITE_DONE) {
            sqlite3_finalize(st);
            rollbackTx();
            return -2;
        }
    }
    sqlite3_finalize(st);
    sqlite3_stmt* up = nullptr;
    const char* upSql =
        "UPDATE stations SET total_piles=total_piles+?, free_piles=free_piles+? "
        "WHERE id=?";
    if (sqlite3_prepare_v2(db_, upSql, -1, &up, nullptr) != SQLITE_OK) {
        rollbackTx();
        return -2;
    }
    sqlite3_bind_int(up, 1, count);
    sqlite3_bind_int(up, 2, count);
    sqlite3_bind_int(up, 3, stationId);
    const int urc = sqlite3_step(up);
    sqlite3_finalize(up);
    if (urc != SQLITE_DONE || sqlite3_changes(db_) <= 0) {
        rollbackTx();
        return -3;
    }
    if (!commitTx())
        return -3;
    return 1;
}

int Store::deleteDeviceIfIdle(int id) {
    if (!db_)
        return -1;
    if (!beginTx())
        return -1;
    sqlite3_stmt* st = nullptr;
    if (sqlite3_prepare_v2(db_, "SELECT state,station_id FROM devices WHERE id=?",
                           -1, &st, nullptr) != SQLITE_OK) {
        rollbackTx();
        return -1;
    }
    sqlite3_bind_int(st, 1, id);
    int state = -1, station = -1;
    if (sqlite3_step(st) == SQLITE_ROW) {
        state = sqlite3_column_int(st, 0);
        station = sqlite3_column_int(st, 1);
    }
    sqlite3_finalize(st);
    if (state < 0) {
        rollbackTx();
        return -1;
    }
    if (state != 0) {
        rollbackTx();
        return 0;
    }
    sqlite3_stmt* del = nullptr;
    if (sqlite3_prepare_v2(db_, "DELETE FROM devices WHERE id=?", -1, &del,
                           nullptr) != SQLITE_OK) {
        rollbackTx();
        return -1;
    }
    sqlite3_bind_int(del, 1, id);
    const int rc = sqlite3_step(del);
    sqlite3_finalize(del);
    if (rc != SQLITE_DONE) {
        rollbackTx();
        return -1;
    }
    sqlite3_stmt* up = nullptr;
    const char* upSql =
        "UPDATE stations SET total_piles=MAX(0,total_piles-1), "
        "free_piles=MAX(0,free_piles-1) WHERE id=?";
    if (sqlite3_prepare_v2(db_, upSql, -1, &up, nullptr) != SQLITE_OK) {
        rollbackTx();
        return -1;
    }
    sqlite3_bind_int(up, 1, station);
    const int urc = sqlite3_step(up);
    sqlite3_finalize(up);
    if (urc != SQLITE_DONE) {
        rollbackTx();
        return -1;
    }
    if (!commitTx())
        return -1;
    return 1;
}

// ---------- B2：审计 / 运维日志 ----------

bool Store::appendAudit(const QString& username, const QString& action,
                        const QString& detail, bool ok) {
    auto lk = lockGuard();
    if (!db_)
        return false;
    sqlite3_stmt* st = nullptr;
    const char* sql = "INSERT INTO admin_audit(username,action,detail,result,created_at)"
                      " VALUES(?,?,?,?,?)";
    if (sqlite3_prepare_v2(db_, sql, -1, &st, nullptr) != SQLITE_OK)
        return false;
    const QByteArray u = username.toUtf8();
    const QByteArray a = action.toUtf8();
    const QByteArray d = detail.toUtf8();
    const QByteArray r = ok ? QByteArrayLiteral("ok") : QByteArrayLiteral("fail");
    const QByteArray iso = isoNowUtc().toUtf8();
    sqlite3_bind_text(st, 1, u.constData(), u.size(), SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 2, a.constData(), a.size(), SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 3, d.constData(), d.size(), SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 4, r.constData(), r.size(), SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 5, iso.constData(), iso.size(), SQLITE_TRANSIENT);
    const int rc = sqlite3_step(st);
    sqlite3_finalize(st);
    return rc == SQLITE_DONE;
}

QVector<AuditRow> Store::listAudit(int limit, int offset) const {
    QVector<AuditRow> out;
    auto lk = lockGuard();
    if (!db_)
        return out;
    sqlite3_stmt* st = nullptr;
    const char* sql =
        "SELECT id,username,action,detail,result,created_at FROM admin_audit "
        "ORDER BY id DESC LIMIT ? OFFSET ?";
    if (sqlite3_prepare_v2(db_, sql, -1, &st, nullptr) != SQLITE_OK)
        return out;
    sqlite3_bind_int(st, 1, limit);
    sqlite3_bind_int(st, 2, offset);
    while (sqlite3_step(st) == SQLITE_ROW) {
        AuditRow r;
        r.id = sqlite3_column_int(st, 0);
        r.username = columnText(st, 1);
        r.action = columnText(st, 2);
        r.detail = columnText(st, 3);
        r.result = columnText(st, 4);
        r.at = fromDbIso(columnText(st, 5));
        out.push_back(r);
    }
    sqlite3_finalize(st);
    return out;
}

qint64 Store::countAudit() const {
    auto lk = lockGuard();
    if (!db_)
        return -1;
    sqlite3_stmt* st = nullptr;
    if (sqlite3_prepare_v2(db_, "SELECT COUNT(*) FROM admin_audit", -1, &st,
                           nullptr) != SQLITE_OK)
        return -1;
    qint64 n = 0;
    if (sqlite3_step(st) == SQLITE_ROW)
        n = sqlite3_column_int64(st, 0);
    sqlite3_finalize(st);
    return n;
}

bool Store::appendDeviceOp(int deviceId, const QString& opType,
                           const QString& opBy, const QString& detail) {
    auto lk = lockGuard();
    if (!db_)
        return false;
    sqlite3_stmt* st = nullptr;
    const char* sql = "INSERT INTO device_ops(device_id,op_type,op_by,detail,created_at)"
                      " VALUES(?,?,?,?,?)";
    if (sqlite3_prepare_v2(db_, sql, -1, &st, nullptr) != SQLITE_OK)
        return false;
    const QByteArray t = opType.toUtf8();
    const QByteArray by = opBy.toUtf8();
    const QByteArray d = detail.toUtf8();
    const QByteArray iso = isoNowUtc().toUtf8();
    sqlite3_bind_int(st, 1, deviceId);
    sqlite3_bind_text(st, 2, t.constData(), t.size(), SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 3, by.constData(), by.size(), SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 4, d.constData(), d.size(), SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 5, iso.constData(), iso.size(), SQLITE_TRANSIENT);
    const int rc = sqlite3_step(st);
    sqlite3_finalize(st);
    return rc == SQLITE_DONE;
}

QVector<DeviceOpRow> Store::listDeviceOps(int limit, int offset) const {
    QVector<DeviceOpRow> out;
    auto lk = lockGuard();
    if (!db_)
        return out;
    sqlite3_stmt* st = nullptr;
    const char* sql =
        "SELECT id,device_id,op_type,op_by,detail,created_at FROM device_ops "
        "ORDER BY id DESC LIMIT ? OFFSET ?";
    if (sqlite3_prepare_v2(db_, sql, -1, &st, nullptr) != SQLITE_OK)
        return out;
    sqlite3_bind_int(st, 1, limit);
    sqlite3_bind_int(st, 2, offset);
    while (sqlite3_step(st) == SQLITE_ROW) {
        DeviceOpRow r;
        r.id = sqlite3_column_int(st, 0);
        r.deviceId = sqlite3_column_int(st, 1);
        r.opType = columnText(st, 2);
        r.opBy = columnText(st, 3);
        r.detail = columnText(st, 4);
        r.at = fromDbIso(columnText(st, 5));
        out.push_back(r);
    }
    sqlite3_finalize(st);
    return out;
}

qint64 Store::countDeviceOps() const {
    auto lk = lockGuard();
    if (!db_)
        return -1;
    sqlite3_stmt* st = nullptr;
    if (sqlite3_prepare_v2(db_, "SELECT COUNT(*) FROM device_ops", -1, &st,
                           nullptr) != SQLITE_OK)
        return -1;
    qint64 n = 0;
    if (sqlite3_step(st) == SQLITE_ROW)
        n = sqlite3_column_int64(st, 0);
    sqlite3_finalize(st);
    return n;
}

// ---------- B2：管理端列表聚合 / 统计 ----------

bool Store::listDevicesAdmin(const DeviceFilter& f, int limit, int offset,
                             QVector<DeviceRow>* rows) const {
    if (!rows)
        return false;
    rows->clear();
    auto lk = lockGuard();
    if (!db_)
        return false;
    QString sql = QStringLiteral(
        "SELECT d.id,d.station_id,s.name,d.type,d.state,d.power_kw,"
        " COUNT(o.id) AS sessions,"
        " COALESCE(SUM(CASE WHEN o.finished_at IS NOT NULL THEN"
        "  (julianday(o.finished_at)-julianday(o.started_at))*86400.0 ELSE 0 END),0)"
        " FROM devices d JOIN stations s ON s.id=d.station_id"
        " LEFT JOIN orders o ON o.device_id=d.id AND o.status IN (2,3)"
        " WHERE 1=1");
    if (f.stationId >= 0)
        sql += QStringLiteral(" AND d.station_id=%1").arg(f.stationId);
    if (f.type >= 0)
        sql += QStringLiteral(" AND d.type=%1").arg(f.type);
    if (f.state >= 0)
        sql += QStringLiteral(" AND d.state=%1").arg(f.state);
    const QString q = f.q.trimmed();
    if (!q.isEmpty())
        sql += QStringLiteral(" AND CAST(d.id AS TEXT) LIKE ?");
    sql += QStringLiteral(" GROUP BY d.id ORDER BY d.id LIMIT ? OFFSET ?");
    sqlite3_stmt* st = nullptr;
    if (sqlite3_prepare_v2(db_, sql.toUtf8().constData(), -1, &st, nullptr) !=
        SQLITE_OK)
        return false;
    int bi = 1;
    QByteArray pat;
    if (!q.isEmpty()) {
        pat = ("%" + q + "%").toUtf8();
        sqlite3_bind_text(st, bi++, pat.constData(), pat.size(),
                          SQLITE_TRANSIENT);
    }
    sqlite3_bind_int(st, bi++, limit);
    sqlite3_bind_int(st, bi++, offset);
    while (sqlite3_step(st) == SQLITE_ROW) {
        DeviceRow r;
        r.dev.id = sqlite3_column_int(st, 0);
        r.dev.stationId = sqlite3_column_int(st, 1);
        r.stationName = columnText(st, 2);
        r.dev.type = static_cast<ncs::DeviceType>(sqlite3_column_int(st, 3));
        r.dev.state = static_cast<ncs::DeviceState>(sqlite3_column_int(st, 4));
        r.dev.powerKw = sqlite3_column_double(st, 5);
        r.sessions = sqlite3_column_int64(st, 6);
        r.chargeSec = sqlite3_column_double(st, 7);
        rows->push_back(r);
    }
    sqlite3_finalize(st);
    return true;
}

qint64 Store::countDevicesAdmin(const DeviceFilter& f) const {
    auto lk = lockGuard();
    if (!db_)
        return -1;
    QString sql = QStringLiteral(
        "SELECT COUNT(*) FROM devices d JOIN stations s ON s.id=d.station_id"
        " WHERE 1=1");
    if (f.stationId >= 0)
        sql += QStringLiteral(" AND d.station_id=%1").arg(f.stationId);
    if (f.type >= 0)
        sql += QStringLiteral(" AND d.type=%1").arg(f.type);
    if (f.state >= 0)
        sql += QStringLiteral(" AND d.state=%1").arg(f.state);
    const QString q = f.q.trimmed();
    if (!q.isEmpty())
        sql += QStringLiteral(" AND CAST(d.id AS TEXT) LIKE ?");
    sqlite3_stmt* st = nullptr;
    if (sqlite3_prepare_v2(db_, sql.toUtf8().constData(), -1, &st, nullptr) !=
        SQLITE_OK)
        return -1;
    int bi = 1;
    QByteArray pat;
    if (!q.isEmpty()) {
        pat = ("%" + q + "%").toUtf8();
        sqlite3_bind_text(st, bi++, pat.constData(), pat.size(),
                          SQLITE_TRANSIENT);
    }
    qint64 n = 0;
    if (sqlite3_step(st) == SQLITE_ROW)
        n = sqlite3_column_int64(st, 0);
    sqlite3_finalize(st);
    return n;
}

RevenueAgg Store::revenueWindow(const QString& fromIso,
                                       const QString& toIso) const {
    RevenueAgg a;
    auto lk = lockGuard();
    if (!db_)
        return a;
    QString sql = QStringLiteral(
        "SELECT COALESCE(SUM(amount_cents),0), COUNT(*), "
        "COALESCE(SUM(energy_kwh),0) FROM orders WHERE status=3");
    if (!fromIso.isEmpty())
        sql += QStringLiteral(" AND finished_at >= ?");
    if (!toIso.isEmpty())
        sql += QStringLiteral(" AND finished_at < ?");
    sqlite3_stmt* st = nullptr;
    if (sqlite3_prepare_v2(db_, sql.toUtf8().constData(), -1, &st, nullptr) !=
        SQLITE_OK)
        return a;
    int bi = 1;
    QByteArray fr = fromIso.toUtf8();
    QByteArray to = toIso.toUtf8();
    if (!fromIso.isEmpty())
        sqlite3_bind_text(st, bi++, fr.constData(), fr.size(), SQLITE_TRANSIENT);
    if (!toIso.isEmpty())
        sqlite3_bind_text(st, bi++, to.constData(), to.size(), SQLITE_TRANSIENT);
    if (sqlite3_step(st) == SQLITE_ROW) {
        a.cents = sqlite3_column_int64(st, 0);
        a.orders = sqlite3_column_int64(st, 1);
        a.energyKwh = sqlite3_column_double(st, 2);
    }
    sqlite3_finalize(st);
    return a;
}

QVector<DailyRevenue> Store::dailyRevenue(int days) const {
    QVector<DailyRevenue> out;
    auto lk = lockGuard();
    if (!db_ || days <= 0)
        return out;
    const QDate today = QDateTime::currentDateTimeUtc().date();
    const QDate start = today.addDays(-(days - 1));
    const QString fromIso =
        QStringLiteral("%1T00:00:00").arg(start.toString(Qt::ISODate));
    sqlite3_stmt* st = nullptr;
    const char* sql =
        "SELECT substr(finished_at,1,10), SUM(amount_cents), COUNT(*), "
        "SUM(energy_kwh) FROM orders WHERE status=3 AND finished_at >= ? "
        "GROUP BY substr(finished_at,1,10)";
    if (sqlite3_prepare_v2(db_, sql, -1, &st, nullptr) != SQLITE_OK)
        return out;
    const QByteArray fr = fromIso.toUtf8();
    sqlite3_bind_text(st, 1, fr.constData(), fr.size(), SQLITE_TRANSIENT);
    QMap<QString, DailyRevenue> byDay;
    while (sqlite3_step(st) == SQLITE_ROW) {
        DailyRevenue r;
        r.day = columnText(st, 0);
        r.cents = sqlite3_column_int64(st, 1);
        r.orders = sqlite3_column_int64(st, 2);
        r.energyKwh = sqlite3_column_double(st, 3);
        byDay.insert(r.day, r);
    }
    sqlite3_finalize(st);
    for (int i = 0; i < days; ++i) {
        const QString key = start.addDays(i).toString(Qt::ISODate);
        const auto it = byDay.constFind(key);
        if (it != byDay.constEnd())
            out.push_back(it.value());
        else {
            DailyRevenue r;
            r.day = key;
            out.push_back(r);
        }
    }
    return out;
}

QVector<int> Store::deviceStateCounts() const {
    QVector<int> out(5, 0);  // DeviceState 0..4
    auto lk = lockGuard();
    if (!db_)
        return out;
    sqlite3_stmt* st = nullptr;
    if (sqlite3_prepare_v2(db_, "SELECT state, COUNT(*) FROM devices GROUP BY state",
                           -1, &st, nullptr) != SQLITE_OK)
        return out;
    while (sqlite3_step(st) == SQLITE_ROW) {
        const int stv = sqlite3_column_int(st, 0);
        if (stv >= 0 && stv < out.size())
            out[stv] = sqlite3_column_int(st, 1);
    }
    sqlite3_finalize(st);
    return out;
}

QMap<int, qint64> Store::paidCount7dByStation() const {
    QMap<int, qint64> out;
    auto lk = lockGuard();
    if (!db_)
        return out;
    sqlite3_stmt* st = nullptr;
    const char* sql =
        "SELECT station_id, COUNT(*) FROM orders WHERE status=3 AND finished_at >= ? "
        "GROUP BY station_id";
    if (sqlite3_prepare_v2(db_, sql, -1, &st, nullptr) != SQLITE_OK)
        return out;
    const QDateTime cutoff = QDateTime::currentDateTimeUtc().addDays(-7);
    const QByteArray iso = cutoff.toString(Qt::ISODate).toUtf8();
    sqlite3_bind_text(st, 1, iso.constData(), iso.size(), SQLITE_TRANSIENT);
    while (sqlite3_step(st) == SQLITE_ROW)
        out.insert(sqlite3_column_int(st, 0), sqlite3_column_int64(st, 1));
    sqlite3_finalize(st);
    return out;
}

QVector<std::pair<int,int>> Store::listDeviceStations() const {
    QVector<std::pair<int,int>> out;
    auto lk = lockGuard();
    if (!db_)
        return out;
    sqlite3_stmt* st = nullptr;
    if (sqlite3_prepare_v2(db_, "SELECT id, station_id FROM devices ORDER BY id",
                           -1, &st, nullptr) != SQLITE_OK)
        return out;
    while (sqlite3_step(st) == SQLITE_ROW)
        out.push_back({sqlite3_column_int(st, 0), sqlite3_column_int(st, 1)});
    sqlite3_finalize(st);
    return out;
}
}  // namespace backend
}  // namespace ncs
