#ifndef NCS_ADMIN_ADMINAPI_H
#define NCS_ADMIN_ADMINAPI_H

#include <functional>
#include <QString>
#include <QVector>

#include "HttpJsonClient.h"
#include "entities.h"

namespace ncs {
namespace admin {
using HttpJsonClient = ::ncs::client::HttpJsonClient;

// 管理端 API 薄封装(共享 HttpJsonClient；信封解析)。
class AdminApi {
public:
    explicit AdminApi(QString baseUrl = QStringLiteral("http://127.0.0.1:8080"));

    using Err = QString;               // 空 = 成功
    using Cb = std::function<void(const Err&)>;
    using BoolCb = std::function<void(bool ok, const Err&)>;
    using IdCb = std::function<void(int id, const Err&)>;
    using UsersCb = std::function<void(const QVector<ncs::User>&, const Err&)>;
    using StationsCb = std::function<void(const QVector<ncs::Station>&, const Err&)>;
    using DevicesCb = std::function<void(const QVector<ncs::Device>&, const Err&)>;

    void login(const QString& user, const QString& pass, BoolCb done);
    void listUsers(const QString& phone, UsersCb done);
    void setFrozen(int userId, bool frozen, Cb done);
    void listStations(StationsCb done);
    void listDevices(int stationId, DevicesCb done);
    void createStation(const QString& name, const QString& addr, double lat,
                       double lng, ncs::MoneyCents price, IdCb done);
    void deleteStation(int id, Cb done);
    void createDevices(int stationId, int count, int type, double powerKw,
                       Cb done);
    void deleteDevice(int id, Cb done);

private:
    HttpJsonClient cli_;
};

}  // namespace admin
}  // namespace ncs

#endif  // NCS_ADMIN_ADMINAPI_H
