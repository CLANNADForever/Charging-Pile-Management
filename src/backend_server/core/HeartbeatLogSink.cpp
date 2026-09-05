#include "HeartbeatLogSink.h"

#include <cstdio>

namespace ncs {
namespace backend {

void HeartbeatLogSink::onHeartbeat(const Heartbeat& hb) {
    count_.fetch_add(1);
    lastDevice_.store(hb.deviceId);
    std::printf("[sim] hb device=%d v=%.1f i=%.1f t=%.1f\n", hb.deviceId,
                hb.voltage, hb.current, hb.temperature);
    std::fflush(stdout);
}

}  // namespace backend
}  // namespace ncs
