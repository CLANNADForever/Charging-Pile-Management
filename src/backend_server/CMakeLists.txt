# 后端 HTTP 服务。不含 GUI；实体沿用 Qt 类型(链 Qt Core)，DB 用系统 SQLite C API，
# HTTP/JSON 用 vendored 的 cpp-httplib + nlohmann。
add_library(ncs_server_lib STATIC
    BackendApp.h
    BackendApp.cpp
    core/AuthService.h
    core/AuthService.cpp
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
