#ifndef NCS_CLIENT_VIEWS_MYSESSIONSPAGE_H
#define NCS_CLIENT_VIEWS_MYSESSIONSPAGE_H

#include <QVector>
#include <QWidget>

#include "entities.h"

class QListWidget;
class QLabel;

namespace ncs {
namespace client {

class IChargeService;

// "我的充电会话"：列出活跃订单(预约/充电中/待支付)，点击恢复任意一个。
class MySessionsPage : public QWidget {
    Q_OBJECT
public:
    explicit MySessionsPage(IChargeService* service, QWidget* parent = nullptr);

    void setPhone(const QString& phone);
    void refresh();

signals:
    void sessionChosen(const ncs::Order& order);
    void backRequested();

private:
    IChargeService* service_ = nullptr;
    QString phone_;
    QVector<ncs::Order> orders_;
    QListWidget* list_ = nullptr;
    QLabel* status_ = nullptr;
};

}  // namespace client
}  // namespace ncs

#endif  // NCS_CLIENT_VIEWS_MYSESSIONSPAGE_H
