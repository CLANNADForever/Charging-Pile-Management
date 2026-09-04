#ifndef NCS_COMMON_MONEY_H
#define NCS_COMMON_MONEY_H

#include <QtGlobal>
#include <QString>

namespace ncs {

// 金额统一以最小整数单位"分"保存，避免 double 精度误差。
// 显示时用 format_cents() 格式化；业务内不要用浮点做金额运算。
using MoneyCents = qint64;

// 分 → 元字符串：1250 -> "12.50"；0 -> "0.00"；-1 -> "-0.01"
QString format_cents(MoneyCents cents);

}  // namespace ncs

#endif  // NCS_COMMON_MONEY_H
