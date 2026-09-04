#ifndef NCS_CLIENT_VIEWS_MAINWINDOW_H
#define NCS_CLIENT_VIEWS_MAINWINDOW_H

#include <QMainWindow>

namespace ncs {
namespace client {

// C 端主窗口：本片只做竖屏壳窗占位，登录/选桩/我的等页面由后续 change 加入。
class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
};

}  // namespace client
}  // namespace ncs

#endif  // NCS_CLIENT_VIEWS_MAINWINDOW_H
