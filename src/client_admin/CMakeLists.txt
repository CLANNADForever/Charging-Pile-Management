# B 端(新 UI)可执行 ncs_admin；数据层 ncs_front_core。
add_executable(ncs_admin
    main.cpp
    ui/AdminPage.h
    ui/AdminLoginWindow.h
    ui/AdminLoginWindow.cpp
    ui/AdminMainWindow.h
    ui/AdminMainWindow.cpp
    ui/RevenuePage.h
    ui/RevenuePage.cpp
    ui/ChargerStatusPage.h
    ui/ChargerStatusPage.cpp
    ui/ChargerPage.h
    ui/ChargerPage.cpp
    ui/StationPage.h
    ui/StationPage.cpp
    ui/UserPage.h
    ui/UserPage.cpp
    ui/PredictPage.h
    ui/PredictPage.cpp
    theme/AdminTheme.h
    theme/AdminTheme.cpp
    common/Toast.h
    common/Toast.cpp
    common/Utils.h
    common/Utils.cpp
    resources/resources.qrc
)
target_include_directories(ncs_admin PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(ncs_admin PRIVATE ncs_front_core Qt6::Widgets Qt6::Charts)
