#include <QApplication>
#include <QWebEngineProfile>
#include <QWebEngineSettings>

#include "theme/Theme.h"
#include "ui/LoginWindow.h"
#include "ui/MainWindow.h"

int main(int argc, char *argv[])
{
    // 在 WebEngine 初始化前设置 Chromium flags:
    // --ignore-gpu-blocklist 让 VM/软件渲染环境下 WebGL 可用,否则腾讯地图白屏。
    // 用 qputenv 写进代码,保证无论从 Qt Creator 还是命令行启动都生效。
    qputenv("QTWEBENGINE_CHROMIUM_FLAGS", "--ignore-gpu-blocklist");

    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("NCS"));
    app.setApplicationDisplayName(QStringLiteral("NCS · 充电"));

    // 允许本地页面(qrc/file)访问远程 URL(腾讯地图脚本),否则会被安全策略拦截
    QWebEngineProfile::defaultProfile()->settings()->setAttribute(
        QWebEngineSettings::LocalContentCanAccessRemoteUrls, true);

    Theme::apply(app);

    auto *login = new LoginWindow;
    MainWindow *win = nullptr;

    QObject::connect(login, &LoginWindow::loginSucceeded, [&]() {
        if (!win) {
            win = new MainWindow;
            // 登出=销毁并重建主窗口(避免“换号登录显示上一个人”)
            QObject::connect(win, &MainWindow::logoutRequested, [&]() {
                if (win) {
                    win->close();
                    delete win;
                    win = nullptr;
                }
                login->reset();
                login->show();
            });
        }
        win->show();
        login->close();
    });

    login->show();
    return app.exec();
}
