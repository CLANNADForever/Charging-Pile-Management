// 模拟器 v2：注册到后端并听命令，桩状态机服从后端(占用/开始/停止)。
// 收到 start 后该桩开始累积电量，随心跳上报；收到 stop 停止并清零本会话电量。
#include <arpa/inet.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <thread>
#include <vector>

#include <nlohmann/json.hpp>

namespace {

double devicePowerKw(int deviceId) {
    return (deviceId % 2 == 0) ? 120.0 : 7.0;  // 快/慢桩演示分布
}

}  // namespace

int main(int argc, char* argv[]) {
    const std::string host = argc > 1 ? argv[1] : "127.0.0.1";
    const int port = argc > 2 ? std::atoi(argv[2]) : 18000;
    const int intervalSec = argc > 3 ? std::atoi(argv[3]) : 2;
    const int startId = argc > 4 ? std::atoi(argv[4]) : 1;
    const int count = argc > 5 ? std::atoi(argv[5]) : 9;

    std::vector<bool> charging(count, false);   // 每桩是否充电中
    std::vector<double> energy(count, 0.0);     // 本会话累计电量 kWh

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
            std::printf("[sim] connect failed, retry\n");
            std::this_thread::sleep_for(std::chrono::seconds(intervalSec));
            continue;
        }

        // 注册本进程负责的桩
        nlohmann::json reg{{"type", "register"}};
        for (int i = 0; i < count; ++i)
            reg["devices"].push_back(startId + i);
        const std::string regLine = reg.dump() + "\n";
        send(fd, regLine.data(), regLine.size(), MSG_NOSIGNAL);
        std::printf("[sim] registered %d devices\n", count);

        std::string inbuf;
        bool alive = true;
        while (alive) {
            // 非阻塞等读(有命令就处理，无则等一个周期)
            pollfd pfd{fd, POLLIN, 0};
            const int pr = poll(&pfd, 1, intervalSec * 1000);
            if (pr > 0 && (pfd.revents & POLLIN)) {
                char chunk[512];
                const ssize_t n = recv(fd, chunk, sizeof(chunk), 0);
                if (n <= 0) {
                    alive = false;
                    break;
                }
                inbuf.append(chunk, static_cast<size_t>(n));
                std::size_t pos;
                while ((pos = inbuf.find('\n')) != std::string::npos) {
                    const std::string line = inbuf.substr(0, pos);
                    inbuf.erase(0, pos + 1);
                    try {
                        const auto j = nlohmann::json::parse(line);
                        const std::string cmd = j.value("cmd", "");
                        const int deviceId = j.value("device_id", -1);
                        const int idx = deviceId - startId;
                        if (idx < 0 || idx >= count)
                            continue;
                        if (cmd == "start") {
                            charging[idx] = true;
                            std::printf("[sim] dev %d -> charging\n", deviceId);
                        } else if (cmd == "stop") {
                            charging[idx] = false;
                            energy[idx] = 0.0;  // 会话结束清零(电量已上报累计)
                            std::printf("[sim] dev %d -> stop\n", deviceId);
                        }
                    } catch (...) {
                    }
                }
            }

            // 周期心跳
            for (int i = 0; i < count; ++i) {
                const int deviceId = startId + i;
                if (charging[i])
                    energy[i] += devicePowerKw(deviceId) * (intervalSec / 3600.0);
                const bool on = charging[i];
                const double current = on ? devicePowerKw(deviceId) * 1000.0 / 220.0 : 0.0;
                nlohmann::json hb{
                    {"type", "heartbeat"},
                    {"device_id", deviceId},
                    {"voltage", 220.0 + (deviceId % 5) * 1.3},
                    {"current", current},
                    {"temperature", 24.0 + (deviceId % 6) + (on ? 15.0 : 0.0)},
                    {"power_kw", on ? devicePowerKw(deviceId) : 0.0},
                    {"energy_kwh", energy[i]},
                    {"charging", on},
                    {"ts", std::chrono::duration_cast<std::chrono::milliseconds>(
                               std::chrono::system_clock::now().time_since_epoch())
                               .count()},
                };
                const std::string line = hb.dump() + "\n";
                if (send(fd, line.data(), line.size(), MSG_NOSIGNAL) < 0) {
                    alive = false;
                    break;
                }
            }
        }
        close(fd);
        std::printf("[sim] disconnected, reconnect\n");
    }
    return 0;
}
