add_library(gql_shared SHARED)

target_link_libraries(gql_shared
  PUBLIC
  gql_ast
  PRIVATE
  gql_ast_print_component
  gql_common_component
  gql_parser_component
  gql_rewrite_component
  gql_syntax_analyzer_component
)

target_compile_definitions(gql_shared
  PRIVATE GQL_BUILD_SHARED
)

target_compile_definitions(gql_shared
  INTERFACE GQL_USE_SHARED
)

get_target_property(gql_shared_SOURCES gql_shared SOURCES)
source_group(
  TREE ${CMAKE_CURRENT_SOURCE_DIR}../..
  PREFIX Sources
  FILES ${gql_shared_SOURCES}
)
