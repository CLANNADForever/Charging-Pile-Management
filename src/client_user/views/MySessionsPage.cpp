#include "MySessionsPage.h"

#include <QLabel>
#include <QListWidget>
#include <QListWidgetItem>
#include <QPushButton>
#include <QVBoxLayout>

#include "services/IChargeService.h"
#include "money.h"

namespace ncs {
namespace client {

namespace {
QString statusText(ncs::OrderStatus st) {
    switch (st) {
        case ncs::OrderStatus::Reserved: return QStringLiteral("预约中");
        case ncs::OrderStatus::Charging: return QStringLiteral("充电中");
        case ncs::OrderStatus::Completed: return QStringLiteral("待支付");
        case ncs::OrderStatus::Paid: return QStringLiteral("已支付");
        case ncs::OrderStatus::Canceled: return QStringLiteral("已取消");
    }
    return QStringLiteral("未知");
}
}  // namespace

MySessionsPage::MySessionsPage(IChargeService* service, QWidget* parent)
    : QWidget(parent), service_(service) {
    setObjectName(QStringLiteral("mySessionsPage"));
    auto* layout = new QVBoxLayout(this);
    auto* heading = new QLabel(QStringLiteral("我的充电会话"), this);
    heading->setObjectName(QStringLiteral("sessionsHeading"));
    heading->setAlignment(Qt::AlignCenter);

    list_ = new QListWidget(this);
    list_->setObjectName(QStringLiteral("sessionList"));

    auto* refreshBtn = new QPushButton(QStringLiteral("刷新"), this);
    refreshBtn->setObjectName(QStringLiteral("btnSessionsRefresh"));
    auto* backBtn = new QPushButton(QStringLiteral("返回"), this);
    backBtn->setObjectName(QStringLiteral("btnSessionsBack"));

    status_ = new QLabel(this);
    status_->setObjectName(QStringLiteral("sessionsStatus"));
    status_->setWordWrap(true);

    layout->addWidget(heading);
    layout->addWidget(list_, 1);
    layout->addWidget(refreshBtn);
    layout->addWidget(backBtn);
    layout->addWidget(status_);

    connect(refreshBtn, &QPushButton::clicked, this, &MySessionsPage::refresh);
    connect(backBtn, &QPushButton::clicked, this, &MySessionsPage::backRequested);
    connect(list_, &QListWidget::itemClicked, this,
            [this](QListWidgetItem* item) {
                const int id = item->data(Qt::UserRole).toInt();
                for (const auto& o : orders_) {
                    if (o.id == id) {
                        emit sessionChosen(o);
                        return;
                    }
                }
            });
}

void MySessionsPage::setPhone(const QString& phone) {
    phone_ = phone;
}

void MySessionsPage::refresh() {
    list_->clear();
    orders_.clear();
    if (phone_.isEmpty() || !service_) {
        status_->setText(QStringLiteral("未登录或无服务"));
        return;
    }
    status_->setText(QStringLiteral("加载中…"));
    service_->listActive(phone_,
                         [this](const QVector<ncs::Order>& orders, const QString& err) {
                             list_->clear();
                             if (!err.isEmpty()) {
                                 status_->setText(QStringLiteral("加载失败：") + err);
                                 return;
                             }
                             orders_ = orders;
                             if (orders_.isEmpty()) {
                                 status_->setText(QStringLiteral("暂无活跃会话"));
                                 return;
                             }
                             status_->setText(
                                 QStringLiteral("%1 个活跃会话(点击恢复)").arg(orders_.size()));
                             for (const auto& o : orders_) {
                                 const QString line =
                                     QStringLiteral("订单 #%1 · 桩 #%2 · %3 · %4 元")
                                         .arg(o.id)
                                         .arg(o.deviceId)
                                         .arg(statusText(o.status))
                                         .arg(o.status == ncs::OrderStatus::Charging
                                                  ? QStringLiteral("实时")
                                                  : format_cents(o.amountCents));
                                 auto* item = new QListWidgetItem(line, list_);
                                 item->setData(Qt::UserRole, o.id);
                                 list_->addItem(item);
                             }
                         });
}

}  // namespace client
}  // namespace ncs
