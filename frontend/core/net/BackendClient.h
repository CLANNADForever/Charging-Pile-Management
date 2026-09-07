#pragma once

#include <QJsonObject>
#include <QJsonValue>
#include <QString>

// 小型同步 HTTP 客户端(新前端 core 用)。UI 按同步拿结果 → 这里阻塞到响应/超时。
// 信封 {code,message,data}:code=0 成功。base url 可用环境变量 NCS_BACKEND_URL 覆盖。
namespace ncsfe {

class BackendClient
{
public:
    struct Reply {
        bool ok = false;      // 传输层/解析成功
        int code = -1;        // 信封 code(-1=未解析)
        QString message;
        QJsonValue data;
    };

    static QString baseUrl();
    static void setToken(const QString &t) { s_token = t; }
    static QString token() { return s_token; }

    static Reply get(const QString &path);
    static Reply post(const QString &path, const QJsonObject &body);
    static Reply patch(const QString &path, const QJsonObject &body);  // 后端 PATCH
    static Reply sendDelete(const QString &path);                     // 后端 DELETE

    // 原始 body 上传(头像 PNG 等)；err 空=成功
    static bool postRaw(const QString &path, const QByteArray &body,
                        const QByteArray &contentType, QString *err);
    // 原始字节下载；失败返回空并写 err
    static QByteArray getBytes(const QString &path, QString *err);

private:
    static Reply request(const QByteArray &verb, const QString &path,
                         const QJsonObject *body);
    static QString s_token;
};

}  // namespace ncsfe
