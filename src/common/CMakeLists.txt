set(SOURCES
    error.cpp
    features.cpp
    formatted_error.h
)

add_library(gql_common STATIC
    ${SOURCES}
)

add_library(gql_common_component INTERFACE)

target_sources(gql_common_component
    INTERFACE
    ${SOURCES}
)

target_include_directories(gql_common
    PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/fmt/include
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_include_directories(gql_common_component
    INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/fmt/include
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(gql_common fmt::fmt)
target_link_libraries(gql_common_component INTERFACE fmt::fmt-header-only)
