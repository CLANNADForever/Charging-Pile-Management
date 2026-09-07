#pragma once

#include "Page.h"

class QLabel;
class QLineEdit;
class QPushButton;

// 用户中心:个人信息、充值、优惠券/订单入口、退出登录 (UC-U-05)。
class UserCenterPage : public Page
{
    Q_OBJECT
public:
    explicit UserCenterPage(QWidget *parent = nullptr);

    // 刷新界面数据(昵称/余额/头像)
    void refresh();

signals:
    void logoutRequested();
    void openCoupons();
    void openOrders();

private slots:
    void onChangeAvatar();
    void onEditNickname();
    void onRecharge();

private:
    QPushButton *m_avatarBtn = nullptr;
    QPushButton *m_nicknameBtn = nullptr;
    QLabel *m_phoneLabel = nullptr;
    QLabel *m_balanceLabel = nullptr;
    QLineEdit *m_rechargeEdit = nullptr;
};
