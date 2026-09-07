find_package(Qt6 REQUIRED COMPONENTS Test Widgets)

add_executable(ncs_tests
    tst_common.cpp
)
target_include_directories(ncs_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/src/common
    ${CMAKE_SOURCE_DIR}/src/backend_server
)
target_link_libraries(ncs_tests PRIVATE
    Qt6::Test Qt6::Widgets ncs_front_core ncs_server_lib
)
add_test(NAME ncs_offscreen COMMAND ncs_tests)
set_tests_properties(ncs_offscreen PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_OPENGL=software;LIBGL_ALWAYS_SOFTWARE=1;QTWEBENGINE_DISABLE_SANDBOX=1")

add_subdirectory(server)
