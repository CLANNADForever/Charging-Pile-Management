#pragma once

#include "AdminPage.h"

class QLineEdit;
class QPushButton;
class QTableWidget;

// 充电站管理页(UC-A-06)。
class StationPage : public AdminPage
{
    Q_OBJECT
public:
    explicit StationPage(QWidget *parent = nullptr);
    void refresh() override;

private:
    void rebuildTable();
    void onAdd();
    void onEdit();
    void onDelete();
    void onStationSelected();
    int selectedStationId() const;
    void updateActionState();

    QLineEdit *m_searchEdit = nullptr;
    QTableWidget *m_stationTable = nullptr;
    QTableWidget *m_chargerTable = nullptr;
    QPushButton *m_editBtn = nullptr;
    QPushButton *m_deleteBtn = nullptr;
};
