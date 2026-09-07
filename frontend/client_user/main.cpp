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
            QObject::connect(win, &MainWindow::logoutRequested, [&]() {
                win->close();
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
