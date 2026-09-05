#include "HistoryPage.h"

#include <QLabel>
#include <QListWidget>
#include <QListWidgetItem>
#include <QPushButton>
#include <QVBoxLayout>

#include "services/IChargeService.h"
#include "money.h"

namespace ncs {
namespace client {

namespace { constexpr int kPage = 10; }

HistoryPage::HistoryPage(IChargeService* service, QWidget* parent)
    : QWidget(parent), service_(service) {
    setObjectName(QStringLiteral("historyPage"));
    auto* layout = new QVBoxLayout(this);
    auto* heading = new QLabel(QStringLiteral("历史订单(已支付)"), this);
    heading->setAlignment(Qt::AlignCenter);
    list_ = new QListWidget(this);
    list_->setObjectName(QStringLiteral("historyList"));
    status_ = new QLabel(this);
    status_->setObjectName(QStringLiteral("historyStatus"));
    status_->setWordWrap(true);
    auto* prev = new QPushButton(QStringLiteral("上一页"), this);
    auto* next = new QPushButton(QStringLiteral("下一页"), this);
    auto* back = new QPushButton(QStringLiteral("返回"), this);
    connect(prev, &QPushButton::clicked, this, [this] {
        if (offset_ >= kPage) { offset_ -= kPage; load(); }
    });
    connect(next, &QPushButton::clicked, this, [this] {
        if (offset_ + kPage < total_) { offset_ += kPage; load(); }
    });
    connect(back, &QPushButton::clicked, this, &HistoryPage::backRequested);
    layout->addWidget(heading);
    layout->addWidget(list_, 1);
    layout->addWidget(status_);
    layout->addWidget(prev);
    layout->addWidget(next);
    layout->addWidget(back);
}

void HistoryPage::load() {
    list_->clear();
    if (phone_.isEmpty() || !service_) { status_->setText(QStringLiteral("无数据")); return; }
    status_->setText(QStringLiteral("加载中…"));
    service_->listHistory(phone_, kPage, offset_,
                          [this](const HistoryResult& r) {
                              list_->clear();
                              if (!r.ok) { status_->setText(r.message); return; }
                              total_ = r.total;
                              status_->setText(
                                  QStringLiteral("共 %1 条(第 %2 页)")
                                      .arg(total_)
                                      .arg(offset_ / kPage + 1));
                              for (const auto& o : r.items) {
                                  const QString line =
                                      QStringLiteral("订单#%1 · 桩#%2 · %3 kWh · 实付 %4 元")
                                          .arg(o.id)
                                          .arg(o.deviceId)
                                          .arg(o.energyKwh, 0, 'f', 2)
                                          .arg(format_cents(o.amountCents));
                                  list_->addItem(new QListWidgetItem(line, list_));
                              }
                          });
}

}  // namespace client
}  // namespace ncs
