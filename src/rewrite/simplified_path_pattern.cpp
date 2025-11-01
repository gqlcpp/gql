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

#include "common/formatted_error.h"
#include "gql/ast/algorithm.h"
#include "gql/ast/nodes/statements.h"


namespace gql::rewrite {

namespace {

ast::LabelExpressionPtr RewriteAsLabelExpression(
    const ast::SimplifiedContents& contents);

ast::ParenthesizedPathPatternExpressionPtr Rewrite(
    const ast::SimplifiedContents& contents,
    ast::EdgeDirectionPattern direction);

ast::LabelExpressionPtr RewriteAsLabelExpression(
    const ast::SimplifiedPrimary& primary) {
  if (auto* label_name = std::get_if<ast::LabelName>(&primary)) {
    ast::LabelExpressionPtr expr;
    expr->SetInputPosition(label_name->inputPosition());
    expr->option = *label_name;
    return expr;
  }
  return RewriteAsLabelExpression(std::get<ast::SimplifiedContents>(primary));
}

ast::LabelExpressionPtr RewriteAsLabelExpression(
    const ast::SimplifiedTertiary& tertiary) {
  auto inner = RewriteAsLabelExpression(tertiary.primary);
  if (!tertiary.isNegation) {
    return inner;
  }

  ast::LabelExpressionPtr result;
  result->SetInputPosition(tertiary.inputPosition());
  auto& logical = result->option.emplace<ast::LabelExpression::Negation>();
  logical.expr = std::move(inner);
  return result;
}

ast::LabelExpressionPtr RewriteAsLabelExpression(
    const ast::SimplifiedFactorHigh& factor) {
  if (!std::holds_alternative<ast::SimplifiedFactorHigh::NoQuantifier>(
          factor.quantifier)) {
    throw FormattedError(factor, ErrorCode::E0091,
                         "Quantifier is not expected");
  }

  auto& tertiary = *factor.tertiary;
  if (tertiary.direction) {
    throw FormattedError(tertiary, ErrorCode::E0092,
                         "Direction is not expected");
  }
  return RewriteAsLabelExpression(tertiary);
}

ast::LabelExpressionPtr RewriteAsLabelExpression(
    const ast::SimplifiedFactorLow& conjunction) {
  if (conjunction.factors.size() == 1) {
    return RewriteAsLabelExpression(conjunction.factors[0]);
  }

  ast::LabelExpressionPtr result;
  result->SetInputPosition(conjunction.inputPosition());
  auto& logical = result->option.emplace<ast::LabelExpression::Logical>();
  logical.op = ast::LabelExpression::Logical::Op::And;
  logical.terms.reserve(conjunction.factors.size());
  for (const auto& term : conjunction.factors) {
    logical.terms.push_back(RewriteAsLabelExpression(term));
  }
  return result;
}

ast::LabelExpressionPtr RewriteAsLabelExpression(
    const ast::SimplifiedTerm& term) {
  if (term.factors.size() != 1) {
    throw FormattedError(term, ErrorCode::E0093,
                         "Concatenation is not expected");
  }

  return RewriteAsLabelExpression(term.factors[0]);
}

ast::LabelExpressionPtr RewriteAsLabelExpression(
    const ast::SimplifiedContents& contents) {
  if (contents.op == ast::SimplifiedContents::Op::MultisetAlternation) {
    throw FormattedError(contents, ErrorCode::E0094,
                         "Multiset alternation is not expected");
  }

  if (contents.terms.size() == 1) {
    return RewriteAsLabelExpression(contents.terms[0]);
  }

  ast::LabelExpressionPtr result;
  result->SetInputPosition(contents.inputPosition());
  auto& logical = result->option.emplace<ast::LabelExpression::Logical>();
  logical.op = ast::LabelExpression::Logical::Op::Or;
  logical.terms.reserve(contents.terms.size());
  for (const auto& term : contents.terms) {
    logical.terms.push_back(RewriteAsLabelExpression(term));
  }
  return result;
}

ast::ElementPattern CreateEdgePattern(ast::EdgeDirectionPattern direction,
                                      ast::LabelExpressionPtr label_expr) {
  ast::ElementPattern result;
  auto& edge = result.emplace<ast::EdgePattern>();
  edge.SetInputPosition(label_expr->inputPosition());
  edge.direction = direction;
  edge.filler.SetInputPosition(label_expr->inputPosition());
  edge.filler.labelExpr = std::move(*label_expr);
  return result;
}

ast::PathPrimary Rewrite(const ast::SimplifiedPrimary& primary,
                         ast::EdgeDirectionPattern direction) {
  return ast::variant_switch(
      primary,
      [&](const ast::LabelName&) -> ast::PathPrimary {
        return CreateEdgePattern(direction, RewriteAsLabelExpression(primary));
      },
      [&](const ast::SimplifiedContents& content) -> ast::PathPrimary {
        return Rewrite(content, direction);
      });
}

ast::PathPrimary Rewrite(const ast::SimplifiedTertiary& tertiary,
                         ast::EdgeDirectionPattern direction) {
  if (tertiary.direction || tertiary.isNegation) {
    if (tertiary.direction)
      direction = *tertiary.direction;

    return CreateEdgePattern(direction, RewriteAsLabelExpression(tertiary));
  }

  return Rewrite(tertiary.primary, direction);
}

ast::PathFactor Rewrite(const ast::SimplifiedFactorHigh& factor,
                        ast::EdgeDirectionPattern direction) {
  ast::PathFactor result;
  result.SetInputPosition(factor.inputPosition());
  result.quantifier = factor.quantifier;
  result.pattern = Rewrite(*factor.tertiary, direction);
  return result;
}

ast::PathFactor Rewrite(const ast::SimplifiedFactorLow& factor,
                        ast::EdgeDirectionPattern direction) {
  if (factor.factors.size() == 1) {
    return Rewrite(factor.factors[0], direction);
  }

  ast::PathFactor result;
  result.SetInputPosition(factor.inputPosition());
  result.pattern =
      CreateEdgePattern(direction, RewriteAsLabelExpression(factor));
  return result;
}

ast::PathTerm Rewrite(const ast::SimplifiedTerm& term,
                      ast::EdgeDirectionPattern direction) {
  ast::PathTerm result;
  result.reserve(term.factors.size());
  for (const auto& factor : term.factors) {
    result.push_back(Rewrite(factor, direction));
  }
  return result;
}

ast::ParenthesizedPathPatternExpressionPtr Rewrite(
    const ast::SimplifiedContents& contents,
    ast::EdgeDirectionPattern direction) {
  ast::ParenthesizedPathPatternExpressionPtr result;
  result->SetInputPosition(contents.inputPosition());
  result->pattern.SetInputPosition(contents.inputPosition());
  result->pattern.op = contents.op;
  result->pattern.terms.reserve(contents.terms.size());
  for (const auto& term : contents.terms) {
    result->pattern.terms.push_back(Rewrite(term, direction));
  }

  return result;
}

}  // namespace

// ISO/IEC 39075:2024 16.12
ast::ParenthesizedPathPatternExpressionPtr RewriteSimplifiedPathPattern(
    const ast::SimplifiedPathPatternExpression& expr) {
  return Rewrite(expr.contents, expr.direction);
}

void RewriteSimplifiedPathPattern(ast::GQLProgram& program) {
  ast::ForEachNodeOfType<ast::PathPrimary>(program, [](ast::PathPrimary& node) {
    if (auto* path_pattern =
            std::get_if<ast::SimplifiedPathPatternExpression>(&node)) {
      node = RewriteSimplifiedPathPattern(*path_pattern);
      return ast::VisitorResult::kSkipChildren;
    }
    return ast::VisitorResult::kContinue;
  });
}

}  // namespace gql::rewrite
