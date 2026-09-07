#pragma once

#include <QString>
#include <QWidget>

// 底部导航按钮:自绘矢量图标 + 文字,激活态用品牌电光蓝。
class NavButton : public QWidget
{
    Q_OBJECT
public:
    enum Icon {
        Home,    // 首页
        Charge,  // 充电(闪电,呼应 Logo 负形)
        Profile, // 我的
    };

    NavButton(Icon icon, const QString &label, QWidget *parent = nullptr);

    void setActive(bool active);
    bool isActive() const { return m_active; }

    QSize sizeHint() const override;

signals:
    void clicked();

protected:
    void paintEvent(QPaintEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    void drawIcon(QPainter &painter, const QRectF &rect) const;

    Icon m_icon;
    QString m_label;
    bool m_active = false;
};
