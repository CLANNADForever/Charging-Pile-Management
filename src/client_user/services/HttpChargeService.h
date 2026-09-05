#ifndef NCS_CLIENT_SERVICES_HTTPCHARGESERVICE_H
#define NCS_CLIENT_SERVICES_HTTPCHARGESERVICE_H

#include <QString>

#include "HttpJsonClient.h"
#include "IChargeService.h"

namespace ncs {
namespace client {

class HttpChargeService : public IChargeService {
public:
    explicit HttpChargeService(
        QString baseUrl = QStringLiteral("http://127.0.0.1:8080"));

    void reserve(const QString& phone, int deviceId,
                 OrderCallback done) override;
    void start(int orderId, OrderCallback done) override;
    void finish(int orderId, OrderCallback done) override;
    void pay(int orderId, OrderCallback done) override;
    void cancel(int orderId, OrderCallback done) override;
    void live(int orderId, LiveCallback done) override;
    void listActive(const QString& phone, OrdersCallback done) override;
    void listHistory(const QString& phone, int limit, int offset,
                    HistoryCallback done) override;

private:
    void postOrder(const QString& path, const QJsonObject* body,
                   OrderCallback done);

    HttpJsonClient client_;
};

}  // namespace client
}  // namespace ncs

#endif  // NCS_CLIENT_SERVICES_HTTPCHARGESERVICE_H
