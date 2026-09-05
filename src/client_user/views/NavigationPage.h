#ifndef NCS_CLIENT_VIEWS_NAVIGATIONPAGE_H
#define NCS_CLIENT_VIEWS_NAVIGATIONPAGE_H

#include <QWidget>

class QLabel;
class QVBoxLayout;
class QWebEngineView;

namespace ncs {
namespace client {

// 导航/路线页：内嵌 QWebEngineView 加载 腾讯地图 URI 路线(免 key 免鉴权渲染)，
// 另提供外部打开兜底。view 懒创建，避免无头测试触发引擎。
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
    QString routeUrl() const;

    double myLat_ = 0, myLng_ = 0, stLat_ = 0, stLng_ = 0;
    QString stName_;
    QVBoxLayout* layout_ = nullptr;
    QLabel* info_ = nullptr;
    QWebEngineView* view_ = nullptr;
};

}  // namespace client
}  // namespace ncs

#endif  // NCS_CLIENT_VIEWS_NAVIGATIONPAGE_H
