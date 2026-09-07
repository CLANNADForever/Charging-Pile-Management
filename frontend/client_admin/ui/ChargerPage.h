#pragma once

#include "AdminPage.h"

class QComboBox;
class QLineEdit;
class QPushButton;
class QTableWidget;

// 充电桩管理页(UC-A-05)。
class ChargerPage : public AdminPage
{
    Q_OBJECT
public:
    explicit ChargerPage(QWidget *parent = nullptr);
    void refresh() override;

private:
    void rebuildTable();
    void onAddCharger();
    void onReboot();
    void onMarkFault();
    void onRecover();
    void onDelete();
    int selectedChargerId() const;
    void updateActionState();

    QComboBox *m_stationFilter = nullptr;
    QComboBox *m_statusFilter = nullptr;
    QLineEdit *m_searchEdit = nullptr;
    QTableWidget *m_table = nullptr;
    QPushButton *m_rebootBtn = nullptr;
    QPushButton *m_faultBtn = nullptr;
    QPushButton *m_recoverBtn = nullptr;
    QPushButton *m_deleteBtn = nullptr;
};
