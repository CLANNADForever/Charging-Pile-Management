cmake_minimum_required(VERSION 3.16)
project(charging-pile VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Widgets Charts Network WebEngineWidgets WebChannel Multimedia MultimediaWidgets)

add_subdirectory(third_party/zxing-cpp/core)
add_subdirectory(src)

enable_testing()
add_subdirectory(tests)
