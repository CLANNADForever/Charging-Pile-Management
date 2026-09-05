#include "HttpChargeService.h"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonObject>

namespace ncs {
namespace client {

namespace {

QDateTime isoTo(QString s) {
    return s.isEmpty() ? QDateTime() : QDateTime::fromString(s, Qt::ISODate).toUTC();
}

ncs::Order orderFromJson(const QJsonObject& o) {
    ncs::Order x;
    x.id = o.value(QStringLiteral("id")).toInt();
    x.phone = o.value(QStringLiteral("phone")).toString();
    x.stationId = o.value(QStringLiteral("station_id")).toInt();
    x.deviceId = o.value(QStringLiteral("device_id")).toInt();
    x.unitPriceCents = static_cast<MoneyCents>(
        o.value(QStringLiteral("unit_price_cents")).toDouble());
    x.amountCents = static_cast<MoneyCents>(
        o.value(QStringLiteral("amount_cents")).toDouble());
    x.energyKwh = o.value(QStringLiteral("energy_kwh")).toDouble();
    x.status =
        static_cast<ncs::OrderStatus>(o.value(QStringLiteral("status")).toInt());
    x.startedAt = isoTo(o.value(QStringLiteral("started_at")).toString());
    x.finishedAt = isoTo(o.value(QStringLiteral("finished_at")).toString());
    return x;
}

}  // namespace

HttpChargeService::HttpChargeService(QString baseUrl)
    : client_(std::move(baseUrl)) {}

void HttpChargeService::postOrder(const QString& path, const QJsonObject* body,
                                  OrderCallback done) {
    auto respond = [done = std::move(done)](const HttpJsonClient::Reply& h) {
        OrderResult r;
        if (!h.transportOk) {
            r.message = h.error.isEmpty() ? QStringLiteral("网络请求失败")
                                          : QStringLiteral("网络错误：") + h.error;
        } else if (h.code != 0) {
            r.message = h.message;
        } else {
            r.ok = true;
            r.message = h.message;
            if (h.data.isObject())
                r.order = orderFromJson(h.data.toObject());
        }
        done(r);
    };
    if (body)
        client_.post(path, *body, std::move(respond));
    else
        client_.post(path, QJsonObject{}, std::move(respond));
}

void HttpChargeService::reserve(const QString& phone, int deviceId,
                                OrderCallback done) {
    const QJsonObject body{{QStringLiteral("phone"), phone},
                            {QStringLiteral("device_id"), deviceId}};
    postOrder(QStringLiteral("/api/orders"), &body, std::move(done));
}

void HttpChargeService::start(int orderId, OrderCallback done) {
    postOrder(QStringLiteral("/api/orders/%1/start").arg(orderId), nullptr,
              std::move(done));
}

void HttpChargeService::finish(int orderId, OrderCallback done) {
    postOrder(QStringLiteral("/api/orders/%1/finish").arg(orderId), nullptr,
              std::move(done));
}

void HttpChargeService::pay(int orderId, OrderCallback done) {
    postOrder(QStringLiteral("/api/orders/%1/pay").arg(orderId), nullptr,
              std::move(done));
}

void HttpChargeService::cancel(int orderId, OrderCallback done) {
    postOrder(QStringLiteral("/api/orders/%1/cancel").arg(orderId), nullptr,
              std::move(done));
}

void HttpChargeService::listActive(const QString& phone, OrdersCallback done) {
    client_.get(QStringLiteral("/api/orders/active?phone=%1").arg(phone),
                [done = std::move(done)](const HttpJsonClient::Reply& h) {
                    QVector<ncs::Order> out;
                    QString err;
                    if (!h.transportOk) {
                        err = h.error;
                    } else if (h.code != 0) {
                        err = h.message;
                    } else {
                        const QJsonArray arr = h.data.toArray();
                        for (const auto& v : arr)
                            out.push_back(orderFromJson(v.toObject()));
                    }
                    done(out, err);
                });
}

void HttpChargeService::live(int orderId, LiveCallback done) {
    client_.get(QStringLiteral("/api/orders/%1/live").arg(orderId),
                [done = std::move(done)](const HttpJsonClient::Reply& h) {
                    LiveInfo l;
                    if (!h.transportOk) {
                        l.message = h.error;
                    } else if (h.code != 0) {
                        l.message = h.message;
                    } else {
                        l.ok = true;
                        const QJsonObject o = h.data.toObject();
                        l.status = static_cast<ncs::OrderStatus>(
                            o.value(QStringLiteral("status")).toInt());
                        l.energyKwh = o.value(QStringLiteral("energy_kwh")).toDouble();
                        l.amountCents = static_cast<MoneyCents>(
                            o.value(QStringLiteral("amount_cents")).toDouble());
                    }
                    done(l);
                });
}

void HttpChargeService::listHistory(const QString& phone, int limit,
                                        int offset, HistoryCallback done) {
    client_.get(QStringLiteral("/api/orders/history?phone=%1&limit=%2&offset=%3")
                    .arg(phone)
                    .arg(limit)
                    .arg(offset),
                [done = std::move(done)](const HttpJsonClient::Reply& h) {
                    HistoryResult r;
                    if (!h.transportOk) {
                        r.message = h.error;
                    } else if (h.code != 0) {
                        r.message = h.message;
                    } else {
                        r.ok = true;
                        const QJsonObject o = h.data.toObject();
                        r.total = o.value(QStringLiteral("total")).toVariant().toLongLong();
                        const QJsonArray arr =
                            o.value(QStringLiteral("items")).toArray();
                        for (const auto& v : arr)
                            r.items.push_back(orderFromJson(v.toObject()));
                    }
                    done(r);
                });
}

}  // namespace client
}  // namespace ncs
