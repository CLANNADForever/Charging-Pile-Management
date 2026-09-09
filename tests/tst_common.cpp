// 无头冒烟/回归：实体基础 + 新前端 core 服务(同步走后端)对 进程内后端+模拟器 的全链路。
// 覆盖：C 登录/站桩读、充值、头像、充电闭环(预约→开始→心跳电量→结束→支付)、B 端写(CRUD+重启)。
#include <QtTest>
#include <QApplication>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <chrono>
#include <cstring>
#include <memory>
#include <thread>

#include <QDir>
#include <QImage>
#include <QString>

#include "money.h"
#include "billing.h"
#include "entities.h"

#include "BackendApp.h"

#include "core/service/UserService.h"
#include "core/service/StationService.h"
#include "core/service/ChargeService.h"
#include "core/service/AdminService.h"
#include "core/net/BackendClient.h"

namespace {

int simConnect(int port)
{
    const int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0)
        return -1;
    sockaddr_in a{};
    a.sin_family = AF_INET;
    a.sin_port = htons(static_cast<uint16_t>(port));
    inet_pton(AF_INET, "127.0.0.1", &a.sin_addr);
    if (connect(fd, reinterpret_cast<sockaddr*>(&a), sizeof(a)) != 0) {
        close(fd);
        return -1;
    }
    return fd;
}
bool simSend(int fd, const std::string &line)
{
    return send(fd, line.data(), line.size(), MSG_NOSIGNAL) >= 0;
}

}  // namespace

class TstNcs : public QObject {
    Q_OBJECT
private slots:
    void initTestCase();
    void cleanupTestCase();
    void moneyAndEntities();
    void smokeLoginStations();
    void rechargeNicknameAvatar();
    void chargeFullLoopWithSim();
    void adminWriteSmoke();

private:
    QString db_;
    std::unique_ptr<ncs::backend::BackendApp> app_;
    int port_ = 0;
    int simPort_ = 0;
    std::thread th_;
};

void TstNcs::initTestCase()
{
    db_ = QStringLiteral("/tmp/ncs_smoke_%1.db").arg(::getpid());
    app_ = std::make_unique<ncs::backend::BackendApp>(db_);
    QVERIFY2(app_->init(), qPrintable(app_->lastError()));
    port_ = app_->server().bind_to_any_port("127.0.0.1");
    th_ = std::thread([this] { app_->server().listen_after_bind(); });

    const int probe = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in a{};
    a.sin_family = AF_INET;
    a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    a.sin_port = 0;
    bind(probe, reinterpret_cast<sockaddr*>(&a), sizeof(a));
    socklen_t alen = sizeof(a);
    getsockname(probe, reinterpret_cast<sockaddr*>(&a), &alen);
    simPort_ = ntohs(a.sin_port);
    close(probe);
    QVERIFY(app_->startSimListener(simPort_));
    std::this_thread::sleep_for(std::chrono::milliseconds(250));

    qputenv("NCS_BACKEND_URL",
            QStringLiteral("http://127.0.0.1:%1").arg(port_).toUtf8());
}

void TstNcs::cleanupTestCase()
{
    if (app_) {
        app_->server().stop();
        app_->stopSimListener();
        th_.join();
    }
    ::unlink(db_.toLocal8Bit().constData());
}

void TstNcs::moneyAndEntities()
{
    QCOMPARE(ncs::format_cents(1250), QStringLiteral("12.50"));
    QCOMPARE(ncs::charging_amount_cents(2.0, 100), ncs::MoneyCents(200));
    ncs::Order o;
    QCOMPARE(int(o.status), int(ncs::OrderStatus::Reserved));
}

static QString userLogin(const QString &phone)
{
    const QString hint = UserService::instance().requestCode(phone);
    if (!hint.contains(QStringLiteral("已发送")))
        return hint;
    // send-code 演示返回随机码并明文放在提示里(形如 "验证码已发送（模拟）：982378")，
    // 与 C 端真实流程一致:从提示末尾提取数字后登录
    int ci = hint.lastIndexOf(QStringLiteral("："));
    if (ci < 0)
        ci = hint.lastIndexOf(QLatin1Char(':'));
    QString code;
    if (ci >= 0) {
        const QString tail = hint.mid(ci + 1);
        int n = 0;
        while (n < tail.size() && tail[n].isDigit())
            ++n;
        code = tail.left(n);
    }
    if (code.isEmpty())
        return hint;  // 未能从提示解析出验证码,直接透传提示供排查
    return UserService::instance().login(phone, code);
}

void TstNcs::smokeLoginStations()
{
    QVERIFY2(userLogin(QStringLiteral("13800138000")).isEmpty(), "C 登录应成功");
    const QList<Station> stations = StationService::instance().listStations();
    QVERIFY(stations.size() >= 1);
    if (!stations.isEmpty())
        QVERIFY(!StationService::instance().chargersByStation(stations.first().id).isEmpty());
}

void TstNcs::rechargeNicknameAvatar()
{
    const QString phone = QStringLiteral("13811112222");
    QVERIFY2(userLogin(phone).isEmpty(), "登录");
    QString err;
    QVERIFY2(UserService::instance().recharge(100.0, &err), qPrintable(err));
    QVERIFY(qAbs(UserService::instance().current().balance - 100.0) < 0.01);
    QVERIFY2(UserService::instance().updateNickname(QStringLiteral("冒烟昵称"), &err),
             qPrintable(err));
    QCOMPARE(UserService::instance().current().nickname, QStringLiteral("冒烟昵称"));

    // 头像：造一张临时 PNG → 上传后端 → 本地缓存路径
    const QString png = QDir::temp().filePath(QStringLiteral("ncs_av_smoke.png"));
    QImage img(4, 4, QImage::Format_RGB32);
    img.fill(Qt::blue);
    QVERIFY(img.save(png, "PNG"));
    QVERIFY2(UserService::instance().uploadAvatar(png, &err), qPrintable(err));
    QVERIFY(!UserService::instance().current().avatarPath.isEmpty());
    QFile::remove(png);
}

void TstNcs::chargeFullLoopWithSim()
{
    const QString phone = QStringLiteral("13900009999");
    QVERIFY2(userLogin(phone).isEmpty(), "登录");
    QString err;
    QVERIFY2(UserService::instance().recharge(30.0, &err), qPrintable(err));  // 足够支付

    // 选一个空闲桩(首站第一个空闲)
    const QList<Station> stations = StationService::instance().listStations();
    QVERIFY(!stations.isEmpty());
    int devId = -1, stId = -1;
    for (const Station &s : stations) {
        const QList<Charger> cs = StationService::instance().chargersByStation(s.id);
        for (const Charger &c : cs) {
            if (c.status == 0) {
                devId = c.id;
                stId = s.id;
                break;
            }
        }
        if (devId > 0)
            break;
    }
    QVERIFY(devId > 0);

    // 模拟器连接注册该桩并心跳出电量
    const int fd = simConnect(simPort_);
    QVERIFY(fd >= 0);
    QVERIFY(simSend(fd, "{\"type\":\"register\",\"devices\":[" +
                            std::to_string(devId) + "]}\n"));

    const Order reserved = ChargeService::instance().createReservation(stId, devId);
    QVERIFY2(reserved.orderNo.toInt() > 0, qPrintable(ChargeService::instance().lastError()));
    ChargeService::instance().startCharge(reserved.orderNo);
    QVERIFY2(ChargeService::instance().lastError().isEmpty(),
             qPrintable(ChargeService::instance().lastError()));
    std::this_thread::sleep_for(std::chrono::milliseconds(120));
    QVERIFY(simSend(fd, "{\"type\":\"heartbeat\",\"device_id\":" +
                            std::to_string(devId) +
                            ",\"power_kw\":120.0,\"energy_kwh\":2.5,\"sim_state\":1}\n"));
    std::this_thread::sleep_for(std::chrono::milliseconds(120));

    const Order settled = ChargeService::instance().settle(reserved.orderNo, 0, 0, 0);
    QVERIFY2(settled.orderNo.toInt() > 0, qPrintable(ChargeService::instance().lastError()));
    QVERIFY(qAbs(settled.energy - 2.5) < 1e-6);  // 电量以后端为准
    QVERIFY2(ChargeService::instance().pay(reserved.orderNo).isEmpty(),
             qPrintable(ChargeService::instance().pay(reserved.orderNo)));
    QVERIFY(!ChargeService::instance().isPendingPay(reserved.orderNo));
    close(fd);
}

void TstNcs::adminWriteSmoke()
{
    // B 端登录 → CRUD + 重启(全部走后端)
    QVERIFY2(AdminService::instance().login(QStringLiteral("admin"),
                                            QStringLiteral("admin123")),
             "admin 登录");
    const int sid = StationService::instance().addStation(
        QStringLiteral("冒烟测试站"), QStringLiteral("测试路"), 30.0, 120.0,
        1.2, 2, 120.0);
    QVERIFY(sid > 0);
    bool found = false;
    for (const Station &s : StationService::instance().listStations())
        if (s.id == sid)
            found = true;
    QVERIFY(found);
    QVERIFY(StationService::instance().updateStation(
        sid, QStringLiteral("冒烟测试站改"), QStringLiteral("测试路2号"), 30.0, 120.0, 1.1));

    QList<Charger> devs;
    for (const Charger &c : StationService::instance().chargersByStation(sid)) {
        devs.append(c);
        Q_UNUSED(c);
    }
    QVERIFY(devs.size() >= 2);
    const int d0 = devs[0].id;
    QString err;
    QVERIFY2(StationService::instance().setChargerStatus(d0, 2, &err), qPrintable(err));  // 标记故障
    QVERIFY2(StationService::instance().rebootCharger(d0, &err), qPrintable(err));         // 远程重启
    QVERIFY2(StationService::instance().setChargerStatus(d0, 0, &err), qPrintable(err));   // 恢复正常
    for (const Charger &c : devs)
        QVERIFY(StationService::instance().deleteCharger(c.id));  // 空闲可删
    QVERIFY(StationService::instance().deleteStation(sid));
}

QTEST_MAIN(TstNcs)
#include "tst_common.moc"
