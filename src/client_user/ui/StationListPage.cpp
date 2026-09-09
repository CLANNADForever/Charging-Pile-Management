#include "StationListPage.h"

#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QScrollArea>
#include <QVariantAnimation>
#include <QVBoxLayout>

#include <algorithm>
#include <functional>

#include "MapWidget.h"
#include "StationCard.h"
#include "core/service/StationService.h"

namespace {

// 拖拽手柄:始终位于地图与列表之间,拖动改变比例。
class DragHandle : public QWidget
{
public:
    std::function<void(int)> onDrag;
    std::function<void()> onRelease;

    using QWidget::QWidget;

protected:
    void mousePressEvent(QMouseEvent *e) override
    {
        m_lastY = int(e->globalPosition().y());
    }
    void mouseMoveEvent(QMouseEvent *e) override
    {
        const int y = int(e->globalPosition().y());
        const int dy = y - m_lastY;
        if (dy != 0 && onDrag)
            onDrag(dy);
        m_lastY = y;
    }
    void mouseReleaseEvent(QMouseEvent *) override
    {
        if (onRelease)
            onRelease();
    }
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(0xD8, 0xE0, 0xE8));
        p.drawRoundedRect(QRectF(width() / 2.0 - 22.0, height() / 2.0 - 2.0, 44.0, 4.0), 2.0, 2.0);
    }

private:
    int m_lastY = 0;
};

} // namespace

StationListPage::StationListPage(QWidget *parent)
    : Page(parent)
{
    const auto loc = StationService::instance().currentLocation();
    m_userLat = loc.latitude;
    m_userLon = loc.longitude;
    m_allStations = StationService::instance().listStations();
    m_predictedFree = StationService::instance().predictedFreeRatio();

    m_rootLayout = new QVBoxLayout(this);
    m_rootLayout->setContentsMargins(0, 0, 0, 0);
    m_rootLayout->setSpacing(0);

    // 0 搜索栏
    m_rootLayout->addWidget(buildSearchBar());

    // 1 地图
    m_map = new MapWidget(this);
    m_map->setMinimumHeight(0);
    m_map->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Ignored);
    m_rootLayout->addWidget(m_map, 0);
    connect(m_map, &MapWidget::stationClicked, this, [this](int id) { emit openStation(id); });

    // 2 拖拽手柄
    auto *handle = new DragHandle(this);
    handle->setFixedHeight(20);
    handle->setCursor(Qt::SizeVerCursor);
    handle->onDrag = [this](int dy) {
        setMapRatio(m_mapRatio + double(dy) / qMax(height(), 1));
    };
    handle->onRelease = [this]() {
        if (m_mapRatio < 0.29)
            snapTo(0.0);
        else if (m_mapRatio < 0.79)
            snapTo(0.58);
        else
            snapTo(1.0);
    };
    m_rootLayout->addWidget(handle, 0);

    // 3 列表
    m_listWidget = new QWidget(this);
    m_listWidget->setObjectName(QStringLiteral("listPanel"));
    m_listWidget->setMinimumHeight(0);
    m_listWidget->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Ignored);
    m_rootLayout->addWidget(m_listWidget, 0);
    buildListContent();

    applyFilters();
    setMapRatio(0.58);
}

QWidget *StationListPage::buildSearchBar()
{
    auto *bar = new QWidget(this);
    bar->setObjectName(QStringLiteral("searchBar"));
    auto *lay = new QVBoxLayout(bar);
    lay->setContentsMargins(20, 12, 20, 10);
    lay->setSpacing(6);

    auto *loc = new QLabel(QStringLiteral("当前位置 · ")
                               + StationService::instance().currentLocation().label, bar);
    loc->setObjectName(QStringLiteral("hintLabel"));

    auto *searchBtn = new QPushButton(QStringLiteral("搜索充电站"), bar);
    searchBtn->setObjectName(QStringLiteral("searchButton"));
    searchBtn->setCursor(Qt::PointingHandCursor);
    connect(searchBtn, &QPushButton::clicked, this, &StationListPage::openSearch);

    lay->addWidget(loc);
    lay->addWidget(searchBtn);
    return bar;
}

void StationListPage::buildListContent()
{
    auto *lay = new QVBoxLayout(m_listWidget);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(0);

    // 优惠横幅
    auto *banner = new QPushButton(QStringLiteral("新人充电券 · 领券立减"), m_listWidget);
    banner->setObjectName(QStringLiteral("couponBanner"));
    banner->setCursor(Qt::PointingHandCursor);
    banner->setFixedHeight(44);
    connect(banner, &QPushButton::clicked, this, &StationListPage::openCoupons);
    lay->addWidget(banner);

    // 排序 / 距离 / 筛选
    auto *filterRow = new QHBoxLayout;
    filterRow->setContentsMargins(12, 8, 12, 8);
    filterRow->setSpacing(8);

    m_sortCombo = new QComboBox(m_listWidget);
    m_sortCombo->addItems({ QStringLiteral("推荐排序"), QStringLiteral("距离优先"),
                            QStringLiteral("价格优先"), QStringLiteral("低拥堵优先") });
    connect(m_sortCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int) { applyFilters(); });

    m_distanceCombo = new QComboBox(m_listWidget);
    m_distanceCombo->addItem(QStringLiteral("不限"), -1);
    m_distanceCombo->addItem(QStringLiteral("3km 内"), 3);
    m_distanceCombo->addItem(QStringLiteral("5km 内"), 5);
    m_distanceCombo->addItem(QStringLiteral("10km 内"), 10);
    m_distanceCombo->addItem(QStringLiteral("30km 内"), 30);
    connect(m_distanceCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int) {
                m_maxDistanceKm = m_distanceCombo->currentData().toDouble();
                applyFilters();
            });

    m_filterBtn = new QPushButton(QStringLiteral("筛选"), m_listWidget);
    m_filterBtn->setObjectName(QStringLiteral("filterButton"));
    m_filterBtn->setCursor(Qt::PointingHandCursor);
    auto *menu = new QMenu(m_filterBtn);
    const QStringList facilities = { QStringLiteral("卫生间"), QStringLiteral("休息室"),
        QStringLiteral("餐饮"), QStringLiteral("雨棚"), QStringLiteral("便利店"),
        QStringLiteral("自动售货机"), QStringLiteral("饮用水"), QStringLiteral("可洗车"),
        QStringLiteral("有人值守") };
    for (const QString &f : facilities) {
        auto *act = menu->addAction(f);
        act->setCheckable(true);
        connect(act, &QAction::toggled, this, [this, f](bool checked) {
            if (checked)
                m_activeFacilities.append(f);
            else
                m_activeFacilities.removeAll(f);
            applyFilters();
        });
    }
    m_filterBtn->setMenu(menu);

    for (auto *w : { static_cast<QWidget *>(m_sortCombo),
                     static_cast<QWidget *>(m_distanceCombo),
                     static_cast<QWidget *>(m_filterBtn) })
        w->setFixedHeight(36);  // 与筛选按钮风格一致
    filterRow->addWidget(m_sortCombo, 2);
    filterRow->addWidget(m_distanceCombo, 2);
    filterRow->addWidget(m_filterBtn, 1);
    lay->addLayout(filterRow);

    // 卡片列表
    auto *scroll = new QScrollArea(m_listWidget);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    auto *container = new QWidget(scroll);
    m_cardsLayout = new QVBoxLayout(container);
    m_cardsLayout->setContentsMargins(12, 4, 12, 12);
    m_cardsLayout->setSpacing(10);
    scroll->setWidget(container);
    lay->addWidget(scroll, 1);
}

void StationListPage::refresh()
{
    applyFilters();
}

void StationListPage::setMapRatio(qreal ratio)
{
    m_mapRatio = qBound(0.0, ratio, 1.0);

    m_map->setVisible(m_mapRatio > 0.03);
    m_listWidget->setVisible(m_mapRatio < 0.97);

    m_rootLayout->setStretch(1, qRound(m_mapRatio * 1000));
    m_rootLayout->setStretch(3, qRound((1.0 - m_mapRatio) * 1000));
}

void StationListPage::snapTo(qreal target)
{
    auto *anim = new QVariantAnimation(this);
    anim->setDuration(220);
    anim->setStartValue(m_mapRatio);
    anim->setEndValue(target);
    anim->setEasingCurve(QEasingCurve::OutCubic);
    connect(anim, &QVariantAnimation::valueChanged, this, [this](const QVariant &v) {
        setMapRatio(v.toReal());
    });
    connect(anim, &QVariantAnimation::finished, anim, &QObject::deleteLater);
    anim->start();
}

void StationListPage::applyFilters()
{
    m_filteredStations.clear();
    for (const Station &s : m_allStations) {
        const double d = StationService::haversineKm(m_userLat, m_userLon, s.latitude, s.longitude);
        if (m_maxDistanceKm > 0 && d > m_maxDistanceKm)
            continue;

        bool ok = true;
        for (const QString &f : m_activeFacilities) {
            if (!s.facilities.contains(f)) {
                ok = false;
                break;
            }
        }
        if (!ok)
            continue;

        m_filteredStations.append(s);
    }

    const int sortIdx = m_sortCombo->currentIndex();
    if (sortIdx == 2) {
        std::sort(m_filteredStations.begin(), m_filteredStations.end(),
                  [](const Station &a, const Station &b) { return a.unitPrice < b.unitPrice; });
    } else if (sortIdx == 3) {
        const auto freeRatio = [this](const Station &s) {
            return m_predictedFree.value(s.id, -1.0);
        };
        std::sort(m_filteredStations.begin(), m_filteredStations.end(),
                  [this, &freeRatio](const Station &a, const Station &b) {
                      const double ra = freeRatio(a);
                      const double rb = freeRatio(b);
                      if (ra != rb)
                          return ra > rb;
                      return StationService::haversineKm(
                                 m_userLat, m_userLon, a.latitude, a.longitude)
                             < StationService::haversineKm(
                                 m_userLat, m_userLon, b.latitude, b.longitude);
                  });
    } else {
        std::sort(m_filteredStations.begin(), m_filteredStations.end(),
                  [this](const Station &a, const Station &b) {
                      return StationService::haversineKm(m_userLat, m_userLon, a.latitude, a.longitude)
                          < StationService::haversineKm(m_userLat, m_userLon, b.latitude, b.longitude);
                  });
    }

    rebuildList();
    m_map->setData(m_filteredStations, m_userLat, m_userLon);
}

void StationListPage::rebuildList()
{
    while (QLayoutItem *item = m_cardsLayout->takeAt(0)) {
        if (QWidget *w = item->widget())
            w->deleteLater();
        delete item;
    }

    if (m_filteredStations.isEmpty()) {
        auto *empty = new QLabel(QStringLiteral("暂无充电站数据"), m_listWidget);
        empty->setObjectName(QStringLiteral("hintLabel"));
        empty->setAlignment(Qt::AlignCenter);
        m_cardsLayout->addWidget(empty);
        return;
    }

    for (const Station &s : m_filteredStations) {
        const double d = StationService::haversineKm(m_userLat, m_userLon, s.latitude, s.longitude);
        const QList<Charger> chargers = StationService::instance().chargersByStation(s.id);

        int freeCount = 0;
        QStringList types;
        for (const Charger &c : chargers) {
            if (c.status == 0)
                ++freeCount;
            if (!types.contains(c.type))
                types.append(c.type);
        }

        QString tag;
        if (m_sortCombo->currentIndex() == 3) {
            const double ratio = m_predictedFree.value(s.id, -1.0);
            if (ratio >= 0.0)
                tag = QStringLiteral("低拥堵 · 预测空闲 %1%")
                          .arg(qRound(ratio * 100.0));
        }
        auto *card = new StationCard(s, d, types, freeCount, chargers.size(),
                                     m_listWidget, tag);
        connect(card, &StationCard::clicked, this, [this](int id) { emit openStation(id); });
        connect(card, &StationCard::navClicked, this, [this](int id) { emit openNavigation(id); });
        m_cardsLayout->addWidget(card);
    }
    m_cardsLayout->addStretch();
}
