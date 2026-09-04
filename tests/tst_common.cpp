// 无头测试：QT_QPA_PLATFORM=offscreen 下验证共享实体与 C 端壳窗。
#include <QtTest>
#include <QLabel>

#include "entities.h"
#include "views/MainWindow.h"

class TstNcs : public QObject {
    Q_OBJECT
private slots:
    void userDefaults();
    void userFields();
    void deviceState();
    void windowShell();
};

void TstNcs::userDefaults() {
    ncs::User u;
    QCOMPARE(u.phone, QString());
    QCOMPARE(u.balance, 0.0);
}

void TstNcs::userFields() {
    ncs::User u;
    u.phone = QStringLiteral("13800138000");
    u.balance = 12.5;
    QCOMPARE(u.phone, QStringLiteral("13800138000"));
    QCOMPARE(u.balance, 12.5);
}

void TstNcs::deviceState() {
    ncs::Device d;
    QCOMPARE(int(d.state), int(ncs::DeviceState::Idle));
    d.state = ncs::DeviceState::Charging;
    QCOMPARE(int(d.state), 1);
    d.powerKw = 7.2;
    QVERIFY(d.powerKw > 7.0);
}

void TstNcs::windowShell() {
    ncs::client::MainWindow w;
    w.show();
    QCOMPARE(w.windowTitle(), QStringLiteral("NCS 车主端"));
    QVERIFY2(w.width() == 420 && w.height() == 760,
             qPrintable(QStringLiteral("期望竖屏 420x760，实际 %1x%2").arg(w.width()).arg(w.height())));
    QLabel* ph = w.findChild<QLabel*>("homePlaceholder");
    QVERIFY(ph != nullptr);
    QVERIFY(!ph->text().isEmpty());
}

QTEST_MAIN(TstNcs)
#include "tst_common.moc"
