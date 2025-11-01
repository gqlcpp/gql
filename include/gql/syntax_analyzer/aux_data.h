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

#include <string>
#include <unordered_map>
#include <unordered_set>

#include "gql/syntax_analyzer/defs.h"

namespace gql::syntax_analyzer {

struct PathPatternAuxData {
  // Exposed unconditional singleton element references.
  // TODO: Clarify case of internal variables of selective path pattern
  std::unordered_set<std::string> joinableVariables;
};

struct GraphPatternAuxData {
  struct Variable {
    GraphPatternVariableType type;
    VariableDegreeOfExposure degreeOfExposure;
    bool isTemp;
  };

  using Variables = std::unordered_map<std::string, Variable>;
  Variables variables;
};

// Used in GraphPatternWhereClause and ParenthesizedPathPatternWhereClause.
struct GraphPatternWhereClauseAuxData {
  using Variable = GraphPatternAuxData::Variable;

  std::unordered_map<std::string, Variable> referencedVariables;
};

// Used in PathFactor and PathPatternExpression.
struct PathVariableReferenceScopeAuxData {
  using Variable = GraphPatternAuxData::Variable;

  // Element variables declared in this path factor with particular degree of
  // exposure and binding context for the first time in the graph (i.e. deepest
  // such path factor). Unconditional singleton variables may be declared
  // multiple times.
  std::unordered_map<std::string, Variable> declaredVariables;
};

// Used in GeneralSetFunction and BinarySetFunction.
struct AggregateFunctionAuxData {
  // If set, indicates that the function is used to aggregate group list
  // variable items. Otherwise, function aggregates working table rows.
  std::optional<std::string> groupListVariable;
};

}  // namespace gql::syntax_analyzer
