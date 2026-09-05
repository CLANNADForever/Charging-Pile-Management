// 最简硬件模拟器：独立进程，TCP 长连后端，周期心跳上报 JSON-lines。
// 用法: ncs_simulator [host] [port] [间隔秒] [起始桩号] [桩数]
// 默认: 127.0.0.1 18000 2 1 9
// 适配点：数据字段/周期/桩状态机后续演进，都在 main() 里集中改，协议不易变。
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <thread>

#include <nlohmann/json.hpp>

namespace {
int deviceStateSeed(int deviceId) {
    // 演示用确定性状态：id%3==0 -> 充电中(有电流)；否则空闲/待机(电流≈0)
    return deviceId % 3 == 0;
}
}  // namespace

int main(int argc, char* argv[]) {
    const std::string host = argc > 1 ? argv[1] : "127.0.0.1";
    const int port = argc > 2 ? std::atoi(argv[2]) : 18000;
    const int intervalSec = argc > 3 ? std::atoi(argv[3]) : 2;
    const int startId = argc > 4 ? std::atoi(argv[4]) : 1;
    const int count = argc > 5 ? std::atoi(argv[5]) : 9;

    std::printf("[sim] connect %s:%d devices %d..%d every %ds\n", host.c_str(),
                port, startId, startId + count - 1, intervalSec);

    for (;;) {
        const int fd = socket(AF_INET, SOCK_STREAM, 0);
        if (fd < 0) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            continue;
        }
        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(static_cast<uint16_t>(port));
        inet_pton(AF_INET, host.c_str(), &addr.sin_addr);
        if (connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
            close(fd);
            std::printf("[sim] 连接失败，%ds 后重试\n", intervalSec);
            std::this_thread::sleep_for(std::chrono::seconds(intervalSec));
            continue;
        }
        std::printf("[sim] connected\n");

        bool alive = true;
        while (alive) {
            for (int i = 0; i < count; ++i) {
                const int deviceId = startId + i;
                const bool charging = deviceStateSeed(deviceId);
                const double current = charging ? (6.0 + (deviceId % 7)) : 0.0;
                nlohmann::json hb{
                    {"type", "heartbeat"},
                    {"device_id", deviceId},
                    {"voltage", 220.0 + (deviceId % 5) * 1.3},
                    {"current", current},
                    {"temperature", 24.0 + (deviceId % 6) + (charging ? 15.0 : 0.0)},
                    {"power_kw", 220.0 * current / 1000.0},
                    {"energy_kwh", 0.0},
                    {"ts", std::chrono::duration_cast<std::chrono::milliseconds>(
                               std::chrono::system_clock::now().time_since_epoch())
                               .count()},
                };
                const std::string line = hb.dump() + "\n";
                const ssize_t n =
                    send(fd, line.data(), line.size(), MSG_NOSIGNAL);
                if (n < 0) {
                    alive = false;
                    break;
                }
            }
            if (alive)
                std::this_thread::sleep_for(std::chrono::seconds(intervalSec));
        }
        close(fd);
        std::printf("[sim] 连接断开，重连中\n");
    }
    return 0;
}
