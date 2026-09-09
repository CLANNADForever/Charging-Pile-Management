#include "MapWidget.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPushButton>
#include <QResizeEvent>
#include <QUrl>
#include <QVBoxLayout>
#include <QWebChannel>
#include <QWebEngineView>

namespace {
const QString kMapUrl = QStringLiteral("qrc:/map.html");
}

// JS → C++ 桥:地图点击电站标记时回传站点 id
class MapBridge : public QObject
{
    Q_OBJECT
public:
    using QObject::QObject;

public slots:
    void onStationClicked(int stationId) { emit stationClicked(stationId); }

signals:
    void stationClicked(int stationId);
};

MapWidget::MapWidget(QWidget *parent)
    : QWidget(parent)
{
    auto *bridge = new MapBridge(this);
    m_channel = new QWebChannel(this);
    m_channel->registerObject(QStringLiteral("bridge"), bridge);

    m_view = new QWebEngineView(this);
    m_view->page()->setWebChannel(m_channel);
    connect(bridge, &MapBridge::stationClicked, this, &MapWidget::stationClicked);
    connect(m_view, &QWebEngineView::loadFinished, this, [this](bool ok) {
        m_loaded = ok;
        if (ok)
            injectData();
    });

    auto *lay = new QVBoxLayout(this);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->addWidget(m_view);

    // 悬浮"定位"按钮:回到用户当前位置
    m_locateBtn = new QPushButton(QStringLiteral("定位"), this);
    m_locateBtn->setObjectName(QStringLiteral("locateButton"));
    m_locateBtn->setCursor(Qt::PointingHandCursor);
    m_locateBtn->setFixedSize(64, 32);
    connect(m_locateBtn, &QPushButton::clicked, this, [this]() {
        if (m_userLat == 0.0 && m_userLon == 0.0)
            return;  // 未知定位:不跳转
        const QString js = QStringLiteral("if(window.focusStation) focusStation(%1,%2);")
                               .arg(m_userLat, 0, 'f', 6)
                               .arg(m_userLon, 0, 'f', 6);
        m_view->page()->runJavaScript(js);
    });

    // 用 http baseUrl 加载,规避腾讯地图对 file:// / qrc:// 协议的限制
    QFile mapFile(QStringLiteral(":/map.html"));
    if (mapFile.open(QIODevice::ReadOnly))
        m_view->setHtml(QString::fromUtf8(mapFile.readAll()),
                        QUrl(QStringLiteral("http://localhost/map.html")));
    else
        m_view->load(QUrl(kMapUrl));
}

void MapWidget::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    if (m_locateBtn) {
        m_locateBtn->move(width() - m_locateBtn->width() - 12,
                          height() - m_locateBtn->height() - 12);
        m_locateBtn->raise();
    }
}

void MapWidget::setData(const QList<Station> &stations, double userLat, double userLon)
{
    m_stations = stations;
    m_userLat = userLat;
    m_userLon = userLon;
    if (m_loaded)
        injectData();
}

void MapWidget::injectData()
{
    QJsonArray arr;
    for (const Station &s : m_stations) {
        QJsonObject o;
        o.insert(QStringLiteral("id"), s.id);
        o.insert(QStringLiteral("name"), s.name);
        o.insert(QStringLiteral("lat"), s.latitude);
        o.insert(QStringLiteral("lng"), s.longitude);
        arr.append(o);
    }
    const QString json = QString::fromUtf8(
        QJsonDocument(arr).toJson(QJsonDocument::Compact));
    // 两个独立调用：setStations(含视野对齐)出错也不影响用户蓝点绘制
    m_view->page()->runJavaScript(
        QStringLiteral("if(window.setStations) setStations(") + json +
        QStringLiteral(");"));
    if (m_userLat != 0.0 || m_userLon != 0.0)
        m_view->page()->runJavaScript(
            QStringLiteral("if(window.setUserLocation) setUserLocation(%1,%2);")
                .arg(m_userLat, 0, 'f', 6)
                .arg(m_userLon, 0, 'f', 6));
}

#include "MapWidget.moc"
