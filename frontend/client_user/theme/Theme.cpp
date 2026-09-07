#include "Theme.h"

#include <QApplication>

namespace Theme {

QString qss()
{
    return QStringLiteral(R"qss(
/* ===== NCS 用户端全局主题 (Motion-Driven + Bento) ===== */
QWidget {
    font-family: "Microsoft YaHei", "PingFang SC", "Noto Sans CJK SC", "Helvetica", sans-serif;
    font-size: 14px;
    color: #1A1D26;
}

QWidget#root {
    background: #F4F6FB;
}
QWidget#loginRoot {
    background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #E8F2FF, stop:0.34 #F4F6FB, stop:1 #F4F6FB);
}

/* 底部导航栏 */
QWidget#navBar {
    background: #FFFFFF;
    border-top: 1px solid #EEF0F4;
}
NavButton {
    background: transparent;
    border: none;
}

/* 卡片 (Bento 白卡 + 细描边) */
QFrame#card {
    background: #FFFFFF;
    border: 1px solid #EEF0F4;
    border-radius: 16px;
}
QFrame#logoCard {
    background: #FFFFFF;
    border: 1px solid #EEF0F4;
    border-radius: 24px;
}
QFrame#stationCard {
    background: #FFFFFF;
    border: 1px solid #EEF0F4;
    border-radius: 16px;
}
QFrame#divider {
    background: #EEF0F4;
}
QFrame#brandBar {
    background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #2F80FF, stop:0.5 #00D4FF, stop:1 #00B368);
    border-radius: 2px;
}

/* 标题 / 文字 */
QLabel#pageTitle {
    font-size: 17px;
    font-weight: bold;
    color: #1A1D26;
}
QLabel#sectionTitle {
    font-size: 15px;
    font-weight: bold;
    color: #1A1D26;
}
QLabel#hintLabel {
    font-size: 12px;
    color: #9AA0AC;
}
QLabel#valueLabel {
    font-size: 14px;
    color: #1A1D26;
}
QLabel#appTitle {
    font-size: 21px;
    font-weight: bold;
    color: #0A1128;
}
QLabel#appSubtitle {
    font-size: 13px;
    color: #9AA0AC;
}
QLabel#balanceBig {
    font-size: 30px;
    font-weight: bold;
    color: #2F80FF;
}
QLabel#couponEmoji {
    font-size: 40px;
}

/* 输入框 */
QLineEdit {
    background: #FFFFFF;
    border: 1px solid #E3E6EC;
    border-radius: 12px;
    padding: 11px 14px;
    font-size: 15px;
    selection-background-color: #2F80FF;
}
QLineEdit:focus {
    border: 1px solid #2F80FF;
}

/* 主按钮(品牌渐变) */
QPushButton#primaryButton {
    background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #2F80FF, stop:1 #00B3D1);
    color: #FFFFFF;
    border: none;
    border-radius: 12px;
    padding: 13px 0;
    font-size: 15px;
    font-weight: bold;
}
QPushButton#primaryButton:pressed { background: #1E6AE0; }
QPushButton#primaryButton:disabled { background: #C3D4EE; }

/* 描边按钮 */
QPushButton#ghostButton {
    background: #FFFFFF;
    color: #2F80FF;
    border: 1px solid #2F80FF;
    border-radius: 12px;
    padding: 11px 0;
    font-size: 13px;
}
QPushButton#ghostButton:disabled {
    color: #B9CDEA;
    border-color: #D5E4F7;
}

/* 文字按钮 */
QPushButton#textButton {
    background: transparent;
    color: #2F80FF;
    border: none;
    font-size: 14px;
}

/* 危险文字按钮 */
QPushButton#dangerTextButton {
    background: transparent;
    color: #EF4444;
    border: none;
    font-size: 14px;
}

/* 返回按钮 */
QPushButton#backButton {
    background: transparent;
    border: none;
    color: #1A1D26;
    font-size: 26px;
}

/* 入口行 */
QPushButton#entryButton {
    background: transparent;
    border: none;
    text-align: left;
    padding: 16px 4px;
    font-size: 15px;
    color: #1A1D26;
}
QPushButton#entryButton:pressed { background: #F4F6FB; }

/* 分类 tab */
QPushButton#tabButton {
    background: #FFFFFF;
    color: #8A8F99;
    border: 1px solid #E3E6EC;
    border-radius: 18px;
    padding: 8px 0;
    font-size: 13px;
}
QPushButton#tabButton:checked {
    background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #2F80FF, stop:1 #00B3D1);
    color: #FFFFFF;
    border: 1px solid transparent;
    font-weight: bold;
}

/* 搜索栏 */
QWidget#searchBar {
    background: #FFFFFF;
}
QPushButton#searchButton {
    background: #FFFFFF;
    border: 1px solid #E3E6EC;
    border-radius: 22px;
    padding: 11px 16px;
    text-align: left;
    color: #9AA0AC;
    font-size: 14px;
}

/* 优惠横幅 */
QPushButton#couponBanner {
    background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #FF5A4E, stop:1 #FFB300);
    color: #FFFFFF;
    border: none;
    border-radius: 12px;
    margin: 8px 12px 0 12px;
    font-size: 14px;
    font-weight: bold;
    text-align: left;
    padding-left: 16px;
}

/* 筛选 / 定位按钮 */
QPushButton#filterButton {
    background: #FFFFFF;
    border: 1px solid #E3E6EC;
    border-radius: 18px;
    padding: 7px 0;
    font-size: 13px;
    color: #1A1D26;
}
QPushButton#locateButton {
    background: #E6FFFFFF;
    border: 1px solid #E3E6EC;
    border-radius: 16px;
    color: #2F80FF;
    font-size: 12px;
}

/* 下拉框 */
QComboBox {
    background: #FFFFFF;
    border: 1px solid #E3E6EC;
    border-radius: 18px;
    padding: 7px 10px;
    font-size: 13px;
    color: #1A1D26;
}
QComboBox::drop-down { border: none; width: 20px; }
QComboBox QAbstractItemView {
    background: #FFFFFF;
    border: 1px solid #E3E6EC;
    selection-background-color: #EAF3FF;
    selection-color: #2F80FF;
}

/* 下拉菜单(筛选) */
QMenu {
    background: #FFFFFF;
    border: 1px solid #E3E6EC;
    border-radius: 12px;
    padding: 8px;
}
QMenu::item {
    padding: 9px 24px 9px 16px;
    border-radius: 8px;
    color: #1A1D26;
    font-size: 14px;
}
QMenu::item:selected {
    background: #EAF3FF;
    color: #2F80FF;
}
QMenu::item:checked {
    color: #2F80FF;
    font-weight: bold;
}
QMenu::separator {
    height: 1px;
    background: #EEF0F4;
    margin: 4px 8px;
}

/* 页面栈与滚动区:统一浅色底,避免深色块 */
QStackedWidget {
    background: #F4F6FB;
}
QScrollArea {
    background: transparent;
    border: none;
}
QScrollArea > QWidget > QWidget {
    background: transparent;
}

/* 列表 */
QListWidget {
    background: transparent;
    border: none;
}
QListWidget::item {
    background: transparent;
}

/* 表格 */
QTableWidget {
    background: #FFFFFF;
    border: none;
    gridline-color: #EEF0F4;
}
QHeaderView::section {
    background: #F7F9FC;
    border: none;
    padding: 9px 4px;
    font-size: 12px;
    color: #8A8F99;
}
QTableWidget::item {
    padding: 7px 4px;
}

/* Toast */
QLabel#toast {
    background: #DD0A1128;
    color: #FFFFFF;
    border-radius: 20px;
    font-size: 13px;
}
)qss");
}

void apply(QApplication &app)
{
    app.setStyle(QStringLiteral("Fusion"));
    app.setStyleSheet(qss());
}

QLinearGradient brandGradient(Qt::Orientation orientation)
{
    QLinearGradient g;
    if (orientation == Qt::Horizontal) {
        g.setStart(0.0, 0.0);
        g.setFinalStop(1.0, 0.0);
    } else {
        g.setStart(0.0, 0.0);
        g.setFinalStop(0.0, 1.0);
    }
    g.setColorAt(0.0, ElectricBlue);
    g.setColorAt(0.5, Cyan);
    g.setColorAt(1.0, BrandGreen);
    return g;
}

} // namespace Theme
