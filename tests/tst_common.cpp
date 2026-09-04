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
    void userFields();
    void deviceState();
    void stationDefaults();
    void orderFields();
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
    // 元 → 分，四舍五入
    QCOMPARE(ncs::yuan_to_cents(0.0), ncs::MoneyCents(0));
    QCOMPARE(ncs::yuan_to_cents(12.5), ncs::MoneyCents(1250));
    QCOMPARE(ncs::yuan_to_cents(0.25), ncs::MoneyCents(25));
    QCOMPARE(ncs::yuan_to_cents(1.999), ncs::MoneyCents(200));
}

void TstNcs::chargeAmounts() {
    // kWh × 分/kWh → 分(half-up)
    QCOMPARE(ncs::charging_amount_cents(2.0, 100), ncs::MoneyCents(200));
    QCOMPARE(ncs::charging_amount_cents(0.5, 3), ncs::MoneyCents(2));   // 1.5 → 2
    QCOMPARE(ncs::charging_amount_cents(0.4, 3), ncs::MoneyCents(1));   // 1.2 → 1
    QCOMPARE(ncs::charging_amount_cents(0.0, 100), ncs::MoneyCents(0));
}

void TstNcs::userDefaults() {
    ncs::User u;
    QCOMPARE(u.phone, QString());
    QCOMPARE(u.balanceCents, ncs::MoneyCents(0));
}

void TstNcs::userFields() {
    ncs::User u;
    u.phone = QStringLiteral("13800138000");
    u.balanceCents = 1250;
    QCOMPARE(u.phone, QStringLiteral("13800138000"));
    QCOMPARE(u.balanceCents, ncs::MoneyCents(1250));
}

void TstNcs::deviceState() {
    ncs::Device d;
    QCOMPARE(int(d.state), int(ncs::DeviceState::Idle));
    d.state = ncs::DeviceState::Charging;
    QCOMPARE(int(d.state), 1);
    d.powerKw = 7.2;
    QVERIFY(d.powerKw > 7.0);
}

void TstNcs::stationDefaults() {
    ncs::Station s;
    QCOMPARE(s.id, 0);
    QCOMPARE(s.name, QString());
    QCOMPARE(s.totalPiles, 0);
    QCOMPARE(s.freePiles, 0);
}

void TstNcs::orderFields() {
    ncs::Order o;
    QCOMPARE(int(o.status), int(ncs::OrderStatus::Charging));
    QCOMPARE(o.amountCents, ncs::MoneyCents(0));

    o.phone = QStringLiteral("13800138000");
    o.deviceId = 7;
    o.energyKwh = 1.5;
    o.amountCents = ncs::charging_amount_cents(1.5, 200);  // 1.5kWh × 2.00元 = 3.00元
    o.status = ncs::OrderStatus::Completed;
    QCOMPARE(o.phone, QStringLiteral("13800138000"));
    QCOMPARE(o.amountCents, ncs::MoneyCents(300));
    QCOMPARE(int(o.status), int(ncs::OrderStatus::Completed));
}

void TstNcs::invalidPhoneFails() {
    ncs::client::MockUserService svc;
    auto r = svc.requestCode(QStringLiteral("123"));       // 太短
    QVERIFY(!r.ok);
    r = svc.requestCode(QStringLiteral("23800138000"));    // 非 1 开头
    QVERIFY(!r.ok);
    r = svc.requestCode(QStringLiteral("13800138000"));
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
    QCOMPARE(stack->currentIndex(), 0);  // 仍停留在登录页
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
    QCOMPARE(stack->currentIndex(), 1);  // 切到个人中心

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
