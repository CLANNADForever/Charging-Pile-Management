# 共享层：跨端 HTTP JSON 客户端(Qt Core/Network)。C/admin 共用一份，勿复制。
find_package(Qt6 REQUIRED COMPONENTS Core Network)
add_library(ncs_shared STATIC
    net/HttpJsonClient.h
    net/HttpJsonClient.cpp
)
target_include_directories(ncs_shared PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/net)
target_link_libraries(ncs_shared PUBLIC Qt6::Core Qt6::Network)
