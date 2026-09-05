#ifndef NCS_BACKEND_BACKENDAPP_H
#define NCS_BACKEND_BACKENDAPP_H

#include <QString>
#include <httplib.h>

#include "core/AuthService.h"
#include "database/Store.h"

namespace ncs {
namespace backend {

// HTTP 装配层：把 Store + AuthService 暴露为 REST。
// 本服务不包含任何 GUI；仅依赖 Qt Core/Sql(实体与持久层用了 Qt 类型)。
class BackendApp {
public:
    explicit BackendApp(const QString& dbPath);
    ~BackendApp();

    // 打开数据库并注册路由；失败返回 false(细节见 lastError())
    bool init();
    const QString& lastError() const { return error_; }

    httplib::Server& server() { return srv_; }
    AuthService& auth() { return auth_; }

    static constexpr const char* kService = "ncs-backend";
    static constexpr const char* kVersion = "0.1.0";

private:
    void registerRoutes();

    QString dbPath_;
    QString error_;
    Store store_;
    AuthService auth_;
    httplib::Server srv_;
};

}  // namespace backend
}  // namespace ncs

#endif  // NCS_BACKEND_BACKENDAPP_H
