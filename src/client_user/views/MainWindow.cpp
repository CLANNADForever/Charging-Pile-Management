#include "MainWindow.h"

#include <QBuffer>
#include <QFileDialog>
#include <QImage>
#include <QInputDialog>
#include <QMessageBox>
#include <QPixmap>
#include <QStackedWidget>

#include "services/IChargeService.h"
#include "services/IStationService.h"
#include "services/IUserService.h"
#include "views/ChargePage.h"
#include "views/HistoryPage.h"
#include "views/LoginPage.h"
#include "views/MySessionsPage.h"
#include "views/ProfilePage.h"
#include "views/StationDetailPage.h"
#include "views/StationListPage.h"

namespace ncs {
namespace client {

MainWindow::MainWindow(IUserService* userService,
                       IStationService* stationService,
                       IChargeService* chargeService, QWidget* parent)
    : QMainWindow(parent), userService_(userService), chargeService_(chargeService) {
    setWindowTitle(QStringLiteral("NCS 车主端"));
    setObjectName(QStringLiteral("ncsUserMainWindow"));
    setFixedSize(420, 760);

    stack_ = new QStackedWidget(this);
    auto* loginPage = new LoginPage(userService, stack_);
    profilePage_ = new ProfilePage(User{}, stack_);
    stationPage_ = new StationListPage(stationService, stack_);
    detailPage_ = new StationDetailPage(stationService, stack_);
    chargePage_ = new ChargePage(chargeService, stack_);
    sessionsPage_ = new MySessionsPage(chargeService, stack_);
    historyPage_ = new HistoryPage(chargeService, stack_);
    stack_->addWidget(loginPage);      // 0
    stack_->addWidget(profilePage_);   // 1
    stack_->addWidget(stationPage_);   // 2
    stack_->addWidget(detailPage_);    // 3
    stack_->addWidget(chargePage_);    // 4
    stack_->addWidget(sessionsPage_);  // 5
    stack_->addWidget(historyPage_);   // 6
    setCentralWidget(stack_);

    connect(loginPage, &LoginPage::loginSucceeded, this,
            &MainWindow::onLoginSucceeded);
    connect(profilePage_, &ProfilePage::goFindStations, this,
            &MainWindow::onGoFindStations);
    connect(profilePage_, &ProfilePage::goSessions, this,
            &MainWindow::onGoSessions);
    connect(profilePage_, &ProfilePage::rechargeRequested, this,
            &MainWindow::onRecharge);
    connect(profilePage_, &ProfilePage::nicknameRequested, this,
            &MainWindow::onNickname);
    connect(profilePage_, &ProfilePage::historyRequested, this,
            &MainWindow::onHistory);
    connect(profilePage_, &ProfilePage::avatarRequested, this,
            &MainWindow::onAvatar);
    connect(stationPage_, &StationListPage::stationChosen, this,
            &MainWindow::onStationChosen);
    connect(detailPage_, &StationDetailPage::backRequested, this,
            &MainWindow::onDetailBack);
    connect(detailPage_, &StationDetailPage::deviceChosen, this,
            &MainWindow::onDeviceChosen);
    connect(detailPage_, &StationDetailPage::navRequested, this,
            &MainWindow::onDetailNav);
    connect(chargePage_, &ChargePage::backRequested, this,
            &MainWindow::onChargeBack);
    connect(sessionsPage_, &MySessionsPage::backRequested, this,
            [this] { stack_->setCurrentIndex(1); });
    connect(sessionsPage_, &MySessionsPage::sessionChosen, this,
            &MainWindow::onSessionChosen);
    connect(historyPage_, &HistoryPage::backRequested, this,
            [this] { stack_->setCurrentIndex(1); });
}

void MainWindow::setCurrentUser(const ncs::User& u) {
    user_ = u;
    userPhone_ = u.phone;
    profilePage_->setUser(u);
}

void MainWindow::onLoginSucceeded(const ncs::User& user) {
    setCurrentUser(user);
    stack_->setCurrentIndex(1);
    refreshAvatar();
}

void MainWindow::onGoFindStations() {
    stationPage_->refresh();
    stack_->setCurrentIndex(2);
}

void MainWindow::onGoSessions() {
    sessionsPage_->setPhone(userPhone_);
    sessionsPage_->refresh();
    stack_->setCurrentIndex(5);
}

void MainWindow::onStationChosen(int stationId) {
    lastStationId_ = stationId;
    const ncs::Station* st = stationPage_->stationById(stationId);
    if (st) {
        lastStName_ = st->name;
        lastStLat_ = st->latitude;
        lastStLng_ = st->longitude;
        detailPage_->setStation(*st);
    }
    detailPage_->setMyActive({});
    detailPage_->load(stationId);
    refreshMyActive();
    stack_->setCurrentIndex(3);
}

void MainWindow::onDetailBack() {
    stationPage_->refresh();
    stack_->setCurrentIndex(2);
}

void MainWindow::onDeviceChosen(int deviceId) {
    chargeOrigin_ = 3;
    const auto it = myActiveOrders_.find(deviceId);
    if (it != myActiveOrders_.end()) {  // 我在这台桩有活跃单 → 直接进实时/结算
        chargePage_->resumeSession(userPhone_, it.value());
    } else {
        chargePage_->startSession(userPhone_, deviceId);
    }
    stack_->setCurrentIndex(4);
}

void MainWindow::onSessionChosen(const ncs::Order& order) {
    chargeOrigin_ = 5;
    chargePage_->resumeSession(userPhone_, order);
    stack_->setCurrentIndex(4);
}

void MainWindow::onChargeBack() {
    if (chargeOrigin_ == 5) {
        sessionsPage_->refresh();
        stack_->setCurrentIndex(5);
    } else {
        if (lastStationId_ > 0)
            detailPage_->load(lastStationId_);
        stack_->setCurrentIndex(3);
    }
}

void MainWindow::refreshMyActive() {
    if (!chargeService_)
        return;
    chargeService_->listActive(
        userPhone_, [this](const QVector<ncs::Order>& orders, const QString& err) {
            myActiveOrders_.clear();
            if (!err.isEmpty())
                return;
            for (const auto& o : orders)
                if (o.id > 0 && o.deviceId > 0 &&
                    (o.status == ncs::OrderStatus::Reserved ||
                     o.status == ncs::OrderStatus::Charging ||
                     o.status == ncs::OrderStatus::Completed))
                    myActiveOrders_.insert(o.deviceId, o);
            if (detailPage_)
                detailPage_->setMyActive(orders);
        });
}

void MainWindow::onDetailNav() {
    emit routeRequested(stationPage_->myLat(), stationPage_->myLng(),
                        lastStLat_, lastStLng_, lastStName_);
}

void MainWindow::pushPage(QWidget* page, std::function<void()> onBack) {
    if (!page)
        return;
    prevIndex_ = stack_->currentIndex();
    pushedPage_ = page;
    pushedBack_ = std::move(onBack);
    stack_->addWidget(page);
    stack_->setCurrentWidget(page);
}

void MainWindow::popPage() {
    if (!pushedPage_)
        return;
    stack_->removeWidget(pushedPage_);
    pushedPage_ = nullptr;
    auto cb = std::move(pushedBack_);
    pushedBack_ = nullptr;
    stack_->setCurrentIndex(prevIndex_);  // 总是回到进入前那页
    if (cb)
        cb();  // 回调只做刷新等副作用
}

void MainWindow::refreshStationDetail() {
    if (lastStationId_ > 0)
        detailPage_->load(lastStationId_);
}

void MainWindow::onRecharge() {
    bool ok = false;
    const double yuan = QInputDialog::getDouble(
        this, QStringLiteral("充值"), QStringLiteral("充值金额(元)"), 0, 0,
        100000, 2, &ok);
    if (!ok)
        return;
    const MoneyCents cents = static_cast<MoneyCents>(qRound(yuan * 100.0));
    if (cents <= 0)
        return;
    userService_->recharge(userPhone_, cents, [this](const LoginResult& r) {
        QMessageBox::information(this, QStringLiteral("充值"),
                                 r.ok ? QStringLiteral("充值成功")
                                      : QStringLiteral("失败：") + r.message);
        if (r.ok)
            setCurrentUser(r.user);
    });
}

void MainWindow::onNickname() {
    bool ok = false;
    const QString name =
        QInputDialog::getText(this, QStringLiteral("改昵称"),
                              QStringLiteral("新昵称"), QLineEdit::Normal,
                              user_.nickname, &ok);
    if (!ok || name.trimmed().isEmpty())
        return;
    userService_->setNickname(userPhone_, name.trimmed(),
                              [this](const LoginResult& r) {
                                  QMessageBox::information(
                                      this, QStringLiteral("改昵称"),
                                      r.ok ? QStringLiteral("已保存")
                                           : QStringLiteral("失败：") + r.message);
                                  if (r.ok)
                                      setCurrentUser(r.user);
                              });
}

void MainWindow::onHistory() {
    historyPage_->setPhone(userPhone_);
    historyPage_->load();
    stack_->setCurrentIndex(6);
}

void MainWindow::onAvatar() {
    const QString file = QFileDialog::getOpenFileName(
        this, QStringLiteral("选择头像图片"), QString(),
        QStringLiteral("图片 (*.png *.jpg *.jpeg *.bmp)"));
    if (file.isEmpty())
        return;
    QImage img(file);
    if (img.isNull()) {
        QMessageBox::warning(this, QStringLiteral("头像"),
                             QStringLiteral("无法读取图片"));
        return;
    }
    const QImage scaled =
        img.scaled(256, 256, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    QByteArray png;
    QBuffer buf(&png);
    buf.open(QIODevice::WriteOnly);
    scaled.save(&buf, "PNG");
    userService_->uploadAvatar(userPhone_, png,
                               [this](const QString& err, const QString&) {
                                   if (!err.isEmpty()) {
                                       QMessageBox::warning(
                                           this, QStringLiteral("头像"),
                                           QStringLiteral("上传失败：") + err);
                                       return;
                                   }
                                   refreshAvatar();
                               });
}

void MainWindow::refreshAvatar() {
    if (!userService_)
        return;
    userService_->downloadAvatar(userPhone_, [this](const QByteArray& bytes) {
        QPixmap pm;
        if (!bytes.isEmpty() && pm.loadFromData(bytes))
            profilePage_->setAvatarPixmap(pm);
    });
}

}  // namespace client
}  // namespace ncs
