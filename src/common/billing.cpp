#include "billing.h"

#include <cmath>

namespace ncs {

MoneyCents yuan_to_cents(double yuan) {
    // 四舍五入到分（half-up）：先放大到分再取整。
    return static_cast<MoneyCents>(std::floor(yuan * 100.0 + 0.5));
}

MoneyCents charging_amount_cents(double energyKwh, MoneyCents priceCentsPerKwh) {
    const double valueInCents = energyKwh * static_cast<double>(priceCentsPerKwh);
    // 金额非负才做 half-up；负值走 llround(远离零) 兜底。
    if (valueInCents >= 0.0)
        return static_cast<MoneyCents>(std::floor(valueInCents + 0.5));
    return static_cast<MoneyCents>(std::llround(valueInCents));
}

}  // namespace ncs
