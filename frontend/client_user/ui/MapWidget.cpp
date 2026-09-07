#include "MapWidget.h"

#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QTimer>
#include <QtMath>

#include "theme/Theme.h"

namespace {
// 柔和浅灰地图底色 + 网格(贴近真实地图的浅色观感)
const QColor kMapBg(0xEE, 0xF2, 0xF6);
const QColor kMapGrid(0xE0, 0xE6, 0xED);
constexpr qreal kClickRadius = 22.0;
} // namespace

MapWidget::MapWidget(QWidget *parent)
    : QWidget(parent)
{
    m_locateBtn = new QPushButton(QStringLiteral("定位"), this);
    m_locateBtn->setObjectName(QStringLiteral("locateButton"));
    m_locateBtn->setCursor(Qt::PointingHandCursor);
    connect(m_locateBtn, &QPushButton::clicked, this, [this]() { update(); });

    // 呼吸脉冲动画(用户定位点)
    m_pulseTimer = new QTimer(this);
    m_pulseTimer->setInterval(40);
    connect(m_pulseTimer, &QTimer::timeout, this, [this]() {
        m_pulse += 0.12;
        if (m_pulse > 6.283185)
            m_pulse = 0.0;
        update();
    });
    m_pulseTimer->start();
}

void MapWidget::setData(const QList<Station> &stations, double userLat, double userLon)
{
    m_stations = stations;
    m_userLat = userLat;
    m_userLon = userLon;
    updateBounds();
    update();
}

void MapWidget::updateBounds()
{
    m_minLat = m_userLat;
    m_maxLat = m_userLat;
    m_minLon = m_userLon;
    m_maxLon = m_userLon;
    for (const Station &s : m_stations) {
        m_minLat = qMin(m_minLat, s.latitude);
        m_maxLat = qMax(m_maxLat, s.latitude);
        m_minLon = qMin(m_minLon, s.longitude);
        m_maxLon = qMax(m_maxLon, s.longitude);
    }
}

QPointF MapWidget::project(double lat, double lon) const
{
    const double pad = 44.0;
    const double latRange = qMax(m_maxLat - m_minLat, 0.01);
    const double lonRange = qMax(m_maxLon - m_minLon, 0.01);
    const double scale = qMin((width() - 2 * pad) / lonRange,
                              (height() - 2 * pad) / latRange);
    const double x = width() / 2.0 + (lon - (m_minLon + m_maxLon) / 2.0) * scale;
    const double y = height() / 2.0 - (lat - (m_minLat + m_maxLat) / 2.0) * scale;
    return QPointF(x, y);
}

void MapWidget::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    repositionLocateButton();
}

void MapWidget::repositionLocateButton()
{
    m_locateBtn->setFixedSize(64, 32);
    m_locateBtn->move(width() - 64 - 12, height() - 32 - 12);
}

void MapWidget::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    // 底色 + 网格
    p.fillRect(rect(), kMapBg);
    p.setPen(kMapGrid);
    for (int x = 0; x < width(); x += 40)
        p.drawLine(x, 0, x, height());
    for (int y = 0; y < height(); y += 40)
        p.drawLine(0, y, width(), y);

    if (m_stations.isEmpty())
        return;

    QFont f = p.font();
    f.setPixelSize(10);
    p.setFont(f);

    // 电站标记(绿点 + 白描边)
    for (const Station &s : m_stations) {
        const QPointF pt = project(s.latitude, s.longitude);
        p.setPen(Qt::NoPen);
        p.setBrush(Theme::BrandGreen);
        p.drawEllipse(pt, 7, 7);
        p.setPen(QPen(Qt::white, 2));
        p.setBrush(Qt::NoBrush);
        p.drawEllipse(pt, 7, 7);
        p.setPen(QColor(0x3A, 0x3F, 0x4A));
        p.drawText(QRectF(pt.x() - 40, pt.y() + 10, 80, 14),
                   Qt::AlignHCenter | Qt::AlignTop, s.name);
    }

    // 用户定位(蓝点 + 呼吸外环)
    const QPointF u = project(m_userLat, m_userLon);
    const qreal ringR = 11.0 + 3.0 * qSin(m_pulse);
    p.setPen(QPen(Theme::ElectricBlue, 2));
    p.setBrush(Qt::NoBrush);
    p.drawEllipse(u, ringR, ringR);
    p.setPen(Qt::NoPen);
    p.setBrush(Theme::ElectricBlue);
    p.drawEllipse(u, 5, 5);
}

void MapWidget::mousePressEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton) {
        QWidget::mousePressEvent(event);
        return;
    }

    int bestId = -1;
    qreal bestDist = kClickRadius;
    for (const Station &s : m_stations) {
        const QPointF pt = project(s.latitude, s.longitude);
        const qreal dx = event->position().x() - pt.x();
        const qreal dy = event->position().y() - pt.y();
        const qreal d = qSqrt(dx * dx + dy * dy);
        if (d < bestDist) {
            bestDist = d;
            bestId = s.id;
        }
    }

    if (bestId >= 0)
        emit stationClicked(bestId);

    QWidget::mousePressEvent(event);
}
