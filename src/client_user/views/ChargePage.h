#ifndef NCS_CLIENT_VIEWS_CHARGEPAGE_H
#define NCS_CLIENT_VIEWS_CHARGEPAGE_H

#include <QWidget>

#include "entities.h"
#include "money.h"

class QLabel;
class QPushButton;
class QTimer;

namespace ncs {
namespace client {

class IChargeService;

// 充电会话页：可新开(预约→开始→结束→支付)，也可从"我的充电会话"恢复任意活跃订单。
class ChargePage : public QWidget {
    Q_OBJECT
public:
    explicit ChargePage(IChargeService* service, QWidget* parent = nullptr);

    void startSession(const QString& phone, int deviceId);
    void resumeSession(const QString& phone, const ncs::Order& order);

signals:
    void backRequested();

private slots:
    void onReserve();
    void onStart();
    void onFinish();
    void onPay();
    void onCancel();
    void onBack();
    void onPoll();

private:
    enum Phase { PIdle, PReserved, PCharging, PBill, PPaid };
    void setPhase(Phase p);
    void refreshUi();
    void beginPolling();
    void applyOrder(const ncs::Order& o);

    IChargeService* service_ = nullptr;
    QString phone_;
    ncs::Order cur_;   // 当前订单快照(id/device/unitPrice/amount 等)
    Phase phase_ = PIdle;

    QLabel* status_ = nullptr;
    QLabel* live_ = nullptr;
    QPushButton* btnReserve_ = nullptr;
    QPushButton* btnStart_ = nullptr;
    QPushButton* btnCancel_ = nullptr;
    QPushButton* btnFinish_ = nullptr;
    QPushButton* btnPay_ = nullptr;
    QPushButton* btnBack_ = nullptr;
    QTimer* timer_ = nullptr;
};

}  // namespace client
}  // namespace ncs

#endif  // NCS_CLIENT_VIEWS_CHARGEPAGE_H
