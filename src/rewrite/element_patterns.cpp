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

void RewriteElementPatterns(ast::PathTerm& expr) {
  bool lastWasEdge = true;
  for (auto elIt = expr.begin(); elIt != expr.end(); ++elIt) {
    auto& el = *elIt;
    if (auto* element = std::get_if<ast::ElementPattern>(&el.pattern)) {
      if (auto* edge = std::get_if<ast::EdgePattern>(element)) {
        if (!std::holds_alternative<ast::PathFactor::NoQuantifier>(
                el.quantifier)) {
          // Rewrite "->*" to "(() -> ())*"
          ast::PathFactor newNode;
          newNode.pattern = ast::NodePattern{};

          ast::ParenthesizedPathPatternExpressionPtr ppe;
          auto& terms = ppe->pattern.terms.emplace_back();
          terms.emplace_back(newNode);
          terms.emplace_back().pattern = *element;
          terms.emplace_back(newNode);
          SetInputPositionRecursive(ppe, edge->inputPosition());

          el.pattern = std::move(ppe);
          lastWasEdge = false;
        } else {
          if (lastWasEdge) {
            // Insert empty node pattern between edges
            ast::PathFactor newNode;
            newNode.pattern = ast::NodePattern{};
            SetInputPositionRecursive(newNode, edge->inputPosition());
            elIt = expr.emplace(elIt, newNode);
            ++elIt;
          }
          lastWasEdge = true;
        }
      } else {
        lastWasEdge = false;
      }
    } else {
      lastWasEdge = false;
    }
  }

  if (auto* element = std::get_if<ast::ElementPattern>(&expr.back().pattern)) {
    if (auto* edge = std::get_if<ast::EdgePattern>(element)) {
      ast::PathFactor newNode;
      newNode.pattern = ast::NodePattern{};
      SetInputPositionRecursive(newNode, edge->inputPosition());
      expr.push_back(newNode);
    }
  }
}

void RewriteElementPatterns(ast::GQLProgram& program) {
  ast::ForEachNodeOfType<ast::PathTerm>(program, [](ast::PathTerm& node) {
    RewriteElementPatterns(node);
    return ast::VisitorResult::kContinue;
  });
}

}  // namespace gql::rewrite