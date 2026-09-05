// 站/桩查询服务接口：找桩页只依赖本接口(先 HTTP 真后端；UI 测试可用假实现)。
#ifndef NCS_CLIENT_SERVICES_ISTATIONSERVICE_H
#define NCS_CLIENT_SERVICES_ISTATIONSERVICE_H

#include <QVector>

#include "entities.h"

namespace ncs {
namespace client {

class IStationService {
public:
    virtual ~IStationService() = default;
    virtual QVector<ncs::Station> listStations() = 0;              // 空=失败/无
    virtual QVector<ncs::Device> listDevices(int stationId) = 0;
};

}  // namespace client
}  // namespace ncs

#endif  // NCS_CLIENT_SERVICES_ISTATIONSERVICE_H
