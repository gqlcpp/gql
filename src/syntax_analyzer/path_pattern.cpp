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

#include "syntax_analyzer.h"

#include "common/formatted_error.h"
#include "gql/ast/algorithm.h"
#include "gql/ast/ast.h"
#include "graph_pattern_context.h"
#include "type_helpers.h"

namespace gql {

using namespace detail;

void SyntaxAnalyzer::Process(ast::PathPatternExpression& pathPattern,
                             GraphPatternContext& graphPatternContext,
                             ExecutionContext& context) {
  // BEGIN feature checks
  if (pathPattern.terms.size() > 1) {
    if (pathPattern.op == ast::PathPatternExpression::Op::Union) {
      ThrowIfFeatureNotSupported(standard::Feature::G032, pathPattern);
    } else {
      ThrowIfFeatureNotSupported(standard::Feature::G030, pathPattern);
    }
  }
  // END feature checks

  VariableReferenceScope unionVariableScope(graphPatternContext, pathPattern);
  PathPatternUnionScope pathPatternUnionScope(graphPatternContext);
  if (pathPattern.terms.size() > 1) {
    pathPatternUnionScope.Activate();
  }
  for (auto& term : pathPattern.terms) {
    PathPatternUnionOperandScope pathPatternUnionOperandScope(
        graphPatternContext);
    if (pathPatternUnionScope.isActive()) {
      pathPatternUnionOperandScope.Activate();
    }

    for (auto& factor : term) {
      VariableReferenceScope pathFactorScope(graphPatternContext, factor);
      QuantifiedPathPrimaryScope quantifiedPathPrimaryScope(graphPatternContext,
                                                            factor);
      QuestionedPathPrimaryScope questionedPathPrimaryScope(graphPatternContext,
                                                            factor);

      ast::variant_switch(
          factor.quantifier, [&](const ast::PathFactor::NoQuantifier&) {},
          [&](const ast::PathFactor::Optional&) {
            questionedPathPrimaryScope.Activate();
          },
          [&](const ast::GraphPatternQuantifier& value) {
            if (value.upper) {
              ThrowIfFeatureNotSupported(standard::Feature::G060, factor);
            } else {
              ThrowIfFeatureNotSupported(standard::Feature::G061, factor);
            }

            quantifiedPathPrimaryScope.Activate(!!value.upper,
                                                static_cast<int>(value.lower));
          });

      ast::variant_switch(
          factor.pattern,
          [&](ast::ElementPattern& value) {
            auto processFiller = [&](ast::ElementPatternFiller& filler) {
              if (filler.labelExpr) {
                ProcessFallback(*filler.labelExpr, context);
              }
              if (filler.predicate) {
                throw FormattedError(filler, ErrorCode::E0111,
                                     "Element predicate must be rewritten to "
                                     "parenthesized path pattern where clause");
              }
            };
            ast::variant_switch(
                value,
                [&](ast::NodePattern& value) {
                  NodePatternScope nodePatternScope(graphPatternContext);
                  if (value.filler.var) {
                    graphPatternContext.DeclareNodeVariable(*value.filler.var);
                  }
                  processFiller(value.filler);
                },
                [&](ast::EdgePattern& value) {
                  EdgePatternScope edgePatternScope(graphPatternContext);
                  if (value.filler.var) {
                    graphPatternContext.DeclareEdgeVariable(*value.filler.var);
                  }
                  processFiller(value.filler);
                });
          },
          [&](ast::ParenthesizedPathPatternExpressionPtr& value) {
            // BEGIN feature checks
            if (value->var) {
              ThrowIfFeatureNotSupported(standard::Feature::G048, *value->var);
            }
            if (value->pathMode != ast::PathMode::WALK) {
              ThrowIfFeatureNotSupported(standard::Feature::G049, *value);
            }
            if (value->where) {
              ThrowIfFeatureNotSupported(standard::Feature::G050,
                                         *value->where);
            }
            // END feature checks

            ParenthesizedPathPatternExpressionScope parenthesizedScope(
                graphPatternContext, value->var.has_value(), *value);
            Process(value->pathMode, *value, context);
            if (value->var) {
              graphPatternContext.DeclareSubpathVariable(*value->var);
            }

            PathModeScope pathModeScope(graphPatternContext, value->pathMode);
            Process(value->pattern, graphPatternContext, context);

            if (value->where) {
              graphPatternContext.AddSearchCondition(&*value->where);
            }
          },
          [&](const ast::SimplifiedPathPatternExpression& value) {
            throw FormattedError(
                value, ErrorCode::E0060,
                "SimplifiedPathPatternExpression must be rewritten to "
                "ElementPattern");
          });
    }
  }
}

void SyntaxAnalyzer::Process(ast::PathMode pathMode,
                             const ast::Node& node,
                             ExecutionContext&) const {
  switch (pathMode) {
    case ast::PathMode::WALK:
      break;
    case ast::PathMode::TRAIL:
      ThrowIfFeatureNotSupported(standard::Feature::G011, node);
      break;
    case ast::PathMode::SIMPLE:
      ThrowIfFeatureNotSupported(standard::Feature::G012, node);
      break;
    case ast::PathMode::ACYCLIC:
      ThrowIfFeatureNotSupported(standard::Feature::G013, node);
      break;
  }
}

void SyntaxAnalyzer::Process(const ast::PathPatternPrefix& prefix,
                             ExecutionContext& context) const {
  Process(prefix.mode, prefix, context);
  if (prefix.pathSearchPrefix) {
    auto& value = *prefix.pathSearchPrefix;
    switch (value.search) {
      case ast::PathSearchPrefix::Search::All:
        break;
      case ast::PathSearchPrefix::Search::Any:
        ThrowIfFeatureNotSupported(standard::Feature::G016, prefix);
        break;
      case ast::PathSearchPrefix::Search::CountedShortestGroup:
        if (auto* num = std::get_if<ast::UnsignedInteger>(&value.number)) {
          if (*num == 1) {
            ThrowIfFeatureNotSupported(standard::Feature::G017, prefix);
          }
        }
        ThrowIfFeatureNotSupported(standard::Feature::G020, prefix);
        break;
      case ast::PathSearchPrefix::Search::CountedShortestPath:
        if (auto* num = std::get_if<ast::UnsignedInteger>(&value.number)) {
          if (*num == 1) {
            ThrowIfFeatureNotSupported(standard::Feature::G018, prefix);
          }
        }
        ThrowIfFeatureNotSupported(standard::Feature::G019, prefix);
        break;
    }
  }
}

void SyntaxAnalyzer::Process(const ast::LabelExpression& expr,
                             ExecutionContext&) const {
  if (std::holds_alternative<ast::LabelExpression::Wildcard>(expr.option)) {
    ThrowIfFeatureNotSupported(standard::Feature::G074, expr);
  }
}

void SyntaxAnalyzer::Process(ast::PropertyKeyValuePair& expr,
                             ExecutionContext& context) {
  ProcessValueExpression(expr.value, context);
}

}  // namespace gql
