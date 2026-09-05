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

    // users
    const char* sqlUsers = "CREATE TABLE IF NOT EXISTS users ("
                           "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
                           "  phone TEXT NOT NULL UNIQUE,"
                           "  nickname TEXT NOT NULL,"
                           "  balance_cents INTEGER NOT NULL DEFAULT 0,"
                           "  status INTEGER NOT NULL DEFAULT 0,"
                           "  registered_at TEXT NOT NULL"
                           ")";
    char* err = nullptr;
    if (sqlite3_exec(db_, sqlUsers, nullptr, nullptr, &err) != SQLITE_OK) {
        sqlite3_free(err);
        return false;
    }
    // stations
    const char* sqlStations = "CREATE TABLE IF NOT EXISTS stations ("
                              "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
                              "  name TEXT NOT NULL,"
                              "  address TEXT NOT NULL,"
                              "  latitude REAL NOT NULL DEFAULT 0,"
                              "  longitude REAL NOT NULL DEFAULT 0,"
                              "  total_piles INTEGER NOT NULL DEFAULT 0,"
                              "  price_cents INTEGER NOT NULL DEFAULT 0,"
                              "  free_piles INTEGER NOT NULL DEFAULT 0"
                              ")";
    if (sqlite3_exec(db_, sqlStations, nullptr, nullptr, &err) != SQLITE_OK) {
        sqlite3_free(err);
        return false;
    }
    // devices
    const char* sqlDevices = "CREATE TABLE IF NOT EXISTS devices ("
                             "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
                             "  station_id INTEGER NOT NULL,"
                             "  type INTEGER NOT NULL DEFAULT 0,"
                             "  state INTEGER NOT NULL DEFAULT 0,"
                             "  power_kw REAL NOT NULL DEFAULT 0,"
                             "  energy_kwh REAL NOT NULL DEFAULT 0"
                             ")";
    if (sqlite3_exec(db_, sqlDevices, nullptr, nullptr, &err) != SQLITE_OK) {
        sqlite3_free(err);
        return false;
    }
    return seedIfEmptyLocked();
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

bool Store::seedIfEmptyLocked() {
    // 首次(空库)预置假数据：3 站 / 9 桩；free_piles 与桩状态保持一致。
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
        " (1,0,0,120.0,0.0),(1,0,0,120.0,0.0),(1,0,1,120.0,12.5),"  // 望京 2 空闲+1 充电中
        " (2,1,0,7.0,0.0),(2,1,0,7.0,0.0),(2,1,2,0.0,3.2),(2,1,0,7.0,0.0),"  // 中关村 3 空闲+1 故障
        " (3,0,0,180.0,0.0),(3,0,0,180.0,0.0)";  // 亦庄 2 空闲
    if (sqlite3_exec(db_, devices, nullptr, nullptr, &err) != SQLITE_OK) {
        sqlite3_free(err);
        return false;
    }
    return true;
}

QVector<ncs::Station> Store::listStations() const {
    QVector<ncs::Station> out;
    std::lock_guard<std::mutex> lk(mu_);
    if (!db_)
        return out;
    sqlite3_stmt* st = nullptr;
    if (sqlite3_prepare_v2(db_,
                           "SELECT id,name,address,latitude,longitude,"
                           "total_piles,price_cents,free_piles FROM stations ORDER BY id",
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
    std::lock_guard<std::mutex> lk(mu_);
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

}  // namespace backend
}  // namespace ncs
