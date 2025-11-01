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

#include "generated/GQLLexer.h"
#include "generated/GQLParser.h"
#include "gql/ast/print.h"
#include "gql/error.h"

namespace gql::parser::ast_builder {
void BuildAST(GQLParser::UnsignedNumericLiteralContext* ctx,
              ast::UnsignedNumericLiteral& value);
void BuildAST(GQLParser::CharacterStringLiteralContext* ctx,
              std::string& value);
void BuildAST(GQLParser::ValueTypeContext* ctx, ast::ValueType& value);
void BuildAST(GQLParser::ValueExpressionContext* ctx,
              ast::ValueExpression& value);
}  // namespace gql::parser::ast_builder

namespace gql {

using namespace antlr4;

namespace {

struct ParserWrapper {
  ParserWrapper(const char* query)
      : input(query), lexer(&input), tokens(&lexer), parser(&tokens) {
    parser.removeErrorListeners();
  }

  ANTLRInputStream input;
  GQLLexer lexer;
  CommonTokenStream tokens;

  GQLParser parser;
};

auto ParseUnsignedNumericLiteral(const char* query,
                                 size_t* numberOfSyntaxErrors) {
  ParserWrapper p(query);

  ast::UnsignedNumericLiteral value;
  parser::ast_builder::BuildAST(p.parser.unsignedNumericLiteral(), value);
  *numberOfSyntaxErrors = p.parser.getNumberOfSyntaxErrors();
  return value;
}

auto ParseCharacterStringLiteral(const char* query,
                                 size_t* numberOfSyntaxErrors) {
  ParserWrapper p(query);

  std::string value;
  parser::ast_builder::BuildAST(p.parser.characterStringLiteral(), value);
  *numberOfSyntaxErrors = p.parser.getNumberOfSyntaxErrors();
  return value;
}

}  // namespace

TEST(ParseNode, ExactNumericLiteral) {
  size_t numberOfSyntaxErrors;
  auto value = ParseUnsignedNumericLiteral("123", &numberOfSyntaxErrors);
  EXPECT_EQ(numberOfSyntaxErrors, 0);
  auto* exactValue = std::get_if<ast::ExactNumericLiteral>(&value);
  ASSERT_TRUE(exactValue);
  EXPECT_EQ(exactValue->value, 123);
  EXPECT_EQ(exactValue->scale, 0);

  value = ParseUnsignedNumericLiteral("0x1234abCD", &numberOfSyntaxErrors);
  EXPECT_EQ(numberOfSyntaxErrors, 0);
  exactValue = std::get_if<ast::ExactNumericLiteral>(&value);
  ASSERT_TRUE(exactValue);
  EXPECT_EQ(exactValue->value, 0x1234ABCD);
  EXPECT_EQ(exactValue->scale, 0);

  value = ParseUnsignedNumericLiteral("0X1234", &numberOfSyntaxErrors);
  EXPECT_EQ(numberOfSyntaxErrors, 0);
  // Not entire string is parsed, but there is no check for such case yet.
  exactValue = std::get_if<ast::ExactNumericLiteral>(&value);
  ASSERT_TRUE(exactValue);
  EXPECT_EQ(exactValue->value, 0);
  EXPECT_EQ(exactValue->scale, 0);

  value = ParseUnsignedNumericLiteral("0b010101010101001010101111",
                                      &numberOfSyntaxErrors);
  EXPECT_EQ(numberOfSyntaxErrors, 0);
  exactValue = std::get_if<ast::ExactNumericLiteral>(&value);
  ASSERT_TRUE(exactValue);
  EXPECT_EQ(exactValue->value, 0b010101010101001010101111);
  EXPECT_EQ(exactValue->scale, 0);

  value = ParseUnsignedNumericLiteral("1234.56M", &numberOfSyntaxErrors);
  EXPECT_EQ(numberOfSyntaxErrors, 0);
  exactValue = std::get_if<ast::ExactNumericLiteral>(&value);
  ASSERT_TRUE(exactValue);
  EXPECT_EQ(exactValue->value, 123456);
  EXPECT_EQ(exactValue->scale, 2);

  value = ParseUnsignedNumericLiteral("1234.56", &numberOfSyntaxErrors);
  EXPECT_EQ(numberOfSyntaxErrors, 0);
  exactValue = std::get_if<ast::ExactNumericLiteral>(&value);
  ASSERT_TRUE(exactValue);
  EXPECT_EQ(exactValue->value, 123456);
  EXPECT_EQ(exactValue->scale, 2);

  value = ParseUnsignedNumericLiteral(".123456", &numberOfSyntaxErrors);
  EXPECT_EQ(numberOfSyntaxErrors, 0);
  exactValue = std::get_if<ast::ExactNumericLiteral>(&value);
  ASSERT_TRUE(exactValue);
  EXPECT_EQ(exactValue->value, 123456);
  EXPECT_EQ(exactValue->scale, 6);

  value = ParseUnsignedNumericLiteral("123456.M", &numberOfSyntaxErrors);
  EXPECT_EQ(numberOfSyntaxErrors, 0);
  exactValue = std::get_if<ast::ExactNumericLiteral>(&value);
  ASSERT_TRUE(exactValue);
  EXPECT_EQ(exactValue->value, 123456);
  EXPECT_EQ(exactValue->scale, 0);

  value = ParseUnsignedNumericLiteral("123456E3M", &numberOfSyntaxErrors);
  EXPECT_EQ(numberOfSyntaxErrors, 0);
  exactValue = std::get_if<ast::ExactNumericLiteral>(&value);
  ASSERT_TRUE(exactValue);
  EXPECT_EQ(exactValue->value, 123456000);
  EXPECT_EQ(exactValue->scale, 0);

  value = ParseUnsignedNumericLiteral("123456E+3m", &numberOfSyntaxErrors);
  EXPECT_EQ(numberOfSyntaxErrors, 0);
  exactValue = std::get_if<ast::ExactNumericLiteral>(&value);
  ASSERT_TRUE(exactValue);
  EXPECT_EQ(exactValue->value, 123456000);
  EXPECT_EQ(exactValue->scale, 0);

  value = ParseUnsignedNumericLiteral("123456E-3M", &numberOfSyntaxErrors);
  EXPECT_EQ(numberOfSyntaxErrors, 0);
  exactValue = std::get_if<ast::ExactNumericLiteral>(&value);
  ASSERT_TRUE(exactValue);
  EXPECT_EQ(exactValue->value, 123456);
  EXPECT_EQ(exactValue->scale, 3);

  value = ParseUnsignedNumericLiteral("1234.56E+3M", &numberOfSyntaxErrors);
  EXPECT_EQ(numberOfSyntaxErrors, 0);
  exactValue = std::get_if<ast::ExactNumericLiteral>(&value);
  ASSERT_TRUE(exactValue);
  EXPECT_EQ(exactValue->value, 1234560);
  EXPECT_EQ(exactValue->scale, 0);

  value = ParseUnsignedNumericLiteral("1234.56E-3M", &numberOfSyntaxErrors);
  EXPECT_EQ(numberOfSyntaxErrors, 0);
  exactValue = std::get_if<ast::ExactNumericLiteral>(&value);
  ASSERT_TRUE(exactValue);
  EXPECT_EQ(exactValue->value, 123456);
  EXPECT_EQ(exactValue->scale, 5);

  value = ParseUnsignedNumericLiteral(".123456E-3M", &numberOfSyntaxErrors);
  EXPECT_EQ(numberOfSyntaxErrors, 0);
  exactValue = std::get_if<ast::ExactNumericLiteral>(&value);
  ASSERT_TRUE(exactValue);
  EXPECT_EQ(exactValue->value, 123456);
  EXPECT_EQ(exactValue->scale, 9);
}

TEST(ParseNode, ApproximateNumericLiteral) {
  size_t numberOfSyntaxErrors;
  auto value = ParseUnsignedNumericLiteral("123.456F", &numberOfSyntaxErrors);
  EXPECT_EQ(numberOfSyntaxErrors, 0);
  auto* approxValue = std::get_if<double>(&value);
  ASSERT_TRUE(approxValue);
  EXPECT_DOUBLE_EQ(*approxValue, 123.456);

  value = ParseUnsignedNumericLiteral("123.456f", &numberOfSyntaxErrors);
  EXPECT_EQ(numberOfSyntaxErrors, 0);
  approxValue = std::get_if<double>(&value);
  ASSERT_TRUE(approxValue);
  EXPECT_DOUBLE_EQ(*approxValue, 123.456);

  value = ParseUnsignedNumericLiteral("123.456d", &numberOfSyntaxErrors);
  EXPECT_EQ(numberOfSyntaxErrors, 0);
  approxValue = std::get_if<double>(&value);
  ASSERT_TRUE(approxValue);
  EXPECT_DOUBLE_EQ(*approxValue, 123.456);

  value = ParseUnsignedNumericLiteral("123.456D", &numberOfSyntaxErrors);
  EXPECT_EQ(numberOfSyntaxErrors, 0);
  approxValue = std::get_if<double>(&value);
  ASSERT_TRUE(approxValue);
  EXPECT_DOUBLE_EQ(*approxValue, 123.456);

  value = ParseUnsignedNumericLiteral("123.456e78", &numberOfSyntaxErrors);
  EXPECT_EQ(numberOfSyntaxErrors, 0);
  approxValue = std::get_if<double>(&value);
  ASSERT_TRUE(approxValue);
  EXPECT_DOUBLE_EQ(*approxValue, 123.456e78);

  value = ParseUnsignedNumericLiteral("123.456e-78", &numberOfSyntaxErrors);
  EXPECT_EQ(numberOfSyntaxErrors, 0);
  approxValue = std::get_if<double>(&value);
  ASSERT_TRUE(approxValue);
  EXPECT_DOUBLE_EQ(*approxValue, 123.456e-78);

  value = ParseUnsignedNumericLiteral("123.456e+78", &numberOfSyntaxErrors);
  EXPECT_EQ(numberOfSyntaxErrors, 0);
  approxValue = std::get_if<double>(&value);
  ASSERT_TRUE(approxValue);
  EXPECT_DOUBLE_EQ(*approxValue, 123.456e+78);

  value = ParseUnsignedNumericLiteral("1_123.456e+7_8", &numberOfSyntaxErrors);
  EXPECT_EQ(numberOfSyntaxErrors, 0);
  approxValue = std::get_if<double>(&value);
  ASSERT_TRUE(approxValue);
  EXPECT_DOUBLE_EQ(*approxValue, 1123.456e+78);

  EXPECT_THROW(
      {
        value =
            ParseUnsignedNumericLiteral("123.456e+7890", &numberOfSyntaxErrors);
      },
      gql::ParserError);

  EXPECT_THROW(
      {
        value =
            ParseUnsignedNumericLiteral("123.456e+50M", &numberOfSyntaxErrors);
      },
      gql::ParserError);

  EXPECT_THROW(
      {
        value = ParseUnsignedNumericLiteral(
            "3333333333333333333333333333333333333", &numberOfSyntaxErrors);
      },
      gql::ParserError);
}

TEST(ParseNode, CharacterStringLiteral) {
  size_t numberOfSyntaxErrors;
  auto value = ParseCharacterStringLiteral(R"("Hello, \"world\"! \\o/")",
                                           &numberOfSyntaxErrors);
  EXPECT_EQ(numberOfSyntaxErrors, 0);
  EXPECT_EQ(value, "Hello, \"world\"! \\o/");

  value = ParseCharacterStringLiteral(R"("Hello, ""world""! \\o/")",
                                      &numberOfSyntaxErrors);
  EXPECT_EQ(numberOfSyntaxErrors, 0);
  EXPECT_EQ(value, "Hello, \"world\"! \\o/");

  value = ParseCharacterStringLiteral(
      R"(@"Hello, my world! \n \r \t \b \f \' \` \\o/")",
      &numberOfSyntaxErrors);
  EXPECT_EQ(numberOfSyntaxErrors, 0);
  EXPECT_EQ(value, R"(Hello, my world! \n \r \t \b \f \' \` \\o/)");

  value =
      ParseCharacterStringLiteral(R"("Hello, world!\")", &numberOfSyntaxErrors);
  EXPECT_GT(numberOfSyntaxErrors, 0);

  value = ParseCharacterStringLiteral(R"("123\u20ac321\u20ac")",
                                      &numberOfSyntaxErrors);
  EXPECT_EQ(numberOfSyntaxErrors, 0);
  EXPECT_EQ(value, u8"123\u20ac321\u20ac");

  value = ParseCharacterStringLiteral(R"("123\U01f34d321\U01f34d")",
                                      &numberOfSyntaxErrors);
  EXPECT_EQ(numberOfSyntaxErrors, 0);
  EXPECT_EQ(value, u8"123\U0001f34d321\U0001f34d");

  value = ParseCharacterStringLiteral(R"("\T")", &numberOfSyntaxErrors);
  EXPECT_GT(numberOfSyntaxErrors, 0);
}

TEST(ParseNode, ClosedDynamicUnionType1) {
  ParserWrapper p(R"(ANY< FLOAT128 >)");

  ast::ValueType value;
  parser::ast_builder::BuildAST(p.parser.valueType(), value);
  EXPECT_EQ(p.parser.getNumberOfSyntaxErrors(), 0);

  const auto* type = std::get_if<ast::SimpleNumericType>(&value.typeOption);
  ASSERT_TRUE(type);
  EXPECT_EQ(*type, ast::SimpleNumericType::Float128);
}

TEST(ParseNode, ClosedDynamicUnionType2) {
  ParserWrapper p(R"(ANY< INT16 | INT32 >)");

  ast::ValueType value;
  parser::ast_builder::BuildAST(p.parser.valueType(), value);
  EXPECT_EQ(p.parser.getNumberOfSyntaxErrors(), 0);

  const auto* unionType = std::get_if<ast::ValueType::Union>(&value.typeOption);
  ASSERT_TRUE(unionType);
  ASSERT_EQ(unionType->types.size(), 2);

  const auto* type1 =
      std::get_if<ast::SimpleNumericType>(&unionType->types[0]->typeOption);
  const auto* type2 =
      std::get_if<ast::SimpleNumericType>(&unionType->types[1]->typeOption);
  ASSERT_TRUE(type1);
  ASSERT_TRUE(type2);
  EXPECT_EQ(*type1, ast::SimpleNumericType::Int16);
  EXPECT_EQ(*type2, ast::SimpleNumericType::Int32);
}

TEST(ParseNode, ClosedDynamicUnionType3) {
  ParserWrapper p(R"(ANY< INT16 | INT32 | INT64 >)");

  ast::ValueType value;
  parser::ast_builder::BuildAST(p.parser.valueType(), value);
  EXPECT_EQ(p.parser.getNumberOfSyntaxErrors(), 0);

  const auto* unionType = std::get_if<ast::ValueType::Union>(&value.typeOption);
  ASSERT_TRUE(unionType);
  ASSERT_EQ(unionType->types.size(), 3);

  const auto* type1 =
      std::get_if<ast::SimpleNumericType>(&unionType->types[0]->typeOption);
  const auto* type2 =
      std::get_if<ast::SimpleNumericType>(&unionType->types[1]->typeOption);
  const auto* type3 =
      std::get_if<ast::SimpleNumericType>(&unionType->types[2]->typeOption);
  ASSERT_TRUE(type1);
  ASSERT_TRUE(type2);
  ASSERT_TRUE(type3);
  EXPECT_EQ(*type1, ast::SimpleNumericType::Int16);
  EXPECT_EQ(*type2, ast::SimpleNumericType::Int32);
  EXPECT_EQ(*type3, ast::SimpleNumericType::Int64);
}

TEST(ParseAndPrintNode, BooleanValueExpression1) {
  ParserWrapper p("1 OR 2 AND 3 OR 4");

  ast::ValueExpression value;
  parser::ast_builder::BuildAST(p.parser.valueExpression(), value);

  EXPECT_EQ(ast::PrintTree(value), "1 OR 2 AND 3 OR 4");
}

TEST(ParseAndPrintNode, BooleanValueExpression2) {
  ParserWrapper p("1 AND 2 OR 3 AND 4");

  ast::ValueExpression value;
  parser::ast_builder::BuildAST(p.parser.valueExpression(), value);

  EXPECT_EQ(ast::PrintTree(value), "1 AND 2 OR 3 AND 4");
}

TEST(ParseAndPrintNode, BooleanValueExpression3) {
  ParserWrapper p("1 AND (2 OR 3) AND 4");

  ast::ValueExpression value;
  parser::ast_builder::BuildAST(p.parser.valueExpression(), value);

  EXPECT_EQ(ast::PrintTree(value), "1 AND (2 OR 3) AND 4");
}

TEST(ParseAndPrintNode, NumericValueExpression1) {
  ParserWrapper p("1 + 2 * 3 + 4");

  ast::ValueExpression value;
  parser::ast_builder::BuildAST(p.parser.valueExpression(), value);

  EXPECT_EQ(ast::PrintTree(value), "1 + 2 * 3 + 4");
}

TEST(ParseAndPrintNode, NumericValueExpression2) {
  ParserWrapper p("1 * 2 + 3 * 4");

  ast::ValueExpression value;
  parser::ast_builder::BuildAST(p.parser.valueExpression(), value);

  EXPECT_EQ(ast::PrintTree(value), "1 * 2 + 3 * 4");
}

TEST(ParseAndPrintNode, NumericValueExpression3) {
  ParserWrapper p("1 * (2 + 3) * 4");

  ast::ValueExpression value;
  parser::ast_builder::BuildAST(p.parser.valueExpression(), value);

  EXPECT_EQ(ast::PrintTree(value), "1 * (2 + 3) * 4");
}

}  // namespace gql
