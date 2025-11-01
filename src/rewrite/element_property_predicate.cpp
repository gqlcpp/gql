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

#include <fmt/format.h>

#include "gql/ast/algorithm.h"
#include "helpers.h"

namespace gql::rewrite {

void RewriteElementPropertyPredicate(ast::GQLProgram& program) {
  int lastGeneratedId = 0;
  auto generateId = [&lastGeneratedId]() {
    return fmt::format("gql_gen_prop{}", ++lastGeneratedId);
  };

  ast::ForEachNodeOfType<ast::PathPrimary>(
      program, [&generateId](ast::PathPrimary& node) {
        if (auto* elementPattern = std::get_if<ast::ElementPattern>(&node)) {
          ast::InputPosition elementInputPosition;
          auto& filler =
              *gql::ast::variant_switch(*elementPattern, [&](auto& value) {
                elementInputPosition = value.inputPosition();
                return &value.filler;
              });
          if (filler.predicate) {
            if (auto* props = std::get_if<ast::ElementPropertySpecification>(
                    &*filler.predicate)) {
              if (!filler.var) {
                auto& var = filler.var.emplace();
                var.isTemp = true;
                var.name = generateId();
                SetInputPositionRecursive(var, elementInputPosition);
              }

              ast::ParenthesizedPathPatternExpressionPtr parenthesizedExpr;
              auto& expr = parenthesizedExpr->where.emplace().condition;
              bool first = true;
              for (auto& prop : props->props) {
                ast::ValueExpressionPtr propValueExpr;
                auto& cmp = propValueExpr->option
                                .emplace<ast::ValueExpression::Comparison>();
                cmp.op = ast::CompOp::Equals;
                auto& propRef =
                    cmp.left->option.emplace<ast::PropertyReference>();
                propRef.element->option.emplace<ast::BindingVariableReference>()
                    .name = filler.var->name;
                propRef.property = prop.name;
                cmp.right = prop.value;

                if (first) {
                  expr = std::move(propValueExpr);
                  first = false;
                } else {
                  ast::ValueExpressionPtr andExpr;
                  auto& bin =
                      andExpr->option.emplace<ast::ValueExpression::Binary>();
                  bin.op = ast::ValueExpression::Binary::Op::BoolAnd;
                  bin.left = std::move(expr);
                  bin.right = std::move(propValueExpr);
                  expr = std::move(andExpr);
                }
              }
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