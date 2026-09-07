#include <QApplication>

#include "theme/AdminTheme.h"
#include "ui/AdminLoginWindow.h"
#include "ui/AdminMainWindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("NCS Admin"));
    app.setApplicationDisplayName(QStringLiteral("NCS · 运营管理端"));

    AdminTheme::apply(app);

    auto *login = new AdminLoginWindow;
    AdminMainWindow *win = nullptr;

    QObject::connect(login, &AdminLoginWindow::loginSucceeded, [&]() {
        if (!win) {
            win = new AdminMainWindow;
            QObject::connect(win, &AdminMainWindow::logoutRequested, [&]() {
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
