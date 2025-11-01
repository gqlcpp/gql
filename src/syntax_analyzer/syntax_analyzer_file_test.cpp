// Copyright 2025 Oleg Maximenko
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>
#include <regex>

#include "gql/ast/ast.h"
#include "gql/parser/parser.h"
#include "syntax_analyzer.h"

class GQLFileTest : public testing::TestWithParam<std::filesystem::path> {};

TEST_P(GQLFileTest, Verify) {
  std::ifstream file(GetParam(), std::ios::binary);
  std::string query(std::istreambuf_iterator<char>(file), {});

  gql::ast::GQLProgram program;
  try {
    program = gql::parser::ParseProgram(query.c_str());
  } catch (const gql::ParserError& e) {
    if (e.inputPosition().IsSet()) {
      GTEST_FAIL() << "Error in query at position " << e.inputPosition().line
                   << ":" << e.inputPosition().col << "\n"
                   << gql::FormatErrorMessageInQuery(query, e.inputPosition(),
                                                     e.message());
    } else {
      GTEST_FAIL() << e.what() << " in query:\n" << query;
    }
  } catch (const std::runtime_error& e) {
    GTEST_FAIL() << e.what() << " in query:\n" << query;
  }

  gql::SyntaxAnalyzer syntax_analyzer;
  syntax_analyzer.Process(program);
}

std::string GetTestName(
    const testing::TestParamInfo<std::filesystem::path>& info) {
  std::regex unsupportedSymbols("[^a-zA-Z0-9_]");
  return std::regex_replace(info.param.filename().string(), unsupportedSymbols,
                            "_");
}

std::vector<std::filesystem::path> GetTestFiles() {
  std::vector<std::filesystem::path> files;

  constexpr const char kEnvVarName[] = "GQL_TEST_QUERY_DIR";
  const char* path = std::getenv(kEnvVarName);
  if (!path) {
    return {};
  }

  const std::filesystem::path dir(path);
  for (auto const& dir_entry : std::filesystem::directory_iterator{dir})
    if (dir_entry.is_regular_file()) {
      files.push_back(dir_entry.path());
    }
  return files;
}

INSTANTIATE_TEST_SUITE_P(,
                         GQLFileTest,
                         testing::ValuesIn(GetTestFiles()),
                         GetTestName);

GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(GQLFileTest);