#include "PredictService.h"

#include <QJsonArray>
#include <QJsonObject>

#include "core/net/BackendClient.h"
#include "StationService.h"

#include <QProcess>
#include <QDateTime>
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

QList<LoadPoint> relabelToLocalTime(const QList<LoadPoint> &src, int horizon,
                                    bool past24)
{
    Q_UNUSED(horizon);
    const QDateTime now = QDateTime::currentDateTime();
    const QDateTime base(now.date(), QTime(now.time().hour(), 0, 0));
    QList<LoadPoint> out;
    const int n = src.size();
    for (int i = 0; i < n; ++i) {
        LoadPoint p = src[i];
        const QDateTime t = past24
                                ? base.addSecs((i - (n - 1)) * 3600)
                                : base.addSecs((i + 1) * 3600);
        p.label = t.toString(QStringLiteral("HH:mm"));
        out.append(p);
    }
    return out;
}

// 预警阈值 = 完整时域预测峰值 × 75%，保证 6h 视图与 24h 视图前 6 个红点一致。
void applyWarning(QList<LoadPoint> &shown, const QList<LoadPoint> &full)
{
    double maxPred = 0.0;
    for (const LoadPoint &p : full)
        maxPred = qMax(maxPred, p.predicted);
    const double threshold = maxPred * 0.75;
    for (LoadPoint &p : shown)
        p.warning = p.predicted > threshold;
}

} // namespace

QList<LoadPoint> PredictService::loadSeries(int horizon, int stationId,
                                            bool past24) const
{
    Q_UNUSED(stationId); // 桩:暂不按电站区分曲线

    if (past24) {
        // 过去 24 小时演示：取离线逐时回测作“今日实际”，第二线为“昨日同期参考”，
        // 数值口径(逐时)与未来24h(滚动窗口)不同，避免两端曲线完全相同。
        const auto pr = ncsfe::BackendClient::get(
            QStringLiteral("/api/ml/load-forecast?horizon=1"));
        QList<LoadPoint> past;
        if (parseForecastReply(pr, &past, nullptr, nullptr, nullptr)) {
            applyWarning(past, past);
            return relabelToLocalTime(past, 24, true);
        }
    }

    // 优先真实离线模型产物(按 1/6/24h 回测窗口)。
    // 未来 6h 复用未来 24h 的前 6 个点，保证曲线/红点风格一致。
    const int apiHorizon = (!past24 && horizon == 6) ? 24 : horizon;
    const auto r = ncsfe::BackendClient::get(
        QStringLiteral("/api/ml/load-forecast?horizon=%1").arg(apiHorizon));
    QList<LoadPoint> real;
    if (parseForecastReply(r, &real, nullptr, nullptr, nullptr)) {
        const QList<LoadPoint> full = real;  // 24h 完整序列，用于统一预警判定
        if (horizon == 24 && real.size() > 24)
            real = real.mid(0, 24);
        if (horizon == 6 && real.size() > 6)
            real = real.mid(0, 6);
        if (horizon == 1 && !real.isEmpty()) {
            // 未来 1 小时展示 6 个 10 分钟整点采样点。
            QTime base = QTime::fromString(real.first().label,
                                           QStringLiteral("HH:mm"));
            if (!base.isValid())
                base = QTime(0, 0);
            QList<LoadPoint> sub;
            const double value = real.first().predicted;
            for (int i = 1; i <= 6; ++i) {
                LoadPoint p;
                p.label = base.addSecs(i * 10 * 60)
                              .toString(QStringLiteral("HH:mm"));
                p.actual = real.first().actual;
                p.predicted = value;
                sub.append(p);
            }
            real = sub;
        }
        applyWarning(real, full);
        return relabelToLocalTime(real, horizon, past24);
    }

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
    applyWarning(result, result);
    return result;
}

QList<LoadPrediction> PredictService::predictionList(int horizon) const
{
    QList<LoadPrediction> result;
    const auto stations = StationService::instance().listStations();
    if (stations.isEmpty())
        return result;

    // 加载离线预测作为“每桩基线”，再按数据库实际电站/电桩规模展开成 10 站汇总。
    const auto loadR = ncsfe::BackendClient::get(
        QStringLiteral("/api/ml/load-forecast?horizon=%1").arg(horizon));
    QList<LoadPoint> pts;
    QString stationName;
    double energy = 0.0;
    bool anyPeak = false;
    const bool hasMl = parseForecastReply(loadR, &pts, &stationName, &energy,
                                          &anyPeak);

    double modelFreeRatio = 0.5;
    if (hasMl) {
        const auto occR = ncsfe::BackendClient::get(
            QStringLiteral("/api/ml/occupancy?horizon=%1").arg(horizon));
        double freeSum = 0.0;
        int n = 0;
        if (occR.ok && occR.data.isObject()) {
            const QJsonArray occPts = occR.data.toObject()
                                          .value(QStringLiteral("points"))
                                          .toArray();
            for (const auto &v : occPts) {
                freeSum += v.toObject()
                               .value(QStringLiteral("free_pred"))
                               .toDouble();
                ++n;
            }
            if (n > 0)
                modelFreeRatio = freeSum / n / 6.0;  // 样本站共 6 桩
        }
    }

    for (const Station &s : stations) {
        LoadPrediction p;
        p.stationName = s.name;
        const int total = StationService::instance().chargersByStation(s.id).size();
        if (total <= 0)
            continue;

        if (hasMl && !pts.isEmpty()) {
            // 样本站 6 桩：按实际电站桩数等比展开
            const double energyPerPile = pts.first().predicted / 6.0;
            p.predictedEnergy = energyPerPile * total;

            const auto chargers =
                StationService::instance().chargersByStation(s.id);
            int currentFree = 0;
            for (const Charger &c : chargers)
                if (c.status == 0)
                    ++currentFree;
            double ratio = qBound(
                0.0, 0.6 * modelFreeRatio + 0.4 * currentFree / double(total), 1.0);
            p.predictedIdle = qRound(ratio * total);
            p.isPeak = anyPeak && p.predictedIdle < qMax(1, total / 2);
        } else {
            // 兜底：接口不可用时仍给每站合理估算
            const int seed = s.id * 7 + horizon * 3;
            p.predictedEnergy =
                500.0 + 300.0 * std::sin(seed * 0.5) + 100.0 * std::sin(seed * 1.1);
            p.predictedIdle = qMax(0, total - (2 + seed % 4));
            p.isPeak = (seed % 3) == 0;
        }
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
