#pragma once

#include <QList>

#include "Page.h"

class QLabel;
class QLineEdit;
class QListWidget;

// 搜索页:按名称/地址搜索充电站 (UC-U-02)。
class SearchPage : public Page
{
    Q_OBJECT
public:
    explicit SearchPage(QWidget *parent = nullptr);

signals:
    void openStation(int stationId);

private:
    void updateResults(const QString &keyword);

    QLineEdit *m_searchEdit = nullptr;
    QLabel *m_locLabel = nullptr;
    QLabel *m_stateLabel = nullptr;
    QListWidget *m_list = nullptr;
};
