#include "ChargerPage.h"

#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QTableWidget>
#include <QTimer>
#include <QVBoxLayout>

#include "common/Toast.h"
#include "common/Utils.h"
#include "core/service/StationService.h"

namespace {

QString stationNameOf(int stationId)
{
    for (const Station &s : StationService::instance().listStations())
        if (s.id == stationId)
            return s.name;
    return QStringLiteral("未知电站");
}

} // namespace

ChargerPage::ChargerPage(QWidget *parent)
    : AdminPage(parent)
{
    auto *lay = new QVBoxLayout(this);
    lay->setContentsMargins(24, 24, 24, 24);
    lay->setSpacing(16);

    auto *title = new QLabel(QStringLiteral("充电桩管理"), this);
    title->setObjectName(QStringLiteral("pageTitle"));
    lay->addWidget(title);

    // 工具栏
    auto *tool = new QHBoxLayout;
    tool->setSpacing(10);

    m_stationFilter = new QComboBox(this);
    m_stationFilter->addItem(QStringLiteral("全部电站"), -1);
    for (const Station &s : StationService::instance().listStations())
        m_stationFilter->addItem(s.name, s.id);

    m_statusFilter = new QComboBox(this);
    m_statusFilter->addItem(QStringLiteral("全部状态"), -1);
    m_statusFilter->addItem(QStringLiteral("空闲"), 0);
    m_statusFilter->addItem(QStringLiteral("使用中"), 1);
    m_statusFilter->addItem(QStringLiteral("故障"), 2);

    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setPlaceholderText(QStringLiteral("按编号搜索"));

    auto *addBtn = new QPushButton(QStringLiteral("新增电桩"), this);
    addBtn->setObjectName(QStringLiteral("secondaryButton"));
    addBtn->setCursor(Qt::PointingHandCursor);
    connect(addBtn, &QPushButton::clicked, this, &ChargerPage::onAddCharger);

    tool->addWidget(m_stationFilter);
    tool->addWidget(m_statusFilter);
    tool->addWidget(m_searchEdit, 1);
    tool->addWidget(addBtn);
    lay->addLayout(tool);

    // 表格
    m_table = new QTableWidget(0, 7, this);
    m_table->setHorizontalHeaderLabels({
        QStringLiteral("电桩编号"), QStringLiteral("所属电站"), QStringLiteral("类型"),
        QStringLiteral("功率(kW)"), QStringLiteral("状态"), QStringLiteral("累计次数"),
        QStringLiteral("累计时长(h)") });
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->verticalHeader()->setVisible(false);
    m_table->horizontalHeader()->setStretchLastSection(true);
    lay->addWidget(m_table, 1);

    // 操作栏
    auto *actions = new QHBoxLayout;
    actions->setSpacing(10);
    m_rebootBtn = new QPushButton(QStringLiteral("远程重启"), this);
    m_faultBtn = new QPushButton(QStringLiteral("标记故障"), this);
    m_recoverBtn = new QPushButton(QStringLiteral("恢复正常"), this);
    m_deleteBtn = new QPushButton(QStringLiteral("删除"), this);
    for (auto *b : { m_rebootBtn, m_faultBtn, m_recoverBtn }) {
        b->setObjectName(QStringLiteral("secondaryButton"));
        b->setCursor(Qt::PointingHandCursor);
    }
    m_deleteBtn->setObjectName(QStringLiteral("dangerButton"));
    m_deleteBtn->setCursor(Qt::PointingHandCursor);

    connect(m_rebootBtn, &QPushButton::clicked, this, &ChargerPage::onReboot);
    connect(m_faultBtn, &QPushButton::clicked, this, &ChargerPage::onMarkFault);
    connect(m_recoverBtn, &QPushButton::clicked, this, &ChargerPage::onRecover);
    connect(m_deleteBtn, &QPushButton::clicked, this, &ChargerPage::onDelete);

    actions->addWidget(m_rebootBtn);
    actions->addWidget(m_faultBtn);
    actions->addWidget(m_recoverBtn);
    actions->addStretch();
    actions->addWidget(m_deleteBtn);
    lay->addLayout(actions);

    connect(m_stationFilter, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int) { rebuildTable(); });
    connect(m_statusFilter, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int) { rebuildTable(); });
    connect(m_searchEdit, &QLineEdit::textChanged, this, [this](const QString &) { rebuildTable(); });
    connect(m_table, &QTableWidget::itemSelectionChanged, this, &ChargerPage::updateActionState);

    m_autoRefresh = new QTimer(this);
    m_autoRefresh->setInterval(1000);
    connect(m_autoRefresh, &QTimer::timeout, this, &ChargerPage::onAutoPoll);

    rebuildTable();
}

void ChargerPage::refresh()
{
    rebuildTable();
}

void ChargerPage::rebuildTable()
{
    const int stationId = m_stationFilter->currentData().toInt();
    const int status = m_statusFilter->currentData().toInt();
    const QString keyword = m_searchEdit->text().trimmed();

    QList<Charger> list;
    for (const Charger &c : StationService::instance().allChargers()) {
        if (stationId > 0 && c.stationId != stationId)
            continue;
        if (status >= 0 && c.status != status)
            continue;
        if (!keyword.isEmpty() && !c.code.contains(keyword, Qt::CaseInsensitive))
            continue;
        list.append(c);
    }

    m_table->setRowCount(list.size());
    for (int row = 0; row < list.size(); ++row) {
        const Charger &c = list[row];

        auto *c0 = new QTableWidgetItem(c.code);
        c0->setData(Qt::UserRole, c.id);
        c0->setData(Qt::UserRole + 1, c.status);
        auto *c1 = new QTableWidgetItem(stationNameOf(c.stationId));
        auto *c2 = new QTableWidgetItem(c.type);
        auto *c3 = new QTableWidgetItem(QString::number(c.power, 'f', 0));
        auto *c4 = new QTableWidgetItem(Utils::chargerStatusText(c.status));
        c4->setForeground(Utils::chargerStatusColor(c.status));
        auto *c5 = new QTableWidgetItem(QString::number(c.totalCount));
        auto *c6 = new QTableWidgetItem(QString::number(c.totalMinutes / 60.0, 'f', 1));

        m_table->setItem(row, 0, c0);
        m_table->setItem(row, 1, c1);
        m_table->setItem(row, 2, c2);
        m_table->setItem(row, 3, c3);
        m_table->setItem(row, 4, c4);
        m_table->setItem(row, 5, c5);
        m_table->setItem(row, 6, c6);
    }

    updateActionState();
}

int ChargerPage::selectedChargerId() const
{
    const int row = m_table->currentRow();
    if (row < 0)
        return -1;
    const QTableWidgetItem *item = m_table->item(row, 0);
    return item ? item->data(Qt::UserRole).toInt() : -1;
}

void ChargerPage::updateActionState()
{
    const int row = m_table->currentRow();
    const bool has = row >= 0;
    m_rebootBtn->setEnabled(false);
    m_faultBtn->setEnabled(false);
    m_recoverBtn->setEnabled(false);
    m_deleteBtn->setEnabled(has);
    if (!has)
        return;
    const QTableWidgetItem *item = m_table->item(row, 0);
    const int st = item ? item->data(Qt::UserRole + 1).toInt() : -1;
    if (st == 0)
        m_faultBtn->setEnabled(true);   // 空闲可标记故障
    else if (st == 2)
        m_rebootBtn->setEnabled(true), m_recoverBtn->setEnabled(true);  // 故障可重启/恢复
}

void ChargerPage::onAddCharger()
{
    QDialog dlg(this);
    dlg.setWindowTitle(QStringLiteral("新增电桩"));
    auto *form = new QFormLayout(&dlg);

    auto *stationCombo = new QComboBox(&dlg);
    for (const Station &s : StationService::instance().listStations())
        stationCombo->addItem(s.name, s.id);
    form->addRow(QStringLiteral("所属电站"), stationCombo);

    auto *codeEdit = new QLineEdit(&dlg);
    form->addRow(QStringLiteral("电桩编号"), codeEdit);

    auto *typeCombo = new QComboBox(&dlg);
    typeCombo->addItems({ QStringLiteral("快充"), QStringLiteral("慢充"), QStringLiteral("超充") });
    form->addRow(QStringLiteral("类型"), typeCombo);

    auto *powerEdit = new QLineEdit(&dlg);
    powerEdit->setText(QStringLiteral("120"));
    form->addRow(QStringLiteral("功率(kW)"), powerEdit);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    form->addRow(buttons);

    if (dlg.exec() != QDialog::Accepted)
        return;

    const QString code = codeEdit->text().trimmed();
    bool okPower = false;
    const double power = powerEdit->text().toDouble(&okPower);
    if (code.isEmpty() || !okPower || power <= 0) {
        Toast::show(this, QStringLiteral("请填写有效的编号与功率"));
        return;
    }
    for (const Charger &c : StationService::instance().allChargers()) {
        if (c.code == code) {
            Toast::show(this, QStringLiteral("电桩编号已存在"));
            return;
        }
    }
    StationService::instance().addCharger(stationCombo->currentData().toInt(), code,
                                          typeCombo->currentText(), power);
    rebuildTable();
    Toast::show(this, QStringLiteral("已新增电桩"));
}

void ChargerPage::onReboot()
{
    const int id = selectedChargerId();
    if (id < 0)
        return;
    const Charger c = StationService::instance().chargerById(id);
    if (c.status != 2) {
        Toast::show(this, QStringLiteral("仅故障电桩可远程重启"));
        return;
    }
    QString err;
    if (!StationService::instance().rebootCharger(id, &err)) {
        Toast::show(this, err);
        return;
    }
    rebuildTable();
    m_rebootId = id;  // 重启中(4)期间 1s 轮询,离开重启态后自动刷新并停
    m_autoRefresh->start();
    Toast::show(this, QStringLiteral("已下发远程重启,恢复后自动刷新"));
}

void ChargerPage::onMarkFault()
{
    const int id = selectedChargerId();
    if (id < 0)
        return;
    const Charger c = StationService::instance().chargerById(id);
    if (c.status != 0) {
        Toast::show(this, QStringLiteral("仅空闲电桩可标记故障"));
        return;
    }
    QString err;
    if (!StationService::instance().setChargerStatus(id, 2, &err)) {
        Toast::show(this, err);
        return;
    }
    rebuildTable();
    Toast::show(this, QStringLiteral("已标记故障"));
}

void ChargerPage::onRecover()
{
    const int id = selectedChargerId();
    if (id < 0)
        return;
    const Charger c = StationService::instance().chargerById(id);
    if (c.status != 2) {
        Toast::show(this, QStringLiteral("仅故障/重启中电桩可恢复正常"));
        return;
    }
    QString err;
    if (!StationService::instance().setChargerStatus(id, 0, &err)) {
        Toast::show(this, err);
        return;
    }
    rebuildTable();
    Toast::show(this, QStringLiteral("已恢复正常"));
}

void ChargerPage::onDelete()
{
    const int id = selectedChargerId();
    if (id < 0)
        return;

    const auto ret = QMessageBox::question(this, QStringLiteral("删除电桩"),
                                           QStringLiteral("确定要删除该电桩吗?"),
                                           QMessageBox::Yes | QMessageBox::No);
    if (ret != QMessageBox::Yes)
        return;

    if (!StationService::instance().deleteCharger(id)) {
        Toast::show(this, QStringLiteral("使用中的电桩禁止删除"));
        return;
    }
    rebuildTable();
    Toast::show(this, QStringLiteral("已删除"));
}

void ChargerPage::onAutoPoll()
{
    if (m_rebootId <= 0) {
        m_autoRefresh->stop();
        return;
    }
    const Charger cur = StationService::instance().chargerById(m_rebootId);
    rebuildTable();
    if (cur.status != 4) {
        m_autoRefresh->stop();
        m_rebootId = 0;
        Toast::show(this, QStringLiteral("设备已恢复空闲"));
    }
}

