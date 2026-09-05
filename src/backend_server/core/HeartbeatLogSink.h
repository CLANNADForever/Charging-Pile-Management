#ifndef NCS_BACKEND_CORE_HEARTBEATLOGSINK_H
#define NCS_BACKEND_CORE_HEARTBEATLOGSINK_H

#include <atomic>

#include "HeartbeatSink.h"

namespace ncs {
namespace backend {

// 默认处理：计数 + 打日志。后续异常检测/入库替换此实现。
class HeartbeatLogSink : public IHeartbeatSink {
public:
    void onHeartbeat(const Heartbeat& hb) override;
    long long count() const { return count_.load(); }
    int lastDeviceId() const { return lastDevice_.load(); }

private:
    std::atomic<long long> count_{0};
    std::atomic<int> lastDevice_{-1};
};

}  // namespace backend
}  // namespace ncs

#endif  // NCS_BACKEND_CORE_HEARTBEATLOGSINK_H
