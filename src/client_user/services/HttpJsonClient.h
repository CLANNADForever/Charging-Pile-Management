// 极简同步 HTTP JSON 客户端(Qt QNetworkAccessManager，阻塞+超时)。
// User/Station/Order 等服务共用，避免重复实现。
#ifndef NCS_CLIENT_SERVICES_HTTPJSONCLIENT_H
#define NCS_CLIENT_SERVICES_HTTPJSONCLIENT_H

#include <QJsonValue>
#include <QString>

class QJsonObject;

namespace ncs {
namespace client {

class HttpJsonClient {
public:
    explicit HttpJsonClient(QString baseUrl);

    struct Reply {
        bool ok = false;   // 网络层/超时是否正常
        int status = 0;
        QJsonValue root;   // 响应 JSON(对象或数组)
        QString error;
    };

    Reply get(const QString& path) const;
    Reply post(const QString& path, const QJsonObject& json) const;

    const QString& baseUrl() const { return baseUrl_; }

private:
    Reply send(const QByteArray& verb, const QString& path,
               const QJsonObject* json) const;

    QString baseUrl_;
};

}  // namespace client
}  // namespace ncs

#endif  // NCS_CLIENT_SERVICES_HTTPJSONCLIENT_H
