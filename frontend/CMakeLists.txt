# 新前端整合：core 共享库 + C/B 端可执行(ncs_user / ncs_admin，归位名)。
# 数据层走本仓库后端 REST；UI 仅按任务书接线。
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Charts Network)

# ===== core 共享库：Service(同步走后端) + BackendClient =====
add_library(ncs_front_core STATIC
    core/service/AdminService.cpp
    core/service/ChargeService.cpp
    core/service/PredictService.cpp
    core/service/StationService.cpp
    core/service/StatsService.cpp
    core/service/UserService.cpp
    core/net/BackendClient.cpp
)
target_include_directories(ncs_front_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(ncs_front_core PUBLIC Qt6::Core Qt6::Gui Qt6::Network)

# ============ C 端可执行程序 ncs_user ============
add_executable(ncs_user
    client_user/main.cpp
    client_user/ui/MainWindow.h
    client_user/ui/MainWindow.cpp
    client_user/ui/NavButton.h
    client_user/ui/NavButton.cpp
    client_user/ui/Page.h
    client_user/ui/Page.cpp
    client_user/ui/LoginWindow.h
    client_user/ui/LoginWindow.cpp
    client_user/ui/UserCenterPage.h
    client_user/ui/UserCenterPage.cpp
    client_user/ui/OrderListPage.h
    client_user/ui/OrderListPage.cpp
    client_user/ui/OrderDetailPage.h
    client_user/ui/OrderDetailPage.cpp
    client_user/ui/CouponPage.h
    client_user/ui/CouponPage.cpp
    client_user/ui/OrderSettlePage.h
    client_user/ui/OrderSettlePage.cpp
    client_user/ui/StationCard.h
    client_user/ui/StationCard.cpp
    client_user/ui/StationListPage.h
    client_user/ui/StationListPage.cpp
    client_user/ui/SearchPage.h
    client_user/ui/SearchPage.cpp
    client_user/ui/StationDetailPage.h
    client_user/ui/StationDetailPage.cpp
    client_user/ui/MapWidget.h
    client_user/ui/MapWidget.cpp
    client_user/ui/ChargeEntryPage.h
    client_user/ui/ChargeEntryPage.cpp
    client_user/ui/ChargePage.h
    client_user/ui/ChargePage.cpp
    client_user/ui/MapPage.h
    client_user/ui/MapPage.cpp
    client_user/theme/Theme.h
    client_user/theme/Theme.cpp
    client_user/common/Toast.h
    client_user/common/Toast.cpp
    client_user/common/Utils.h
    client_user/common/Utils.cpp
    client_user/resources/resources.qrc
)
target_include_directories(ncs_user PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/client_user)
target_link_libraries(ncs_user PRIVATE ncs_front_core Qt6::Widgets)

# ============ B 端可执行程序 ncs_admin ============
add_executable(ncs_admin
    client_admin/main.cpp
    client_admin/ui/AdminPage.h
    client_admin/ui/AdminLoginWindow.h
    client_admin/ui/AdminLoginWindow.cpp
    client_admin/ui/AdminMainWindow.h
    client_admin/ui/AdminMainWindow.cpp
    client_admin/ui/RevenuePage.h
    client_admin/ui/RevenuePage.cpp
    client_admin/ui/ChargerStatusPage.h
    client_admin/ui/ChargerStatusPage.cpp
    client_admin/ui/ChargerPage.h
    client_admin/ui/ChargerPage.cpp
    client_admin/ui/StationPage.h
    client_admin/ui/StationPage.cpp
    client_admin/ui/UserPage.h
    client_admin/ui/UserPage.cpp
    client_admin/ui/PredictPage.h
    client_admin/ui/PredictPage.cpp
    client_admin/theme/AdminTheme.h
    client_admin/theme/AdminTheme.cpp
    client_admin/common/Toast.h
    client_admin/common/Toast.cpp
    client_admin/common/Utils.h
    client_admin/common/Utils.cpp
    client_admin/resources/resources.qrc
)
target_include_directories(ncs_admin PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/client_admin)
target_link_libraries(ncs_admin PRIVATE ncs_front_core Qt6::Widgets Qt6::Charts)
