// NCS 后端入口。用法: ncs_server [db路径] [端口]
// 默认 db = ./ncs-backend.db，端口 = 8080。
#include <cstdio>
#include <cstdlib>

#include <QString>

#include "BackendApp.h"

int main(int argc, char* argv[]) {
    const QString db =
        argc > 1 ? QString::fromLocal8Bit(argv[1])
                 : QStringLiteral("ncs-backend.db");
    int port = 8080;
    if (argc > 2)
        port = std::atoi(argv[2]);

    ncs::backend::BackendApp app(db);
    if (!app.init()) {
        std::fprintf(stderr, "[ERR] %s\n", qPrintable(app.lastError()));
        return 1;
    }
    std::printf("[NCS backend] db=%s port=%d (health: GET /health)\n",
                qPrintable(db), port);
    std::fflush(stdout);

    if (!app.server().listen("0.0.0.0", port)) {
        std::fprintf(stderr, "[ERR] 监听失败 port=%d\n", port);
        return 1;
    }
    return 0;
}
