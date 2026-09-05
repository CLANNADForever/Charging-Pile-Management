// NCS 管理端(B1)：登录 → 用户风控 / 资产管理(PC 宽屏)。
#include <QApplication>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QHeaderView>
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
#include <QVBoxLayout>

#include "AdminApi.h"
#include "entities.h"
#include "money.h"

namespace ncs {
namespace admin {

// ---------- 登录页 ----------
class LoginPage : public QWidget {
    Q_OBJECT
public:
    explicit LoginPage(AdminApi* api, QWidget* parent = nullptr)
        : QWidget(parent), api_(api) {
        auto* lay = new QVBoxLayout(this);
        auto* u = new QLineEdit(this); u->setPlaceholderText(QStringLiteral("管理员账号"));
        auto* p = new QLineEdit(this); p->setPlaceholderText(QStringLiteral("密码"));
        p->setEchoMode(QLineEdit::Password);
        auto* btn = new QPushButton(QStringLiteral("登录"), this);
        status_ = new QLabel(this);
        lay->addStretch();
        lay->addWidget(new QLabel(QStringLiteral("NCS 管理端登录"), this));
        lay->addWidget(u); lay->addWidget(p); lay->addWidget(btn); lay->addWidget(status_);
        lay->addStretch();
        connect(btn, &QPushButton::clicked, this, [this, u, p] {
            status_->setText(QStringLiteral("登录中…"));
            api_->login(u->text().trimmed(), p->text(),
                        [this](bool ok, const QString& err) {
                            if (!ok) {
                                status_->setText(QStringLiteral("失败：") + err);
                                return;
                            }
                            emit loggedIn();
                        });
        });
    }
signals:
    void loggedIn();
private:
    AdminApi* api_;
    QLabel* status_;
};

// ---------- 用户风控页 ----------
class UsersPage : public QWidget {
    Q_OBJECT
public:
    explicit UsersPage(AdminApi* api, QWidget* parent = nullptr)
        : QWidget(parent), api_(api) {
        auto* lay = new QVBoxLayout(this);
        auto* row = new QHBoxLayout;
        search_ = new QLineEdit(this);
        search_->setPlaceholderText(QStringLiteral("手机号(可留空)"));
        auto* go = new QPushButton(QStringLiteral("搜索"), this);
        row->addWidget(search_, 1); row->addWidget(go);
        table_ = new QTableWidget(0, 5, this);
        table_->setHorizontalHeaderLabels({QStringLiteral("ID"), QStringLiteral("手机号"),
                                           QStringLiteral("昵称"), QStringLiteral("余额"),
                                           QStringLiteral("状态")});
        table_->horizontalHeader()->setStretchLastSection(true);
        auto* bf = new QPushButton(QStringLiteral("冻结"), this);
        auto* bu = new QPushButton(QStringLiteral("解冻"), this);
        auto* br = new QPushButton(QStringLiteral("刷新"), this);
        status_ = new QLabel(this);
        lay->addLayout(row); lay->addWidget(table_, 1);
        auto* brow = new QHBoxLayout; brow->addWidget(bf); brow->addWidget(bu); brow->addWidget(br);
        lay->addLayout(brow); lay->addWidget(status_);
        connect(go, &QPushButton::clicked, this, &UsersPage::refresh);
        connect(br, &QPushButton::clicked, this, &UsersPage::refresh);
        connect(bf, &QPushButton::clicked, this, [this] { freeze(true); });
        connect(bu, &QPushButton::clicked, this, [this] { freeze(false); });
        refresh();
    }
public slots:
    void refresh() {
        api_->listUsers(search_->text().trimmed(),
                        [this](const QVector<User>& users, const QString& err) {
                            table_->setRowCount(0);
                            if (!err.isEmpty()) { status_->setText(err); return; }
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
        if (r < 0) { QMessageBox::warning(this, QStringLiteral("风控"), QStringLiteral("请先选一行")); return; }
        const int id = table_->item(r, 0)->text().toInt();
        api_->setFrozen(id, frozen, [this](const QString& err) {
            if (!err.isEmpty()) QMessageBox::warning(this, QStringLiteral("风控"), err);
            refresh();
        });
    }
    AdminApi* api_;
    QLineEdit* search_;
    QTableWidget* table_;
    QLabel* status_;
};

// ---------- 资产管理页 ----------
class AssetsPage : public QWidget {
    Q_OBJECT
public:
    explicit AssetsPage(AdminApi* api, QWidget* parent = nullptr)
        : QWidget(parent), api_(api) {
        auto* lay = new QHBoxLayout(this);
        // 左：站
        auto* left = new QVBoxLayout;
        stations_ = new QListWidget(this);
        stations_->setObjectName(QStringLiteral("adminStationList"));
        name_ = new QLineEdit(this); name_->setPlaceholderText(QStringLiteral("站名"));
        addr_ = new QLineEdit(this); addr_->setPlaceholderText(QStringLiteral("地址"));
        lat_ = new QLineEdit(this); lat_->setText(QStringLiteral("39.90"));
        lng_ = new QLineEdit(this); lng_->setText(QStringLiteral("116.30"));
        price_ = new QLineEdit(this); price_->setPlaceholderText(QStringLiteral("单价分/度"));
        auto* mk = new QPushButton(QStringLiteral("新建站"), this);
        auto* dSt = new QPushButton(QStringLiteral("删除站"), this);
        auto* rSt = new QPushButton(QStringLiteral("刷新站"), this);
        left->addWidget(new QLabel(QStringLiteral("充电站"), this));
        left->addWidget(stations_, 1);
        left->addWidget(name_); left->addWidget(addr_);
        auto* lr = new QHBoxLayout; lr->addWidget(lat_); lr->addWidget(lng_);
        left->addLayout(lr); left->addWidget(price_);
        left->addWidget(mk); left->addWidget(dSt); left->addWidget(rSt);
        // 右：桩
        auto* right = new QVBoxLayout;
        devices_ = new QListWidget(this);
        devices_->setObjectName(QStringLiteral("adminDeviceList"));
        count_ = new QLineEdit(this); count_->setText(QStringLiteral("1"));
        type_ = new QLineEdit(this); type_->setText(QStringLiteral("0"));
        power_ = new QLineEdit(this); power_->setText(QStringLiteral("120"));
        auto* mkD = new QPushButton(QStringLiteral("批量建桩"), this);
        auto* dD = new QPushButton(QStringLiteral("删除桩"), this);
        right->addWidget(new QLabel(QStringLiteral("站内电桩(点站查看)"), this));
        right->addWidget(devices_, 1);
        auto* dr = new QHBoxLayout;
        dr->addWidget(new QLabel(QStringLiteral("数/型/功率"), this));
        dr->addWidget(count_); dr->addWidget(type_); dr->addWidget(power_);
        right->addLayout(dr);
        right->addWidget(mkD); right->addWidget(dD);
        status_ = new QLabel(this);
        lay->addLayout(left, 1);
        lay->addLayout(right, 1);
        // bottom status overlay
        auto* bl = new QVBoxLayout;
        bl->addLayout(lay, 1); bl->addWidget(status_);
        // wrap: replace lay root
        delete layout();
        auto* root = new QVBoxLayout(this);
        root->addLayout(lay, 1); root->addWidget(status_);

        connect(rSt, &QPushButton::clicked, this, &AssetsPage::loadStations);
        connect(stations_, &QListWidget::currentRowChanged, this,
                [this](int) { loadDevices(); });
        connect(mk, &QPushButton::clicked, this, [this] {
            api_->createStation(name_->text().trimmed(), addr_->text().trimmed(),
                                lat_->text().toDouble(), lng_->text().toDouble(),
                                static_cast<MoneyCents>(price_->text().toLongLong()),
                                [this](int id, const QString& err) {
                                    if (id < 0) { status_->setText(QStringLiteral("建站失败：") + err); return; }
                                    status_->setText(QStringLiteral("已建站 id=%1").arg(id));
                                    loadStations();
                                });
        });
        connect(dSt, &QPushButton::clicked, this, [this] {
            const int id = curStationId();
            if (id <= 0) return;
            api_->deleteStation(id, [this](const QString& err) {
                if (!err.isEmpty()) QMessageBox::warning(this, QStringLiteral("删站"), err);
                else status_->setText(QStringLiteral("已删除"));
                loadStations(); loadDevices();
            });
        });
        connect(mkD, &QPushButton::clicked, this, [this] {
            const int id = curStationId();
            if (id <= 0) return;
            api_->createDevices(id, count_->text().toInt(),
                                type_->text().toInt(), power_->text().toDouble(),
                                [this](const QString& err) {
                                    if (!err.isEmpty()) QMessageBox::warning(this, QStringLiteral("建桩"), err);
                                    loadDevices();
                                });
        });
        connect(dD, &QPushButton::clicked, this, [this] {
            const int row = devices_->currentRow();
            const auto* item = devices_->currentItem();
            if (!item) return;
            const int did = item->data(Qt::UserRole).toInt();
            api_->deleteDevice(did, [this](const QString& err) {
                if (!err.isEmpty()) QMessageBox::warning(this, QStringLiteral("删桩"), err);
                loadDevices();
            });
        });
        loadStations();
    }
public slots:
    void loadStations() {
        api_->listStations([this](const QVector<Station>& st, const QString& err) {
            stations_->clear();
            stationIds_.clear();
            if (!err.isEmpty()) { status_->setText(err); return; }
            status_->setText(QStringLiteral("%1 个站").arg(st.size()));
            for (const auto& s : st) {
                auto* it = new QListWidgetItem(
                    QStringLiteral("%1 · 空闲 %2/%3 · %4 元/度")
                        .arg(s.name)
                        .arg(s.freePiles)
                        .arg(s.totalPiles)
                        .arg(format_cents(s.pricePerKwhCents)),
                    stations_);
                it->setData(Qt::UserRole, s.id);
                stationIds_.append(s.id);
            }
        });
    }
    void loadDevices() {
        devices_->clear();
        const int id = curStationId();
        if (id <= 0) return;
        api_->listDevices(id, [this](const QVector<Device>& devs, const QString& err) {
            devices_->clear();
            if (!err.isEmpty()) { status_->setText(err); return; }
            for (const auto& d : devs) {
                auto* it = new QListWidgetItem(
                    QStringLiteral("桩 #%1 · %2 · %3 · %4kW")
                        .arg(d.id)
                        .arg(d.type == DeviceType::Fast ? QStringLiteral("快充")
                                                        : QStringLiteral("慢充"))
                        .arg(stateText(d.state))
                        .arg(d.powerKw),
                    devices_);
                it->setData(Qt::UserRole, d.id);
            }
        });
    }
private:
    static QString stateText(DeviceState s) {
        switch (s) {
            case DeviceState::Idle: return QStringLiteral("空闲");
            case DeviceState::Charging: return QStringLiteral("充电中");
            case DeviceState::Fault: return QStringLiteral("故障");
            case DeviceState::Reserved: return QStringLiteral("预约");
        }
        return QStringLiteral("?");
    }
    int curStationId() const {
        const auto* it = stations_->currentItem();
        return it ? it->data(Qt::UserRole).toInt() : 0;
    }
    AdminApi* api_;
    QListWidget* stations_;
    QListWidget* devices_;
    QVector<int> stationIds_;
    QLineEdit *name_, *addr_, *lat_, *lng_, *price_, *count_, *type_, *power_;
    QLabel* status_;
};

}  // namespace admin
}  // namespace ncs

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    ncs::admin::AdminApi api;
    QMainWindow win;
    win.resize(1280, 800);
    win.setWindowTitle(QStringLiteral("NCS 管理端"));

    auto* stack = new QStackedWidget(&win);
    auto* login = new ncs::admin::LoginPage(&api);
    auto* tabs = new QTabWidget;
    tabs->addTab(new ncs::admin::UsersPage(&api), QStringLiteral("用户风控"));
    tabs->addTab(new ncs::admin::AssetsPage(&api), QStringLiteral("资产管理"));
    stack->addWidget(login);
    stack->addWidget(tabs);
    win.setCentralWidget(stack);
    QObject::connect(login, &ncs::admin::LoginPage::loggedIn, &win,
                     [stack] { stack->setCurrentIndex(1); });
    win.show();
    return app.exec();
}
#include "main.moc"
