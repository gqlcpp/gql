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

#include <optional>
#include <vector>

#include "gql/gql_export.h"
#include "gql/standard/features.h"

namespace gql {

struct SyntaxAnalyzerConfig {
  bool rewriteResultOrderByClause = false;
  bool rewriteLetStatement = true;
  bool rewriteOptionalMatchStatement = true;

  // Following rewrites are required, only disable them is they were already
  // applied.
  bool rewriteExistsPredicate = true;
  bool rewritePathPatternPrefix = true;
  bool rewriteSimplifiedPathPattern = true;
  bool rewriteElementPatterns = true;
  bool rewriteElementPatternWhereClause = true;
  bool rewriteElementPropertyPredicate = true;

  std::vector<standard::Feature> unsupportedFeatures;
  std::vector<ast::ValueType> supportedPropertyTypes;
};

GQL_EXPORT std::optional<ast::FieldTypeList> AnalyzeSyntax(
    ast::GQLProgram&,
    const SyntaxAnalyzerConfig&);

}  // namespace gql