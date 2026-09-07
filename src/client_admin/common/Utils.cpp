#include "Utils.h"

#include "theme/AdminTheme.h"

namespace Utils {

QString maskPhone(const QString &phone)
{
    if (phone.length() != 11)
        return phone;
    return phone.left(3) + QStringLiteral("****") + phone.right(4);
}

QString formatMoney(double amount)
{
    return QString::number(amount, 'f', 2);
}

QString chargerStatusText(int status)
{
    switch (status) {
    case 0: return QStringLiteral("空闲");
    case 1: return QStringLiteral("使用中");
    case 2: return QStringLiteral("故障");
    default: return QStringLiteral("未知");
    }
}

QColor chargerStatusColor(int status)
{
    switch (status) {
    case 0: return AdminTheme::Green;
    case 1: return AdminTheme::Amber;
    case 2: return AdminTheme::Red;
    default: return AdminTheme::TextSub;
    }
}

QString orderStatusText(int status)
{
    switch (status) {
    case 0: return QStringLiteral("预约");
    case 1: return QStringLiteral("充电中");
    case 2: return QStringLiteral("已完成");
    case 3: return QStringLiteral("已取消");
    default: return QStringLiteral("未知");
    }
}

QColor orderStatusColor(int status)
{
    switch (status) {
    case 0: return AdminTheme::Amber;
    case 1: return AdminTheme::Blue;
    case 2: return AdminTheme::Green;
    case 3: return AdminTheme::TextSub;
    default: return AdminTheme::TextSub;
    }
}

} // namespace Utils
