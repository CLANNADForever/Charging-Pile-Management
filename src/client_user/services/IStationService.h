// 站/桩查询服务接口(异步)。error 非空表示失败。
#ifndef NCS_CLIENT_SERVICES_ISTATIONSERVICE_H
#define NCS_CLIENT_SERVICES_ISTATIONSERVICE_H

#include <functional>

#include <QString>
#include <QVector>

#include "entities.h"

namespace ncs {
namespace client {

using StationListCallback =
    std::function<void(const QVector<ncs::Station>&, const QString& error)>;
using DeviceListCallback =
    std::function<void(const QVector<ncs::Device>&, const QString& error)>;

class IStationService {
public:
    virtual ~IStationService() = default;
    virtual void listStations(StationListCallback done) = 0;
    virtual void listDevices(int stationId, DeviceListCallback done) = 0;
};

}  // namespace client
}  // namespace ncs

#endif  // NCS_CLIENT_SERVICES_ISTATIONSERVICE_H
