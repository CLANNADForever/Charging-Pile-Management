find_package(Qt6 REQUIRED COMPONENTS Network)

# C 端逻辑与界面库：services/ + views/
add_library(ncs_user_ui STATIC
    services/IUserService.h
    services/MockUserService.h
    services/MockUserService.cpp
    services/HttpUserService.h
    services/HttpUserService.cpp
    services/HttpJsonClient.h
    services/HttpJsonClient.cpp
    services/IStationService.h
    services/IChargeService.h
    services/HttpChargeService.h
    services/HttpChargeService.cpp
    services/HttpStationService.h
    services/HttpStationService.cpp
    views/MainWindow.cpp
    views/MainWindow.h
    views/LoginPage.cpp
    views/LoginPage.h
    views/ProfilePage.cpp
    views/ProfilePage.h
    views/StationListPage.cpp
    views/StationListPage.h
    views/StationDetailPage.cpp
    views/StationDetailPage.h
    views/ChargePage.cpp
    views/ChargePage.h
    views/MySessionsPage.cpp
    views/MySessionsPage.h
)
target_include_directories(ncs_user_ui PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(ncs_user_ui PUBLIC Qt6::Widgets Qt6::Network ncs_common)

add_executable(ncs_user main.cpp)
target_link_libraries(ncs_user PRIVATE ncs_user_ui)
