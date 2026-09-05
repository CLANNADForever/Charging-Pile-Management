#ifndef NCS_CLIENT_SERVICES_HTTPUSERSERVICE_H
#define NCS_CLIENT_SERVICES_HTTPUSERSERVICE_H

#include <QString>

#include "HttpJsonClient.h"
#include "IUserService.h"

namespace ncs {
namespace client {

// IUserService 的 HTTP 实现(异步，基于 HttpJsonClient，统一信封解析)。
class HttpUserService : public IUserService {
public:
    explicit HttpUserService(
        QString baseUrl = QStringLiteral("http://127.0.0.1:8080"));

    void requestCode(const QString& phone, LoginCallback done) override;
    void login(const QString& phone, const QString& code,
               LoginCallback done) override;

    const QString& baseUrl() const { return client_.baseUrl(); }

private:
    HttpJsonClient client_;
};

}  // namespace client
}  // namespace ncs

#endif  // NCS_CLIENT_SERVICES_HTTPUSERSERVICE_H
