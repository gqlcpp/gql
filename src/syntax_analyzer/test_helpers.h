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

#pragma once

#include <functional>

#include "gql/ast/nodes/types.h"

namespace gql::test {

class Field {
 public:
  Field(const char* name);

  static Field Any();

  Field& NodeReference();
  Field& EdgeReference();
  Field& Unconditional();
  Field& Conditional();
  Field& NodeReferenceGroup();

 private:
  friend class ParseResult;

  using FieldCheck = std::function<std::string(const ast::ValueType&)>;
  std::vector<FieldCheck> checks_;
  std::string name_;
  bool anyField_ = false;
};

class TestContext {
 protected:
  TestContext() = default;
  TestContext(const char* file, int line);

  const char* testFile_;
  int testLine_;
};

class ParseResult : public TestContext {
 public:
  class ErrorWrapper : TestContext {
   public:
    ErrorWrapper(const TestContext& ctx);

    void Contains(const char* substr);
    void Set(const std::string& message, const std::string& formattedError);
    void SetFinalText(const std::string&);

   private:
    friend class ParseResult;

    bool isSet_ = false;
    std::string message_;
    std::string formattedError_;
    std::string finalText_;
  };

  ParseResult(const char* file, int line);
  ~ParseResult();

  ErrorWrapper ExpectError();
  ParseResult& ExpectSuccess();

  ParseResult& ExpectOmittedResult();
  ParseResult& ExpectTableResult();
  ParseResult& WithFields(std::initializer_list<Field> fields);

  ParseResult& ExpectFinalText(const char*);

  void ExpectErrorContaining(const char* substr) {
    ExpectError().Contains(substr);
  }

 private:
  friend ParseResult ParseProgram(const char*, int, const char*);

  std::optional<ErrorWrapper> error_;
  std::optional<ast::FieldTypeList> result_;
  std::string rewrittenProgramText_;
  bool isErrorChecked_ = false;
  bool skipChecks_ = false;
};

ParseResult ParseProgram(const char* file, int line, const char* txt);

}  // namespace gql::test

#define GQL_TEST_PARSE(txt) ::gql::test::ParseProgram(__FILE__, __LINE__, txt)
