find_package(Qt6 REQUIRED COMPONENTS Test)

# 链接与 ncs_user 同一个 ncs_user_ui 库，不再重复编译 C 端源码
add_executable(ncs_tests
    tst_common.cpp
)
target_include_directories(ncs_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/src/common
)
target_link_libraries(ncs_tests PRIVATE Qt6::Test ncs_user_ui)

add_test(NAME ncs_offscreen COMMAND ncs_tests)
set_tests_properties(ncs_offscreen PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_OPENGL=software;LIBGL_ALWAYS_SOFTWARE=1;QTWEBENGINE_DISABLE_SANDBOX=1")

add_subdirectory(server)

add_subdirectory(e2e)
