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

// 充电交互页：预约→开始→(轮询 live 显示电量/金额)→结算/取消。
class ChargePage : public QWidget {
    Q_OBJECT
public:
    explicit ChargePage(IChargeService* service, QWidget* parent = nullptr);

    void startSession(const QString& phone, int deviceId);

signals:
    void backRequested();

private slots:
    void onReserve();
    void onStart();
    void onFinish();
    void onCancel();
    void onBack();
    void onPoll();

private:
    enum Phase { Idle, Reserved, Charging, Done };
    void setPhase(Phase p);
    void pollNow();

    IChargeService* service_ = nullptr;
    QString phone_;
    int deviceId_ = 0;
    int orderId_ = 0;
    MoneyCents unitPrice_ = 0;
    Phase phase_ = Idle;

    QLabel* status_ = nullptr;
    QLabel* live_ = nullptr;
    QPushButton* btnReserve_ = nullptr;
    QPushButton* btnStart_ = nullptr;
    QPushButton* btnCancel_ = nullptr;
    QPushButton* btnFinish_ = nullptr;
    QPushButton* btnBack_ = nullptr;
    QTimer* timer_ = nullptr;
};

}  // namespace client
}  // namespace ncs

#endif  // NCS_CLIENT_VIEWS_CHARGEPAGE_H
