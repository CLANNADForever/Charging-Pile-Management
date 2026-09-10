#include <QApplication>

#include "theme/AdminTheme.h"
#include "ui/AdminLoginWindow.h"
#include "ui/AdminMainWindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("熠熠ee Admin"));
    app.setApplicationDisplayName(QStringLiteral("熠熠ee · 运营管理端"));

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
        win->showMaximized();
        login->close();
    });

    login->show();
    return app.exec();
}
