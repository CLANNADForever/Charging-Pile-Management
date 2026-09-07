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
// 阶段一:桩实现(确定性模拟数据),用于预测页开发;
// 阶段六接入 ml/predict.py 与 load_prediction 表后替换内部实现,接口不变。
class PredictService
{
public:
    static PredictService &instance();

    // 负荷曲线(折线图):stationId 传 -1 表示全部。
    QList<LoadPoint> loadSeries(int horizon, int stationId) const;

    // 各站预测结果(表格)。
    QList<LoadPrediction> predictionList(int horizon) const;

    // 运行预测(调 Python 重训),失败返回 false(页面保留上次结果)。
    bool runPrediction();

private:
    PredictService() = default;
};
