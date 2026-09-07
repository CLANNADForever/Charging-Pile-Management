# ncs_front_core：新前端数据层(同步走后端 REST)，C/B 端共用。
set(CMAKE_AUTOMOC ON)
add_library(ncs_front_core STATIC
    service/AdminService.cpp
    service/ChargeService.cpp
    service/PredictService.cpp
    service/StationService.cpp
    service/StatsService.cpp
    service/UserService.cpp
    net/BackendClient.cpp
)
# 头以 "core/..." / "core/service/..." 引用 → include 指向 src 根
target_include_directories(ncs_front_core PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(ncs_front_core PUBLIC Qt6::Core Qt6::Gui Qt6::Network)
