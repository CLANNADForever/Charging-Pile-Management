#ifndef NCS_CLIENT_SERVICES_HTTPUSERSERVICE_H
#define NCS_CLIENT_SERVICES_HTTPUSERSERVICE_H

#include <QString>

#include "HttpJsonClient.h"
#include "IUserService.h"

namespace ncs {
namespace client {

// IUserService 的 HTTP 实现(同步，基于 HttpJsonClient)。
class HttpUserService : public IUserService {
public:
    explicit HttpUserService(
        QString baseUrl = QStringLiteral("http://127.0.0.1:8080"));

    LoginResult requestCode(const QString& phone) override;
    LoginResult login(const QString& phone, const QString& code) override;

    const QString& baseUrl() const { return client_.baseUrl(); }

private:
    HttpJsonClient client_;
};

}  // namespace client
}  // namespace ncs

#endif  // NCS_CLIENT_SERVICES_HTTPUSERSERVICE_H
