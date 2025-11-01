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

#include <assert.h>
#include <unordered_set>

#include "common/formatted_error.h"
#include "gql/ast/algorithm.h"
#include "gql/ast/ast.h"
#include "gql/syntax_analyzer/aux_data.h"
#include "graph_pattern_context.h"
#include "type_helpers.h"

namespace gql {

using namespace detail;

static bool IsSelectivePattern(const ast::PathSearchPrefix& psp) {
  return psp.search != ast::PathSearchPrefix::Search::All;
}

static ast::ValueType GetProjectedType(GraphPatternVariableType type,
                                       VariableDegreeOfExposure degree);

static void FillWhereExpressionsAuxData(
    const GraphPatternContext& graphPatternContext) {
  for (auto& cond : graphPatternContext.searchConditionScopes()) {
    auto auxData =
        std::make_shared<syntax_analyzer::GraphPatternWhereClauseAuxData>();
    *cond.auxData = auxData;
    // TODO: Check where to stop search (e.g. nested procedures etc).
    std::unordered_set<std::string> referencedVariables;
    ast::ForEachNodeOfType<ast::BindingVariableReference>(
        *cond.condition, [&](const ast::BindingVariableReference& varRef) {
          referencedVariables.insert(varRef.name);
          return gql::ast::VisitorResult::kContinue;
        });

    for (auto& var : referencedVariables) {
      for (auto* variableScope = cond.variableScope; variableScope;
           variableScope = variableScope->parent) {
        auto varIt = variableScope->localVariables.find(var);
        if (varIt != variableScope->localVariables.end()) {
          auxData->referencedVariables[var] = {
              varIt->second.type, varIt->second.degree, varIt->second.isTemp};
          break;
        }
      }
    }
  }
}

SyntaxAnalyzer::BindingTableType
SyntaxAnalyzer::ProcessGraphPatternBindingTable(
    ast::SimpleMatchStatement& match,
    ExecutionContext& context) {
  ast::GraphPattern& graphPattern = match.pattern;

  // BEGIN feature checks
  if (match.yield) {
    ThrowIfFeatureNotSupported(standard::Feature::GQ19, match);
  }

  if (graphPattern.matchMode &&
      *graphPattern.matchMode == ast::MatchMode::DifferentEdges) {
    ThrowIfFeatureNotSupported(standard::Feature::G002, graphPattern);
  }

  if (graphPattern.keep) {
    if (graphPattern.keep->pathSearchPrefix) {
      ThrowIfFeatureNotSupported(standard::Feature::G007, *graphPattern.keep);
    } else {
      ThrowIfFeatureNotSupported(standard::Feature::G006, *graphPattern.keep);
    }
  }
  // END feature checks

  if (graphPattern.keep) {
    throw FormattedError(*graphPattern.keep, ErrorCode::E0061,
                         "KEEP in graph pattern must be rewritten to "
                         "path pattern");
  }

  if (graphPattern.matchMode &&
      *graphPattern.matchMode == ast::MatchMode::DifferentEdges) {
    int numSelectivePathPatterns = 0;
    for (auto& pathPattern : graphPattern.paths) {
      if (pathPattern.prefix) {
        if (pathPattern.prefix->pathSearchPrefix) {
          if (IsSelectivePattern(*pathPattern.prefix->pathSearchPrefix)) {
            numSelectivePathPatterns++;
            if (numSelectivePathPatterns > 1) {
              break;
            }
          }
        }
      }
    }

    if (numSelectivePathPatterns > 1) {
      // 16.4 Syntax Rule 12
      throw FormattedError(
          graphPattern, ErrorCode::E0062,
          "Multiple selective path patterns are not allowed when "
          "graph pattern has MATCH MODE DIFFERENT EDGES clause");
    }
  }

  GraphPatternContext graphPatternContext(graphPattern.matchMode &&
                                          *graphPattern.matchMode ==
                                              ast::MatchMode::DifferentEdges);
  for (auto& pathPattern : graphPattern.paths) {
    // BEGIN feature checks
    if (pathPattern.var) {
      ThrowIfFeatureNotSupported(standard::Feature::G004, *pathPattern.var);
    }
    if (pathPattern.prefix && pathPattern.prefix->pathSearchPrefix) {
      ThrowIfFeatureNotSupported(standard::Feature::G005, *pathPattern.prefix);
    }
    // END feature checks

    bool isSelectivePattern = false;
    ast::PathMode pathMode = ast::PathMode::WALK;
    if (pathPattern.prefix) {
      Process(*pathPattern.prefix, context);
      pathMode = pathPattern.prefix->mode;
      if (pathPattern.prefix->pathSearchPrefix) {
        isSelectivePattern =
            IsSelectivePattern(*pathPattern.prefix->pathSearchPrefix);
      }
    }

    PathModeScope pathModeScope(graphPatternContext, pathMode);
    PathPatternScope pathPatternScope(graphPatternContext, isSelectivePattern,
                                      pathPattern);
    if (pathPattern.var) {
      graphPatternContext.DeclarePathVariable(*pathPattern.var);
    }
    Process(pathPattern.expr, graphPatternContext, context);
  }
  if (graphPattern.where) {
    graphPatternContext.AddSearchCondition(&*graphPattern.where);
  }
  graphPatternContext.Finalize();

  const auto graphVariables = graphPatternContext.variables();

  // 16.3 Syntax Rules 7 and 9
  for (auto& var : graphVariables) {
    if (var.second.type != GraphPatternVariableType::Subpath) {
      continue;
    }
    if (HasField(context.workingRecord, var.first) ||
        HasField(context.workingTable, var.first)) {
      throw FormattedError(
          var.second.declarationPosition_, ErrorCode::E0063,
          "Subpath variable name cannot be the same as a context variable");
    }
  }

  // Variables that are added to the result
  std::vector<std::string> gpyc;
  if (match.yield) {
    std::unordered_set<std::string> referencedVariables;
    for (auto& yield : *match.yield) {
      // 16.3 Syntax Rule 12.a.i
      if (!referencedVariables.insert(yield.name).second) {
        throw FormattedError(yield, ErrorCode::E0064,
                             "YIELD variable references cannot be duplicated");
      }

      auto varIt = graphVariables.find(yield.name);
      // 16.3 Syntax Rule 12.a.ii
      if (varIt == graphVariables.end()) {
        throw FormattedError(
            yield, ErrorCode::E0065,
            "YIELD item must reference a variable in a graph pattern");
      }
      if (varIt->second.type == GraphPatternVariableType::Subpath) {
        throw FormattedError(yield, ErrorCode::E0066,
                             "YIELD item cannot reference subpath variable");
      }

      gpyc.push_back(yield.name);
    }
  } else {
    for (auto& [var, varDef] : graphVariables) {
      if (!varDef.isTemp && varDef.type != GraphPatternVariableType::Subpath) {
        gpyc.push_back(var);
      }
    }
  }

  // Working record and working table fields that are not variables in the
  // graph
  BindingTableType inputFields;
  // 16.3 Syntax Rule 26
  for (auto& f : context.workingRecord) {
    if (graphVariables.count(f.name.name) == 0) {
      inputFields.push_back(f);
    }
  }
  for (auto& f : context.workingTable) {
    if (graphVariables.count(f.name.name) == 0) {
      inputFields.push_back(f);
    }
  }

  BindingTableType resultType = inputFields;
  for (auto& var : gpyc) {
    auto& varDecl = graphVariables.find(var)->second;

    const ast::FieldType* existingField = nullptr;
    if ((existingField = HasField(context.workingRecord, var)) ||
        (existingField = HasField(context.workingTable, var))) {
      // Check 16.3 Syntax Rule 16.a
      bool isVarGroupDegree =
          (varDecl.degree ==
               VariableDegreeOfExposure::EffectivelyBoundedGroup ||
           varDecl.degree ==
               VariableDegreeOfExposure::EffectivelyUnboundedGroup);
      bool isIncomingGroupDegree = false;
      if (auto* listType = std::get_if<ast::ValueType::List>(
              &existingField->type->typeOption)) {
        isIncomingGroupDegree = listType->isGroup;
      }
      if (isVarGroupDegree != isIncomingGroupDegree) {
        throw FormattedError(varDecl.declarationPosition_, ErrorCode::E0067,
                             "Degree of reference of the variable and the "
                             "one of the field with "
                             "the same name must be the same");
      }
    }

    auto& newField = resultType.emplace_back();
    newField.name.name = var;
    newField.type = GetProjectedType(varDecl.type, varDecl.degree);
  }

  for (auto& cond : graphPatternContext.searchConditionScopes()) {
    ExecutionContext childContext;
    childContext.workingRecord = inputFields;
    // Degree of reference is defined in 20.12 Syntax Rule 11
    for (auto& [var, varDef] : graphVariables) {
      if (varDef.type == GraphPatternVariableType::Subpath) {
        continue;
      }
      auto localVar = cond.variableScope->localVariables.find(var);
      if (localVar != cond.variableScope->localVariables.end()) {
        // Expose the variable with locally bound degree of reference
        auto& type = childContext.workingRecord.emplace_back();
        type.name.name = var;
        type.type = GetProjectedType(varDef.type, localVar->second.degree);
      } else if (cond.scope && !cond.scope->count(var)) {
        // Expose variable as global except if it is not declared in the
        // selective path pattern which contains the search condition
        childContext.inaccessibleVariables[var] = ExecutionContext::
            InaccessibleReason::ReferenceFromSelectivePathPattern;
      } else if (cond.inaccessibleVariables.count(var)) {
        childContext.inaccessibleVariables[var] = ExecutionContext::
            InaccessibleReason::ReferenceToTheAdjacentUnionOperand;
      } else {
        // 20.12 Syntax Rule 11.a.ii.2
        auto degree = VariableDegreeOfExposure::EffectivelyUnboundedGroup;
        for (auto* parentContext = cond.variableScope->parent; parentContext;
             parentContext = parentContext->parent) {
          auto varIt = parentContext->localVariables.find(var);
          if (varIt != parentContext->localVariables.end()) {
            degree = varIt->second.degree;
            break;
          }
        }
        if (degree != VariableDegreeOfExposure::UnconditionalSingleton &&
            degree != VariableDegreeOfExposure::ConditionalSingleton) {
          // A nonlocal reference shall have singleton degree of
          // reference
          childContext.inaccessibleVariables[var] = ExecutionContext::
              InaccessibleReason::NonLocalReferenceWithGroupDegreeOfReference;
        } else {
          auto& type = childContext.workingRecord.emplace_back();
          type.name.name = var;
          type.type = GetProjectedType(varDef.type, degree);
        }
      }
    }

    ProcessBooleanValueExpression(*cond.condition, childContext);
  }

  // Fill graph pattern aux data
  {
    auto auxData = std::make_unique<syntax_analyzer::GraphPatternAuxData>();
    for (auto& [var, varDef] : graphVariables) {
      auxData->variables[var] = {varDef.type, varDef.degree, varDef.isTemp};
    }
    graphPattern.auxData = std::move(auxData);
  }

  FillWhereExpressionsAuxData(graphPatternContext);

  return resultType;
}

static ast::ValueType GetProjectedType(GraphPatternVariableType type,
                                       VariableDegreeOfExposure degree) {
  ast::ValueType elementType;
  switch (type) {
    case GraphPatternVariableType::Node:
      elementType.typeOption.emplace<ast::NodeReferenceValueType>();
      break;
    case GraphPatternVariableType::Edge:
      elementType.typeOption.emplace<ast::EdgeReferenceValueType>();
      break;
    case GraphPatternVariableType::Path:
      elementType.typeOption = ast::SimplePredefinedType::Path;
      break;
    default:
      assert(false);
  }

  ast::ValueType projectedType;
  switch (degree) {
    case VariableDegreeOfExposure::UnconditionalSingleton:
      projectedType = elementType;
      projectedType.notNull = true;
      break;
    case VariableDegreeOfExposure::ConditionalSingleton:
      projectedType = elementType;
      break;
    case VariableDegreeOfExposure::EffectivelyBoundedGroup:
    case VariableDegreeOfExposure::EffectivelyUnboundedGroup: {
      projectedType.notNull = true;
      elementType.notNull = true;
      auto& list = projectedType.typeOption.emplace<ast::ValueType::List>();
      list.isGroup = true;
      list.valueType.emplace() = elementType;
      break;
    }
    default:
      assert(false);
  }
  return projectedType;
}

}  // namespace gql