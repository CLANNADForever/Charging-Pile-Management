#include "NavButton.h"

#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>

#include "theme/Theme.h"

NavButton::NavButton(Icon icon, const QString &label, QWidget *parent)
    : QWidget(parent)
    , m_icon(icon)
    , m_label(label)
{
    setCursor(Qt::PointingHandCursor);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setMinimumHeight(64);
}

void NavButton::setActive(bool active)
{
    if (m_active == active)
        return;
    m_active = active;
    update();
}

QSize NavButton::sizeHint() const
{
    return QSize(120, 64);
}

void NavButton::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    const QColor color = m_active ? Theme::ElectricBlue : Theme::TextSecondary;

    // 顶部激活指示条
    if (m_active) {
        p.setPen(Qt::NoPen);
        p.setBrush(Theme::ElectricBlue);
        p.drawRoundedRect(QRectF(width() * 0.30, 2.0, width() * 0.40, 3.0), 1.5, 1.5);
    }

    // 图标
    const QRectF iconRect(width() / 2.0 - 11.0, 8.0, 22.0, 22.0);
    p.setPen(Qt::NoPen);
    p.setBrush(color);
    drawIcon(p, iconRect);

    // 文字
    p.setPen(color);
    QFont f = p.font();
    f.setPixelSize(11);
    p.setFont(f);
    p.drawText(QRectF(0.0, 33.0, width(), 18.0), Qt::AlignHCenter | Qt::AlignTop, m_label);
}

void NavButton::mouseReleaseEvent(QMouseEvent *event)
{
    if (rect().contains(event->position().toPoint()))
        emit clicked();
    QWidget::mouseReleaseEvent(event);
}

void NavButton::drawIcon(QPainter &p, const QRectF &rect) const
{
    p.save();
    p.translate(rect.topLeft());
    p.scale(rect.width() / 24.0, rect.height() / 24.0);

    switch (m_icon) {
    case Home: {
        QPainterPath house;
        house.moveTo(2.0, 11.0);
        house.lineTo(12.0, 2.0);
        house.lineTo(22.0, 11.0);
        house.lineTo(20.0, 11.0);
        house.lineTo(20.0, 21.0);
        house.lineTo(15.0, 21.0);
        house.lineTo(15.0, 14.0);
        house.lineTo(9.0, 14.0);
        house.lineTo(9.0, 21.0);
        house.lineTo(4.0, 21.0);
        house.lineTo(4.0, 11.0);
        house.closeSubpath();
        p.drawPath(house);
        break;
    }
    case Charge: {
        QPainterPath bolt;
        bolt.moveTo(13.5, 0.0);
        bolt.lineTo(5.5, 13.5);
        bolt.lineTo(11.5, 13.5);
        bolt.lineTo(10.5, 24.0);
        bolt.lineTo(18.5, 10.5);
        bolt.lineTo(12.5, 10.5);
        bolt.closeSubpath();
        p.drawPath(bolt);
        break;
    }
    case Profile: {
        p.drawEllipse(QRectF(7.0, 1.5, 10.0, 10.0));
        QPainterPath body;
        body.addRoundedRect(QRectF(3.0, 13.0, 18.0, 10.0), 6.0, 6.0);
        p.drawPath(body);
        break;
    }
    }

    p.restore();
}
