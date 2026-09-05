#ifndef NCS_CLIENT_VIEWS_NAVIGATIONPAGE_H
#define NCS_CLIENT_VIEWS_NAVIGATIONPAGE_H

#include <QWidget>

class QLabel;

namespace ncs {
namespace client {

// 路线信息页：显示 我的位置→电站 的坐标与直线距离，可跳外部地图(高德 uri)。
// (内嵌 QWebEngine 因 VM 缺 Qt6WebEngineCore dev 依赖暂不可用，跳外部兜底)
class NavigationPage : public QWidget {
    Q_OBJECT
public:
    explicit NavigationPage(QWidget* parent = nullptr);
    void openRoute(double myLat, double myLng, double stLat, double stLng,
                   const QString& stName);

signals:
    void backRequested();

private:
    double myLat_ = 0, myLng_ = 0, stLat_ = 0, stLng_ = 0;
    QString stName_;
    QLabel* info_ = nullptr;
};

}  // namespace client
}  // namespace ncs

#endif  // NCS_CLIENT_VIEWS_NAVIGATIONPAGE_H
