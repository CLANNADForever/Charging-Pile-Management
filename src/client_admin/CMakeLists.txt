# 管理端(B1)：PC 宽屏。共享 HttpJsonClient(ncs_shared) + 实体(ncs_common)。
add_executable(ncs_admin
    main.cpp
    AdminApi.cpp
    AdminApi.h
)
target_include_directories(ncs_admin PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(ncs_admin PRIVATE
    Qt6::Widgets
    ncs_shared
    ncs_common
)
