// 异步 HTTP JSON 客户端(Qt QNetworkAccessManager)。请求由事件循环驱动，
// 结果经回调投递到创建线程 —— 不阻塞调用方(充电页实时刷新依赖此设计)。
#ifndef NCS_CLIENT_SERVICES_HTTPJSONCLIENT_H
#define NCS_CLIENT_SERVICES_HTTPJSONCLIENT_H

#include <functional>

#include <QJsonValue>
#include <QNetworkAccessManager>
#include <QString>

class QJsonObject;

namespace ncs {
namespace client {

class HttpJsonClient {
public:
    explicit HttpJsonClient(QString baseUrl);

    struct Reply {
        bool transportOk = false;  // 网络层是否成功
        int status = 0;
        int code = -1;             // 统一信封 code
        QString message;
        QJsonValue data;           // 信封 data
        QString error;             // 网络错误描述
    };
    using ReplyCallback = std::function<void(const Reply&)>;

    void get(const QString& path, ReplyCallback done);
    void post(const QString& path, const QJsonObject& json,
              ReplyCallback done);
    void patch(const QString& path, const QJsonObject& json,
               ReplyCallback done);
    void sendDelete(const QString& path, ReplyCallback done);

    const QString& baseUrl() const { return baseUrl_; }

private:
    void send(const QByteArray& verb, const QString& path,
              const QJsonObject* json, ReplyCallback done);

    QString baseUrl_;
    QNetworkAccessManager mgr_;
};

}  // namespace client
}  // namespace ncs

#endif  // NCS_CLIENT_SERVICES_HTTPJSONCLIENT_H
