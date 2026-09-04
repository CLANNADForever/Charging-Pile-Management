add_library(ncs_common STATIC
    money.h
    money.cpp
    entities.h
    entities.cpp
)
target_include_directories(ncs_common PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(ncs_common PUBLIC Qt6::Core)
