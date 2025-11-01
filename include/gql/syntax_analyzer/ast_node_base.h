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

#include <memory>

#include "gql/ast/nodes/node.h"

namespace gql::ast {
struct BinarySetFunction;
struct GeneralSetFunction;
struct GraphPattern;
struct GraphPatternWhereClause;
struct ParenthesizedPathPatternWhereClause;
struct PathFactor;
struct PathPattern;
struct PathPatternExpression;
}  // namespace gql::ast

namespace gql::syntax_analyzer {

struct AggregateFunctionAuxData;
struct GraphPatternAuxData;
struct GraphPatternWhereClauseAuxData;
struct PathVariableReferenceScopeAuxData;
struct PathPatternAuxData;

struct GraphPatternBase : ast::Node {
  std::shared_ptr<const GraphPatternAuxData> auxData;
};

struct PathPatternBase : ast::Node {
  std::shared_ptr<const PathPatternAuxData> auxData;
};

struct PathFactorBase : ast::Node {
  std::shared_ptr<const PathVariableReferenceScopeAuxData> auxData;
};

struct PathPatternExpressionBase : ast::Node {
  std::shared_ptr<const PathVariableReferenceScopeAuxData> auxData;
};

// Common base for classes holding expressions in graph pattern.
struct GraphPatternWhereClauseBase : ast::Node {
  std::shared_ptr<const GraphPatternWhereClauseAuxData> auxData;
};

struct AggregateFunctionBase : ast::Node {
  std::shared_ptr<const AggregateFunctionAuxData> auxData;
};

template <typename T>
struct AstNodeBase {
  using type = ast::Node;
};

template <>
struct AstNodeBase<ast::GraphPattern> {
  using type = syntax_analyzer::GraphPatternBase;
};

template <>
struct AstNodeBase<ast::PathPattern> {
  using type = syntax_analyzer::PathPatternBase;
};

template <>
struct AstNodeBase<ast::PathFactor> {
  using type = syntax_analyzer::PathFactorBase;
};

template <>
struct AstNodeBase<ast::PathPatternExpression> {
  using type = syntax_analyzer::PathPatternExpressionBase;
};

template <>
struct AstNodeBase<ast::GraphPatternWhereClause> {
  using type = syntax_analyzer::GraphPatternWhereClauseBase;
};

template <>
struct AstNodeBase<ast::ParenthesizedPathPatternWhereClause> {
  using type = syntax_analyzer::GraphPatternWhereClauseBase;
};

template <>
struct AstNodeBase<ast::GeneralSetFunction> {
  using type = syntax_analyzer::AggregateFunctionBase;
};

template <>
struct AstNodeBase<ast::BinarySetFunction> {
  using type = syntax_analyzer::AggregateFunctionBase;
};

}  // namespace gql::syntax_analyzer

namespace gql::ast {

template <typename T>
using NodeBase = typename syntax_analyzer::AstNodeBase<T>::type;

}  // namespace gql::ast
