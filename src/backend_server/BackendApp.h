#ifndef NCS_BACKEND_BACKENDAPP_H
#define NCS_BACKEND_BACKENDAPP_H

#include <atomic>
#include <thread>

#include <QString>
#include <httplib.h>

#include "core/AuthService.h"
#include "core/HeartbeatLogSink.h"
#include "database/Store.h"

namespace ncs {
namespace backend {

// 装配层：HTTP(REST) + 模拟器 TCP(JSON-lines 心跳)。不含任何 GUI；
// 实体沿用 Qt 类型(Qt Core)，DB 用 SQLite C API。
class BackendApp {
public:
    explicit BackendApp(const QString& dbPath);
    ~BackendApp();

    bool init();                       // 打开数据库并注册 HTTP 路由
    const QString& lastError() const { return error_; }

    httplib::Server& server() { return srv_; }
    AuthService& auth() { return auth_; }

    // 模拟器 TCP 监听(独立线程)；协议见 HeartbeatSink.h
    bool startSimListener(int port);
    void stopSimListener();
    long long simHeartbeatCount() const { return sink_.count(); }
    int simLastDeviceId() const { return sink_.lastDeviceId(); }

    static constexpr const char* kService = "ncs-backend";
    static constexpr const char* kVersion = "0.1.0";

private:
    void registerRoutes();
    void simAcceptLoop();
    void handleSimConnection(int fd);

    QString dbPath_;
    QString error_;
    Store store_;
    AuthService auth_;
    HeartbeatLogSink sink_;
    httplib::Server srv_;

    int simListenFd_ = -1;
    std::atomic<bool> simRunning_{false};
    std::thread simThread_;
};

}  // namespace backend
}  // namespace ncs

#endif  // NCS_BACKEND_BACKENDAPP_H
