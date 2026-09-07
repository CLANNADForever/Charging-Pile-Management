#include "AdminTheme.h"

#include <QApplication>
#include <QBrush>

#include <QtCharts/QAbstractAxis>
#include <QtCharts/QChart>
#include <QtCharts/QLegend>

namespace AdminTheme {

QString qss()
{
    return QStringLiteral(R"qss(
/* ===== NCS 管理端全局主题(深色 · 非纯黑 · Bento) ===== */
QWidget {
    font-family: "Microsoft YaHei", "PingFang SC", "Noto Sans CJK SC", "Helvetica", sans-serif;
    font-size: 14px;
    color: #E6ECF5;
}

QMainWindow, QWidget#appRoot {
    background: #0B1220;
}

/* ===== 侧边栏 ===== */
QWidget#sideBar {
    background: #0E1628;
    border-right: 1px solid #1B2740;
}
QLabel#sideBrand {
    font-size: 16px;
    font-weight: bold;
    color: #E6ECF5;
}
QLabel#sideSub {
    font-size: 11px;
    color: #5A6478;
}

/* ===== 导航列表 ===== */
QListWidget#navList {
    background: transparent;
    border: none;
    outline: none;
}
QListWidget#navList::item {
    color: #8A94A8;
    padding: 12px 20px;
    border: none;
    border-left: 3px solid transparent;
    border-radius: 8px;
    margin: 2px 10px;
}
QListWidget#navList::item:hover {
    background: #151E30;
    color: #E6ECF5;
}
QListWidget#navList::item:selected {
    background: #1B2740;
    color: #4D9FFF;
    border-left: 3px solid #4D9FFF;
    font-weight: bold;
}

/* ===== 顶栏 ===== */
QWidget#topBar {
    background: #0E1628;
    border-bottom: 1px solid #1B2740;
}
QLabel#adminLabel {
    color: #E6ECF5;
    font-size: 14px;
    font-weight: bold;
}

/* ===== 卡片(Bento) ===== */
QFrame#card {
    background: #151E30;
    border: 1px solid #232E44;
    border-radius: 16px;
}

/* ===== 标题 / 文字 ===== */
QLabel#pageTitle {
    font-size: 20px;
    font-weight: bold;
    color: #E6ECF5;
}
QLabel#cardTitle {
    font-size: 15px;
    font-weight: bold;
    color: #E6ECF5;
}
QLabel#hintLabel {
    font-size: 12px;
    color: #8A94A8;
}
QLabel#mutedLabel {
    font-size: 12px;
    color: #5A6478;
}
QLabel#kpiLabel {
    font-size: 13px;
    color: #8A94A8;
}
QLabel#kpiValue {
    font-size: 32px;
    font-weight: bold;
    color: #4D9FFF;
}
QLabel#placeholderTitle {
    font-size: 20px;
    font-weight: bold;
    color: #5A6478;
}

/* ===== 登录页 ===== */
QWidget#loginRoot {
    background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                                stop:0 #0B1220, stop:0.55 #0E1628, stop:1 #0B1220);
}
QLabel#appTitle {
    font-size: 26px;
    font-weight: bold;
    color: #E6ECF5;
}
QLabel#appSubtitle {
    font-size: 14px;
    color: #8A94A8;
}
QLabel#errorLabel {
    font-size: 12px;
    color: #EF4444;
}

/* ===== 输入框 ===== */
QLineEdit {
    background: #0F1726;
    border: 1px solid #232E44;
    border-radius: 8px;
    padding: 11px 14px;
    font-size: 14px;
    color: #E6ECF5;
    selection-background-color: #4D9FFF;
    selection-color: #0B1220;
}
QLineEdit:focus {
    border: 1px solid #4D9FFF;
}
QLineEdit#loginEdit {
    background: #0F1726;
    border: 1px solid #2A3548;
    border-radius: 14px;
    padding: 20px 22px;
    font-size: 18px;
    color: #E6ECF5;
    selection-background-color: #4D9FFF;
}
QLineEdit#loginEdit:focus {
    border: 1px solid #4D9FFF;
}
QLineEdit:disabled {
    color: #5A6478;
    background: #0B1220;
}

/* ===== 主按钮(品牌渐变) ===== */
QPushButton#primaryButton {
    background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #4D9FFF, stop:1 #22D3EE);
    color: #0B1220;
    border: none;
    border-radius: 12px;
    padding: 17px 0;
    font-size: 17px;
    font-weight: bold;
}
QPushButton#primaryButton:hover { background: #5EACFF; }
QPushButton#primaryButton:pressed { background: #3D8AE6; }
QPushButton#primaryButton:disabled {
    background: #1B2740;
    color: #5A6478;
}

/* ===== 描边按钮 ===== */
QPushButton#secondaryButton {
    background: #151E30;
    color: #E6ECF5;
    border: 1px solid #232E44;
    border-radius: 8px;
    padding: 10px 16px;
    font-size: 13px;
}
QPushButton#secondaryButton:hover { background: #1B2740; border-color: #4D9FFF; }
QPushButton#secondaryButton:pressed { background: #0F1726; }
QPushButton#secondaryButton:disabled { color: #5A6478; }
QPushButton#secondaryButton:checked {
    background: #1B2740;
    border-color: #4D9FFF;
    color: #4D9FFF;
    font-weight: bold;
}

/* ===== 文字按钮 ===== */
QPushButton#textButton {
    background: transparent;
    color: #4D9FFF;
    border: none;
    font-size: 13px;
    padding: 6px 8px;
}
QPushButton#textButton:hover { color: #22D3EE; }

/* ===== 危险按钮 ===== */
QPushButton#dangerButton {
    background: transparent;
    color: #EF4444;
    border: 1px solid #EF4444;
    border-radius: 8px;
    padding: 10px 16px;
    font-size: 13px;
}
QPushButton#dangerButton:hover { background: rgba(239, 68, 68, 0.12); }

/* ===== 登录卡内的显示密码按钮 ===== */
QPushButton#togglePwdButton {
    background: transparent;
    color: #8A94A8;
    border: none;
    font-size: 14px;
    padding: 0 6px;
}

/* ===== 下拉框 ===== */
QComboBox {
    background: #0F1726;
    border: 1px solid #232E44;
    border-radius: 8px;
    padding: 8px 12px;
    font-size: 13px;
    color: #E6ECF5;
}
QComboBox:hover { border-color: #4D9FFF; }
QComboBox::drop-down { border: none; width: 22px; }
QComboBox::down-arrow {
    image: none;
    border-left: 4px solid transparent;
    border-right: 4px solid transparent;
    border-top: 5px solid #8A94A8;
    margin-right: 6px;
}
QComboBox QAbstractItemView {
    background: #151E30;
    border: 1px solid #232E44;
    color: #E6ECF5;
    selection-background-color: #1B2740;
    selection-color: #4D9FFF;
    outline: none;
}

/* ===== 菜单 ===== */
QMenu {
    background: #151E30;
    border: 1px solid #232E44;
    border-radius: 10px;
    padding: 6px;
}
QMenu::item {
    padding: 8px 24px 8px 16px;
    border-radius: 6px;
    color: #E6ECF5;
}
QMenu::item:selected {
    background: #1B2740;
    color: #4D9FFF;
}
QMenu::separator {
    height: 1px;
    background: #232E44;
    margin: 4px 8px;
}

/* ===== 表格 ===== */
QTableWidget, QTableView {
    background: #121A29;
    alternate-background-color: #151E30;
    border: 1px solid #232E44;
    border-radius: 10px;
    gridline-color: #232E44;
    color: #E6ECF5;
    selection-background-color: #1B2740;
    selection-color: #4D9FFF;
}
QHeaderView::section {
    background: #151E30;
    color: #8A94A8;
    border: none;
    border-bottom: 1px solid #232E44;
    padding: 10px 8px;
    font-size: 12px;
}
QTableWidget::item {
    padding: 6px 8px;
    border: none;
}

/* ===== 滚动区 ===== */
QScrollArea {
    background: transparent;
    border: none;
}
QScrollArea > QWidget > QWidget {
    background: transparent;
}
QScrollBar:vertical {
    background: #0B1220;
    width: 10px;
    border-radius: 5px;
}
QScrollBar::handle:vertical {
    background: #232E44;
    border-radius: 5px;
    min-height: 30px;
}
QScrollBar::handle:vertical:hover { background: #2E3B54; }
QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }
QScrollBar:horizontal {
    background: #0B1220;
    height: 10px;
    border-radius: 5px;
}
QScrollBar::handle:horizontal {
    background: #232E44;
    border-radius: 5px;
    min-width: 30px;
}
QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { width: 0; }

/* ===== 状态栏 ===== */
QStatusBar {
    background: #0E1628;
    border-top: 1px solid #1B2740;
    color: #8A94A8;
    font-size: 12px;
}
QStatusBar QLabel {
    color: #8A94A8;
    font-size: 12px;
}

/* ===== 勾选框 ===== */
QCheckBox {
    color: #E6ECF5;
    spacing: 8px;
}
QCheckBox::indicator {
    width: 16px;
    height: 16px;
    border: 1px solid #232E44;
    border-radius: 4px;
    background: #0F1726;
}
QCheckBox::indicator:checked {
    background: #4D9FFF;
    border-color: #4D9FFF;
}

/* ===== 进度条 ===== */
QProgressBar {
    background: #0F1726;
    border: 1px solid #232E44;
    border-radius: 6px;
    height: 10px;
    text-align: center;
    color: #E6ECF5;
    font-size: 11px;
}
QProgressBar::chunk {
    background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #4D9FFF, stop:1 #22D3EE);
    border-radius: 5px;
}

/* ===== Toast ===== */
QLabel#toast {
    background: #1B2740;
    color: #E6ECF5;
    border: 1px solid #232E44;
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
    g.setColorAt(0.0, Blue);
    g.setColorAt(1.0, Cyan);
    return g;
}

void styleChart(QChart *chart)
{
    if (!chart)
        return;

    chart->setBackgroundVisible(false);
    chart->setPlotAreaBackgroundVisible(false);
    chart->setTitleBrush(QBrush(TextMain));

    if (auto *legend = chart->legend())
        legend->setLabelColor(TextSub);

    const auto axes = chart->axes();
    for (QAbstractAxis *axis : axes) {
        axis->setLabelsColor(TextSub);
        axis->setTitleBrush(QBrush(TextSub));
        axis->setGridLineColor(Border);
    }
}

} // namespace AdminTheme
