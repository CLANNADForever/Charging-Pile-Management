// 模拟器 v3：注册到后端并听命令。桩业务状态(预约/开始/停止)服从后端；
// 支持"自主故障上报"：autoFaultSec>0 时周期性把第一台桩置故障(故障态心跳上报
// sim_state=2)，后端可下发 restart 远程重启(此时上报 4 重启中)，随后自愈为正常。
// 用法: ncs_sim [host] [port] [intervalSec] [startId] [count] [autoFaultSec]
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

// 功率口径(R3)：id%3==0→超充180 / ==1→快充120 / ==2→慢充7；与 seed/DB 三档一致
double devicePowerKw(int deviceId) {
    const int r = deviceId % 3;
    return r == 0 ? 180.0 : (r == 1 ? 120.0 : 7.0);
}

long long nowMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

}  // namespace

int main(int argc, char* argv[]) {
    const std::string host = argc > 1 ? argv[1] : "127.0.0.1";
    const int port = argc > 2 ? std::atoi(argv[2]) : 18000;
    const int intervalSec = argc > 3 ? std::atoi(argv[3]) : 2;
    const int startId = argc > 4 ? std::atoi(argv[4]) : 1;
    const int count = argc > 5 ? std::atoi(argv[5]) : 9;
    const int autoFaultSec = argc > 6 ? std::atoi(argv[6]) : 0;  // 0=不自动故障

    std::vector<bool> charging(count, false);    // 是否充电中
    std::vector<bool> faulted(count, false);     // 自主故障中
    std::vector<long long> faultFrom(count, 0);  // 上次解除故障时刻(ms)
    std::vector<long long> rebootingUntil(count, 0);  // restart 后恢复时刻(ms)
    std::vector<double> energy(count, 0.0);      // 本会话累计电量 kWh

    std::printf("[sim] connect %s:%d devices %d..%d every %ds autoFault=%d\n",
                host.c_str(), port, startId, startId + count - 1,
                intervalSec, autoFaultSec);

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
                            // 故障中被要求开始：视为自愈后进入充电
                            if (faulted[idx]) {
                                faulted[idx] = false;
                                rebootingUntil[idx] = 0;
                                faultFrom[idx] = nowMs();
                            }
                            charging[idx] = true;
                            std::printf("[sim] dev %d -> charging\n", deviceId);
                        } else if (cmd == "stop") {
                            charging[idx] = false;
                            energy[idx] = 0.0;  // 会话结束清零(电量已上报累计)
                            std::printf("[sim] dev %d -> stop\n", deviceId);
                        } else if (cmd == "restart") {
                            // 仅故障桩响应重启；重启耗时约 3s
                            if (faulted[idx] && nowMs() >= rebootingUntil[idx]) {
                                rebootingUntil[idx] = nowMs() + 3000;
                                std::printf("[sim] dev %d -> restarting(3s)\n",
                                            deviceId);
                            }
                        }
                    } catch (...) {
                    }
                }
            }

            const long long now = nowMs();
            // 自动故障(演示远程重启闭环)：周期性地把第一台桩置故障
            const int faultTarget = startId;
            if (autoFaultSec > 0 && count > 0) {
                const int fi = faultTarget - startId;
                if (!faulted[fi] && !charging[fi] &&
                    now - faultFrom[fi] >= autoFaultSec * 1000LL) {
                    faulted[fi] = true;
                    rebootingUntil[fi] = 0;
                    faultFrom[fi] = now;
                    std::printf("[sim] dev %d -> fault(auto)\n", faultTarget);
                }
            }

            // 周期心跳
            for (int i = 0; i < count; ++i) {
                const int deviceId = startId + i;
                if (faulted[i]) {
                    if (rebootingUntil[i] > 0 && now < rebootingUntil[i]) {
                        // 重启中
                    } else if (rebootingUntil[i] > 0 && now >= rebootingUntil[i]) {
                        faulted[i] = false;
                        rebootingUntil[i] = 0;
                        faultFrom[i] = now;
                        std::printf("[sim] dev %d recovered after reboot\n",
                                    deviceId);
                    }
                }
                const bool on = charging[i] && !faulted[i];
                if (on)
                    energy[i] += devicePowerKw(deviceId) * (intervalSec / 3600.0);
                int simState = faulted[i] ? (rebootingUntil[i] > now ? 4 : 2)
                                          : (charging[i] ? 1 : 0);
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
                    {"sim_state", simState},
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
