// NCS 管理端(B2)：登录(token/角色) → 用户风控 / 资产管理 / 设备监控 / 经营概览 / 审计/运维日志。
// 角色: super 全功能; operator 只读+远程重启; viewer 只读。
#include <QApplication>
#include <QAbstractItemView>
#include <QCheckBox>
#include <QComboBox>
#include <QDateTime>
#include <QGridLayout>
#include <QHash>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMainWindow>
#include <QMessageBox>
#include <QPushButton>
#include <QStackedWidget>
#include <QTableWidget>
#include <QTabWidget>
#include <QTimer>
#include <QVBoxLayout>

#include <functional>

#include "AdminApi.h"
#include "entities.h"
#include "money.h"

namespace ncs {
namespace admin {

bool canFreeze(const QString& role) { return role == QLatin1String("super"); }
bool canWriteAssets(const QString& role) { return role == QLatin1String("super"); }
bool canRestart(const QString& role) {
    return role == QLatin1String("super") || role == QLatin1String("operator");
}
QString roleText(const QString& role) {
    if (role == QLatin1String("super"))
        return QStringLiteral("超级管理员");
    if (role == QLatin1String("operator"))
        return QStringLiteral("运维人员");
    return QStringLiteral("观察员");
}

QString deviceStateText(int s) {
    switch (s) {
        case 0: return QStringLiteral("空闲");
        case 1: return QStringLiteral("充电中");
        case 2: return QStringLiteral("故障");
        case 3: return QStringLiteral("预约");
        case 4: return QStringLiteral("重启中");
    }
    return QStringLiteral("?");
}
QString deviceTypeText(int t) {
    return t == 0 ? QStringLiteral("快充") : QStringLiteral("慢充");
}
QString opActionText(const QString& a) {
    static const QHash<QString, QString> m = {
        {QStringLiteral("login"), QStringLiteral("登录")},
        {QStringLiteral("user.freeze"), QStringLiteral("用户冻结/解冻")},
        {QStringLiteral("station.create"), QStringLiteral("建站")},
        {QStringLiteral("station.update"), QStringLiteral("改站")},
        {QStringLiteral("station.delete"), QStringLiteral("删站")},
        {QStringLiteral("device.create"), QStringLiteral("批量建桩")},
        {QStringLiteral("device.delete"), QStringLiteral("删桩")},
        {QStringLiteral("device.restart"), QStringLiteral("远程重启")},
    };
    return m.value(a, a);
}
QString opTypeText(const QString& t) {
    if (t == QLatin1String("fault"))
        return QStringLiteral("故障");
    if (t == QLatin1String("restart"))
        return QStringLiteral("重启");
    if (t == QLatin1String("recover"))
        return QStringLiteral("恢复");
    return t;
}
QString fmtMs(qint64 ms) {
    if (ms <= 0)
        return QStringLiteral("-");
    return QDateTime::fromMSecsSinceEpoch(ms).toLocalTime()
        .toString(QStringLiteral("HH:mm:ss"));
}

// ---------- 登录页 ----------
class LoginPage : public QWidget {
    Q_OBJECT
public:
    explicit LoginPage(AdminApi* api, QWidget* parent = nullptr)
        : QWidget(parent), api_(api) {
        auto* lay = new QVBoxLayout(this);
        auto* u = new QLineEdit(this);
        u->setPlaceholderText(QStringLiteral("管理员账号"));
        auto* p = new QLineEdit(this);
        p->setPlaceholderText(QStringLiteral("密码"));
        p->setEchoMode(QLineEdit::Password);
        auto* btn = new QPushButton(QStringLiteral("登录"), this);
        status_ = new QLabel(this);
        lay->addStretch();
        lay->addWidget(new QLabel(QStringLiteral("NCS 管理端登录"), this));
        lay->addWidget(u);
        lay->addWidget(p);
        lay->addWidget(btn);
        lay->addWidget(status_);
        lay->addWidget(new QLabel(
            QStringLiteral("演示账号：admin/admin123(super) · operator/operator123(运维) · viewer/viewer123(观察)"),
            this));
        lay->addStretch();
        connect(btn, &QPushButton::clicked, this, [this, u, p] {
            status_->setText(QStringLiteral("登录中…"));
            api_->login(u->text().trimmed(), p->text(),
                        [this](const LoginInfo& info) {
                            if (!info.ok) {
                                status_->setText(QStringLiteral("失败：") + info.err);
                                return;
                            }
                            emit loggedIn(info.role);
                        });
        });
    }
signals:
    void loggedIn(const QString& role);
private:
    AdminApi* api_;
    QLabel* status_;
};

// ---------- 用户风控页 ----------
class UsersPage : public QWidget {
    Q_OBJECT
public:
    explicit UsersPage(AdminApi* api, bool allowFreeze, QWidget* parent = nullptr)
        : QWidget(parent), api_(api) {
        auto* lay = new QVBoxLayout(this);
        auto* row = new QHBoxLayout;
        search_ = new QLineEdit(this);
        search_->setPlaceholderText(QStringLiteral("手机号(可留空)"));
        statusCombo_ = new QComboBox(this);
        statusCombo_->addItems({QStringLiteral("全部"), QStringLiteral("正常"),
                                QStringLiteral("冻结")});
        auto* go = new QPushButton(QStringLiteral("搜索"), this);
        row->addWidget(search_, 1);
        row->addWidget(statusCombo_);
        row->addWidget(go);
        table_ = new QTableWidget(0, 5, this);
        table_->setHorizontalHeaderLabels({QStringLiteral("ID"), QStringLiteral("手机号"),
                                           QStringLiteral("昵称"), QStringLiteral("余额"),
                                           QStringLiteral("状态")});
        table_->horizontalHeader()->setStretchLastSection(true);
        auto* br = new QPushButton(QStringLiteral("刷新"), this);
        status_ = new QLabel(this);
        lay->addLayout(row);
        lay->addWidget(table_, 1);
        auto* brow = new QHBoxLayout;
        if (allowFreeze) {
            auto* bf = new QPushButton(QStringLiteral("冻结"), this);
            auto* bu = new QPushButton(QStringLiteral("解冻"), this);
            brow->addWidget(bf);
            brow->addWidget(bu);
            connect(bf, &QPushButton::clicked, this, [this] { freeze(true); });
            connect(bu, &QPushButton::clicked, this, [this] { freeze(false); });
        }
        brow->addWidget(br);
        lay->addLayout(brow);
        lay->addWidget(status_);
        connect(go, &QPushButton::clicked, this, &UsersPage::refresh);
        connect(br, &QPushButton::clicked, this, &UsersPage::refresh);
        connect(statusCombo_, &QComboBox::currentIndexChanged, this,
                &UsersPage::refresh);
        refresh();
    }
public slots:
    void refresh() {
        api_->listUsers(search_->text().trimmed(), statusCombo_->currentIndex(),
                        [this](const QVector<User>& users, const QString& err) {
                            table_->setRowCount(0);
                            if (!err.isEmpty()) {
                                status_->setText(err);
                                return;
                            }
                            status_->setText(QStringLiteral("%1 个用户").arg(users.size()));
                            for (const auto& u : users) {
                                const int r = table_->rowCount();
                                table_->insertRow(r);
                                auto set = [&](int c, const QString& t) {
                                    table_->setItem(r, c, new QTableWidgetItem(t));
                                };
                                set(0, QString::number(u.id));
                                set(1, u.phone);
                                set(2, u.nickname);
                                set(3, format_cents(u.balanceCents));
                                set(4, u.status == UserStatus::Frozen
                                           ? QStringLiteral("冻结")
                                           : QStringLiteral("正常"));
                            }
                        });
    }
private:
    void freeze(bool frozen) {
        const int r = table_->currentRow();
        if (r < 0) {
            QMessageBox::warning(this, QStringLiteral("风控"), QStringLiteral("请先选一行"));
            return;
        }
        const int id = table_->item(r, 0)->text().toInt();
        api_->setFrozen(id, frozen, [this](const QString& err) {
            if (!err.isEmpty())
                QMessageBox::warning(this, QStringLiteral("风控"), err);
            refresh();
        });
    }
    AdminApi* api_;
    QLineEdit* search_;
    QComboBox* statusCombo_;
    QTableWidget* table_;
    QLabel* status_;
};

// ---------- 资产管理页(站带在线率；写操作仅 super) ----------
class AssetsPage : public QWidget {
    Q_OBJECT
public:
    explicit AssetsPage(AdminApi* api, bool allowWrite, QWidget* parent = nullptr)
        : QWidget(parent), api_(api) {
        auto* root = new QVBoxLayout(this);
        auto* hl = new QHBoxLayout;
        auto* left = new QVBoxLayout;
        auto* qr = new QHBoxLayout;
        qFilter_ = new QLineEdit(this);
        qFilter_->setPlaceholderText(QStringLiteral("站名/地址关键字"));
        auto* rSt = new QPushButton(QStringLiteral("查站"), this);
        qr->addWidget(qFilter_, 1);
        qr->addWidget(rSt);
        stations_ = new QListWidget(this);
        stations_->setObjectName(QStringLiteral("adminStationList"));
        left->addWidget(new QLabel(QStringLiteral("充电站(点站看桩)"), this));
        left->addLayout(qr);
        left->addWidget(stations_, 1);
        if (allowWrite) {
            name_ = new QLineEdit(this);
            name_->setPlaceholderText(QStringLiteral("站名"));
            addr_ = new QLineEdit(this);
            addr_->setPlaceholderText(QStringLiteral("地址"));
            lat_ = new QLineEdit(this);
            lat_->setText(QStringLiteral("39.90"));
            lng_ = new QLineEdit(this);
            lng_->setText(QStringLiteral("116.30"));
            price_ = new QLineEdit(this);
            price_->setPlaceholderText(QStringLiteral("单价(分/度)"));
            auto* mk = new QPushButton(QStringLiteral("新建站"), this);
            auto* dSt = new QPushButton(QStringLiteral("删除站"), this);
            auto* lr = new QHBoxLayout;
            lr->addWidget(lat_);
            lr->addWidget(lng_);
            left->addWidget(name_);
            left->addWidget(addr_);
            left->addLayout(lr);
            left->addWidget(price_);
            left->addWidget(mk);
            left->addWidget(dSt);
            connect(mk, &QPushButton::clicked, this, [this] {
                api_->createStation(name_->text().trimmed(), addr_->text().trimmed(),
                                    lat_->text().toDouble(), lng_->text().toDouble(),
                                    static_cast<MoneyCents>(price_->text().toLongLong()),
                                    [this](int id, const QString& err) {
                                        status_->setText(id > 0
                                                             ? QStringLiteral("已建站 id=%1").arg(id)
                                                             : QStringLiteral("建站失败：") + err);
                                        loadStations();
                                    });
            });
            connect(dSt, &QPushButton::clicked, this, [this] {
                const int id = curStationId();
                if (id <= 0)
                    return;
                api_->deleteStation(id, [this](const QString& err) {
                    if (!err.isEmpty())
                        QMessageBox::warning(this, QStringLiteral("删站"), err);
                    loadStations();
                    loadDevices();
                });
            });
        } else {
            statusHint_ = new QLabel(QStringLiteral("只读角色：不可增删改站点/电桩"), this);
            left->addWidget(statusHint_);
        }
        auto* right = new QVBoxLayout;
        devices_ = new QListWidget(this);
        devices_->setObjectName(QStringLiteral("adminDeviceList"));
        right->addWidget(new QLabel(
            QStringLiteral("站内电桩(编号·类型·状态·在线·功率·次数/时长)"), this));
        right->addWidget(devices_, 1);
        auto* dr = new QHBoxLayout;
        auto* dD = new QPushButton(QStringLiteral("删除桩"), this);
        dr->addWidget(dD);
        if (allowWrite) {
            count_ = new QLineEdit(this);
            count_->setText(QStringLiteral("1"));
            type_ = new QLineEdit(this);
            type_->setText(QStringLiteral("0"));
            power_ = new QLineEdit(this);
            power_->setText(QStringLiteral("120"));
            auto* mkD = new QPushButton(QStringLiteral("批量建桩"), this);
            dr->addWidget(new QLabel(QStringLiteral("数/型/功率"), this));
            dr->addWidget(count_);
            dr->addWidget(type_);
            dr->addWidget(power_);
            dr->addWidget(mkD);
            connect(mkD, &QPushButton::clicked, this, [this] {
                const int id = curStationId();
                if (id <= 0)
                    return;
                api_->createDevices(id, count_->text().toInt(),
                                    type_->text().toInt(), power_->text().toDouble(),
                                    [this](const QString& err) {
                                        if (!err.isEmpty())
                                            QMessageBox::warning(this, QStringLiteral("建桩"), err);
                                        loadDevices();
                                    });
            });
            connect(dD, &QPushButton::clicked, this, [this] {
                const auto* item = devices_->currentItem();
                if (!item)
                    return;
                api_->deleteDevice(item->data(Qt::UserRole).toInt(),
                                   [this](const QString& err) {
                                       if (!err.isEmpty())
                                           QMessageBox::warning(this, QStringLiteral("删桩"), err);
                                       loadDevices();
                                   });
            });
        } else {
            dD->setVisible(false);
        }
        right->addLayout(dr);
        status_ = new QLabel(this);
        hl->addLayout(left, 1);
        hl->addLayout(right, 1);
        root->addLayout(hl, 1);
        root->addWidget(status_);
        connect(rSt, &QPushButton::clicked, this, &AssetsPage::loadStations);
        connect(stations_, &QListWidget::currentRowChanged, this,
                [this](int) { loadDevices(); });
        loadStations();
    }
public slots:
    void loadStations() {
        api_->listStationsOnline(qFilter_->text().trimmed(),
                                 [this](const QVector<AdminStation>& st, const QString& err) {
                                     stations_->clear();
                                     if (!err.isEmpty()) {
                                         status_->setText(err);
                                         return;
                                     }
                                     status_->setText(QStringLiteral("%1 个站").arg(st.size()));
                                     for (const auto& s : st) {
                                         auto* it = new QListWidgetItem(
                                             QStringLiteral("%1 · 空闲 %2/%3 · 在线 %4/%5 · %6 元/度")
                                                 .arg(s.name)
                                                 .arg(s.freePiles)
                                                 .arg(s.totalPiles)
                                                 .arg(s.online)
                                                 .arg(s.totalPiles)
                                                 .arg(format_cents(s.priceCents)),
                                             stations_);
                                         it->setData(Qt::UserRole, s.id);
                                     }
                                 });
    }
    void loadDevices() {
        devices_->clear();
        const int id = curStationId();
        if (id <= 0)
            return;
        api_->listDevicesFiltered(QString(), id, -1, -1, 1, 100,
                                  [this](const QVector<AdminDevice>& devs, qint64, const QString& err) {
                                      if (!err.isEmpty()) {
                                          status_->setText(err);
                                          return;
                                      }
                                      for (const auto& d : devs) {
                                          auto* it = new QListWidgetItem(
                                              QStringLiteral("桩 #%1 · %2 · %3 · %4 · %5kW · 次%6/%7h")
                                                  .arg(d.id)
                                                  .arg(deviceTypeText(d.type))
                                                  .arg(deviceStateText(d.state))
                                                  .arg(d.online ? QStringLiteral("在线") : QStringLiteral("离线"))
                                                  .arg(d.powerKw)
                                                  .arg(d.sessions)
                                                  .arg(d.chargeSec / 3600.0, 0, 'f', 1),
                                              devices_);
                                          it->setData(Qt::UserRole, d.id);
                                      }
                                  });
    }
private:
    int curStationId() const {
        const auto* it = stations_->currentItem();
        return it ? it->data(Qt::UserRole).toInt() : 0;
    }
    AdminApi* api_;
    QLineEdit* qFilter_;
    QListWidget* stations_;
    QListWidget* devices_;
    QLineEdit *name_ = nullptr, *addr_ = nullptr, *lat_ = nullptr, *lng_ = nullptr;
    QLineEdit *price_ = nullptr, *count_ = nullptr, *type_ = nullptr, *power_ = nullptr;
    QLabel* status_ = nullptr;
    QLabel* statusHint_ = nullptr;
};

// ---------- 设备监控页(复杂筛选 + 实时刷新 + 远程重启) ----------
class MonitorPage : public QWidget {
    Q_OBJECT
public:
    explicit MonitorPage(AdminApi* api, bool allowRestart, QWidget* parent = nullptr)
        : QWidget(parent), api_(api), allowRestart_(allowRestart) {
        auto* lay = new QVBoxLayout(this);
        auto* frow = new QHBoxLayout;
        station_ = new QComboBox(this);
        station_->addItem(QStringLiteral("全部站"), -1);
        type_ = new QComboBox(this);
        type_->addItems({QStringLiteral("全部类型"), QStringLiteral("快充"),
                         QStringLiteral("慢充")});
        state_ = new QComboBox(this);
        state_->addItems({QStringLiteral("全部状态"), QStringLiteral("空闲"),
                          QStringLiteral("充电中"), QStringLiteral("故障"),
                          QStringLiteral("预约"), QStringLiteral("重启中")});
        kw_ = new QLineEdit(this);
        kw_->setPlaceholderText(QStringLiteral("桩号"));
        auto* go = new QPushButton(QStringLiteral("查询"), this);
        auto* autoChk = new QCheckBox(QStringLiteral("自动刷新"), this);
        autoChk->setChecked(true);
        frow->addWidget(station_, 1);
        frow->addWidget(type_);
        frow->addWidget(state_);
        frow->addWidget(kw_);
        frow->addWidget(go);
        frow->addWidget(autoChk);
        lay->addLayout(frow);
        table_ = new QTableWidget(0, 11, this);
        table_->setHorizontalHeaderLabels(
            {QStringLiteral("桩号"), QStringLiteral("所属站"), QStringLiteral("类型"),
             QStringLiteral("状态"), QStringLiteral("在线"), QStringLiteral("功率kW"),
             QStringLiteral("电量kWh"), QStringLiteral("累计次数"),
             QStringLiteral("累计时长"), QStringLiteral("最近心跳"),
             QStringLiteral("ID")});
        table_->horizontalHeader()->setStretchLastSection(true);
        table_->setColumnHidden(10, true);
        table_->setSelectionBehavior(QAbstractItemView::SelectRows);
        lay->addWidget(table_, 1);
        auto* brow = new QHBoxLayout;
        auto* rBtn = new QPushButton(QStringLiteral("远程重启选中(仅故障)"), this);
        auto* rfr = new QPushButton(QStringLiteral("刷新"), this);
        status_ = new QLabel(this);
        brow->addWidget(rBtn);
        brow->addWidget(rfr);
        brow->addStretch(1);
        brow->addWidget(status_);
        lay->addLayout(brow);
        if (!allowRestart_) {
            rBtn->setEnabled(false);
            rBtn->setToolTip(QStringLiteral("当前角色无远程重启权限"));
        }
        connect(go, &QPushButton::clicked, this, &MonitorPage::refresh);
        connect(rfr, &QPushButton::clicked, this, &MonitorPage::refresh);
        connect(rBtn, &QPushButton::clicked, this, [this, rBtn] {
            if (!allowRestart_) {
                QMessageBox::warning(this, QStringLiteral("重启"), QStringLiteral("无权限"));
                return;
            }
            const int r = table_->currentRow();
            if (r < 0) {
                QMessageBox::warning(this, QStringLiteral("重启"), QStringLiteral("请先选中一行"));
                return;
            }
            const int state = table_->item(r, 3)->data(Qt::UserRole).toInt();
            if (state != 2) {
                QMessageBox::warning(this, QStringLiteral("重启"), QStringLiteral("仅故障桩可远程重启"));
                return;
            }
            const int id = table_->item(r, 0)->text().toInt();
            const QString dev = table_->item(r, 1)->text();
            const auto ret = QMessageBox::question(
                this, QStringLiteral("远程重启"),
                QStringLiteral("确定对桩 #%1(%2) 下发远程重启吗？将进入重启中并自动恢复。")
                    .arg(id)
                    .arg(dev));
            if (ret != QMessageBox::Yes)
                return;
            api_->restartDevice(id, [this, rBtn](const QString& err) {
                if (!err.isEmpty())
                    QMessageBox::warning(this, QStringLiteral("远程重启"), err);
                status_->setText(err.isEmpty() ? QStringLiteral("已下发重启，等待自动恢复…")
                                               : err);
                refresh();
            });
        });
        timer_ = new QTimer(this);
        timer_->setInterval(3000);
        connect(timer_, &QTimer::timeout, this, &MonitorPage::refresh);
        connect(autoChk, &QCheckBox::toggled, this, [this](bool on) {
            if (on)
                timer_->start();
            else
                timer_->stop();
        });
        timer_->start();
        loadStations();
        refresh();
    }
public slots:
    void refresh() {
        const QString q = kw_->text().trimmed();
        const int st = station_->currentData().toInt();
        const int type = type_->currentIndex() - 1;   // 0 全部 => -1
        const int state = state_->currentIndex() - 1; // 0 全部 => -1
        api_->listDevicesFiltered(q, st, type, state, 1, 200,
                                  [this](const QVector<AdminDevice>& devs, qint64 total,
                                         const QString& err) {
                                      table_->setRowCount(0);
                                      if (!err.isEmpty()) {
                                          status_->setText(err);
                                          return;
                                      }
                                      status_->setText(QStringLiteral("共 %1 台").arg(total));
                                      for (const auto& d : devs) {
                                          const int r = table_->rowCount();
                                          table_->insertRow(r);
                                          auto set = [&](int c, const QString& t, int role = 0) {
                                              auto* it = new QTableWidgetItem(t);
                                              it->setData(Qt::UserRole, role);
                                              table_->setItem(r, c, it);
                                          };
                                          set(0, QString::number(d.id));
                                          set(1, d.stationName);
                                          set(2, deviceTypeText(d.type));
                                          set(3, deviceStateText(d.state), d.state);
                                          set(4, d.online ? QStringLiteral("在线") : QStringLiteral("离线"));
                                          set(5, QString::number(d.powerKw, 'f', 1));
                                          set(6, QString::number(d.energyKwh, 'f', 2));
                                          set(7, QString::number(d.sessions));
                                          set(8, QString::number(d.chargeSec / 3600.0, 'f', 1));
                                          set(9, fmtMs(d.lastTs));
                                          set(10, QString::number(d.id));
                                      }
                                  });
    }
private:
    void loadStations() {
        api_->listStationsOnline(QString(),
                                 [this](const QVector<AdminStation>& st, const QString& err) {
                                     if (!err.isEmpty())
                                         return;
                                     const int cur = station_->currentData().toInt();
                                     station_->clear();
                                     station_->addItem(QStringLiteral("全部站"), -1);
                                     for (const auto& s : st)
                                         station_->addItem(s.name, s.id);
                                     const int ix = station_->findData(cur);
                                     if (ix >= 0)
                                         station_->setCurrentIndex(ix);
                                 });
    }
    AdminApi* api_;
    bool allowRestart_;
    QComboBox *station_, *type_, *state_;
    QLineEdit* kw_;
    QTableWidget* table_;
    QLabel* status_;
    QTimer* timer_;
};

// ---------- 日志/审计通用分页列表(模板, 无 Q_OBJECT) ----------
template <typename RowT>
class LogListPage : public QWidget {
public:
    using LoadFn = std::function<void(
        int page, int pageSize,
        std::function<void(const QVector<RowT>&, qint64, const QString&)>)>;
    LogListPage(const QStringList& headers, std::function<QStringList(const RowT&)> render,
                LoadFn load, QWidget* parent = nullptr)
        : QWidget(parent), render_(std::move(render)), load_(std::move(load)) {
        auto* lay = new QVBoxLayout(this);
        table_ = new QTableWidget(0, headers.size(), this);
        table_->setHorizontalHeaderLabels(headers);
        table_->horizontalHeader()->setStretchLastSection(true);
        lay->addWidget(table_, 1);
        auto* row = new QHBoxLayout;
        auto* prev = new QPushButton(QStringLiteral("上一页"), this);
        auto* next = new QPushButton(QStringLiteral("下一页"), this);
        page_ = new QLabel(this);
        row->addWidget(prev);
        row->addWidget(next);
        row->addStretch(1);
        row->addWidget(page_);
        lay->addLayout(row);
        connect(prev, &QPushButton::clicked, this, [this] {
            if (cur_ > 1) {
                --cur_;
                refresh();
            }
        });
        connect(next, &QPushButton::clicked, this, [this] {
            if (cur_ < pages_) {
                ++cur_;
                refresh();
            }
        });
        refresh();
    }
    void refresh() {
        load_(cur_, kPageSize, [this](const QVector<RowT>& rows, qint64 total,
                                     const QString& err) {
            table_->setRowCount(0);
            if (!err.isEmpty()) {
                page_->setText(err);
                return;
            }
            total_ = total;
            pages_ = static_cast<int>((total_ + kPageSize - 1) / kPageSize);
            page_->setText(QStringLiteral("第 %1/%2 页 · 共 %3 条").arg(cur_).arg(pages_).arg(total_));
            for (const auto& r : rows) {
                const auto cols = render_(r);
                const int rowIdx = table_->rowCount();
                table_->insertRow(rowIdx);
                for (int c = 0; c < cols.size(); ++c)
                    table_->setItem(rowIdx, c, new QTableWidgetItem(cols.at(c)));
            }
        });
    }
private:
    static constexpr int kPageSize = 50;
    int cur_ = 1;
    int pages_ = 1;
    qint64 total_ = 0;
    std::function<QStringList(const RowT&)> render_;
    LoadFn load_;
    QTableWidget* table_;
    QLabel* page_;
};

// ---------- 经营概览页(字段由后端算好，UI 仅展示) ----------
class StatsPage : public QWidget {
    Q_OBJECT
public:
    explicit StatsPage(AdminApi* api, QWidget* parent = nullptr)
        : QWidget(parent), api_(api) {
        auto* lay = new QVBoxLayout(this);
        auto* gr = new QGridLayout;
        int c = 0;
        for (const char* t : {"今日", "本月", "累计"}) {
            auto* lbl = new QLabel(QStringLiteral("%1").arg(QString::fromUtf8(t)), this);
            lbl->setStyleSheet(QStringLiteral("font-weight:bold;"));
            auto* val = new QLabel(QStringLiteral("-"), this);
            gr->addWidget(lbl, 0, c);
            gr->addWidget(val, 1, c);
            cards_.push_back(val);
            ++c;
        }
        lay->addLayout(gr);
        health_ = new QLabel(QStringLiteral("设备健康度：-"), this);
        lay->addWidget(health_);
        lay->addWidget(new QLabel(QStringLiteral("各站当前在线率："), this));
        stTable_ = new QTableWidget(0, 5, this);
        stTable_->setHorizontalHeaderLabels({QStringLiteral("站"), QStringLiteral("桩总数"),
                                             QStringLiteral("在线"), QStringLiteral("离线"),
                                             QStringLiteral("在线率")});
        lay->addWidget(stTable_, 3);
        lay->addWidget(new QLabel(QStringLiteral("近7日营收/订单/电量："), this));
        daily_ = new QTableWidget(0, 4, this);
        daily_->setHorizontalHeaderLabels({QStringLiteral("日期"), QStringLiteral("营收(元)"),
                                           QStringLiteral("订单"), QStringLiteral("电量kWh")});
        lay->addWidget(daily_, 2);
        auto* rfr = new QPushButton(QStringLiteral("刷新"), this);
        lay->addWidget(rfr);
        connect(rfr, &QPushButton::clicked, this, &StatsPage::refresh);
        refresh();
    }
public slots:
    void refresh() {
        api_->statsOverview([this](const QJsonObject& o, const QString& err) {
            if (!err.isEmpty())
                return;
            const auto money = [](const QJsonObject& sub) {
                return format_cents(static_cast<MoneyCents>(
                    sub.value(QStringLiteral("revenue_cents")).toDouble()));
            };
            cards_[0]->setText(money(o.value(QStringLiteral("today")).toObject()));
            cards_[1]->setText(money(o.value(QStringLiteral("month")).toObject()));
            cards_[2]->setText(money(o.value(QStringLiteral("total")).toObject()));
            const QJsonObject h = o.value(QStringLiteral("device_health")).toObject();
            health_->setText(
                QStringLiteral("设备健康度：空闲 %1 · 充电 %2 · 故障 %3 · 预约 %4 · 重启中 %5")
                    .arg(h.value(QStringLiteral("idle")).toInt())
                    .arg(h.value(QStringLiteral("charging")).toInt())
                    .arg(h.value(QStringLiteral("fault")).toInt())
                    .arg(h.value(QStringLiteral("reserved")).toInt())
                    .arg(h.value(QStringLiteral("rebooting")).toInt()));
            stTable_->setRowCount(0);
            for (const auto& v : o.value(QStringLiteral("stations")).toArray()) {
                const QJsonObject s = v.toObject();
                const int r = stTable_->rowCount();
                stTable_->insertRow(r);
                const auto set = [&](int c2, const QString& t) {
                    stTable_->setItem(r, c2, new QTableWidgetItem(t));
                };
                set(0, s.value(QStringLiteral("name")).toString());
                set(1, QString::number(s.value(QStringLiteral("total_piles")).toInt()));
                set(2, QString::number(s.value(QStringLiteral("online")).toInt()));
                set(3, QString::number(s.value(QStringLiteral("offline")).toInt()));
                set(4, QStringLiteral("%1%")
                           .arg(s.value(QStringLiteral("online_rate")).toInt()));
            }
        });
        api_->statsDaily(7, [this](const QJsonObject& o, const QString& err) {
            if (!err.isEmpty())
                return;
            daily_->setRowCount(0);
            for (const auto& v : o.value(QStringLiteral("items")).toArray()) {
                const QJsonObject d = v.toObject();
                const int r = daily_->rowCount();
                daily_->insertRow(r);
                const auto set = [&](int c2, const QString& t) {
                    daily_->setItem(r, c2, new QTableWidgetItem(t));
                };
                set(0, d.value(QStringLiteral("day")).toString());
                set(1, format_cents(static_cast<MoneyCents>(
                          d.value(QStringLiteral("revenue_cents")).toDouble())));
                set(2, QString::number(static_cast<qint64>(
                          d.value(QStringLiteral("orders")).toDouble())));
                set(3, QString::number(d.value(QStringLiteral("energy_kwh")).toDouble(), 'f', 2));
            }
        });
    }
private:
    AdminApi* api_;
    QVector<QLabel*> cards_;
    QLabel* health_;
    QTableWidget *stTable_, *daily_;
};

}  // namespace admin
}  // namespace ncs

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    ncs::admin::AdminApi api;
    QMainWindow win;
    win.resize(1320, 840);
    win.setWindowTitle(QStringLiteral("NCS 管理端"));

    auto* stack = new QStackedWidget(&win);
    auto* login = new ncs::admin::LoginPage(&api);
    stack->addWidget(login);
    win.setCentralWidget(stack);
    QObject::connect(login, &ncs::admin::LoginPage::loggedIn, &win,
                     [&win, &api, stack, login](const QString& role) {
                         const bool freeze = ncs::admin::canFreeze(role);
                         const bool write = ncs::admin::canWriteAssets(role);
                         const bool restart = ncs::admin::canRestart(role);
                         auto* tabs = new QTabWidget;
                         tabs->addTab(new ncs::admin::UsersPage(&api, freeze),
                                      QStringLiteral("用户风控"));
                         tabs->addTab(new ncs::admin::AssetsPage(&api, write),
                                      QStringLiteral("资产管理"));
                         tabs->addTab(new ncs::admin::MonitorPage(&api, restart),
                                      QStringLiteral("设备监控"));
                         tabs->addTab(new ncs::admin::StatsPage(&api),
                                      QStringLiteral("经营概览"));
                         tabs->addTab(
                             new ncs::admin::LogListPage<ncs::admin::OpLogRow>(
                                 {QStringLiteral("时间"), QStringLiteral("桩"), QStringLiteral("操作"),
                                  QStringLiteral("执行人"), QStringLiteral("明细")},
                                 [](const ncs::admin::OpLogRow& r) {
                                     return QStringList{
                                         r.at.toLocalTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")),
                                         QStringLiteral("#%1").arg(r.deviceId),
                                         ncs::admin::opTypeText(r.opType), r.opBy, r.detail};
                                 },
                                 [&api](int page, int size,
                                        std::function<void(const QVector<ncs::admin::OpLogRow>&, qint64,
                                                           const QString&)> cb) {
                                     api.listOps(page, size, cb);
                                 }),
                             QStringLiteral("运维日志"));
                         tabs->addTab(
                             new ncs::admin::LogListPage<ncs::admin::AuditLogRow>(
                                 {QStringLiteral("时间"), QStringLiteral("账号"), QStringLiteral("动作"),
                                  QStringLiteral("明细"), QStringLiteral("结果")},
                                 [](const ncs::admin::AuditLogRow& r) {
                                     return QStringList{
                                         r.at.toLocalTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")),
                                         r.username, ncs::admin::opActionText(r.action), r.detail,
                                         r.result == QLatin1String("ok") ? QStringLiteral("成功")
                                                                          : QStringLiteral("失败")};
                                 },
                                 [&api](int page, int size,
                                        std::function<void(const QVector<ncs::admin::AuditLogRow>&, qint64,
                                                           const QString&)> cb) {
                                     api.listAuditLogs(page, size, cb);
                                 }),
                             QStringLiteral("审计日志"));
                         win.setWindowTitle(QStringLiteral("NCS 管理端 · %1")
                                                .arg(ncs::admin::roleText(role)));
                         stack->insertWidget(stack->indexOf(login) + 1, tabs);
                         stack->setCurrentWidget(tabs);
                     });
    win.show();
    return app.exec();
}
#include "main.moc"
