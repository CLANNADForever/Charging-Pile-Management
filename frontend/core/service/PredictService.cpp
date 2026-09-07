#include "PredictService.h"

#include "StationService.h"

#include <QProcess>
#include <QTime>
#include <cmath>

PredictService &PredictService::instance()
{
    static PredictService s;
    return s;
}

QList<LoadPoint> PredictService::loadSeries(int horizon, int stationId) const
{
    Q_UNUSED(stationId); // 桩:暂不按电站区分曲线

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
    // 阶段六:替换为真实 ml/predict.py 训练并回写 load_prediction。
    // 依次尝试 python3 / python(跨平台),脚本缺失或执行失败返回 false。
    const QString script = QStringLiteral("ml/predict.py");
    for (const QString &py : { QStringLiteral("python3"), QStringLiteral("python") }) {
        QProcess proc;
        proc.setProgram(py);
        proc.setArguments({ script });
        proc.start();
        if (!proc.waitForStarted(3000))
            continue;
        if (!proc.waitForFinished(30000))
            continue;
        return proc.exitStatus() == QProcess::NormalExit && proc.exitCode() == 0;
    }
    return false;
}
