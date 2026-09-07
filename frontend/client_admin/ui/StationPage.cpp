#include "StationPage.h"

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
#include <QVBoxLayout>

#include "common/Toast.h"
#include "common/Utils.h"
#include "core/service/StationService.h"

StationPage::StationPage(QWidget *parent)
    : AdminPage(parent)
{
    auto *lay = new QVBoxLayout(this);
    lay->setContentsMargins(24, 24, 24, 24);
    lay->setSpacing(16);

    auto *title = new QLabel(QStringLiteral("充电站管理"), this);
    title->setObjectName(QStringLiteral("pageTitle"));
    lay->addWidget(title);

    // 工具栏
    auto *tool = new QHBoxLayout;
    tool->setSpacing(10);
    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setPlaceholderText(QStringLiteral("按站名搜索"));

    auto *addBtn = new QPushButton(QStringLiteral("新增电站"), this);
    m_editBtn = new QPushButton(QStringLiteral("修改电站"), this);
    m_deleteBtn = new QPushButton(QStringLiteral("删除电站"), this);
    addBtn->setObjectName(QStringLiteral("secondaryButton"));
    m_editBtn->setObjectName(QStringLiteral("secondaryButton"));
    m_deleteBtn->setObjectName(QStringLiteral("dangerButton"));
    for (auto *b : { addBtn, m_editBtn, m_deleteBtn })
        b->setCursor(Qt::PointingHandCursor);

    connect(addBtn, &QPushButton::clicked, this, &StationPage::onAdd);
    connect(m_editBtn, &QPushButton::clicked, this, &StationPage::onEdit);
    connect(m_deleteBtn, &QPushButton::clicked, this, &StationPage::onDelete);

    tool->addWidget(m_searchEdit, 1);
    tool->addWidget(addBtn);
    tool->addWidget(m_editBtn);
    tool->addWidget(m_deleteBtn);
    lay->addLayout(tool);

    // 电站表格
    m_stationTable = new QTableWidget(0, 8, this);
    m_stationTable->setHorizontalHeaderLabels({
        QStringLiteral("ID"), QStringLiteral("站名"), QStringLiteral("地址"),
        QStringLiteral("经度"), QStringLiteral("纬度"), QStringLiteral("单价"),
        QStringLiteral("总桩数"), QStringLiteral("在线率") });
    m_stationTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_stationTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_stationTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_stationTable->verticalHeader()->setVisible(false);
    m_stationTable->horizontalHeader()->setStretchLastSection(true);
    lay->addWidget(m_stationTable, 3);

    // 电桩明细
    auto *detailTitle = new QLabel(QStringLiteral("电桩明细"), this);
    detailTitle->setObjectName(QStringLiteral("cardTitle"));
    lay->addWidget(detailTitle);

    m_chargerTable = new QTableWidget(0, 4, this);
    m_chargerTable->setHorizontalHeaderLabels({
        QStringLiteral("编号"), QStringLiteral("类型"), QStringLiteral("功率(kW)"),
        QStringLiteral("状态") });
    m_chargerTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_chargerTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_chargerTable->verticalHeader()->setVisible(false);
    m_chargerTable->horizontalHeader()->setStretchLastSection(true);
    lay->addWidget(m_chargerTable, 2);

    connect(m_searchEdit, &QLineEdit::textChanged, this, [this](const QString &) { rebuildTable(); });
    connect(m_stationTable, &QTableWidget::itemSelectionChanged, this, &StationPage::onStationSelected);

    rebuildTable();
}

void StationPage::refresh()
{
    rebuildTable();
}

void StationPage::rebuildTable()
{
    const QString keyword = m_searchEdit->text().trimmed();

    QList<Station> list;
    for (const Station &s : StationService::instance().listStations()) {
        if (!keyword.isEmpty() && !s.name.contains(keyword, Qt::CaseInsensitive))
            continue;
        list.append(s);
    }

    m_stationTable->setRowCount(list.size());
    for (int row = 0; row < list.size(); ++row) {
        const Station &s = list[row];
        const auto chargers = StationService::instance().chargersByStation(s.id);
        int total = chargers.size();
        int fault = 0;
        for (const Charger &c : chargers)
            if (c.status == 2)
                ++fault;
        const double onlineRate = total > 0 ? double(total - fault) / double(total) * 100.0 : 0.0;

        auto *c0 = new QTableWidgetItem(QString::number(s.id));
        c0->setData(Qt::UserRole, s.id);
        auto *c1 = new QTableWidgetItem(s.name);
        auto *c2 = new QTableWidgetItem(s.address);
        auto *c3 = new QTableWidgetItem(QString::number(s.longitude, 'f', 4));
        auto *c4 = new QTableWidgetItem(QString::number(s.latitude, 'f', 4));
        auto *c5 = new QTableWidgetItem(Utils::formatMoney(s.unitPrice));
        auto *c6 = new QTableWidgetItem(QString::number(total));
        auto *c7 = new QTableWidgetItem(QString::number(onlineRate, 'f', 1) + QStringLiteral("%"));

        m_stationTable->setItem(row, 0, c0);
        m_stationTable->setItem(row, 1, c1);
        m_stationTable->setItem(row, 2, c2);
        m_stationTable->setItem(row, 3, c3);
        m_stationTable->setItem(row, 4, c4);
        m_stationTable->setItem(row, 5, c5);
        m_stationTable->setItem(row, 6, c6);
        m_stationTable->setItem(row, 7, c7);
    }

    m_chargerTable->setRowCount(0);
    updateActionState();
}

int StationPage::selectedStationId() const
{
    const int row = m_stationTable->currentRow();
    if (row < 0)
        return -1;
    const QTableWidgetItem *item = m_stationTable->item(row, 0);
    return item ? item->data(Qt::UserRole).toInt() : -1;
}

void StationPage::updateActionState()
{
    const bool has = selectedStationId() >= 0;
    m_editBtn->setEnabled(has);
    m_deleteBtn->setEnabled(has);
}

void StationPage::onStationSelected()
{
    const int id = selectedStationId();
    const auto chargers = id < 0 ? QList<Charger>() : StationService::instance().chargersByStation(id);

    m_chargerTable->setRowCount(chargers.size());
    for (int row = 0; row < chargers.size(); ++row) {
        const Charger &c = chargers[row];
        auto *c0 = new QTableWidgetItem(c.code);
        auto *c1 = new QTableWidgetItem(c.type);
        auto *c2 = new QTableWidgetItem(QString::number(c.power, 'f', 0));
        auto *c3 = new QTableWidgetItem(Utils::chargerStatusText(c.status));
        c3->setForeground(Utils::chargerStatusColor(c.status));
        m_chargerTable->setItem(row, 0, c0);
        m_chargerTable->setItem(row, 1, c1);
        m_chargerTable->setItem(row, 2, c2);
        m_chargerTable->setItem(row, 3, c3);
    }
    updateActionState();
}

void StationPage::onAdd()
{
    QDialog dlg(this);
    dlg.setWindowTitle(QStringLiteral("新增电站"));
    auto *form = new QFormLayout(&dlg);

    auto *nameEdit = new QLineEdit(&dlg);
    auto *addrEdit = new QLineEdit(&dlg);
    auto *latEdit = new QLineEdit(&dlg);
    latEdit->setText(QStringLiteral("30.2800"));
    auto *lonEdit = new QLineEdit(&dlg);
    lonEdit->setText(QStringLiteral("120.1500"));
    auto *priceEdit = new QLineEdit(&dlg);
    priceEdit->setText(QStringLiteral("1.20"));
    auto *countEdit = new QLineEdit(&dlg);
    countEdit->setText(QStringLiteral("6"));
    auto *powerEdit = new QLineEdit(&dlg);
    powerEdit->setText(QStringLiteral("120"));

    form->addRow(QStringLiteral("站名"), nameEdit);
    form->addRow(QStringLiteral("地址"), addrEdit);
    form->addRow(QStringLiteral("纬度"), latEdit);
    form->addRow(QStringLiteral("经度"), lonEdit);
    form->addRow(QStringLiteral("单价(元/度)"), priceEdit);
    form->addRow(QStringLiteral("初始电桩数"), countEdit);
    form->addRow(QStringLiteral("默认功率(kW)"), powerEdit);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    form->addRow(buttons);

    if (dlg.exec() != QDialog::Accepted)
        return;

    const QString name = nameEdit->text().trimmed();
    const QString addr = addrEdit->text().trimmed();
    bool okLat = false, okLon = false, okPrice = false, okCount = false, okPower = false;
    const double lat = latEdit->text().toDouble(&okLat);
    const double lon = lonEdit->text().toDouble(&okLon);
    const double price = priceEdit->text().toDouble(&okPrice);
    const int count = countEdit->text().toInt(&okCount);
    const double power = powerEdit->text().toDouble(&okPower);

    if (name.isEmpty() || addr.isEmpty()) {
        Toast::show(this, QStringLiteral("站名与地址不能为空"));
        return;
    }
    if (!okLat || lat < -90.0 || lat > 90.0 || !okLon || lon < -180.0 || lon > 180.0) {
        Toast::show(this, QStringLiteral("经纬度超出合法范围"));
        return;
    }
    if (!okPrice || price <= 0 || !okCount || count <= 0 || !okPower || power <= 0) {
        Toast::show(this, QStringLiteral("请填写有效的单价/桩数/功率"));
        return;
    }

    StationService::instance().addStation(name, addr, lat, lon, price, count, power);
    rebuildTable();
    Toast::show(this, QStringLiteral("已新增电站并批量建桩"));
}

void StationPage::onEdit()
{
    const int id = selectedStationId();
    if (id < 0)
        return;
    const Station s = StationService::instance().stationDetail(id);

    QDialog dlg(this);
    dlg.setWindowTitle(QStringLiteral("修改电站"));
    auto *form = new QFormLayout(&dlg);

    auto *nameEdit = new QLineEdit(s.name, &dlg);
    auto *addrEdit = new QLineEdit(s.address, &dlg);
    auto *latEdit = new QLineEdit(QString::number(s.latitude, 'f', 4), &dlg);
    auto *lonEdit = new QLineEdit(QString::number(s.longitude, 'f', 4), &dlg);
    auto *priceEdit = new QLineEdit(QString::number(s.unitPrice, 'f', 2), &dlg);

    form->addRow(QStringLiteral("站名"), nameEdit);
    form->addRow(QStringLiteral("地址"), addrEdit);
    form->addRow(QStringLiteral("纬度"), latEdit);
    form->addRow(QStringLiteral("经度"), lonEdit);
    form->addRow(QStringLiteral("单价(元/度)"), priceEdit);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    form->addRow(buttons);

    if (dlg.exec() != QDialog::Accepted)
        return;

    bool okLat = false, okLon = false, okPrice = false;
    const double lat = latEdit->text().toDouble(&okLat);
    const double lon = lonEdit->text().toDouble(&okLon);
    const double price = priceEdit->text().toDouble(&okPrice);

    if (nameEdit->text().trimmed().isEmpty() || addrEdit->text().trimmed().isEmpty()) {
        Toast::show(this, QStringLiteral("站名与地址不能为空"));
        return;
    }
    if (!okLat || lat < -90.0 || lat > 90.0 || !okLon || lon < -180.0 || lon > 180.0) {
        Toast::show(this, QStringLiteral("经纬度超出合法范围"));
        return;
    }
    if (!okPrice || price <= 0) {
        Toast::show(this, QStringLiteral("请填写有效的单价"));
        return;
    }

    StationService::instance().updateStation(id, nameEdit->text().trimmed(),
                                             addrEdit->text().trimmed(), lat, lon, price);
    rebuildTable();
    Toast::show(this, QStringLiteral("已保存修改"));
}

void StationPage::onDelete()
{
    const int id = selectedStationId();
    if (id < 0)
        return;

    const auto ret = QMessageBox::question(this, QStringLiteral("删除电站"),
                                           QStringLiteral("确定要删除该电站吗?"),
                                           QMessageBox::Yes | QMessageBox::No);
    if (ret != QMessageBox::Yes)
        return;

    if (!StationService::instance().deleteStation(id)) {
        Toast::show(this, QStringLiteral("该电站下仍有电桩,禁止删除"));
        return;
    }
    rebuildTable();
    Toast::show(this, QStringLiteral("已删除电站"));
}
