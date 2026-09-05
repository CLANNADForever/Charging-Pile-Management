# 后端 HTTP 服务。说明：本服务不含 GUI，仅依赖 Qt Core/Sql
# (实体与持久层复用了 Qt 类型)；HTTP/JSON 用 vendored 的 cpp-httplib + nlohmann。
find_package(Qt6 REQUIRED COMPONENTS Sql)

# 核心 + HTTP 装配层做成库，供 ncs_server(main) 与后端测试共用
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
    Qt6::Sql
    ncs_common
    Threads::Threads
)

add_executable(ncs_server main.cpp)
target_link_libraries(ncs_server PRIVATE ncs_server_lib)
