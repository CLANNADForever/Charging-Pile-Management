#pragma once

#include <QList>
#include <QString>

// 负荷曲线点(历史实际 vs 模型预测)
struct LoadPoint
{
    QString label;
    double actual = 0.0;
    double predicted = 0.0;
};

// 单站预测结果
struct LoadPrediction
{
    QString stationName;
    double predictedEnergy = 0.0; // 预测充电量(度)
    int predictedIdle = 0;        // 预测空闲桩数
    bool isPeak = false;          // 是否高峰
};

// 预测服务(管理端)。
// 当前:优先从后端 /api/ml/*(ml_data 离线产物)拉取,失败回退确定性模拟数据。
// 后续接入实时特征/load_prediction 表后只需替换内部实现,接口不变。
class PredictService
{
public:
    static PredictService &instance();

    // 负荷曲线(折线图):stationId 传 -1 表示全部;
    // past24=true 时展示过去 24h 双线(actual+predicted),横轴按本机时间回推。
    QList<LoadPoint> loadSeries(int horizon, int stationId,
                                bool past24 = false) const;

    // 各站预测结果(表格)。
    QList<LoadPrediction> predictionList(int horizon) const;

    // 运行预测(调 Python 重训),失败返回 false(页面保留上次结果)。
    bool runPrediction();

private:
    PredictService() = default;
};
