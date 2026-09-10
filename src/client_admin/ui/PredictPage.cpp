#include "PredictPage.h"

#include <QComboBox>
#include <QFrame>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPainter>
#include <QPushButton>
#include <QTableWidget>
#include <QTimer>
#include <QVBoxLayout>

#include <QtCharts/QCategoryAxis>
#include <QtCharts/QChart>
#include <QtCharts/QChartView>
#include <QtCharts/QLineSeries>
#include <QtCharts/QSplineSeries>
#include <QtCharts/QScatterSeries>
#include <QtCharts/QValueAxis>

#include "common/Toast.h"
#include "core/service/PredictService.h"
#include "core/service/StationService.h"
#include "theme/AdminTheme.h"

namespace {

QChart *buildLoadChart(const QList<LoadPoint> &points, bool past24)
{
    auto *chart = new QChart;

    auto *predicted = new QSplineSeries;
    predicted->setName(past24 ? QStringLiteral("昨日同期参考")
                              : QStringLiteral("模型预测负荷"));
    predicted->setColor(QColor(0xFF, 0x7A, 0x3B));
    for (int i = 0; i < points.size(); ++i)
        predicted->append(i, points[i].predicted);

    QPen penP(QColor(0xFF, 0x7A, 0x3B));
    penP.setWidth(3);
    penP.setStyle(Qt::SolidLine);
    predicted->setPen(penP);

    chart->addSeries(predicted);

    QSplineSeries *actual = nullptr;
    if (past24) {
        actual = new QSplineSeries;
        actual->setName(QStringLiteral("历史实际负荷"));
        actual->setColor(QColor(0x35, 0xB8, 0xFF));
        for (int i = 0; i < points.size(); ++i)
            actual->append(i, points[i].actual);
        QPen penA(QColor(0x35, 0xB8, 0xFF));
        penA.setWidth(2);
        actual->setPen(penA);
        chart->addSeries(actual);
    }

    auto *over = new QScatterSeries;
    over->setName(QStringLiteral("超过预警"));
    over->setColor(QColor(0xEF, 0x44, 0x44));
    over->setMarkerSize(9.0);
    for (int i = 0; i < points.size(); ++i)
        if (points[i].warning)
            over->append(i, points[i].predicted);
    if (over->count() > 0)
        chart->addSeries(over);

    auto *axisX = new QCategoryAxis;
    axisX->setLabelsPosition(QCategoryAxis::AxisLabelsPositionOnValue);
    for (int i = 0; i < points.size(); ++i)
        axisX->append(points[i].label, i);
    axisX->setRange(0, qMax(0, points.size() - 1));
    axisX->setLabelsAngle(-45);

    double maxV = 0.0;
    for (const LoadPoint &p : points) {
        maxV = qMax(maxV, p.predicted);
        if (past24)
            maxV = qMax(maxV, p.actual);
    }
    auto *axisY = new QValueAxis;
    axisY->setRange(0.0, maxV * 1.15);
    axisY->setLabelFormat(QStringLiteral("%.0f"));

    chart->addAxis(axisX, Qt::AlignBottom);
    chart->addAxis(axisY, Qt::AlignLeft);
    predicted->attachAxis(axisX);
    predicted->attachAxis(axisY);
    if (over->count() > 0) {
        over->attachAxis(axisX);
        over->attachAxis(axisY);
    }
    if (actual) {
        actual->attachAxis(axisX);
        actual->attachAxis(axisY);
    }

    AdminTheme::styleChart(chart);
    return chart;
}

} // namespace

PredictPage::PredictPage(QWidget *parent)
    : AdminPage(parent)
{
    auto *lay = new QVBoxLayout(this);
    lay->setContentsMargins(24, 24, 24, 24);
    lay->setSpacing(16);

    auto *title = new QLabel(QStringLiteral("智能预测"), this);
    title->setObjectName(QStringLiteral("pageTitle"));
    lay->addWidget(title);

    // 工具栏
    auto *tool = new QHBoxLayout;
    tool->setSpacing(10);

    m_btn1 = new QPushButton(QStringLiteral("过去 24 小时"), this);
    m_btn6 = new QPushButton(QStringLiteral("未来 6 小时"), this);
    m_btn24 = new QPushButton(QStringLiteral("未来 24 小时"), this);
    for (auto *b : { m_btn1, m_btn6, m_btn24 }) {
        b->setObjectName(QStringLiteral("secondaryButton"));
        b->setCheckable(true);
        b->setCursor(Qt::PointingHandCursor);
    }
    m_btn1->setChecked(true);
    m_past24 = true;
    m_horizon = 24;  // 过去 24h 内部仍取 24h 窗口数据, 横轴回推

    m_stationCombo = new QComboBox(this);
    m_stationCombo->addItem(QStringLiteral("全部电站"), -1);
    for (const Station &s : StationService::instance().listStations())
        m_stationCombo->addItem(s.name, s.id);
    m_stationCombo->setEnabled(false);
    m_stationCombo->setToolTip(QStringLiteral("演示数据来自 UrbanEV 离线样本站"));

    m_runBtn = new QPushButton(QStringLiteral("刷新预测"), this);
    m_runBtn->setObjectName(QStringLiteral("secondaryButton"));
    m_runBtn->setCursor(Qt::PointingHandCursor);
    connect(m_runBtn, &QPushButton::clicked, this, &PredictPage::onRunPredict);

    tool->addWidget(m_btn1);
    tool->addWidget(m_btn6);
    tool->addWidget(m_btn24);
    tool->addSpacing(12);
    tool->addWidget(m_stationCombo, 1);
    tool->addWidget(m_runBtn);
    lay->addLayout(tool);

    auto setPast24 = [this]() {
        m_past24 = true;
        m_horizon = 24;
        m_btn1->setChecked(true);
        m_btn6->setChecked(false);
        m_btn24->setChecked(false);
        rebuild();
    };
    auto setFuture = [this](int h) {
        m_past24 = false;
        m_horizon = h;
        m_btn1->setChecked(false);
        m_btn6->setChecked(h == 6);
        m_btn24->setChecked(h == 24);
        rebuild();
    };
    connect(m_btn1, &QPushButton::clicked, this, [setPast24]() { setPast24(); });
    connect(m_btn6, &QPushButton::clicked, this, [setFuture]() { setFuture(6); });
    connect(m_btn24, &QPushButton::clicked, this, [setFuture]() { setFuture(24); });
    connect(m_stationCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int) { rebuild(); });

    // 负荷对比图卡
    auto *chartCard = new QFrame;
    chartCard->setObjectName(QStringLiteral("card"));
    auto *cc = new QVBoxLayout(chartCard);
    cc->setContentsMargins(20, 16, 20, 16);
    cc->setSpacing(12);
    auto *chartTitle = new QLabel(QStringLiteral("智能预测曲线"), chartCard);
    chartTitle->setObjectName(QStringLiteral("cardTitle"));
    cc->addWidget(chartTitle);
    m_chart = new QChartView(chartCard);
    m_chart->setRenderHint(QPainter::Antialiasing);
    m_chart->setBackgroundBrush(AdminTheme::Card);
    m_chart->setMinimumHeight(300);
    cc->addWidget(m_chart);
    lay->addWidget(chartCard, 3);

    // 预测数据表卡
    auto *tableCard = new QFrame;
    tableCard->setObjectName(QStringLiteral("card"));
    auto *tc = new QVBoxLayout(tableCard);
    tc->setContentsMargins(20, 16, 20, 16);
    tc->setSpacing(12);
    auto *tableTitle = new QLabel(QStringLiteral("预测汇总"), tableCard);
    tableTitle->setObjectName(QStringLiteral("cardTitle"));
    tc->addWidget(tableTitle);
    m_table = new QTableWidget(0, 4, tableCard);
    m_table->setHorizontalHeaderLabels({
        QStringLiteral("电站"), QStringLiteral("预测充电量(度)"),
        QStringLiteral("预测空闲桩数"), QStringLiteral("负荷状态") });
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->verticalHeader()->setVisible(false);
    m_table->horizontalHeader()->setStretchLastSection(true);
    tc->addWidget(m_table);
    lay->addWidget(tableCard, 2);

    rebuild();
}

void PredictPage::refresh()
{
    rebuild();
}

void PredictPage::rebuild()
{
    const int stationId = m_stationCombo->currentData().toInt();
    const auto series =
        PredictService::instance().loadSeries(m_horizon, stationId, m_past24);
    const auto list = PredictService::instance().predictionList(m_horizon);

    auto *old = m_chart->chart();
    m_chart->setChart(buildLoadChart(series, m_past24));
    delete old;

    m_table->setRowCount(list.size());
    for (int row = 0; row < list.size(); ++row) {
        const LoadPrediction &p = list[row];
        auto *c0 = new QTableWidgetItem(p.stationName);
        auto *c1 = new QTableWidgetItem(QString::number(p.predictedEnergy, 'f', 1));
        auto *c2 = new QTableWidgetItem(QString::number(p.predictedIdle));
        auto *c3 = new QTableWidgetItem(p.isPeak ? QStringLiteral("高峰") : QStringLiteral("正常"));
        c3->setForeground(p.isPeak ? AdminTheme::Red : AdminTheme::Green);

        m_table->setItem(row, 0, c0);
        m_table->setItem(row, 1, c1);
        m_table->setItem(row, 2, c2);
        m_table->setItem(row, 3, c3);
    }
}

void PredictPage::onRunPredict()
{
    m_runBtn->setEnabled(false);
    Toast::show(this, QStringLiteral("正在刷新预测..."));

    QTimer::singleShot(300, this, [this]() {
        const bool ok = PredictService::instance().runPrediction();
        if (ok)
            rebuild();
        Toast::show(this, ok ? QStringLiteral("预测完成,已刷新")
                             : QStringLiteral("ML 数据未就绪,已保留上次结果"));
        m_runBtn->setEnabled(true);
    });
}
