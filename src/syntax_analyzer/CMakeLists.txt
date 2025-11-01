set(SOURCES
  catalog_modifying.cpp
  data_modifying.cpp
  gql_program.cpp
  graph_pattern_context.cpp
  graph_pattern_context.h
  graph_pattern.cpp
  helpers.h
  match.cpp
  match_utils.cpp
  match_utils.h
  path_pattern.cpp
  procedure.cpp
  query.cpp
  reference.cpp
  result.cpp
  select.cpp
  type_helpers.cpp
  type_helpers.h
  value_expression.cpp
  value_type.cpp
  syntax_analyzer.cpp
  syntax_analyzer.h
  ../../include/gql/syntax_analyzer/ast_node_base.h
  ../../include/gql/syntax_analyzer/aux_data.h
  ../../include/gql/syntax_analyzer/defs.h
)

add_library(gql_syntax_analyzer STATIC
  ${SOURCES}
)

add_library(gql_syntax_analyzer_component INTERFACE)

target_sources(gql_syntax_analyzer_component
  INTERFACE
  ${SOURCES}
)

target_link_libraries(gql_syntax_analyzer
  PRIVATE
  gql_common
  gql_rewrite
)

add_executable(syntax_analyzer_file_test
  syntax_analyzer_file_test.cpp
)

add_executable(syntax_analyzer_test
  graph_pattern_test.cpp
  select_test.cpp
  test_helpers.cpp
  test_helpers.h
)

include(GoogleTest)
gtest_discover_tests(syntax_analyzer_test)

target_link_libraries(syntax_analyzer_file_test
  PRIVATE
  gql_parser
  gql_syntax_analyzer
  GTest::gtest_main
  GTest::gmock_main
)

target_link_libraries(syntax_analyzer_test
  PRIVATE
  gql_ast_print
  gql_parser
  gql_syntax_analyzer
  GTest::gtest_main
  GTest::gmock_main
)
