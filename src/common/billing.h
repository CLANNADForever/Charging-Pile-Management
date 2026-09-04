// 计费规则：金额一律整数"分"；涉及小数换算时统一四舍五入(half-up)到分。
#ifndef NCS_COMMON_BILLING_H
#define NCS_COMMON_BILLING_H

#include <QtGlobal>

#include "money.h"

namespace ncs {

// 元 → 分（四舍五入到分）。用于展示/换算；业务内尽量保持整数运算。
MoneyCents yuan_to_cents(double yuan);

// 充电费用 = energyKwh × 单价(分/kWh)，结果四舍五入到"分"。
// 例：0.5 kWh × 3 分/kWh = 1.5 分 → 2 分(half-up)
MoneyCents charging_amount_cents(double energyKwh, MoneyCents priceCentsPerKwh);

}  // namespace ncs

#endif  // NCS_COMMON_BILLING_H
