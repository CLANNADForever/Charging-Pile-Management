// 手机号校验与演示短信码：前后端共用同一套规则。
#ifndef NCS_COMMON_PHONE_H
#define NCS_COMMON_PHONE_H

#include <QString>

namespace ncs {

// 11 位大陆手机号(1 开头)
bool is_valid_phone11(const QString& phone);

// 演示固定短信验证码(模拟下发；Mock 与后端一致)
QString demo_sms_code();

}  // namespace ncs

#endif  // NCS_COMMON_PHONE_H
