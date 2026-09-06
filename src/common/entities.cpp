#include "entities.h"

#include <cmath>

namespace ncs {
const char* project_name() {
    return "NCS 充电桩管理平台";
}

namespace {
// 固定 9 项，顺序即 bit 位(bit0..bit8)
const char* kAmenityNames[] = {"卫生间", "休息室", "餐饮", "雨棚", "便利店",
                               "自动售货机", "饮用水", "可洗车", "有人值守"};
constexpr int kAmenityCount = 9;
}  // namespace

QStringList stationAmenityNames(int mask) {
    QStringList out;
    for (int i = 0; i < kAmenityCount; ++i)
        if (mask & (1 << i))
            out << QString::fromUtf8(kAmenityNames[i]);
    return out;
}

double haversine_km(double lat1, double lon1, double lat2, double lon2) {
    constexpr double kPi = 3.14159265358979323846;
    const double r = 6371.0;
    const auto rad = [](double d) { return d * kPi / 180.0; };
    const double dLat = rad(lat2 - lat1);
    const double dLon = rad(lon2 - lon1);
    const double a = std::sin(dLat / 2) * std::sin(dLat / 2) +
                     std::cos(rad(lat1)) * std::cos(rad(lat2)) *
                         std::sin(dLon / 2) * std::sin(dLon / 2);
    return 2 * r * std::asin(std::sqrt(a));
}

int stationAmenityMask(const QStringList& names) {
    int mask = 0;
    for (const QString& n : names)
        for (int i = 0; i < kAmenityCount; ++i)
            if (n == QString::fromUtf8(kAmenityNames[i]))
                mask |= (1 << i);
    return mask;
}

PowerTier power_tier(double powerKw) {
    if (powerKw >= 180.0)
        return PowerTier::Ultra;
    if (powerKw >= 30.0)
        return PowerTier::Fast;
    return PowerTier::Slow;
}

int soc_pct(const Order& o) {
    if (o.batteryCapKwh <= 0)
        return qBound(0, o.startSocPct, 100);
    return qBound(0, static_cast<int>(qRound(
                          o.startSocPct + o.energyKwh / o.batteryCapKwh * 100.0)),
                  100);
}

MoneyCents stationTierPriceCents(const Station& s, double powerKw) {
    MoneyCents p = 0;
    switch (power_tier(powerKw)) {
        case PowerTier::Slow:
            p = s.priceSlowCents;
            break;
        case PowerTier::Ultra:
            p = s.priceUltraCents;
            break;
        case PowerTier::Fast:
        default:
            p = s.pricePerKwhCents;
            break;
    }
    if (p <= 0)
        p = s.pricePerKwhCents;  // 未配置档回退快充档
    return p;
}
}  // namespace ncs
