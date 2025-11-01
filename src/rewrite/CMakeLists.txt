set(SOURCES
    element_patterns.cpp
    element_pattern_where.cpp
    element_property_predicate.cpp
    exists_predicate.cpp
    helpers.h
    path_pattern_prefix.cpp
    simplified_path_pattern.cpp
)

add_library(gql_rewrite STATIC
    ${SOURCES}
)

add_library(gql_rewrite_component INTERFACE)

target_sources(gql_rewrite_component
    INTERFACE
    ${SOURCES}
)

target_link_libraries(gql_rewrite
    gql_ast
    gql_common
)

add_executable(gql_rewrite_test
    rewrite_unittest.cpp
)

target_link_libraries(gql_rewrite_test
    gql_ast_print
    gql_ast_test_utils
    gql_parser
    gql_rewrite
    GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(gql_rewrite_test)

