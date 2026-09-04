find_package(Qt6 REQUIRED COMPONENTS Test Widgets)

add_executable(ncs_tests
    tst_common.cpp
    ../src/client_user/views/MainWindow.cpp
    ../src/client_user/views/MainWindow.h
)
target_include_directories(ncs_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/src/common
    ${CMAKE_SOURCE_DIR}/src/client_user
)
target_link_libraries(ncs_tests PRIVATE Qt6::Test Qt6::Widgets ncs_common)

add_test(NAME ncs_offscreen COMMAND ncs_tests)
set_tests_properties(ncs_offscreen PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
