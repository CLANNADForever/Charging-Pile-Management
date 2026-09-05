// 无头测试：QT_QPA_PLATFORM=offscreen 下覆盖金额/计费/实体/登录闭环。
#include <QtTest>
#include <QApplication>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QStackedWidget>

#include "money.h"
#include "billing.h"
#include "entities.h"
#include "services/MockUserService.h"
#include "views/MainWindow.h"

class TstNcs : public QObject {
    Q_OBJECT
private slots:
    void moneyFormat();
    void billingHalfUp();
    void chargeAmounts();
    void userDefaults();
    void userRiskFields();
    void deviceStateType();
    void stationDefaultsAndPrice();
    void orderDefaultsReservedAndSnapshot();
    void orderCompleted();
    void invalidPhoneFails();
    void wrongCodeShowsError();
    void loginSuccessRegistersAndShowsProfile();
    void windowShell();
};

void TstNcs::moneyFormat() {
    QCOMPARE(ncs::format_cents(0), QStringLiteral("0.00"));
    QCOMPARE(ncs::format_cents(5), QStringLiteral("0.05"));
    QCOMPARE(ncs::format_cents(1250), QStringLiteral("12.50"));
    QCOMPARE(ncs::format_cents(100000), QStringLiteral("1000.00"));
    QCOMPARE(ncs::format_cents(-1), QStringLiteral("-0.01"));
}

void TstNcs::billingHalfUp() {
    QCOMPARE(ncs::yuan_to_cents(0.0), ncs::MoneyCents(0));
    QCOMPARE(ncs::yuan_to_cents(12.5), ncs::MoneyCents(1250));
    QCOMPARE(ncs::yuan_to_cents(0.25), ncs::MoneyCents(25));
    QCOMPARE(ncs::yuan_to_cents(1.999), ncs::MoneyCents(200));
}

void TstNcs::chargeAmounts() {
    QCOMPARE(ncs::charging_amount_cents(2.0, 100), ncs::MoneyCents(200));
    QCOMPARE(ncs::charging_amount_cents(0.5, 3), ncs::MoneyCents(2));   // 1.5 -> 2
    QCOMPARE(ncs::charging_amount_cents(0.4, 3), ncs::MoneyCents(1));   // 1.2 -> 1
    QCOMPARE(ncs::charging_amount_cents(0.0, 100), ncs::MoneyCents(0));
}

void TstNcs::userDefaults() {
    ncs::User u;
    QCOMPARE(u.id, 0);
    QCOMPARE(u.phone, QString());
    QCOMPARE(u.balanceCents, ncs::MoneyCents(0));
    QCOMPARE(int(u.status), int(ncs::UserStatus::Normal));
    QVERIFY(u.registeredAt.isNull());
}

void TstNcs::userRiskFields() {
    ncs::User u;
    u.id = 1;
    u.phone = QStringLiteral("13800138000");
    u.balanceCents = 1250;
    u.status = ncs::UserStatus::Frozen;
    u.registeredAt = QDateTime::fromString(QStringLiteral("2026-09-04T10:00:00"), Qt::ISODate);
    QCOMPARE(u.id, 1);
    QCOMPARE(u.phone, QStringLiteral("13800138000"));
    QCOMPARE(u.balanceCents, ncs::MoneyCents(1250));
    QCOMPARE(int(u.status), int(ncs::UserStatus::Frozen));
    QVERIFY(!u.registeredAt.isNull());
}

void TstNcs::deviceStateType() {
    ncs::Device d;
    QCOMPARE(int(d.state), int(ncs::DeviceState::Idle));
    QCOMPARE(int(d.type), int(ncs::DeviceType::Fast));
    d.state = ncs::DeviceState::Charging;
    d.type = ncs::DeviceType::Slow;
    d.powerKw = 7.2;
    QCOMPARE(int(d.state), 1);
    QCOMPARE(int(d.type), int(ncs::DeviceType::Slow));
    QVERIFY(d.powerKw > 7.0);
}

void TstNcs::stationDefaultsAndPrice() {
    ncs::Station s;
    QCOMPARE(s.id, 0);
    QCOMPARE(s.name, QString());
    QCOMPARE(s.totalPiles, 0);
    QCOMPARE(s.freePiles, 0);
    QCOMPARE(s.pricePerKwhCents, ncs::MoneyCents(0));
    s.pricePerKwhCents = 200;  // 2 元/度
    QCOMPARE(ncs::format_cents(s.pricePerKwhCents), QStringLiteral("2.00"));
}

void TstNcs::orderDefaultsReservedAndSnapshot() {
    ncs::Order o;
    QCOMPARE(int(o.status), int(ncs::OrderStatus::Reserved));
    QCOMPARE(o.unitPriceCents, ncs::MoneyCents(0));
    QCOMPARE(o.amountCents, ncs::MoneyCents(0));
}

void TstNcs::orderCompleted() {
    ncs::Order o;
    o.phone = QStringLiteral("13800138000");
    o.deviceId = 7;
    o.energyKwh = 1.5;
    o.unitPriceCents = 200;                        // 开单时单价快照(2 元/度)
    o.amountCents = ncs::charging_amount_cents(1.5, 200);
    o.status = ncs::OrderStatus::Completed;
    QCOMPARE(o.phone, QStringLiteral("13800138000"));
    QCOMPARE(o.unitPriceCents, ncs::MoneyCents(200));
    QCOMPARE(o.amountCents, ncs::MoneyCents(300));
    QCOMPARE(int(o.status), int(ncs::OrderStatus::Completed));
    QVERIFY(!o.startedAt.isValid());
}

void TstNcs::invalidPhoneFails() {
    ncs::client::MockUserService svc;
    ncs::client::LoginResult r;
    svc.requestCode(QStringLiteral("123"), [&](const ncs::client::LoginResult& x) { r = x; });
    QVERIFY(!r.ok);
    svc.requestCode(QStringLiteral("23800138000"), [&](const ncs::client::LoginResult& x) { r = x; });
    QVERIFY(!r.ok);
    svc.requestCode(QStringLiteral("13800138000"), [&](const ncs::client::LoginResult& x) { r = x; });
    QVERIFY(r.ok);
}

void TstNcs::wrongCodeShowsError() {
    ncs::client::MockUserService svc;
    ncs::client::MainWindow w(&svc);
    w.show();
    auto* phone = w.findChild<QLineEdit*>("phoneEdit");
    auto* code = w.findChild<QLineEdit*>("codeEdit");
    auto* login = w.findChild<QPushButton*>("btnLogin");
    auto* status = w.findChild<QLabel*>("loginStatus");
    QVERIFY(phone && code && login && status);

    phone->setText(QStringLiteral("13800138000"));
    code->setText(QStringLiteral("000000"));
    login->click();
    QVERIFY2(status->text().contains("失败"),
             qPrintable(QStringLiteral("期望失败提示，实际: ") + status->text()));

    auto* stack = w.findChild<QStackedWidget*>();
    QVERIFY(stack);
    QCOMPARE(stack->currentIndex(), 0);
}

void TstNcs::loginSuccessRegistersAndShowsProfile() {
    ncs::client::MockUserService svc;
    ncs::client::MainWindow w(&svc);
    w.show();
    auto* phone = w.findChild<QLineEdit*>("phoneEdit");
    auto* code = w.findChild<QLineEdit*>("codeEdit");
    auto* send = w.findChild<QPushButton*>("btnSendCode");
    auto* login = w.findChild<QPushButton*>("btnLogin");
    auto* status = w.findChild<QLabel*>("loginStatus");
    QVERIFY(phone && code && send && login && status);

    phone->setText(QStringLiteral("13800138000"));
    send->click();
    QVERIFY2(status->text().contains("123456"),
             qPrintable(QStringLiteral("mock 发送提示应含验证码，实际: ") + status->text()));

    code->setText(QStringLiteral("123456"));
    login->click();

    auto* stack = w.findChild<QStackedWidget*>();
    QVERIFY(stack);
    QCOMPARE(stack->currentIndex(), 1);

    auto* phoneLbl = w.findChild<QLabel*>("profilePhone");
    auto* balanceLbl = w.findChild<QLabel*>("profileBalance");
    auto* nicknameLbl = w.findChild<QLabel*>("profileNickname");
    QVERIFY(phoneLbl && balanceLbl && nicknameLbl);
    QVERIFY(phoneLbl->text().contains(QStringLiteral("13800138000")));
    QVERIFY(balanceLbl->text().contains(QStringLiteral("0.00")));
    QVERIFY(!nicknameLbl->text().isEmpty());
}

void TstNcs::windowShell() {
    ncs::client::MockUserService svc;
    ncs::client::MainWindow w(&svc);
    w.show();
    QCOMPARE(w.windowTitle(), QStringLiteral("NCS 车主端"));
    QVERIFY2(w.width() == 420 && w.height() == 760,
             qPrintable(QStringLiteral("期望竖屏 420x760，实际 %1x%2").arg(w.width()).arg(w.height())));
}

QTEST_MAIN(TstNcs)
#include "tst_common.moc"
