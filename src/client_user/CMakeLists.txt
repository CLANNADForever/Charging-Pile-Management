add_executable(ncs_user
    main.cpp
    views/MainWindow.cpp
    views/MainWindow.h
)
target_include_directories(ncs_user PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(ncs_user PRIVATE Qt6::Widgets ncs_common)
