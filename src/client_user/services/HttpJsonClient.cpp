#include "HttpJsonClient.h"

#include <QEventLoop>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>
#include <QUrl>

namespace ncs {
namespace client {

HttpJsonClient::HttpJsonClient(QString baseUrl)
    : baseUrl_(std::move(baseUrl)) {}

HttpJsonClient::Reply HttpJsonClient::send(const QByteArray& verb,
                                           const QString& path,
                                           const QJsonObject* json) const {
    Reply out;
    QNetworkAccessManager mgr;
    QNetworkRequest req(QUrl(baseUrl_ + path));
    req.setHeader(QNetworkRequest::ContentTypeHeader,
                  QStringLiteral("application/json"));

    QNetworkReply* reply = nullptr;
    if (verb == "POST" && json) {
        reply = mgr.post(req, QJsonDocument(*json).toJson(QJsonDocument::Compact));
    } else {
        reply = mgr.get(req);
    }

    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    timer.start(4000);
    loop.exec();

    if (reply->error() != QNetworkReply::NoError) {
        out.error = reply->errorString();
        reply->deleteLater();
        return out;
    }
    out.ok = true;
    out.status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QByteArray data = reply->readAll();
    if (!data.isEmpty()) {
        QJsonParseError err;
        const QJsonDocument doc = QJsonDocument::fromJson(data, &err);
        if (err.error == QJsonParseError::NoError)
            out.root = doc.isArray()
                           ? QJsonValue(doc.array())
                           : QJsonValue(doc.object());
    }
    reply->deleteLater();
    return out;
}

HttpJsonClient::Reply HttpJsonClient::get(const QString& path) const {
    return send("GET", path, nullptr);
}

HttpJsonClient::Reply HttpJsonClient::post(const QString& path,
                                           const QJsonObject& json) const {
    return send("POST", path, &json);
}

}  // namespace client
}  // namespace ncs
