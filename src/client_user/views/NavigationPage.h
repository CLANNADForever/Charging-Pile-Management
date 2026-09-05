#ifndef NCS_CLIENT_VIEWS_NAVIGATIONPAGE_H
#define NCS_CLIENT_VIEWS_NAVIGATIONPAGE_H

#include <QWidget>

class QLabel;
class QPushButton;
class QVBoxLayout;
class QWebEngineView;

namespace ncs {
namespace client {

// 导航/路线页：内嵌 QWebEngineView 地图(Leaflet/OSM 两点+直线)，懒创建避免无头测试触发。
class NavigationPage : public QWidget {
    Q_OBJECT
public:
    explicit NavigationPage(QWidget* parent = nullptr);
    void openRoute(double myLat, double myLng, double stLat, double stLng,
                   const QString& stName);

signals:
    void backRequested();

private:
    void ensureView();

    double myLat_ = 0, myLng_ = 0, stLat_ = 0, stLng_ = 0;
    QString stName_;
    QVBoxLayout* layout_ = nullptr;
    QLabel* info_ = nullptr;
    QPushButton* openBtn_ = nullptr;
    QWebEngineView* view_ = nullptr;
};

}  // namespace client
}  // namespace ncs

#endif  // NCS_CLIENT_VIEWS_NAVIGATIONPAGE_H
