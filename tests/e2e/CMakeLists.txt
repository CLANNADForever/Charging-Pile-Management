# 端到端：起真实后端(BackendApp)线程 + HttpUserService(Qt 客户端)打它
find_package(Qt6 REQUIRED COMPONENTS Test)
add_executable(ncs_e2e_tests
    tst_e2e.cpp
)
target_link_libraries(ncs_e2e_tests PRIVATE
    Qt6::Test
    ncs_user_ui
    ncs_server_lib
)
add_test(NAME ncs_e2e COMMAND ncs_e2e_tests)
set_tests_properties(ncs_e2e PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
