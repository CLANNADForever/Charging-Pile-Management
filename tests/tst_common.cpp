// 无头冒烟：实体/金额/计费基础 + 新前端 core(同步走后端)登录/站桩读不回归。
// 做法：进程内起真实 ncs_server(HTTP) → NCS_BACKEND_URL 指向它 → 走 User/Station/Admin Service。
#include <QtTest>
#include <QApplication>

#include <thread>
#include <memory>

#include <QString>

#include "money.h"
#include "billing.h"
#include "entities.h"

#include "BackendApp.h"

#include "core/service/UserService.h"
#include "core/service/StationService.h"
#include "core/service/AdminService.h"
#include "core/net/BackendClient.h"

class TstNcs : public QObject {
    Q_OBJECT
private slots:
    void moneyFormat();
    void billingHalfUp();
    void entityDefaults();
    void smokeLoginStationsAdmin();
};

void TstNcs::moneyFormat()
{
    QCOMPARE(ncs::format_cents(0), QStringLiteral("0.00"));
    QCOMPARE(ncs::format_cents(1250), QStringLiteral("12.50"));
    QCOMPARE(ncs::format_cents(100000), QStringLiteral("1000.00"));
}

void TstNcs::billingHalfUp()
{
    QCOMPARE(ncs::charging_amount_cents(2.0, 100), ncs::MoneyCents(200));
    QCOMPARE(ncs::charging_amount_cents(0.5, 3), ncs::MoneyCents(2));
}

void TstNcs::entityDefaults()
{
    ncs::Order o;
    QCOMPARE(int(o.status), int(ncs::OrderStatus::Reserved));
    ncs::Station s;
    QCOMPARE(s.id, 0);
}

void TstNcs::smokeLoginStationsAdmin()
{
    const QString db = QStringLiteral("/tmp/ncs_smoke_%1.db").arg(::getpid());
    {
        ncs::backend::BackendApp app(db);
        QVERIFY2(app.init(), qPrintable(app.lastError()));
        const int port = app.server().bind_to_any_port("127.0.0.1");
        std::thread th([&] { app.server().listen_after_bind(); });
        std::this_thread::sleep_for(std::chrono::milliseconds(250));

        qputenv("NCS_BACKEND_URL",
                QStringLiteral("http://127.0.0.1:%1").arg(port).toUtf8());

        // 登录(C 端)：send-code + login(演示码 123456)
        const QString hint = UserService::instance().requestCode(QStringLiteral("13800138000"));
        QVERIFY2(hint.contains(QStringLiteral("已发送")), qPrintable(hint));
        const QString err =
            UserService::instance().login(QStringLiteral("13800138000"),
                                          QStringLiteral("123456"));
        QVERIFY2(err.isEmpty(), qPrintable(err));
        QCOMPARE(UserService::instance().current().phone,
                 QStringLiteral("13800138000"));

        // 站/桩读(后端 seed)
        const QList<Station> stations = StationService::instance().listStations();
        QVERIFY2(stations.size() >= 1, "后端应有 seed 站");
        if (!stations.isEmpty()) {
            const QList<Charger> chargers =
                StationService::instance().chargersByStation(stations.first().id);
            QVERIFY2(!chargers.isEmpty(), "首个站应有电桩");
        }

        // B 端登录(admin/admin123)后 Admin 读(用户列表)
        QVERIFY2(AdminService::instance().login(QStringLiteral("admin"),
                                                QStringLiteral("admin123")),
                 "admin 登录应成功");
        const QList<User> users =
            UserService::instance().listUsers(QStringLiteral("13800138000"));
        QVERIFY2(!users.isEmpty(), "管理员用户搜索应返回该用户");

        app.server().stop();
        th.join();
    }
    ::unlink(db.toLocal8Bit().constData());
}

QTEST_MAIN(TstNcs)
#include "tst_common.moc"
