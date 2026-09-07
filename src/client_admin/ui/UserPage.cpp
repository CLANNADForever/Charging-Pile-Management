#include "UserPage.h"

#include <QDialog>
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
#include "core/service/ChargeService.h"
#include "core/service/UserService.h"
#include "theme/AdminTheme.h"

UserPage::UserPage(QWidget *parent)
    : AdminPage(parent)
{
    auto *lay = new QVBoxLayout(this);
    lay->setContentsMargins(24, 24, 24, 24);
    lay->setSpacing(16);

    auto *title = new QLabel(QStringLiteral("用户管理"), this);
    title->setObjectName(QStringLiteral("pageTitle"));
    lay->addWidget(title);

    // 工具栏
    auto *tool = new QHBoxLayout;
    tool->setSpacing(10);
    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setPlaceholderText(QStringLiteral("按手机号搜索"));
    m_freezeBtn = new QPushButton(QStringLiteral("冻结 / 解冻"), this);
    m_freezeBtn->setObjectName(QStringLiteral("secondaryButton"));
    m_freezeBtn->setCursor(Qt::PointingHandCursor);
    connect(m_freezeBtn, &QPushButton::clicked, this, &UserPage::onToggleFreeze);

    tool->addWidget(m_searchEdit, 1);
    tool->addWidget(m_freezeBtn);
    lay->addLayout(tool);

    // 用户表格
    m_table = new QTableWidget(0, 6, this);
    m_table->setHorizontalHeaderLabels({
        QStringLiteral("ID"), QStringLiteral("手机号"), QStringLiteral("昵称"),
        QStringLiteral("余额(元)"), QStringLiteral("注册时间"), QStringLiteral("状态") });
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->verticalHeader()->setVisible(false);
    m_table->horizontalHeader()->setStretchLastSection(true);
    lay->addWidget(m_table, 1);

    auto *hint = new QLabel(QStringLiteral("双击用户行查看其订单历史"), this);
    hint->setObjectName(QStringLiteral("hintLabel"));
    lay->addWidget(hint);

    connect(m_searchEdit, &QLineEdit::textChanged, this, [this](const QString &) { rebuildTable(); });
    connect(m_table, &QTableWidget::itemSelectionChanged, this, &UserPage::updateActionState);
    connect(m_table, &QTableWidget::cellDoubleClicked, this, &UserPage::onUserDoubleClicked);

    rebuildTable();
}

void UserPage::refresh()
{
    rebuildTable();
}

void UserPage::rebuildTable()
{
    const QString keyword = m_searchEdit->text().trimmed();
    const QList<User> list = UserService::instance().listUsers(keyword);

    m_table->setRowCount(list.size());
    for (int row = 0; row < list.size(); ++row) {
        const User &u = list[row];

        auto *c0 = new QTableWidgetItem(QString::number(u.id));
        c0->setData(Qt::UserRole, u.phone);
        auto *c1 = new QTableWidgetItem(Utils::maskPhone(u.phone));
        auto *c2 = new QTableWidgetItem(u.nickname);
        auto *c3 = new QTableWidgetItem(Utils::formatMoney(u.balance));
        auto *c4 = new QTableWidgetItem(u.registeredAt);
        auto *c5 = new QTableWidgetItem(u.frozen ? QStringLiteral("冻结") : QStringLiteral("正常"));
        c5->setForeground(u.frozen ? AdminTheme::Amber : AdminTheme::Green);

        m_table->setItem(row, 0, c0);
        m_table->setItem(row, 1, c1);
        m_table->setItem(row, 2, c2);
        m_table->setItem(row, 3, c3);
        m_table->setItem(row, 4, c4);
        m_table->setItem(row, 5, c5);
    }
    updateActionState();
}

QString UserPage::selectedUserPhone() const
{
    const int row = m_table->currentRow();
    if (row < 0)
        return QString();
    const QTableWidgetItem *item = m_table->item(row, 0);
    return item ? item->data(Qt::UserRole).toString() : QString();
}

void UserPage::updateActionState()
{
    m_freezeBtn->setEnabled(!selectedUserPhone().isEmpty());
}

void UserPage::onToggleFreeze()
{
    const QString phone = selectedUserPhone();
    if (phone.isEmpty())
        return;

    User target;
    bool found = false;
    for (const User &u : UserService::instance().listUsers()) {
        if (u.phone == phone) {
            target = u;
            found = true;
            break;
        }
    }
    if (!found)
        return;

    const bool freeze = !target.frozen;
    const QString action = freeze ? QStringLiteral("冻结") : QStringLiteral("解冻");

    const auto ret = QMessageBox::question(this, action + QStringLiteral("用户"),
                                           QStringLiteral("确定要%1该用户吗?").arg(action),
                                           QMessageBox::Yes | QMessageBox::No);
    if (ret != QMessageBox::Yes)
        return;

    if (freeze) {
        const auto orders = ChargeService::instance().listOrdersByPhone(phone);
        bool active = false;
        for (const Order &o : orders) {
            if (o.status == 0 || o.status == 1) {
                active = true;
                break;
            }
        }
        if (active) {
            const auto ret2 = QMessageBox::question(
                this, QStringLiteral("提示"),
                QStringLiteral("该用户有正在进行的充电,冻结后仍会保留订单,是否继续?"),
                QMessageBox::Yes | QMessageBox::No);
            if (ret2 != QMessageBox::Yes)
                return;
        }
    }

    UserService::instance().setUserStatus(phone, freeze);
    rebuildTable();
    Toast::show(this, QStringLiteral("已%1").arg(action));
}

void UserPage::onUserDoubleClicked(int row, int)
{
    const QTableWidgetItem *item = m_table->item(row, 0);
    if (!item)
        return;
    const QString phone = item->data(Qt::UserRole).toString();
    const auto orders = ChargeService::instance().listOrdersByPhone(phone);

    QDialog dlg(this);
    dlg.setWindowTitle(QStringLiteral("订单历史"));
    dlg.resize(620, 380);
    auto *lay = new QVBoxLayout(&dlg);
    auto *label = new QLabel(QStringLiteral("用户 %1 的订单").arg(Utils::maskPhone(phone)), &dlg);
    label->setObjectName(QStringLiteral("cardTitle"));
    lay->addWidget(label);

    auto *table = new QTableWidget(orders.size(), 5, &dlg);
    table->setHorizontalHeaderLabels({
        QStringLiteral("订单号"), QStringLiteral("电站"), QStringLiteral("金额"),
        QStringLiteral("状态"), QStringLiteral("开始时间") });
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->verticalHeader()->setVisible(false);
    table->horizontalHeader()->setStretchLastSection(true);
    for (int i = 0; i < orders.size(); ++i) {
        const Order &o = orders[i];
        table->setItem(i, 0, new QTableWidgetItem(o.orderNo));
        table->setItem(i, 1, new QTableWidgetItem(o.stationName));
        table->setItem(i, 2, new QTableWidgetItem(Utils::formatMoney(o.amount)));
        auto *s = new QTableWidgetItem(Utils::orderStatusText(o.status));
        s->setForeground(Utils::orderStatusColor(o.status));
        table->setItem(i, 3, s);
        table->setItem(i, 4, new QTableWidgetItem(o.startTime));
    }
    lay->addWidget(table);
    dlg.exec();
}
