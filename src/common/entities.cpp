#include "entities.h"

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

int stationAmenityMask(const QStringList& names) {
    int mask = 0;
    for (const QString& n : names)
        for (int i = 0; i < kAmenityCount; ++i)
            if (n == QString::fromUtf8(kAmenityNames[i]))
                mask |= (1 << i);
    return mask;
}
}  // namespace ncs
