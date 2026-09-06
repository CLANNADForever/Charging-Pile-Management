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

// 模拟器实时状态快照(在线/功率/电量/最近心跳)，供管理端监控/在线率使用
struct SimLive {
    int deviceId = 0;
    bool online = false;
    double powerKw = 0.0;
    double energyKwh = 0.0;
    long long lastTsMs = 0;
};

class ChargeService;

// 装配层：HTTP(REST，含充电闭环与 B2 管理端 RBAC/监控/重启/审计) + 模拟器 TCP。
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
    double simPowerKw(int deviceId) const;  // R8 live 实时功率(-1 无上报)

    // B2：实时注册表 / 会话 / 重启
    bool sendSimRestart(int deviceId);
    void forgetDevice(int deviceId);  // 设备删除后清理实时注册表
    QVector<SimLive> simLiveSnapshot() const;

    QString issueAdminToken(const QString& username, const QString& role);
    bool adminSession(const QString& token, QString* username,
                      QString* role) const;
    // 远程重启：Fault →(下发 restart + 置 Rebooting)→ 心跳/超时自动回 Idle
    bool adminRestartDevice(int deviceId, const QString& opBy, QString* err);
    bool startRestartSweeper(int rebootSec);
    void stopRestartSweeper();

    static constexpr const char* kService = "ncs-backend";
    static constexpr const char* kVersion = "0.1.0";

private:
    void registerRoutes();
    void simAcceptLoop();
    void handleSimConnection(int fd);
    // 校验 Authorization: Bearer <token>；失败已写 401 响应并返回 false
    bool requireAdmin(const httplib::Request& req, httplib::Response& res,
                      QString* username, QString* role);
    void applySimState(int deviceId, int simState);  // 心跳驱动的故障/恢复流转
    void restartLoop(int rebootSec);

    struct Session {
        QString username;
        QString role;
    };

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
    int reserveTimeoutSec_ = 900;  // R7 预约截止(=超时配置)，默认 15 分钟

    mutable std::mutex simMu_;  // 保护以下 device* 实时注册表
    std::map<int, int> deviceFd_;          // deviceId -> 连接 fd(注册的模拟器)
    std::map<int, double> deviceEnergy_;   // deviceId -> 最近心跳 energy_kwh
    std::map<int, double> devicePower_;    // deviceId -> 最近心跳 power_kw
    std::map<int, long long> deviceLastTs_;  // deviceId -> 最近心跳 epoch ms

    // 管理端会话 token
    mutable std::mutex sessionMu_;
    std::map<QString, Session> sessions_;

    // 远程重启超时自恢复
    std::atomic<bool> restartRunning_{false};
    std::thread restartThread_;
    mutable std::mutex rebootingMu_;
    std::map<int, long long> rebootingSince_;  // deviceId -> 开始重启 epoch ms
};

}  // namespace backend
}  // namespace ncs

#endif  // NCS_BACKEND_BACKENDAPP_H
