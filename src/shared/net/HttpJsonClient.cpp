#include "HttpJsonClient.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>

namespace ncs {
namespace client {

HttpJsonClient::HttpJsonClient(QString baseUrl)
    : baseUrl_(std::move(baseUrl)) {}

void HttpJsonClient::send(const QByteArray& verb, const QString& path,
                          const QJsonObject* json, ReplyCallback done) {
    QNetworkRequest req(QUrl(baseUrl_ + path));
    req.setHeader(QNetworkRequest::ContentTypeHeader,
                  QStringLiteral("application/json"));

    QNetworkReply* reply = nullptr;
    const QByteArray body =
        json ? QJsonDocument(*json).toJson(QJsonDocument::Compact) : QByteArray();
    if (verb == "POST")
        reply = mgr_.post(req, body);
    else if (verb == "PATCH")
        reply = mgr_.sendCustomRequest(req, "PATCH", body);
    else if (verb == "DELETE")
        reply = mgr_.deleteResource(req);
    else
        reply = mgr_.get(req);

    QObject::connect(reply, &QNetworkReply::finished,
                     [reply, done = std::move(done)] {
                         Reply out;
                         if (reply->error() != QNetworkReply::NoError) {
                             out.transportOk = false;
                             out.error = reply->errorString();
                         } else {
                             out.transportOk = true;
                             out.status = reply->attribute(
                                 QNetworkRequest::HttpStatusCodeAttribute).toInt();
                             const QByteArray data = reply->readAll();
                             QJsonParseError err;
                             const QJsonDocument doc =
                                 QJsonDocument::fromJson(data, &err);
                             if (err.error == QJsonParseError::NoError &&
                                 doc.isObject()) {
                                 const QJsonObject o = doc.object();
                                 out.code = o.value(QStringLiteral("code")).toInt(-1);
                                 out.message =
                                     o.value(QStringLiteral("message")).toString();
                                 out.data = o.value(QStringLiteral("data"));
                             } else {
                                 out.code = -1;
                                 out.error = QStringLiteral("响应非 JSON");
                             }
                         }
                         reply->deleteLater();
                         done(out);
                     });
}

void HttpJsonClient::get(const QString& path, ReplyCallback done) {
    send("GET", path, nullptr, std::move(done));
}

void HttpJsonClient::patch(const QString& path, const QJsonObject& json,
                           ReplyCallback done) {
    send("PATCH", path, &json, std::move(done));
}

void HttpJsonClient::sendDelete(const QString& path, ReplyCallback done) {
    send("DELETE", path, nullptr, std::move(done));
}

void HttpJsonClient::post(const QString& path, const QJsonObject& json,
                          ReplyCallback done) {
    send("POST", path, &json, std::move(done));
}

}  // namespace client
}  // namespace ncs
