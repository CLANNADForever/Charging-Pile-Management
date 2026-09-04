add_executable(ncs_user
    main.cpp
    services/IUserService.h
    services/MockUserService.h
    services/MockUserService.cpp
    views/MainWindow.cpp
    views/MainWindow.h
    views/LoginPage.cpp
    views/LoginPage.h
    views/ProfilePage.cpp
    views/ProfilePage.h
)
target_include_directories(ncs_user PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(ncs_user PRIVATE Qt6::Widgets ncs_common)
