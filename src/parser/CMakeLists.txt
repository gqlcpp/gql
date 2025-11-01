add_subdirectory(antlr4_runtime)

# Just to show header files in the IDE
file(GLOB_RECURSE GQL_HEADER_FILES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}/../../include/gql ${CMAKE_CURRENT_SOURCE_DIR}/../../include/gql/*.*)

foreach(i ${GQL_HEADER_FILES})
  get_filename_component(_dir ${i} PATH)
  set(_pregroup "Headers/${_dir}")
  set(_name "../../include/gql/${i}")
  list(APPEND GQL_HEADERS
    ${_name}
  )
  string(REPLACE "/" "\\" _group ${_pregroup})
  source_group(${_group} FILES ${_name})
endforeach()

set(SOURCES
  ${GQL_HEADERS}
  ast_builder.cpp
  generated/GQLLexer.cpp
  generated/GQLParser.cpp
  parser.cpp
  unescape_string.cpp
)

add_library(gql_parser STATIC
  ${SOURCES}
)

add_library(gql_parser_component INTERFACE)

target_sources(gql_parser_component
  INTERFACE
  ${SOURCES}
)

target_include_directories(gql_parser
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(gql_parser
  PUBLIC
  gql_ast
  PRIVATE
  antlr4_static
  gql_common
)

target_link_libraries(gql_parser_component
  INTERFACE
  antlr4_component
  gql_ast
)

add_executable(gql_parser_test
  parse_file_test.cpp
  parser_test.cpp
  parser_unittest.cpp
  traverse_expression_unittest.cpp
)

if(0)
  if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_link_options(gql_parser_test PRIVATE -Wl,--gc-sections -Wl,--print-gc-sections)
  elseif(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
    target_link_options(gql_parser_test PRIVATE /MAP:linker_map.txt)
    target_link_options(gql_parser_test PRIVATE /OPT:REF /VERBOSE)
  endif()
endif()

target_link_libraries(gql_parser_test
  gql_parser
  gql_ast_print
  gql_ast_test_utils
  antlr4_static
  GTest::gtest_main
  GTest::gmock_main
)

target_include_directories(gql_parser_test
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
)

include(GoogleTest)
gtest_discover_tests(gql_parser_test)
