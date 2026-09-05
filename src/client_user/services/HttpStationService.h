#ifndef NCS_CLIENT_SERVICES_HTTPSTATIONSERVICE_H
#define NCS_CLIENT_SERVICES_HTTPSTATIONSERVICE_H

#include <QString>

#include "HttpJsonClient.h"
#include "IStationService.h"

namespace ncs {
namespace client {

class HttpStationService : public IStationService {
public:
    explicit HttpStationService(
        QString baseUrl = QStringLiteral("http://127.0.0.1:8080"));

    void listStations(StationListCallback done) override;
    void listDevices(int stationId, DeviceListCallback done) override;

private:
    HttpJsonClient client_;
};

}  // namespace client
}  // namespace ncs

#endif  // NCS_CLIENT_SERVICES_HTTPSTATIONSERVICE_H
