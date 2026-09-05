# 后端测试：直测 Store/AuthService + 起真实 HTTP 服务用 httplib 客户端打接口。
add_executable(ncs_server_tests
    test_server.cpp
)
target_link_libraries(ncs_server_tests PRIVATE ncs_server_lib)
add_test(NAME ncs_backend COMMAND ncs_server_tests)
