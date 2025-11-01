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

#include "test_helpers.h"

#include <gtest/gtest.h>
#include <algorithm>
#include <unordered_set>

#include "gql/ast/print.h"
#include "gql/parser/parser.h"
#include "syntax_analyzer.h"

namespace gql::test {

static std::string ToLower(std::string text) {
  std::transform(text.begin(), text.end(), text.begin(),
                 [](unsigned char c) { return std::tolower(c); });
  return text;
}

// static
Field Field::Any() {
  auto f = Field("");
  f.anyField_ = true;
  return f;
}

Field::Field(const char* name) : name_(name) {}

Field& Field::NodeReference() {
  checks_.push_back([](const ast::ValueType& type) -> std::string {
    if (!std::holds_alternative<ast::NodeReferenceValueType>(type.typeOption)) {
      return "Expected a node reference";
    }
    return {};
  });
  return *this;
}

Field& Field::EdgeReference() {
  checks_.push_back([](const ast::ValueType& type) -> std::string {
    if (!std::holds_alternative<ast::EdgeReferenceValueType>(type.typeOption)) {
      return "Expected an edge reference";
    }
    return {};
  });
  return *this;
}

Field& Field::NodeReferenceGroup() {
  // TODO: Implement group type check
  return *this;
}

Field& Field::Unconditional() {
  checks_.push_back([](const ast::ValueType& type) -> std::string {
    if (!type.notNull) {
      return "Expected material type";
    }
    return {};
  });
  return *this;
}

Field& Field::Conditional() {
  checks_.push_back([](const ast::ValueType& type) -> std::string {
    if (type.notNull) {
      return "Expected nullable type";
    }
    return {};
  });
  return *this;
}

#define GQL_TEST_FAILURE(msg)                  \
  GTEST_MESSAGE_AT_(testFile_, testLine_, msg, \
                    ::testing::TestPartResult::kNonFatalFailure)

TestContext::TestContext(const char* file, int line)
    : testFile_(file), testLine_(line) {}

ParseResult::ErrorWrapper::ErrorWrapper(const TestContext& ctx)
    : TestContext(ctx) {}

void ParseResult::ErrorWrapper::Set(const std::string& message,
                                    const std::string& formattedError) {
  message_ = message;
  formattedError_ = formattedError;
  isSet_ = true;
}

void ParseResult::ErrorWrapper::SetFinalText(const std::string& text) {
  finalText_ = text;
}

void ParseResult::ErrorWrapper::Contains(const char* substr) {
  if (!isSet_) {
    return;
  }

  auto lowerMessage = ToLower(message_);
  auto lowerSubstr = ToLower(substr);
  if (lowerMessage.find(lowerSubstr) == std::string::npos) {
    GQL_TEST_FAILURE("Expected error containing substring")
        << "Expected substring '" << substr << "' but got '" << message_ << "'";
  }
}

ParseResult::ParseResult(const char* file, int line)
    : TestContext(file, line) {}

ParseResult::~ParseResult() {
  if (!isErrorChecked_) {
    ExpectSuccess();
  }
}

// Expect that an error is present
ParseResult::ErrorWrapper ParseResult::ExpectError() {
  if (skipChecks_) {
    return ErrorWrapper(*this);
  }

  isErrorChecked_ = true;
  if (!error_.has_value()) {
    GQL_TEST_FAILURE("Expected error");
    return ErrorWrapper(*this);
  }
  return *error_;
}

ParseResult& ParseResult::ExpectSuccess() {
  if (skipChecks_) {
    return *this;
  }

  if (error_.has_value()) {
    std::stringstream str;
    str << error_->formattedError_;
    if (!error_->finalText_.empty()) {
      str << "\nProgram final text:\n" << error_->finalText_;
    }
    GQL_TEST_FAILURE("Unexpected error") << str.str();
    skipChecks_ = true;
  }
  return *this;
}

// Expect that the result is omitted (empty)
ParseResult& ParseResult::ExpectOmittedResult() {
  ExpectSuccess();

  if (skipChecks_) {
    return *this;
  }

  if (result_.has_value()) {
    GQL_TEST_FAILURE("Omitted result expected");
    skipChecks_ = true;
  }
  return *this;
}

// Expect that the result is a table (not empty)
ParseResult& ParseResult::ExpectTableResult() {
  ExpectSuccess();

  if (skipChecks_) {
    return *this;
  }

  if (!result_.has_value()) {
    GQL_TEST_FAILURE("Table result expected");
    skipChecks_ = true;
  }
  return *this;
}

// Expect that the result table has specific fields
ParseResult& ParseResult::WithFields(std::initializer_list<Field> fields) {
  ExpectTableResult();

  if (skipChecks_) {
    return *this;
  }

  const auto& resultTable = *result_;
  std::unordered_set<std::string> expectedFields;
  bool hasAnyField = false;
  for (const auto& field : fields) {
    if (field.anyField_) {
      hasAnyField = true;
      continue;
    }

    if (!expectedFields.insert(field.name_).second) {
      GQL_TEST_FAILURE("Duplicate field name")
          << "Field name '" << field.name_ << "'";
      skipChecks_ = true;
      return *this;
    }
    auto resultIt =
        std::find_if(resultTable.begin(), resultTable.end(),
                     [&](const auto& f) { return f.name.name == field.name_; });
    if (resultIt == resultTable.end()) {
      GQL_TEST_FAILURE("Expected field not found in the result")
          << "Field name '" << field.name_ << "'";
      skipChecks_ = true;
      return *this;
    }

    for (auto& c : field.checks_) {
      auto message = c(*resultIt->type);
      if (!message.empty()) {
        GQL_TEST_FAILURE("Field check failed")
            << "Field name '" << field.name_ << "': " << message;
        skipChecks_ = true;
        return *this;
      }
    }
  }

  if (!hasAnyField && resultTable.size() > expectedFields.size()) {
    for (auto& f : resultTable) {
      if (!expectedFields.count(f.name.name)) {
        GQL_TEST_FAILURE("Unexpected field found in the result")
            << "Field name '" << f.name.name << "'";
        skipChecks_ = true;
        return *this;
      }
    }
  }

  return *this;
}

ParseResult& ParseResult::ExpectFinalText(const char* expectedText) {
  ExpectSuccess();

  if (skipChecks_) {
    return *this;
  }

  if (rewrittenProgramText_ != expectedText) {
    GQL_TEST_FAILURE("Rewritten program text does not match expected")
        << "Expected:\n"
        << expectedText << "\n"
        << "Actual:\n"
        << rewrittenProgramText_;
    skipChecks_ = true;
  }

  return *this;
}

static std::string FormatErrorMessage(const char* query,
                                      const gql::ParserError& e) {
  std::stringstream str;
  if (e.inputPosition().IsSet()) {
    str << "Error in query at position " << e.inputPosition().line << ":"
        << e.inputPosition().col << "\n"
        << FormatErrorMessageInQuery(query, e.inputPosition(), e.message());
  } else {
    str << e.what() << " in query:\n" << query;
  }
  return str.str();
}

ParseResult ParseProgram(const char* file, int line, const char* query) {
  ParseResult result(file, line);
  gql::ast::GQLProgram program;
  std::string printedProgram;
  try {
    program = gql::parser::ParseProgram(query);
    printedProgram = gql::ast::PrintTree(program);

    gql::SyntaxAnalyzer syntax_analyzer;
    result.result_ = syntax_analyzer.Process(program);
    result.rewrittenProgramText_ = gql::ast::PrintTree(program);
    {
      // Check that rewrite by SyntaxAnalyzer only happens once and that no
      // errors are introduced by rewrite
      gql::SyntaxAnalyzer syntax_analyzer2;
      EXPECT_NO_THROW(syntax_analyzer2.Process(program));
      EXPECT_EQ(result.rewrittenProgramText_, gql::ast::PrintTree(program));
    }
  } catch (const gql::ParserError& e) {
    result.error_.emplace(result).Set(e.message(),
                                      FormatErrorMessage(query, e));
    if (!printedProgram.empty()) {
      // If it's not a parse error and the program was rewritten pass final text
      // to the error wrapper
      auto printedFinalProgram = gql::ast::PrintTree(program);
      if (printedFinalProgram != printedProgram) {
        result.error_->SetFinalText(printedFinalProgram);
      }
    }
  }
  return result;
}

}  // namespace gql::test
