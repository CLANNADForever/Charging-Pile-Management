# 模拟器：纯 C++(无 Qt)，便于将来搬板子/容器。协议=TCP JSON-lines 心跳。
add_executable(ncs_simulator main.cpp)
target_include_directories(ncs_simulator PRIVATE ${CMAKE_SOURCE_DIR}/third_party)
target_compile_features(ncs_simulator PRIVATE cxx_std_17)
