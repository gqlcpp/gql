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

#include "match_utils.h"

#include "gql/ast/algorithm.h"

namespace gql::detail {

static void CollectMatchOutputColumns(const ast::PathPatternExpression& expr,
                                      std::unordered_set<std::string>& cols) {
  for (auto& term : expr.terms) {
    for (auto& factor : term) {
      ast::variant_switch(
          factor.pattern,
          [&](const ast::ElementPattern& value) {
            ast::variant_switch(value, [&](const auto& value) {
              if (value.filler.var) {
                cols.insert(value.filler.var->name);
              }
            });
          },
          [&](const ast::ParenthesizedPathPatternExpressionPtr& value) {
            CollectMatchOutputColumns(value->pattern, cols);
          },
          [&](const ast::SimplifiedPathPatternExpression&) {});
    }
  }
}

static void CollectMatchOutputColumns(const ast::SimpleMatchStatement& match,
                                      std::unordered_set<std::string>& cols) {
  if (match.yield) {
    for (auto& yield : *match.yield) {
      cols.insert(yield.name);
    }
  } else {
    for (auto& pathPattern : match.pattern.paths) {
      if (pathPattern.var) {
        cols.insert(pathPattern.var->name);
      }
      CollectMatchOutputColumns(pathPattern.expr, cols);
    }
  }
}

void CollectMatchOutputColumns(const ast::MatchStatement& statement,
                               std::unordered_set<std::string>& cols) {
  ast::variant_switch(
      statement,
      [&](const ast::SimpleMatchStatement& statement) {
        CollectMatchOutputColumns(statement, cols);
      },
      [&](const ast::OptionalMatchStatement& statement) {
        for (auto& stmt : statement.statements->statements) {
          CollectMatchOutputColumns(stmt, cols);
        }
      });
}

}  // namespace gql::detail
