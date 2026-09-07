#pragma once

#include "AdminPage.h"

class QLineEdit;
class QPushButton;
class QTableWidget;

// 用户管理页(UC-A-07)。
class UserPage : public AdminPage
{
    Q_OBJECT
public:
    explicit UserPage(QWidget *parent = nullptr);
    void refresh() override;

private:
    void rebuildTable();
    void onToggleFreeze();
    void onUserDoubleClicked(int row, int column);
    QString selectedUserPhone() const;
    void updateActionState();

    QLineEdit *m_searchEdit = nullptr;
    QTableWidget *m_table = nullptr;
    QPushButton *m_freezeBtn = nullptr;
};
