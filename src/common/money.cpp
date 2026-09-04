#include "money.h"

namespace ncs {

QString format_cents(MoneyCents cents) {
    const bool neg = cents < 0;
    const MoneyCents absVal = neg ? -cents : cents;
    const MoneyCents yuan = absVal / 100;
    const int fen = static_cast<int>(absVal % 100);
    QString s = QString::number(yuan) + QLatin1Char('.') +
                QString::number(fen).rightJustified(2, QLatin1Char('0'));
    return neg ? QLatin1Char('-') + s : s;
}

}  // namespace ncs
