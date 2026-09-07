# C 端(新 UI)可执行 ncs_user；数据层 ncs_front_core。
add_executable(ncs_user
    main.cpp

    ui/MainWindow.h
    ui/MainWindow.cpp
    ui/NavButton.h
    ui/NavButton.cpp
    ui/Page.h
    ui/Page.cpp
    ui/LoginWindow.h
    ui/LoginWindow.cpp
    ui/UserCenterPage.h
    ui/UserCenterPage.cpp
    ui/OrderListPage.h
    ui/OrderListPage.cpp
    ui/OrderDetailPage.h
    ui/OrderDetailPage.cpp
    ui/CouponPage.h
    ui/CouponPage.cpp
    ui/OrderSettlePage.h
    ui/OrderSettlePage.cpp
    ui/StationCard.h
    ui/StationCard.cpp
    ui/StationListPage.h
    ui/StationListPage.cpp
    ui/SearchPage.h
    ui/SearchPage.cpp
    ui/StationDetailPage.h
    ui/StationDetailPage.cpp
    ui/MapWidget.h
    ui/MapWidget.cpp
    ui/ChargeEntryPage.h
    ui/ChargeEntryPage.cpp
    ui/ChargePage.h
    ui/ChargePage.cpp
    ui/MapPage.h
    ui/MapPage.cpp
    theme/Theme.h
    theme/Theme.cpp
    common/Toast.h
    common/Toast.cpp
    common/Utils.h
    common/Utils.cpp
    resources/resources.qrc
)
target_include_directories(ncs_user PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(ncs_user PRIVATE ncs_front_core Qt6::Widgets)
