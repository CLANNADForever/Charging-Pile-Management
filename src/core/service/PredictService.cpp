#include "PredictService.h"

#include <QJsonArray>
#include <QJsonObject>

#include "core/net/BackendClient.h"
#include "StationService.h"

#include <QProcess>
#include <QTime>
#include <cmath>

PredictService &PredictService::instance()
{
    static PredictService s;
    return s;
}

namespace {

bool parseForecastReply(const ncsfe::BackendClient::Reply &r,
                        QList<LoadPoint> *points, QString *stationName,
                        double *energySum, bool *anyPeak)
{
    if (!r.ok || !r.data.isObject())
        return false;
    const QJsonObject data = r.data.toObject();
    if (stationName)
        *stationName = data.value(QStringLiteral("station_name")).toString();
    const QJsonArray arr = data.value(QStringLiteral("points")).toArray();
    if (arr.isEmpty())
        return false;

    double sum = 0.0;
    bool peak = false;
    points->clear();
    for (const auto &v : arr) {
        const QJsonObject o = v.toObject();
        LoadPoint p;
        p.label = o.value(QStringLiteral("time")).toString().mid(11, 5);
        p.actual = o.value(QStringLiteral("actual")).toDouble();
        p.predicted = o.value(QStringLiteral("load")).toDouble();
        sum += p.predicted;
        peak = peak || o.value(QStringLiteral("peak")).toBool(false);
        points->append(p);
    }
    if (energySum)
        *energySum = sum;
    if (anyPeak)
        *anyPeak = peak;
    return true;
}

} // namespace

QList<LoadPoint> PredictService::loadSeries(int horizon, int stationId) const
{
    Q_UNUSED(stationId); // 桩:暂不按电站区分曲线

    // 优先真实离线模型产物(按 1/6/24h 回测窗口)。
    const auto r = ncsfe::BackendClient::get(
        QStringLiteral("/api/ml/load-forecast?horizon=%1").arg(horizon));
    QList<LoadPoint> real;
    if (parseForecastReply(r, &real, nullptr, nullptr, nullptr))
        return real;

    int points = 24;
    int stepMin = 60;
    if (horizon == 1) {
        points = 6;
        stepMin = 10;
    } else if (horizon == 6) {
        points = 12;
        stepMin = 30;
    }

    QList<LoadPoint> result;
    const QTime base = QTime::currentTime();
    for (int i = 0; i < points; ++i) {
        const QTime t = base.addSecs((i + 1) * stepMin * 60);
        const double seed = double(i) + double(horizon) * 2.0;

        LoadPoint p;
        p.label = t.toString(QStringLiteral("HH:mm"));
        p.actual = 220.0 + 120.0 * std::sin(seed * 0.6) + 40.0 * std::sin(seed * 0.9);
        p.predicted = p.actual + 18.0 * std::sin(seed * 0.4) - 9.0;
        result.append(p);
    }
    return result;
}

QList<LoadPrediction> PredictService::predictionList(int horizon) const
{
    // 从真实产物生成汇总行: 该时域第一窗口电量 + 平均空闲桩 + 是否含高峰。
    const auto loadR = ncsfe::BackendClient::get(
        QStringLiteral("/api/ml/load-forecast?horizon=%1").arg(horizon));
    QList<LoadPoint> pts;
    QString stationName;
    double energy = 0.0;
    bool anyPeak = false;
    if (parseForecastReply(loadR, &pts, &stationName, &energy, &anyPeak)) {
        const auto occR = ncsfe::BackendClient::get(
            QStringLiteral("/api/ml/occupancy?horizon=%1").arg(horizon));
        double idleAvg = 0.0;
        int n = 0;
        if (occR.ok && occR.data.isObject()) {
            const QJsonArray occPts = occR.data.toObject()
                                          .value(QStringLiteral("points"))
                                          .toArray();
            for (const auto &v : occPts) {
                idleAvg += v.toObject()
                               .value(QStringLiteral("free_pred"))
                               .toDouble();
                ++n;
            }
            if (n > 0)
                idleAvg /= n;
        }

        LoadPrediction p;
        p.stationName = stationName.isEmpty()
                            ? QStringLiteral("演示站(UrbanEV 离线模型)")
                            : stationName + QStringLiteral("(离线模型)");
        p.predictedEnergy = pts.isEmpty() ? energy : pts.first().predicted;
        p.predictedIdle = qMax(0, qRound(idleAvg));
        p.isPeak = anyPeak;
        return { p };
    }

    QList<LoadPrediction> result;
    const auto stations = StationService::instance().listStations();
    for (const Station &s : stations) {
        const int seed = s.id * 7 + horizon * 3;

        LoadPrediction p;
        p.stationName = s.name;
        p.predictedEnergy = 500.0 + 300.0 * std::sin(seed * 0.5) + 100.0 * std::sin(seed * 1.1);
        const int total = StationService::instance().chargersByStation(s.id).size();
        p.predictedIdle = qMax(0, total - (2 + seed % 4));
        p.isPeak = (seed % 3) == 0;
        result.append(p);
    }
    return result;
}

bool PredictService::runPrediction()
{
    // 离线模型训练在 PyCharm 侧完成,产物已随 ml_data 部署。
    // “运行预测”改为校验后端 ML 数据是否就绪;就绪即刷新展示。
    const auto r = ncsfe::BackendClient::get(QStringLiteral("/api/ml/status"));
    return r.ok && r.data.isObject() &&
           r.data.toObject().value(QStringLiteral("ready")).toBool(false);
}
