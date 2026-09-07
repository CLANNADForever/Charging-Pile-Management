#include <QApplication>

#include "theme/Theme.h"
#include "ui/LoginWindow.h"
#include "ui/MainWindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("NCS"));
    app.setApplicationDisplayName(QStringLiteral("NCS · 充电"));

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
