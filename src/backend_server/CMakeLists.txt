# 后端 HTTP + 模拟器 TCP。不含 GUI；实体 Qt 类型(Qt Core)；DB=SQLite C API。
add_library(ncs_server_lib STATIC
    BackendApp.h
    BackendApp.cpp
    core/AuthService.h
    core/AuthService.cpp
    core/HeartbeatSink.h
    core/HeartbeatLogSink.h
    core/HeartbeatLogSink.cpp
    database/Store.h
    database/Store.cpp
)
target_include_directories(ncs_server_lib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/third_party
)
target_link_libraries(ncs_server_lib PUBLIC
    Qt6::Core
    ncs_common
    Threads::Threads
    sqlite3
)

add_executable(ncs_server main.cpp)
target_link_libraries(ncs_server PRIVATE ncs_server_lib)
