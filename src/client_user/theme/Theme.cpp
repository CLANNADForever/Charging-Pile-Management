#include "Theme.h"

#include <QApplication>
#include <QGraphicsOpacityEffect>
#include <QPropertyAnimation>
#include <QWebEngineView>
#include <QWidget>

namespace Theme {

QString qss()
{
    return QStringLiteral(R"qss(
/* ===== NCS 用户端全局主题 (柔和能量环 / Soft-glow) ===== */
QWidget {
    font-family: "Microsoft YaHei", "PingFang SC", "Noto Sans CJK SC", "Helvetica", sans-serif;
    font-size: 14px;
    color: #2A3240;
}

QWidget#root {
    background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #EDF3F9, stop:0.55 #F4F7FB, stop:1 #F8FAFC);
}
QWidget#loginRoot {
    background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #E3EFFB, stop:0.5 #F2F6FB, stop:1 #F8FAFC);
}

/* 底部导航栏(柔和浮起 + 极淡蓝绿渐变呼应扫码按钮) */
QWidget#navBar {
    background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #EDF4FD, stop:0.5 #ECF7F3, stop:1 #EAF6EE);
    border-top: 1px solid #EEF1F6;
}
NavButton {
    background: transparent;
    border: none;
}

/* 底部导航中间「扫码充电」按钮(凸起感:白描边 + 渐变) */
QPushButton#scanChargeButton {
    background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #5EA8FF, stop:0.55 #3EC9C0, stop:1 #45D094);
    color: #FFFFFF;
    border: 3px solid #FFFFFF;
    border-radius: 34px;
}
QPushButton#scanChargeButton:pressed { background: #4C93E8; }

/* 卡片 (暖白 + 大圆角 + 极淡描边) */
QFrame#card {
    background: #FDFEFF;
    border: 1px solid #EEF1F6;
    border-radius: 20px;
}
QFrame#logoCard {
    background: #FDFEFF;
    border: 1px solid #EEF1F6;
    border-radius: 26px;
}
QFrame#stationCard {
    background: #FDFEFF;
    border: 1px solid #EEF1F6;
    border-radius: 20px;
}
QFrame#divider {
    background: #EEF1F6;
}
QFrame#brandBar {
    background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #5EA8FF, stop:0.5 #3EC9C0, stop:1 #45D094);
    border-radius: 2px;
}

/* 标题 / 文字 */
QLabel#pageTitle {
    font-size: 17px;
    font-weight: bold;
    color: #2A3240;
}
QLabel#sectionTitle {
    font-size: 15px;
    font-weight: bold;
    color: #2A3240;
}
QLabel#hintLabel {
    font-size: 12px;
    color: #98A1B0;
}
QLabel#valueLabel {
    font-size: 14px;
    color: #2A3240;
}
QLabel#appTitle {
    font-size: 21px;
    font-weight: bold;
    color: #0A1128;
}
QLabel#appSubtitle {
    font-size: 13px;
    color: #98A1B0;
}
QLabel#balanceBig {
    font-size: 30px;
    font-weight: bold;
    color: #5EA8FF;
}
QLabel#couponEmoji {
    font-size: 40px;
}

/* 输入框 */
QLineEdit {
    background: #FFFFFF;
    border: 1px solid #E6EBF2;
    border-radius: 14px;
    padding: 11px 14px;
    font-size: 15px;
    selection-background-color: #5EA8FF;
}
QLineEdit:focus {
    border: 1px solid #5EA8FF;
}

/* 主按钮(柔和品牌渐变) */
QPushButton#primaryButton {
    background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #5EA8FF, stop:0.55 #3EC9C0, stop:1 #45D094);
    color: #FFFFFF;
    border: none;
    border-radius: 14px;
    padding: 13px 0;
    font-size: 15px;
    font-weight: bold;
}
QPushButton#primaryButton:hover { background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #6FB4FF, stop:0.55 #4FD4CC, stop:1 #54DCA2); }
QPushButton#primaryButton:pressed { background: #4C93E8; }
QPushButton#primaryButton:disabled { background: #C7D8EE; }

/* 描边按钮 */
QPushButton#ghostButton {
    background: #FFFFFF;
    color: #5EA8FF;
    border: 1px solid #BBD7FF;
    border-radius: 14px;
    padding: 11px 0;
    font-size: 13px;
}
QPushButton#ghostButton:hover { background: #F0F6FF; }
QPushButton#ghostButton:pressed { background: #E4EFFB; }
QPushButton#ghostButton:disabled {
    color: #C0D2EC;
    border-color: #DDE8F6;
}

/* 文字按钮 */
QPushButton#textButton {
    background: transparent;
    color: #5EA8FF;
    border: none;
    font-size: 14px;
}
QPushButton#textButton:hover { color: #3E8FEA; }
QPushButton#textButton:pressed { color: #2F7AD6; }

/* 危险文字按钮 */
QPushButton#dangerTextButton {
    background: transparent;
    color: #F06A6A;
    border: none;
    font-size: 14px;
}

/* 返回按钮 */
QPushButton#backButton {
    background: transparent;
    border: none;
    color: #2A3240;
    font-size: 26px;
}

/* 入口行 */
QPushButton#entryButton {
    background: transparent;
    border: none;
    text-align: left;
    padding: 16px 4px;
    font-size: 15px;
    color: #2A3240;
}
QPushButton#entryButton:hover { background: #F6F9FD; }
QPushButton#entryButton:pressed { background: #EEF3FA; }

/* 分类 tab */
QPushButton#tabButton {
    background: #FFFFFF;
    color: #8B93A1;
    border: 1px solid #E6EBF2;
    border-radius: 18px;
    padding: 8px 0;
    font-size: 13px;
}
QPushButton#tabButton:hover { border: 1px solid #BBD7FF; }
QPushButton#tabButton:checked {
    background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #5EA8FF, stop:0.55 #3EC9C0, stop:1 #45D094);
    color: #FFFFFF;
    border: 1px solid transparent;
    font-weight: bold;
}

/* 搜索栏 */
QWidget#searchBar {
    background: #FFFFFF;
}

/* 扫码预览框 */
QVideoWidget#scanPreview {
    background: #0B1220;
    border: 1px solid #EEF1F6;
    border-radius: 16px;
}

/* 主页列表面板:白色抽屉,顶部圆角 */
QWidget#listPanel {
    background: #FFFFFF;
    border-top-left-radius: 18px;
    border-top-right-radius: 18px;
}
QPushButton#searchButton {
    background: #FFFFFF;
    border: 1px solid #E6EBF2;
    border-radius: 22px;
    padding: 11px 16px;
    text-align: left;
    color: #98A1B0;
    font-size: 14px;
}
QPushButton#searchButton:hover { border-color: #BBD7FF; }
QPushButton#searchButton:pressed { background: #F0F6FF; }

/* 优惠横幅 */
QPushButton#couponBanner {
    background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #FF8A7A, stop:1 #FFB84D);
    color: #FFFFFF;
    border: none;
    border-radius: 14px;
    margin: 8px 12px 0 12px;
    font-size: 14px;
    font-weight: bold;
    text-align: left;
    padding-left: 16px;
}

/* 筛选 / 定位按钮 */
QPushButton#filterButton {
    background: #FFFFFF;
    border: 1px solid #E6EBF2;
    border-radius: 18px;
    padding: 7px 0;
    font-size: 13px;
    color: #2A3240;
}
QPushButton#filterButton:hover { border-color: #BBD7FF; background: #FAFCFF; }
QPushButton#filterButton:pressed { background: #EDF3FC; }
QPushButton#locateButton {
    background: #F0F6FF;
    border: 1px solid #DCE8F7;
    border-radius: 16px;
    color: #5EA8FF;
    font-size: 12px;
}

/* 下拉框 */
QComboBox {
    background: #FFFFFF;
    border: 1px solid #E6EBF2;
    border-radius: 18px;
    padding: 7px 10px;
    font-size: 13px;
    color: #2A3240;
}
QComboBox::drop-down { border: none; width: 20px; }
QComboBox QAbstractItemView {
    background: #FFFFFF;
    border: 1px solid #E6EBF2;
    selection-background-color: #EAF3FF;
    selection-color: #5EA8FF;
}

/* 下拉菜单(筛选) */
QMenu {
    background: #FFFFFF;
    border: 1px solid #E6EBF2;
    border-radius: 12px;
    padding: 8px;
}
QMenu::item {
    padding: 9px 24px 9px 16px;
    border-radius: 8px;
    color: #2A3240;
    font-size: 14px;
}
QMenu::item:selected {
    background: #EAF3FF;
    color: #5EA8FF;
}
QMenu::item:checked {
    color: #5EA8FF;
    font-weight: bold;
}
QMenu::separator {
    height: 1px;
    background: #EEF1F6;
    margin: 4px 8px;
}

/* 页面栈与滚动区:统一浅色底,避免深色块 */
QStackedWidget {
    background: transparent;
}
QScrollArea {
    background: transparent;
    border: none;
}
QScrollArea > QWidget > QWidget {
    background: transparent;
}

/* 滑动条:细 + 圆角 + 半透明,去掉默认方方正正的粗条 */
QScrollBar:vertical {
    background: transparent;
    width: 6px;
    margin: 0;
}
QScrollBar::handle:vertical {
    background: rgba(150, 162, 178, 0.55);
    border-radius: 3px;
    min-height: 30px;
}
QScrollBar::handle:vertical:hover {
    background: rgba(120, 134, 152, 0.75);
}
QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
    height: 0;
    background: transparent;
    border: none;
}
QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical {
    background: transparent;
}
QScrollBar:horizontal {
    background: transparent;
    height: 6px;
    margin: 0;
}
QScrollBar::handle:horizontal {
    background: rgba(150, 162, 178, 0.55);
    border-radius: 3px;
    min-width: 30px;
}
QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal {
    width: 0;
    background: transparent;
    border: none;
}
QScrollBar::add-page:horizontal, QScrollBar::sub-page:horizontal {
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

/* 选桩充电页的电桩卡片列表 */
QListWidget#chargerList::item {
    background: #FDFEFF;
    border: 1px solid #EEF1F6;
    border-radius: 14px;
    padding: 10px 16px;
    margin: 0 0 8px 0;
    font-size: 15px;
    color: #2A3240;
}
QListWidget#chargerList::item:hover {
    border: 1px solid #BBD7FF;
}
QListWidget#chargerList::item:selected {
    background: #EAF3FF;
    border: 1px solid #5EA8FF;
    color: #2A3240;
}

/* 表格 */
QTableWidget {
    background: #FFFFFF;
    border: none;
    gridline-color: #EEF1F6;
}
QHeaderView::section {
    background: #F7F9FC;
    border: none;
    padding: 9px 4px;
    font-size: 12px;
    color: #8B93A1;
}
QTableWidget::item {
    padding: 7px 4px;
}

/* Toast */
QLabel#toast {
    background: #DD2A3240;
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

void fadeIn(QWidget *w)
{
    if (!w)
        return;
    // QGraphicsOpacityEffect 的离屏渲染与 QWebEngineView 冲突(会崩溃/黑屏),跳过含地图的页面
    if (w->findChild<QWebEngineView *>())
        return;
    // 离屏淡入;动画结束移除特效。注意 setGraphicsEffect(nullptr) 会删除旧特效,切勿再手动 delete
    auto *eff = new QGraphicsOpacityEffect(w);
    w->setGraphicsEffect(eff);
    auto *anim = new QPropertyAnimation(eff, "opacity", w);
    anim->setDuration(220);
    anim->setStartValue(0.0);
    anim->setEndValue(1.0);
    anim->setEasingCurve(QEasingCurve::OutCubic);
    QObject::connect(anim, &QPropertyAnimation::finished, w, [w]() {
        w->setGraphicsEffect(nullptr);
    });
    anim->start(QAbstractAnimation::DeleteWhenStopped);
}

} // namespace Theme
