#include "phone.h"

#include <QRegularExpression>

namespace ncs {

bool is_valid_phone11(const QString& phone) {
    static const QRegularExpression re(QStringLiteral("^1[0-9]{10}$"));
    return re.match(phone).hasMatch();
}

QString demo_sms_code() {
    return QStringLiteral("123456");
}

}  // namespace ncs
