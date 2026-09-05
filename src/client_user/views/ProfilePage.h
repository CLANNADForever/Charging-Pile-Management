#ifndef NCS_CLIENT_VIEWS_PROFILEPAGE_H
#define NCS_CLIENT_VIEWS_PROFILEPAGE_H

#include <QWidget>

#include "entities.h"

class QLabel;

namespace ncs {
namespace client {

// 个人中心占位页：展示登录用户信息 + 找桩入口。钱包/订单后续 change 再加。
class ProfilePage : public QWidget {
    Q_OBJECT
public:
    explicit ProfilePage(const ncs::User& user, QWidget* parent = nullptr);
    void setUser(const ncs::User& user);

signals:
    void goFindStations();
    void goSessions();

private:
    QLabel* nickname_ = nullptr;
    QLabel* phone_ = nullptr;
    QLabel* balance_ = nullptr;
};

}  // namespace client
}  // namespace ncs

#endif  // NCS_CLIENT_VIEWS_PROFILEPAGE_H
