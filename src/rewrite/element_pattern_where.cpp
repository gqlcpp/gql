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

#include "gql/rewrite.h"

#include "gql/ast/algorithm.h"
#include "helpers.h"

namespace gql::rewrite {

void RewriteElementPatternWhereClause(ast::GQLProgram& program) {
  ast::ForEachNodeOfType<ast::PathPrimary>(program, [](ast::PathPrimary& node) {
    if (auto* elementPattern = std::get_if<ast::ElementPattern>(&node)) {
      ast::InputPosition elementInputPosition;
      auto& filler =
          *gql::ast::variant_switch(*elementPattern, [&](auto& value) {
            elementInputPosition = value.inputPosition();
            return &value.filler;
          });
      if (filler.predicate) {
        if (auto* whereClause = std::get_if<ast::ElementPatternWhereClause>(
                &*filler.predicate)) {
          ast::ParenthesizedPathPatternExpressionPtr parenthesizedExpr;
          parenthesizedExpr->where.emplace().condition =
              std::move(whereClause->condition);
          parenthesizedExpr->where->SetInputPosition(
              whereClause->inputPosition());
          filler.predicate.reset();

          parenthesizedExpr->pattern.terms.emplace_back()
              .emplace_back()
              .pattern = std::move(node);
          node = std::move(parenthesizedExpr);
          SetInputPositionRecursive(node, elementInputPosition);
        }
      }

      return ast::VisitorResult::kSkipChildren;
    }
    return ast::VisitorResult::kContinue;
  });
}

}  // namespace gql::rewrite