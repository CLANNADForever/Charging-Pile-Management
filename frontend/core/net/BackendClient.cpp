#include "BackendClient.h"

#include <QByteArray>
#include <QEventLoop>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>
#include <QUrl>

namespace ncsfe {

QString BackendClient::s_token;

QString BackendClient::baseUrl()
{
    const QByteArray env = qgetenv("NCS_BACKEND_URL");
    return env.isEmpty() ? QStringLiteral("http://127.0.0.1:8080")
                         : QString::fromUtf8(env);
}

BackendClient::Reply BackendClient::request(const QByteArray &verb,
                                            const QString &path,
                                            const QJsonObject *body)
{
    Reply out;
    QNetworkAccessManager mgr;
    QNetworkRequest req(QUrl(baseUrl() + path));
    req.setHeader(QNetworkRequest::ContentTypeHeader,
                  QStringLiteral("application/json"));
    if (!s_token.isEmpty())
        req.setRawHeader("Authorization", "Bearer " + s_token.toUtf8());

    const QByteArray payload =
        body ? QJsonDocument(*body).toJson(QJsonDocument::Compact) : QByteArray();
    QNetworkReply *reply = nullptr;
    if (verb == "POST")
        reply = mgr.post(req, payload);
    else
        reply = mgr.get(req);

    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);
    QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    timer.start(4000);
    loop.exec();
    timer.stop();

    if (reply->error() != QNetworkReply::NoError) {
        out.message = reply->errorString();
    } else {
        const QByteArray data = reply->readAll();
        QJsonParseError perr;
        const QJsonDocument doc = QJsonDocument::fromJson(data, &perr);
        if (perr.error == QJsonParseError::NoError && doc.isObject()) {
            const QJsonObject o = doc.object();
            out.code = o.value(QStringLiteral("code")).toInt(-1);
            out.message = o.value(QStringLiteral("message")).toString();
            out.data = o.value(QStringLiteral("data"));
            out.ok = (out.code == 0);
        } else {
            out.message = QStringLiteral("后端响应非 JSON");
        }
    }
    reply->deleteLater();
    return out;
}

BackendClient::Reply BackendClient::get(const QString &path)
{
    return request("GET", path, nullptr);
}

BackendClient::Reply BackendClient::post(const QString &path,
                                         const QJsonObject &body)
{
    return request("POST", path, &body);
}

}  // namespace ncsfe
