#ifndef NCS_ADMIN_ADMINAPI_H
#define NCS_ADMIN_ADMINAPI_H

#include <functional>
#include <QDateTime>
#include <QJsonObject>
#include <QString>
#include <QVector>

#include "HttpJsonClient.h"
#include "entities.h"

namespace ncs {
namespace admin {
using HttpJsonClient = ::ncs::client::HttpJsonClient;
using Err = QString;  // 空 = 成功

// 管理端 API 薄封装(共享 HttpJsonClient；信封解析；登录后自动携带 token)。

struct LoginInfo {
    bool ok = false;
    QString err;
    QString username;
    QString role;
    QString token;
};

// B2 管理端列表/日志行
struct AdminStation {
    int id = 0;
    QString name;
    QString address;
    double latitude = 0.0;
    double longitude = 0.0;
    int totalPiles = 0;
    int freePiles = 0;
    MoneyCents priceCents = 0;
    int online = 0;
    int offline = 0;
    int onlineRate = 0;  // 0-100
};
struct AdminDevice {
    int id = 0;
    int stationId = 0;
    QString stationName;
    int type = 0;
    int state = 0;
    bool online = false;
    double powerKw = 0.0;
    double energyKwh = 0.0;
    qint64 lastTs = 0;
    qint64 sessions = 0;
    double chargeSec = 0.0;
};
struct OpLogRow {
    int id = 0;
    int deviceId = 0;
    QString opType;
    QString opBy;
    QString detail;
    QDateTime at;
};
struct AuditLogRow {
    int id = 0;
    QString username;
    QString action;
    QString detail;
    QString result;
    QDateTime at;
};

class AdminApi {
public:
    explicit AdminApi(QString baseUrl = QStringLiteral("http://127.0.0.1:8080"));

    using Cb = std::function<void(const Err&)>;
    using BoolCb = std::function<void(bool ok, const Err&)>;
    using IdCb = std::function<void(int id, const Err&)>;
    using LoginCb = std::function<void(const LoginInfo&)>;
    using UsersCb = std::function<void(const QVector<ncs::User>&, const Err&)>;
    using StationsCb =
        std::function<void(const QVector<ncs::Station>&, const Err&)>;
    using DevicesCb =
        std::function<void(const QVector<ncs::Device>&, const Err&)>;
    using StationRowsCb =
        std::function<void(const QVector<AdminStation>&, const Err&)>;
    using DeviceRowsCb =
        std::function<void(const QVector<AdminDevice>&, qint64 total, const Err&)>;
    using OpsCb =
        std::function<void(const QVector<OpLogRow>&, qint64 total, const Err&)>;
    using AuditCb =
        std::function<void(const QVector<AuditLogRow>&, qint64 total, const Err&)>;
    using OverviewCb = std::function<void(const QJsonObject&, const Err&)>;

    // 认证：成功后自动在后续请求带 Authorization
    void login(const QString& user, const QString& pass, LoginCb done);

    // B1 风控/资产
    void listUsers(const QString& phone, UsersCb done);
    void listUsers(const QString& phone, int statusFilter,
                   UsersCb done);  // 0 全部/1 正常/2 冻结
    void setFrozen(int userId, bool frozen, Cb done);
    void listStations(StationsCb done);  // 旧：公共站列表
    void listDevices(int stationId, DevicesCb done);  // 旧：单站基础列表
    void createStation(const QString& name, const QString& addr, double lat,
                       double lng, ncs::MoneyCents price, IdCb done);
    void deleteStation(int id, Cb done);
    void createDevices(int stationId, int count, int type, double powerKw,
                       Cb done);
    void deleteDevice(int id, Cb done);

    // B2 复杂筛选/实时监控/重启/日志/统计
    void listStationsOnline(const QString& q, StationRowsCb done);
    void listDevicesFiltered(const QString& q, int stationId, int type,
                             int state, int page, int pageSize,
                             DeviceRowsCb done);
    void restartDevice(int id, Cb done);
    void listOps(int page, int pageSize, OpsCb done);
    void listAuditLogs(int page, int pageSize, AuditCb done);
    void statsOverview(OverviewCb done);
    void statsDaily(int days, OverviewCb done);  // 返回 data 数组

private:
    HttpJsonClient cli_;
};

}  // namespace admin
}  // namespace ncs

#endif  // NCS_ADMIN_ADMINAPI_H
