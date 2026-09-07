#include "ChargeService.h"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonObject>
#include <QUrl>

#include "StationService.h"
#include "UserService.h"
#include "core/net/BackendClient.h"

ChargeService &ChargeService::instance()
{
    static ChargeService s;
    return s;
}

ChargeService::ChargeService() = default;

namespace {

QString isoToLocal(const QString &iso)
{
    if (iso.isEmpty())
        return QString();
    const QDateTime dt = QDateTime::fromString(iso, Qt::ISODate);
    if (!dt.isValid())
        return iso;
    return dt.toLocalTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
}

// 后端状态 → 前端状态(0 预约/1 充电中/2 已完成含待支付与已支付/3 已取消)
int toFront(int backendStatus)
{
    switch (backendStatus) {
        case 0: return 0;  // Reserved
        case 1: return 1;  // Charging
        case 2:            // Completed(待支付)
        case 3: return 2;  // Paid
        case 4: return 3;  // Canceled
        default: return -1;
    }
}

Order fromOrderJson(const QJsonObject &o)
{
    Order x;
    x.userPhone = o.value(QStringLiteral("phone")).toString();
    x.orderNo = QString::number(o.value(QStringLiteral("id")).toInt());
    x.stationId = o.value(QStringLiteral("station_id")).toInt();
    x.chargerId = o.value(QStringLiteral("device_id")).toInt();
    x.unitPrice = o.value(QStringLiteral("unit_price_cents")).toDouble() / 100.0;
    x.amount = o.value(QStringLiteral("amount_cents")).toDouble() / 100.0;
    x.energy = o.value(QStringLiteral("energy_kwh")).toDouble();
    x.status = toFront(o.value(QStringLiteral("status")).toInt());
    x.paid = o.value(QStringLiteral("status")).toInt() == 3;  // Paid
    x.startTime = isoToLocal(o.value(QStringLiteral("started_at")).toString());
    x.endTime = isoToLocal(o.value(QStringLiteral("finished_at")).toString());
    x.chargerNo = QStringLiteral("GG-%1").arg(x.chargerId, 2, 10, QLatin1Char('0'));
    if (o.contains(QStringLiteral("station_name")))
        x.stationName = o.value(QStringLiteral("station_name")).toString();
    if (o.contains(QStringLiteral("device_power_kw")))
        x.power = o.value(QStringLiteral("device_power_kw")).toDouble();
    if (o.contains(QStringLiteral("duration_sec")))
        x.durationSec = o.value(QStringLiteral("duration_sec")).toInt();
    if (o.contains(QStringLiteral("balance_cents")))
        x.balanceAfter = o.value(QStringLiteral("balance_cents")).toDouble() / 100.0;
    return x;
}

QByteArray encode(const QString &s)
{
    return QUrl::toPercentEncoding(s);
}

}  // namespace

Order ChargeService::enrich(Order o) const
{
    if (o.stationName.isEmpty() && o.stationId > 0) {
        const Station st = StationService::instance().stationDetail(o.stationId);
        o.stationName = st.name;
    }
    if (o.power <= 0.0 && o.stationId > 0 && o.chargerId > 0) {
        const QList<Charger> cs = StationService::instance().chargersByStation(o.stationId);
        for (const Charger &c : cs) {
            if (c.id == o.chargerId) {
                o.chargerNo = c.code;
                o.power = c.power;
                break;
            }
        }
    }
    return o;
}

QList<Order> ChargeService::combinedOrders(const QString &phone) const
{
    QList<Order> out;
    if (phone.isEmpty())
        return out;
    const QByteArray ph = encode(phone);
    // 活跃(预约/充电/待支付) + 历史(已支付)
    const ncsfe::BackendClient::Reply act = ncsfe::BackendClient::get(
        QStringLiteral("/api/orders/active?phone=%1").arg(QString::fromUtf8(ph)));
    if (act.ok && act.data.isArray())
        for (const QJsonValue &v : act.data.toArray()) {
            Order o = enrich(fromOrderJson(v.toObject()));
            out.append(o);
        }
    const ncsfe::BackendClient::Reply his = ncsfe::BackendClient::get(
        QStringLiteral("/api/orders/history?phone=%1&limit=200&offset=0")
            .arg(QString::fromUtf8(ph)));
    if (his.ok && his.data.isObject()) {
        const QJsonArray items = his.data.toObject()
            .value(QStringLiteral("items")).toArray();
        for (const QJsonValue &v : items)
            out.append(enrich(fromOrderJson(v.toObject())));
    }
    return out;
}

QList<Order> ChargeService::listOrders(int statusFilter) const
{
    const QString phone = UserService::instance().current().phone;
    QList<Order> all = combinedOrders(phone);
    QList<Order> result;
    for (const Order &o : all) {
        if (statusFilter < 0 || o.status == statusFilter)
            result.append(o);
    }
    return result;
}

Order ChargeService::orderDetail(const QString &orderNo) const
{
    const int id = orderNo.toInt();
    if (id <= 0)
        return Order();
    const ncsfe::BackendClient::Reply r = ncsfe::BackendClient::get(
        QStringLiteral("/api/orders/%1").arg(id));
    if (!r.ok || !r.data.isObject())
        return Order();
    Order o = fromOrderJson(r.data.toObject());
    return enrich(o);
}

QList<Order> ChargeService::listOrdersByPhone(const QString &phone) const
{
    QList<Order> all = combinedOrders(phone);
    QList<Order> out;
    for (const Order &o : all)
        if (o.userPhone == phone)
            out.append(o);
    return out;
}

bool ChargeService::hasUnsettledOrder() const
{
    const QString phone = UserService::instance().current().phone;
    if (phone.isEmpty())
        return false;
    const QByteArray ph = encode(phone);
    const ncsfe::BackendClient::Reply act = ncsfe::BackendClient::get(
        QStringLiteral("/api/orders/active?phone=%1").arg(QString::fromUtf8(ph)));
    if (act.ok && act.data.isArray()) {
        for (const QJsonValue &v : act.data.toArray()) {
            if (v.toObject().value(QStringLiteral("status")).toInt() == 2)
                return true;  // 存在待支付账单
        }
    }
    return false;
}

Order ChargeService::activeOrder() const
{
    const QString phone = UserService::instance().current().phone;
    Order empty;
    empty.status = -1;
    if (phone.isEmpty())
        return empty;
    const QByteArray ph = encode(phone);
    const ncsfe::BackendClient::Reply act = ncsfe::BackendClient::get(
        QStringLiteral("/api/orders/active?phone=%1").arg(QString::fromUtf8(ph)));
    if (act.ok && act.data.isArray()) {
        for (const QJsonValue &v : act.data.toArray()) {
            if (v.toObject().value(QStringLiteral("status")).toInt() == 2)
                return enrich(fromOrderJson(v.toObject()));  // 待支付账单(去结算)
        }
    }
    return empty;
}

Order ChargeService::createReservation(int stationId, int chargerId)
{
    m_lastError.clear();
    const QString phone = UserService::instance().current().phone;
    const ncsfe::BackendClient::Reply r = ncsfe::BackendClient::post(
        QStringLiteral("/api/orders"),
        QJsonObject{{QStringLiteral("phone"), phone},
                    {QStringLiteral("device_id"), chargerId}});
    Order empty;
    empty.status = -1;
    if (!r.ok) {
        m_lastError = r.message.isEmpty() ? QStringLiteral("预约失败(网络不可达后端)")
                                          : r.message;
        return empty;
    }
    Order o = enrich(fromOrderJson(r.data.toObject()));
    return o;
}

void ChargeService::startCharge(const QString &orderNo)
{
    m_lastError.clear();
    const int id = orderNo.toInt();
    if (id <= 0)
        return;
    const ncsfe::BackendClient::Reply r = ncsfe::BackendClient::post(
        QStringLiteral("/api/orders/%1/start").arg(id), QJsonObject{});
    if (!r.ok)
        m_lastError = r.message;
}

void ChargeService::cancelReservation(const QString &orderNo)
{
    m_lastError.clear();
    const int id = orderNo.toInt();
    if (id <= 0)
        return;
    const ncsfe::BackendClient::Reply r = ncsfe::BackendClient::post(
        QStringLiteral("/api/orders/%1/cancel").arg(id), QJsonObject{});
    if (!r.ok)
        m_lastError = r.message;
}

Order ChargeService::settle(const QString &orderNo, double, double, int)
{
    // 决策：结算 = 后端 finish 生成待支付账单(真实电量/金额/时长以后端为准)。
    m_lastError.clear();
    const int id = orderNo.toInt();
    if (id <= 0)
        return Order();
    const ncsfe::BackendClient::Reply r = ncsfe::BackendClient::post(
        QStringLiteral("/api/orders/%1/finish").arg(id), QJsonObject{});
    if (!r.ok) {
        m_lastError = r.message.isEmpty() ? QStringLiteral("结束充电失败") : r.message;
        return Order();
    }
    return orderDetail(orderNo);
}

bool ChargeService::isPendingPay(const QString &orderNo) const
{
    const int id = orderNo.toInt();
    if (id <= 0)
        return false;
    const ncsfe::BackendClient::Reply r = ncsfe::BackendClient::get(
        QStringLiteral("/api/orders/%1").arg(id));
    return r.ok && r.data.isObject() &&
           r.data.toObject().value(QStringLiteral("status")).toInt() == 2;
}

QString ChargeService::pay(const QString &orderNo)
{
    const int id = orderNo.toInt();
    if (id <= 0)
        return QStringLiteral("订单号无效");
    const ncsfe::BackendClient::Reply r = ncsfe::BackendClient::post(
        QStringLiteral("/api/orders/%1/pay").arg(id), QJsonObject{});
    if (!r.ok)
        return r.message.isEmpty() ? QStringLiteral("支付失败(网络不可达后端)")
                                   : r.message;
    m_lastError.clear();
    return QString();
}
