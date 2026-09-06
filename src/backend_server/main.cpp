// NCS 后端入口。用法: ncs_server [db路径] [http端口] [模拟器TCP端口]
// 默认 db=./ncs-backend.db http=8080 simTCP=18000
#include <cstdio>
#include <cstdlib>

#include <QString>

#include "BackendApp.h"

int main(int argc, char* argv[]) {
    const QString db = argc > 1 ? QString::fromLocal8Bit(argv[1])
                                : QStringLiteral("ncs-backend.db");
    int httpPort = 8080;
    int simPort = 18000;
    int reserveTimeoutSec = 900;  // 预约超时释放(秒)，R6 默认 15 分钟
    int rebootSec = 5;            // 远程重启自动恢复超时(秒)
    if (argc > 2)
        httpPort = std::atoi(argv[2]);
    if (argc > 3)
        simPort = std::atoi(argv[3]);
    if (argc > 4)
        reserveTimeoutSec = std::atoi(argv[4]);
    if (argc > 5)
        rebootSec = std::atoi(argv[5]);

    ncs::backend::BackendApp app(db);
    if (!app.init()) {
        std::fprintf(stderr, "[ERR] %s\n", qPrintable(app.lastError()));
        return 1;
    }
    app.startReserveSweeper(reserveTimeoutSec);  // start sweeper: auto-release expired reservations
    app.startRestartSweeper(rebootSec);          // 重启中桩超时未上报则强制恢复
    if (!app.startSimListener(simPort)) {
        std::fprintf(stderr, "[WARN] 模拟器 TCP 监听失败 port=%d(继续 HTTP)\n",
                     simPort);
    }
    std::printf("[NCS backend] db=%s http=%d simTCP=%d\n", qPrintable(db),
                httpPort, simPort);
    std::fflush(stdout);

    if (!app.server().listen("0.0.0.0", httpPort)) {
        std::fprintf(stderr, "[ERR] HTTP 监听失败 port=%d\n", httpPort);
        return 1;
    }
    return 0;
}
