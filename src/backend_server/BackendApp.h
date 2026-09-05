#ifndef NCS_BACKEND_BACKENDAPP_H
#define NCS_BACKEND_BACKENDAPP_H

#include <atomic>
#include <map>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#include <QString>
#include <httplib.h>

#include "core/AuthService.h"
#include "core/HeartbeatLogSink.h"
#include "database/Store.h"

namespace ncs {
namespace backend {

class ChargeService;

// 装配层：HTTP(REST，含充电闭环) + 模拟器 TCP(JSON-lines 心跳 + 命令下发)。
class BackendApp {
public:
    explicit BackendApp(const QString& dbPath);
    ~BackendApp();

    bool init();
    const QString& lastError() const { return error_; }

    httplib::Server& server() { return srv_; }
    AuthService& auth() { return auth_; }

    bool startSimListener(int port);
    void stopSimListener();
    bool startReserveSweeper(int timeoutSec);  // 周期释放超时预约
    void stopReserveSweeper();
    long long simHeartbeatCount() const { return sink_.count(); }
    int simLastDeviceId() const { return sink_.lastDeviceId(); }

    // 模拟器命令/遥测(内部供 ChargeService 回调)
    void registerSimDevices(int fd, const std::vector<int>& ids);
    void unregisterSimFd(int fd);
    bool sendSimCommand(int deviceId, bool start);
    double simEnergy(int deviceId) const;

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
    std::unique_ptr<ChargeService> charge_;
    httplib::Server srv_;

    int simListenFd_ = -1;
    std::atomic<bool> simRunning_{false};
    std::thread simThread_;
    std::atomic<bool> sweepRunning_{false};
    std::thread sweepThread_;

    mutable std::mutex simMu_;       // 保护 deviceFd_ / deviceEnergy_
    std::map<int, int> deviceFd_;    // deviceId -> 连接 fd(注册的模拟器)
    std::map<int, double> deviceEnergy_;  // deviceId -> 最近心跳 energy_kwh
};

}  // namespace backend
}  // namespace ncs

#endif  // NCS_BACKEND_BACKENDAPP_H
