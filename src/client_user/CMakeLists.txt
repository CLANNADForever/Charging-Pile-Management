# C 端逻辑与界面库：services/ + views/（main 之外的整套 C 端可复用层）
add_library(ncs_user_ui STATIC
    services/IUserService.h
    services/MockUserService.h
    services/MockUserService.cpp
    views/MainWindow.cpp
    views/MainWindow.h
    views/LoginPage.cpp
    views/LoginPage.h
    views/ProfilePage.cpp
    views/ProfilePage.h
)
target_include_directories(ncs_user_ui PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(ncs_user_ui PUBLIC Qt6::Widgets ncs_common)

# 薄 main：只负责装配并启动
add_executable(ncs_user main.cpp)
target_link_libraries(ncs_user PRIVATE ncs_user_ui)
