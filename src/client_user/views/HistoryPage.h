#ifndef NCS_CLIENT_VIEWS_HISTORYPAGE_H
#define NCS_CLIENT_VIEWS_HISTORYPAGE_H

#include <QWidget>

class QListWidget;
class QLabel;
class QPushButton;

namespace ncs {
namespace client {

class IChargeService;

// 历史订单(已支付)列表，可翻页。
class HistoryPage : public QWidget {
    Q_OBJECT
public:
    explicit HistoryPage(IChargeService* service, QWidget* parent = nullptr);
    void setPhone(const QString& phone) { phone_ = phone; }
    void load();

signals:
    void backRequested();

private:
    IChargeService* service_ = nullptr;
    QString phone_;
    int offset_ = 0;
    qint64 total_ = 0;
    QListWidget* list_ = nullptr;
    QLabel* status_ = nullptr;
};

}  // namespace client
}  // namespace ncs

#endif  // NCS_CLIENT_VIEWS_HISTORYPAGE_H
