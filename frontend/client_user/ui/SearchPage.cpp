#include "SearchPage.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QVBoxLayout>

#include "common/Utils.h"
#include "core/service/StationService.h"

SearchPage::SearchPage(QWidget *parent)
    : Page(parent)
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    root->addWidget(makeHeader(QStringLiteral("搜索")));

    auto *body = new QVBoxLayout;
    body->setContentsMargins(16, 12, 16, 12);
    body->setSpacing(12);

    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setPlaceholderText(QStringLiteral("输入电站名称或地址"));
    m_searchEdit->setClearButtonEnabled(true);
    connect(m_searchEdit, &QLineEdit::textChanged, this, &SearchPage::updateResults);
    body->addWidget(m_searchEdit);

    m_locLabel = new QLabel(QStringLiteral("当前位置 · ")
                                + StationService::instance().currentLocation().label, this);
    m_locLabel->setObjectName(QStringLiteral("hintLabel"));
    body->addWidget(m_locLabel);

    m_stateLabel = new QLabel(QStringLiteral("热门推荐"), this);
    m_stateLabel->setObjectName(QStringLiteral("sectionTitle"));
    body->addWidget(m_stateLabel);

    m_list = new QListWidget(this);
    m_list->setFrameShape(QFrame::NoFrame);
    m_list->setSpacing(8);
    connect(m_list, &QListWidget::itemClicked, this, [this](QListWidgetItem *item) {
        emit openStation(item->data(Qt::UserRole).toInt());
    });
    body->addWidget(m_list, 1);

    root->addLayout(body, 1);

    updateResults(QString());
}

void SearchPage::updateResults(const QString &keyword)
{
    m_list->clear();
    const QString kw = keyword.trimmed();
    m_stateLabel->setText(kw.isEmpty() ? QStringLiteral("热门推荐") : QStringLiteral("搜索结果"));

    const auto loc = StationService::instance().currentLocation();
    for (const Station &s : StationService::instance().listStations()) {
        const bool match = s.name.contains(kw, Qt::CaseInsensitive)
            || s.address.contains(kw, Qt::CaseInsensitive);
        if (!match)
            continue;

        const double d = StationService::haversineKm(loc.latitude, loc.longitude,
                                                     s.latitude, s.longitude);
        auto *item = new QListWidgetItem(m_list);
        item->setText(QStringLiteral("%1\n%2 · ¥%3/度 · %4km")
                          .arg(s.name, s.address,
                               Utils::formatMoney(s.unitPrice),
                               QString::number(d, 'f', 1)));
        item->setData(Qt::UserRole, s.id);
        item->setSizeHint(QSize(0, 52));
        m_list->addItem(item);
    }

    if (m_list->count() == 0) {
        auto *empty = new QListWidgetItem(QStringLiteral("未找到相关电站"), m_list);
        empty->setFlags(Qt::NoItemFlags);
    }
}
