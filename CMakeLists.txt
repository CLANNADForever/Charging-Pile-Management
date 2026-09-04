cmake_minimum_required(VERSION 3.16)
project(charging-pile VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets)

add_executable(charging-pile
    src/main.cpp
    src/mainwindow.cpp
    src/mainwindow.h
)
target_link_libraries(charging-pile PRIVATE Qt6::Widgets)
