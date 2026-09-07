#include "Utils.h"

#include "theme/Theme.h"

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

QString formatDuration(int totalSeconds)
{
    const int h = totalSeconds / 3600;
    const int m = (totalSeconds % 3600) / 60;
    const int s = totalSeconds % 60;
    return QStringLiteral("%1:%2:%3")
        .arg(h, 2, 10, QLatin1Char('0'))
        .arg(m, 2, 10, QLatin1Char('0'))
        .arg(s, 2, 10, QLatin1Char('0'));
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
    case 0: return Theme::Warning;
    case 1: return Theme::ElectricBlue;
    case 2: return Theme::Success;
    case 3: return Theme::TextSecondary;
    default: return Theme::TextSecondary;
    }
}

} // namespace Utils
