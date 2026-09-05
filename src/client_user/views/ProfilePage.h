#ifndef NCS_CLIENT_VIEWS_PROFILEPAGE_H
#define NCS_CLIENT_VIEWS_PROFILEPAGE_H

#include <QWidget>

#include "entities.h"

class QLabel;

namespace ncs {
namespace client {

// 个人中心：头像 + 信息 + 功能入口(充值/昵称/历史/找桩/我的会话/头像)。
class ProfilePage : public QWidget {
    Q_OBJECT
public:
    explicit ProfilePage(const ncs::User& user, QWidget* parent = nullptr);
    void setUser(const ncs::User& user);
    void setAvatarPixmap(const QPixmap& pm);

signals:
    void goFindStations();
    void goSessions();
    void rechargeRequested();
    void nicknameRequested();
    void historyRequested();
    void avatarRequested();

private:
    QLabel* avatar_ = nullptr;
    QLabel* nickname_ = nullptr;
    QLabel* phone_ = nullptr;
    QLabel* balance_ = nullptr;
};

}  // namespace client
}  // namespace ncs

#endif  // NCS_CLIENT_VIEWS_PROFILEPAGE_H
