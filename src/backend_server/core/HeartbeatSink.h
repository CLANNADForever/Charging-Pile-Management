// 模拟器上报的数据抽象与处理接口。
// 适配点：后续(异常检测/负荷预测)只换/加 IHeartbeatSink 实现即可，
// 网络层与协议保持不变。
#ifndef NCS_BACKEND_CORE_HEARTBEATSINK_H
#define NCS_BACKEND_CORE_HEARTBEATSINK_H

namespace ncs {
namespace backend {

// 单条遥测心跳(JSON-lines: {"type":"heartbeat","device_id":N,...})
struct Heartbeat {
    int deviceId = 0;
    double voltage = 0.0;
    double current = 0.0;
    double temperature = 0.0;
    double powerKw = 0.0;
    double energyKwh = 0.0;
    long long tsMs = 0;
    int simState = -1;   // 0 正常/1 充电 2 故障 4 重启中(-1 无)
};

class IHeartbeatSink {
public:
    virtual ~IHeartbeatSink() = default;
    virtual void onHeartbeat(const Heartbeat& hb) = 0;
};

}  // namespace backend
}  // namespace ncs

#endif  // NCS_BACKEND_CORE_HEARTBEATSINK_H
