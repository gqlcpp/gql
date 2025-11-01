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

#include "gql/ast/ast.h"
#include "gql/gql_export.h"

namespace gql::rewrite {

// ISO/IEC 39075:2024 19.4 Syntax Rules 2 and 3
GQL_EXPORT void RewriteExistsPredicate(ast::ExistsPredicate&);
GQL_EXPORT void RewriteExistsPredicate(ast::GQLProgram&);

// ISO/IEC 39075:2024 16.4.6
// Propagate <path pattern prefix> from <graph pattern> KEEP clause to contained
// <path pattern>s. May throw an exception if the input violates syntax rules.
GQL_EXPORT void RewritePathPatternPrefix(ast::GraphPattern&);
GQL_EXPORT void RewritePathPatternPrefix(ast::GQLProgram&);

// ISO/IEC 39075:2024 16.12
// Rewrite <simplified path pattern expression> to <path pattern expression>.
// May throw an exception if the input violates syntax rules.
GQL_EXPORT ast::ParenthesizedPathPatternExpressionPtr
RewriteSimplifiedPathPattern(const ast::SimplifiedPathPatternExpression&);
GQL_EXPORT void RewriteSimplifiedPathPattern(ast::GQLProgram&);

// ISO/IEC 39075:2024 16.7 Syntax rule 14
GQL_EXPORT void RewriteElementPatterns(ast::PathTerm&);
GQL_EXPORT void RewriteElementPatterns(ast::GQLProgram&);

// ISO/IEC 39075:2024 16.7 Syntax rule 12
GQL_EXPORT void RewriteElementPatternWhereClause(ast::GQLProgram&);

// ISO/IEC 39075:2024 16.3 Syntax rule 21 (and for those changed also apply 16.7
// Syntax rule 12)
GQL_EXPORT void RewriteElementPropertyPredicate(ast::GQLProgram&);

}  // namespace gql::rewrite
