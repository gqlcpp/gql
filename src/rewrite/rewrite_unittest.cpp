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

#include "gql/ast/print.h"
#include "gql/parser/parser.h"
#include "gql/rewrite.h"
#include "input_positions_test.h"

struct RewriteTestParam {
  std::string original;
  std::string expected;
};

class RewriteTest : public testing::TestWithParam<RewriteTestParam> {
 protected:
  void SetUp() override {
    try {
      program = gql::parser::ParseProgram(GetParam().original.c_str());
    } catch (const std::runtime_error& e) {
      GTEST_FAIL() << e.what();
    }
  }

  void TearDown() override {
    gql::ast::CheckInputPositions(program, ignorePositionOrder);

    int count = 0;
    gql::ast::ForEachNodeInTree(program, [&count](auto*) {
      count++;
      return gql::ast::VisitorResult::kContinue;
    });
    EXPECT_GT(count, 0);

    EXPECT_EQ(gql::ast::PrintTree(program), GetParam().expected);
  }

  gql::ast::GQLProgram program;
  bool ignorePositionOrder = false;
};

class SimplifiedPathPattern : public RewriteTest {};

TEST_P(SimplifiedPathPattern, Check) {
  gql::rewrite::RewriteSimplifiedPathPattern(program);
}

INSTANTIATE_TEST_SUITE_P(
    ,
    SimplifiedPathPattern,
    testing::Values(
        RewriteTestParam{
            R"(MATCH (p :Person)-/IS_FRIENDS_WITH/->(friend :Person) WHERE EXISTS {MATCH (p)-/WORKS_FOR/->(:Company {name: "GQL, Inc."})} RETURN p, r, friend)",
            R"(MATCH (p :Person) (-[:IS_FRIENDS_WITH]->) (friend :Person) WHERE EXISTS {MATCH (p) (-[:WORKS_FOR]->) (:Company {name: "GQL, Inc."})} RETURN p, r, friend)"},
        RewriteTestParam{
            R"(MATCH () ~/ LABEL1 |+| (LABEL2 | LABEL3a LABEL3b) |+| ((<LABEL4> |+| LABEL5a & LABEL5b) | LABEL6) /~> ())",
            R"(MATCH () (~[:LABEL1]~> |+| (~[:LABEL2]~> | ~[:LABEL3a]~> ~[:LABEL3b]~>) |+| ((<-[:LABEL4]-> |+| ~[:(LABEL5a&LABEL5b)]~>) | ~[:LABEL6]~>)) ())"},
        RewriteTestParam{
            "MATCH () ~/ LABEL1 & !(LABEL2|LABEL3) & !LABEL5 /~> ()",
            "MATCH () (~[:(LABEL1&!(LABEL2|LABEL3)&!LABEL5)]~>) ()"},
        RewriteTestParam{
            "MATCH () ~/ <(LABEL1 & !(LABEL2|LABEL3) & !LABEL5) /~> ()",
            "MATCH () (<-[:(LABEL1&!(LABEL2|LABEL3)&!LABEL5)]-) ()"},
        RewriteTestParam{
            R"(MATCH () ~/ LABEL1 | ((!LABEL2|LABEL3)? |+| (!LABEL5>){1,3}) /~> ())",
            R"(MATCH () (~[:LABEL1]~> | ((~[:!LABEL2]~> | ~[:LABEL3]~>) ? |+| (-[:!LABEL5]->) {1, 3})) ())"}));

class ElementPatterns : public RewriteTest {};

TEST_P(ElementPatterns, Check) {
  gql::rewrite::RewriteElementPatterns(program);
}

INSTANTIATE_TEST_SUITE_P(
    ,
    ElementPatterns,
    testing::Values(
        RewriteTestParam{R"(MATCH -)", R"(MATCH ()-())"},
        RewriteTestParam{R"(MATCH -?)", R"(MATCH (()-()) ?)"},
        RewriteTestParam{R"(MATCH ()(-()-){4})", R"(MATCH () (()-()-()) {4})"},
        RewriteTestParam{R"(MATCH - - - | - | - -)",
                         R"(MATCH ()-()-()-() | ()-() | ()-()-())"},
        RewriteTestParam{
            R"(MATCH - - - (-) (- -{3} - (- (-)-?) - - (- -){2})- - (- -) )",
            R"(MATCH ()-()-()- (()-()) (()- (()-()) {3} - (()- (()-()) (()-()) ?) -()- (()-()-()) {2}) -()- (()-()-()))"},
        RewriteTestParam{R"(MATCH - - -)", R"(MATCH ()-()-()-())"}));

class ElementPatternWhereClause : public RewriteTest {};

TEST_P(ElementPatternWhereClause, Check) {
  gql::rewrite::RewriteElementPatternWhereClause(program);
}

INSTANTIATE_TEST_SUITE_P(
    ,
    ElementPatternWhereClause,
    testing::Values(RewriteTestParam{"MATCH (a WHERE a.prop=3)",
                                     "MATCH ((a) WHERE a.prop = 3)"},
                    RewriteTestParam{"MATCH -[b WHERE b.prop=3]-",
                                     "MATCH (-[b]- WHERE b.prop = 3)"}));

class ElementPropertyPredicate : public RewriteTest {
 public:
  ElementPropertyPredicate() {
    // It will be impossible to guarantee position order after the rewrite. And
    // to override or drop the positions would be overkill.
    ignorePositionOrder = true;
  }
};

TEST_P(ElementPropertyPredicate, Check) {
  gql::rewrite::RewriteElementPropertyPredicate(program);
}

INSTANTIATE_TEST_SUITE_P(
    ,
    ElementPropertyPredicate,
    testing::Values(
        RewriteTestParam{"MATCH (a {prop:3})", "MATCH ((a) WHERE a.prop = 3)"},
        RewriteTestParam{"MATCH -[b {prop:3}]-",
                         "MATCH (-[b]- WHERE b.prop = 3)"},
        RewriteTestParam{"MATCH (a :LABEL1 {prop:3})",
                         "MATCH ((a :LABEL1) WHERE a.prop = 3)"},
        RewriteTestParam{"MATCH (a {prop1:3, prop2:x.n})",
                         "MATCH ((a) WHERE a.prop1 = 3 AND a.prop2 = x.n)"},
        RewriteTestParam{"MATCH (a {prop1:x.p1, prop2:y.p2, prop3:z.p3})",
                         "MATCH ((a) WHERE a.prop1 = x.p1 AND a.prop2 = y.p2 "
                         "AND a.prop3 = z.p3)"},
        RewriteTestParam{
            "MATCH ({prop:3})",
            "MATCH ((TEMP gql_gen_prop1) WHERE gql_gen_prop1.prop = 3)"},
        RewriteTestParam{"MATCH (a {prop:3})",
                         "MATCH ((a) WHERE a.prop = 3)"}));
