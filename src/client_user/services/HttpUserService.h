// IUserService 的真实 HTTP 实现：Qt QNetworkAccessManager + JSON。
// 同步语义(阻塞等结果，带超时)，与现有纯虚接口保持一致；
// 后续可改为信号槽异步版本而不动上层 UI。
#ifndef NCS_CLIENT_SERVICES_HTTPUSERSERVICE_H
#define NCS_CLIENT_SERVICES_HTTPUSERSERVICE_H

#include <QJsonObject>
#include <QString>

#include "IUserService.h"

namespace ncs {
namespace client {

class HttpUserService : public IUserService {
public:
    explicit HttpUserService(
        QString baseUrl = QStringLiteral("http://127.0.0.1:8080"));

    LoginResult requestCode(const QString& phone) override;
    LoginResult login(const QString& phone, const QString& code) override;

    const QString& baseUrl() const { return baseUrl_; }

private:
    struct HttpOut {
        bool ok = false;      // 网络层/超时是否正常
        int status = 0;
        QJsonObject body;
        QString error;
    };
    HttpOut post(const QString& path, const QJsonObject& json) const;

    QString baseUrl_;
};

}  // namespace client
}  // namespace ncs

#endif  // NCS_CLIENT_SERVICES_HTTPUSERSERVICE_H
