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

#include "graph_pattern_context.h"

#include <assert.h>
#include <limits>
#include <stdexcept>

#include "common/formatted_error.h"
#include "gql/syntax_analyzer/aux_data.h"

namespace gql::detail {

namespace {

std::string VariableTypeToString(GraphPatternVariableType type) {
  switch (type) {
    case GraphPatternVariableType::Node:
      return "node";
    case GraphPatternVariableType::Edge:
      return "edge";
    case GraphPatternVariableType::Path:
      return "path";
    case GraphPatternVariableType::Subpath:
      return "subpath";
    default:
      assert(false);
      return "";
  }
}

}  // namespace

GraphPatternContext::GraphPatternContext(bool differentEdgesMatchMode)
    : differentEdgesMatchMode_(differentEdgesMatchMode) {
  minimumPathLength_.push(0);
  nonZeroNodeCount_.push(false);
  exposedVariables_.emplace();
  declarationsInUnions_.emplace();
  isRestrictivePathMode_.emplace();
  EnterVariableScope();
}

GraphPatternContext::~GraphPatternContext() {
  if (!std::uncaught_exceptions()) {
    assert(exposedVariables_.size() == 1);
    assert(declarationsInUnions_.size() == 1);
    assert(isRestrictivePathMode_.size() == 1);
    assert(minimumPathLength_.size() == 1);
    assert(nonZeroNodeCount_.size() == 1);
  }
}

GraphPatternContext::Variables GraphPatternContext::variables() const {
  Variables result;
  for (auto& [var, varDecl] : variableDeclarations_) {
    auto& varDef = exposedVariables_.top().find(var)->second;
    result[var] = {varDecl.type_, varDecl.firstDeclarationPosition_,
                   varDef.isTemp, varDef.degree};
  }
  return result;
}

const GraphPatternContext::SearchConditionScopes&
GraphPatternContext::searchConditionScopes() const {
  return searchConditionScopes_;
}

void GraphPatternContext::Finalize() {
  for (auto& cond : searchConditionScopes_) {
    for (auto varIt = cond.inaccessibleVariables.begin();
         varIt != cond.inaccessibleVariables.end();) {
      if (varIt->second < declarationsInUnions_.top()[varIt->first]) {
        varIt = cond.inaccessibleVariables.erase(varIt);
      } else {
        ++varIt;
      }
    }
  }
  ExitVariableScope();
}

void GraphPatternContext::DeclareVariable(const std::string& name,
                                          const ast::Node& node,
                                          GraphPatternVariableType type,
                                          bool isTemp) {
  // Check that we traverse variables in the order of their appearance in the
  // query.
  if (!variableDeclarations_.empty()) {
    assert(!node.inputPosition().IsSet() ||
           !lastVariableDeclarationPosition_.IsSet() ||
           node.inputPosition() > lastVariableDeclarationPosition_);
  }
  if (!lastVariableDeclarationPosition_.IsSet()) {
    lastVariableDeclarationPosition_ = node.inputPosition();
  }

  auto it = variableDeclarations_.find(name);
  if (it == variableDeclarations_.end()) {
    auto& newVar = variableDeclarations_[name];
    newVar.declarationOrder_ =
        static_cast<int>(variableDeclarations_.size() - 1);
    newVar.type_ = type;
    newVar.firstDeclarationPosition_ = node.inputPosition();
  } else {
    if (it->second.type_ != type) {
      throw FormattedError(
          node, ErrorCode::E0001,
          "{0} variable \"{1}\" was declared before as a {2} variable",
          VariableTypeToString(type), name,
          VariableTypeToString(it->second.type_));
    }
    switch (type) {
      case GraphPatternVariableType::Path:
        throw FormattedError(
            node, ErrorCode::E0002,
            "Path variable \"{0}\" was declared more than once", name);
      case GraphPatternVariableType::Subpath:
        // 16.4 Syntax Rule 4: Two <path pattern>s shall not expose the same
        // subpath variable
        // 16.7 Syntax Rule 22b: A <parenthesized path pattern expression> PPPE
        // that simply contains a <subpath variable declaration> that declares
        // EV exposes EV as an unconditional singleton variable. PPPE shall not
        // contain another <parenthesized path pattern expression> that declares
        // EV.
        throw FormattedError(
            node, ErrorCode::E0003,
            "Subpath variable \"{0}\" was declared more than once", name);
      case GraphPatternVariableType::Node:
      case GraphPatternVariableType::Edge:
        break;
      default:
        assert(false);
        break;
    }
  }
  ExposeNewVariable(name, ExposedVariable{type, node.inputPosition(), isTemp});
  declarationsInUnions_.top()[name]++;
}

void GraphPatternContext::DeclarePathVariable(const ast::PathVariable& var) {
  DeclareVariable(var.name, var, GraphPatternVariableType::Path);
}

void GraphPatternContext::DeclareSubpathVariable(
    const ast::SubpathVariable& var) {
  DeclareVariable(var.name, var, GraphPatternVariableType::Subpath);
}

void GraphPatternContext::DeclareNodeVariable(
    const ast::ElementVariableDeclaration& var) {
  DeclareVariable(var.name, var, GraphPatternVariableType::Node, var.isTemp);

  if (expectingLeftBoundaryVariable_ && !var.isTemp) {
    leftBoundaryVariable_ = var.name;
    expectingLeftBoundaryVariable_ = false;
  }
  possibleRightBoundaryVariable_ = var.name;
}

void GraphPatternContext::DeclareEdgeVariable(
    const ast::ElementVariableDeclaration& var) {
  DeclareVariable(var.name, var, GraphPatternVariableType::Edge, var.isTemp);
}

void GraphPatternContext::EnterPathPattern(bool isSelectivePattern) {
  isInsideSelectivePattern_ = isSelectivePattern;
  expectingLeftBoundaryVariable_ = isSelectivePattern;
  leftBoundaryVariable_.reset();
  possibleRightBoundaryVariable_.reset();
  exposedVariables_.emplace();

  if (isInsideSelectivePattern_) {
    // 4.11.5
    EnterVariableScope();
    indexOfFirstSearchConditionScopeInPathPattern_ =
        searchConditionScopes_.size();
  }

  nonZeroNodeCount_.push(false);
}

void GraphPatternContext::ExitPathPattern(ast::PathPattern& node) {
  // 16.6 Syntax Rule 8: A selective <path pattern> SPP shall not contain
  // a reference to a graph pattern variable that is not declared by SPP.
  if (isInsideSelectivePattern_) {
    // 4.11.5
    std::unordered_set<std::string> varSet;
    for (auto& v : exposedVariables_.top()) {
      varSet.insert(v.first);
    }
    ExitVariableScope();

    for (auto i = indexOfFirstSearchConditionScopeInPathPattern_;
         i < searchConditionScopes_.size(); ++i) {
      searchConditionScopes_[i].scope = varSet;
    }

    // 16.6 Syntax Rule 5
    for (auto& v : exposedVariables_.top()) {
      if (leftBoundaryVariable_ && v.first == *leftBoundaryVariable_ ||
          possibleRightBoundaryVariable_ &&
              v.first == *possibleRightBoundaryVariable_) {
        assert(v.second.degree ==
               VariableDegreeOfExposure::UnconditionalSingleton);
      } else {
        v.second.isStrictInterior_ = true;
      }
    }
  }

  auto auxData = std::make_unique<syntax_analyzer::PathPatternAuxData>();
  for (auto& v : exposedVariables_.top()) {
    if (v.second.degree == VariableDegreeOfExposure::UnconditionalSingleton) {
      auxData->joinableVariables.insert(v.first);
    }
  }
  node.auxData = std::move(auxData);

  auto localExposedVariables = std::move(exposedVariables_.top());
  exposedVariables_.pop();
  for (auto& v : localExposedVariables) {
    // 16.7 Syntax Rule 22.h
    if (v.second.degree ==
        VariableDegreeOfExposure::EffectivelyUnboundedGroup) {
      v.second.degree = VariableDegreeOfExposure::EffectivelyBoundedGroup;
    }
    ExposeVariable(v.first, v.second);
  }

  if (!nonZeroNodeCount_.top()) {
    // 16.7 Syntax Rule 15
    throw FormattedError(
        node, ErrorCode::E0109,
        "Path pattern shall have minimum node count that is greater than zero");
  }
  nonZeroNodeCount_.pop();
}

void GraphPatternContext::EnterParenthesizedPathPatternExpression() {
  EnterVariableScope();

  nonZeroNodeCount_.push(false);
}

void GraphPatternContext::ExitParenthesizedPathPatternExpression(
    bool hasSubpathVariable,
    const ast::Node& node) {
  ExitVariableScope();

  if (hasSubpathVariable) {
    // 16.7 Syntax Rule 20 "If a <parenthesized path pattern expression>
    // PPPE simply contains a <subpath variable declaration>, then the
    // minimum node count of PPPE shall be greater than 0 (zero). And 16
    // too
    if (!nonZeroNodeCount_.top()) {
      throw FormattedError(node, ErrorCode::E0110,
                           "Subpath pattern shall have minimum node count "
                           "that is greater than zero");
    }
  }
  bool nonZeroNodeCount = nonZeroNodeCount_.top();
  nonZeroNodeCount_.pop();
  nonZeroNodeCount_.top() |= nonZeroNodeCount;
}

void GraphPatternContext::EnterVariableScope() {
  exposedVariables_.emplace();
  variableScopes_.emplace_back();
  if (!variableScopeStack_.empty()) {
    variableScopes_.back().parent = variableScopeStack_.top();
  }
  variableScopeStack_.push(&variableScopes_.back());
}

void GraphPatternContext::ExitVariableScope() {
  for (auto& [var, def] : exposedVariables_.top()) {
    variableScopeStack_.top()->localVariables[var] = def;
  }
  variableScopeStack_.pop();
  AppendExposedVariables();
}

void GraphPatternContext::EnterPathMode(ast::PathMode mode) {
  isRestrictivePathMode_.push(isRestrictivePathMode_.top() ||
                              mode != ast::PathMode::WALK);
}

void GraphPatternContext::ExitPathMode() {
  isRestrictivePathMode_.pop();
}

void GraphPatternContext::EnterQuantifiedPathPrimary(const ast::Node& node,
                                                     bool bounded) {
  if (isInsideQuantifiedPathPrimary_) {
    // 16.7 Syntax Rule 9: The <path primary> simply contained in a <quantified
    // path primary> shall not contain a <quantified path primary> at the same
    // depth of graph pattern matching.
    throw FormattedError(node, ErrorCode::E0004,
                         "Nested quantified path primary is not allowed");
  }
  isInsideQuantifiedPathPrimary_ = true;

  if (!bounded && !IsInsideRestrictiveSearch() && !isInsideSelectivePattern_ &&
      !differentEdgesMatchMode_) {
    // 16.4 Syntax Rule 14
    throw FormattedError(
        node, ErrorCode::E0005,
        "An unbounded quantified path primary shall be inside a restrictive "
        "search or a selective path pattern");
  }

  expectingLeftBoundaryVariable_ = false;

  minimumPathLength_.push(0);
  nonZeroNodeCount_.push(false);

  exposedVariables_.emplace();
}

void GraphPatternContext::ExitQuantifiedPathPrimary(const ast::Node& node,
                                                    bool bounded,
                                                    int lowerBound) {
  isInsideQuantifiedPathPrimary_ = false;

  // 16.7 Syntax Rule 22.e
  auto localExposedVariables = std::move(exposedVariables_.top());
  exposedVariables_.pop();
  for (auto& v : localExposedVariables) {
    if (v.second.degree !=
        VariableDegreeOfExposure::EffectivelyUnboundedGroup) {
      if (bounded || IsInsideRestrictiveSearch()) {
        v.second.degree = VariableDegreeOfExposure::EffectivelyBoundedGroup;
      } else {
        v.second.degree = VariableDegreeOfExposure::EffectivelyUnboundedGroup;
      }
    }
    ExposeNewVariable(v.first, v.second);
  }

  possibleRightBoundaryVariable_.reset();

  if (minimumPathLength_.top() == 0) {
    throw FormattedError(
        node, ErrorCode::E0006,
        "A quantified path primary shall have minimum path length that is "
        "greater than zero");
  }
  int minimumPathLength = minimumPathLength_.top();
  minimumPathLength_.pop();
  minimumPathLength_.top() += minimumPathLength * lowerBound;

  bool nonZeroNodeCount = nonZeroNodeCount_.top();
  nonZeroNodeCount_.pop();
  nonZeroNodeCount_.top() |= nonZeroNodeCount && lowerBound > 0;
}

void GraphPatternContext::EnterQuestionedPathPrimary() {
  expectingLeftBoundaryVariable_ = false;

  minimumPathLength_.push(0);
  nonZeroNodeCount_.push(false);

  exposedVariables_.emplace();
}

void GraphPatternContext::ExitQuestionedPathPrimary(const ast::Node& node) {
  auto localExposedVariables = std::move(exposedVariables_.top());
  exposedVariables_.pop();
  for (auto& v : localExposedVariables) {
    switch (v.second.degree) {
      case VariableDegreeOfExposure::EffectivelyBoundedGroup:
      case VariableDegreeOfExposure::EffectivelyUnboundedGroup:
      case VariableDegreeOfExposure::ConditionalSingleton:
        break;
      case VariableDegreeOfExposure::UnconditionalSingleton:
        v.second.degree = VariableDegreeOfExposure::ConditionalSingleton;
        break;
    }
    ExposeNewVariable(v.first, v.second);
  }

  possibleRightBoundaryVariable_.reset();

  if (minimumPathLength_.top() == 0) {
    throw FormattedError(
        node, ErrorCode::E0007,
        "A questioned path primary shall have minimum path length that is "
        "greater than zero");
  }
  minimumPathLength_.pop();
  nonZeroNodeCount_.pop();
}

void GraphPatternContext::EnterPathPatternUnion() {
  exposedVariables_.emplace();

  expectingLeftBoundaryVariable_ = false;

  minimumPathLength_.push(std::numeric_limits<int>::max());
  nonZeroNodeCount_.push(true);

  pathPatternUnion_.emplace()
      .indexOfFirstSearchConditionScopeInOperands_.push_back(
          searchConditionScopes_.size());
}

void GraphPatternContext::ExitPathPatternUnion() {
  AppendExposedVariables();

  possibleRightBoundaryVariable_.reset();

  int minimumPathLength = minimumPathLength_.top();
  minimumPathLength_.pop();
  minimumPathLength_.top() += minimumPathLength;

  bool nonZeroNodeCount = nonZeroNodeCount_.top();
  nonZeroNodeCount_.pop();
  nonZeroNodeCount_.top() |= nonZeroNodeCount;

  auto& currentUnion = pathPatternUnion_.top();
  for (size_t i = 0; i < currentUnion.declarationsInOperands_.size(); i++) {
    auto& declarations = currentUnion.declarationsInOperands_[i];
    for (size_t j = 0; j < currentUnion.declarationsInOperands_.size(); j++) {
      if (i == j) {
        continue;
      }
      for (auto k = currentUnion.indexOfFirstSearchConditionScopeInOperands_[j];
           k < currentUnion.indexOfFirstSearchConditionScopeInOperands_[j + 1];
           k++) {
        for (auto& var : declarations) {
          searchConditionScopes_[k].inaccessibleVariables[var.first] +=
              var.second;
        }
      }
    }
  }
  pathPatternUnion_.pop();
}

void GraphPatternContext::EnterPathPatternUnionOperand() {
  exposedVariables_.emplace();

  minimumPathLength_.push(0);
  nonZeroNodeCount_.push(false);

  declarationsInUnions_.emplace();
}

void GraphPatternContext::ExitPathPatternUnionOperand() {
  // 16.7 Syntax Rule 22.d
  auto exposedVariables = std::move(exposedVariables_.top());
  exposedVariables_.pop();
  auto& unionExposedVariables = exposedVariables_.top();
  for (auto& v : unionExposedVariables) {
    if (v.second.degree == VariableDegreeOfExposure::UnconditionalSingleton &&
        !exposedVariables.count(v.first)) {
      v.second.degree = VariableDegreeOfExposure::ConditionalSingleton;
    }
  }
  for (auto& v : exposedVariables) {
    auto it = unionExposedVariables.find(v.first);
    if (it == unionExposedVariables.end()) {
      if (!pathPatternUnion_.top().isFirstOperand() &&
          v.second.degree == VariableDegreeOfExposure::UnconditionalSingleton) {
        v.second.degree = VariableDegreeOfExposure::ConditionalSingleton;
      }
      auto varCopy = v;
      unionExposedVariables.insert(std::move(varCopy));
    } else {
      if (v.second.degree ==
              VariableDegreeOfExposure::EffectivelyUnboundedGroup ||
          it->second.degree ==
              VariableDegreeOfExposure::EffectivelyUnboundedGroup) {
        it->second.degree = VariableDegreeOfExposure::EffectivelyUnboundedGroup;
      } else if (v.second.degree ==
                     VariableDegreeOfExposure::EffectivelyBoundedGroup ||
                 it->second.degree ==
                     VariableDegreeOfExposure::EffectivelyBoundedGroup) {
        it->second.degree = VariableDegreeOfExposure::EffectivelyBoundedGroup;
      } else if (v.second.degree ==
                     VariableDegreeOfExposure::ConditionalSingleton ||
                 it->second.degree ==
                     VariableDegreeOfExposure::ConditionalSingleton) {
        it->second.degree = VariableDegreeOfExposure::ConditionalSingleton;
      }
    }
  }

  int minimumPathLength = minimumPathLength_.top();
  minimumPathLength_.pop();
  minimumPathLength_.top() =
      std::min(minimumPathLength_.top(), minimumPathLength);

  bool nonZeroNodeCount = nonZeroNodeCount_.top();
  nonZeroNodeCount_.pop();
  nonZeroNodeCount_.top() = nonZeroNodeCount && nonZeroNodeCount_.top();

  pathPatternUnion_.top().indexOfFirstSearchConditionScopeInOperands_.push_back(
      searchConditionScopes_.size());
  pathPatternUnion_.top().declarationsInOperands_.push_back(
      declarationsInUnions_.top());

  {
    auto localVariables = std::move(declarationsInUnions_.top());
    declarationsInUnions_.pop();
    for (auto& v : localVariables) {
      declarationsInUnions_.top()[v.first] += v.second;
    }
  }
}

void GraphPatternContext::EnterNodePattern() {
  nonZeroNodeCount_.top() = true;
}

void GraphPatternContext::ExitNodePattern() {}

void GraphPatternContext::EnterEdgePattern() {
  expectingLeftBoundaryVariable_ = false;
  possibleRightBoundaryVariable_.reset();

  minimumPathLength_.top()++;
}

void GraphPatternContext::ExitEdgePattern() {}

void GraphPatternContext::AddSearchCondition(
    ast::ParenthesizedPathPatternWhereClause* where) {
  auto& newCondition = searchConditionScopes_.emplace_back();
  newCondition.condition = where->condition.get();
  newCondition.auxData = &where->auxData;
  newCondition.variableScope = variableScopeStack_.top();
}

void GraphPatternContext::AddSearchCondition(
    ast::GraphPatternWhereClause* where) {
  auto& newCondition = searchConditionScopes_.emplace_back();
  newCondition.condition = where->condition.get();
  newCondition.auxData = &where->auxData;
  newCondition.variableScope = variableScopeStack_.top();
}

void GraphPatternContext::AppendExposedVariables() {
  auto newExposedVariables = std::move(exposedVariables_.top());
  exposedVariables_.pop();

  for (auto& v : newExposedVariables) {
    ExposeVariable(v.first, v.second);
  }
}

void GraphPatternContext::ExposeNewVariable(const std::string& name,
                                            ExposedVariable variable) {
  ExposeVariable(name, variable);

  if (variable.type == GraphPatternVariableType::Node ||
      variable.type == GraphPatternVariableType::Edge) {
    assert(currentVariableReferenceScope_);
    currentVariableReferenceScope_->declaredVariables[name] = {
        variable.type, variable.degree, /*isTemp=*/false};
  }
}

void GraphPatternContext::ExposeVariable(const std::string& name,
                                         ExposedVariable variable) {
  auto& currentExposedVariables = exposedVariables_.top();
  auto [it, inserted] = currentExposedVariables.emplace(name, variable);
  if (!inserted) {
    auto& existingVar = it->second;
    assert(variable.isTemp == existingVar.isTemp);
    // 16.4 Syntax Rule 3: In a <path pattern list>, if two <path pattern>s
    // expose an element variable EV, then both shall expose EV as an
    // unconditional singleton variable.
    //
    // 16.7 Syntax Rule 22c
    if (variable.degree != VariableDegreeOfExposure::UnconditionalSingleton ||
        existingVar.degree !=
            VariableDegreeOfExposure::UnconditionalSingleton) {
      throw FormattedError(variable.declarationPosition_, ErrorCode::E0008,
                           "Element variable \"{0}\" was declared before and "
                           "has incompatible degree of exposure",
                           name);
    }

    // 16.6 Syntax Rule 7: A strict interior variable of one selective <path
    // pattern> shall not be equivalent to an exterior variable, or to an
    // interior variable of another selective <path pattern>.
    if (variable.isStrictInterior_ || existingVar.isStrictInterior_) {
      throw FormattedError(
          variable.declarationPosition_, ErrorCode::E0009,
          "Element variable \"{0}\" is a strict interior variable of one "
          "selective path pattern and can't be exposed by another",
          name);
    }
  }
}

bool GraphPatternContext::IsInsideRestrictiveSearch() const {
  return isRestrictivePathMode_.top();
}

PathPatternScope::PathPatternScope(GraphPatternContext& context,
                                   bool isSelectivePattern,
                                   ast::PathPattern& node)
    : context_(context), node_(node) {
  context_.EnterPathPattern(isSelectivePattern);
}

PathPatternScope ::~PathPatternScope() noexcept(false) {
  if (!std::uncaught_exceptions()) {
    context_.ExitPathPattern(node_);
  }
}

VariableReferenceScope::VariableReferenceScope(GraphPatternContext& context,
                                               ast::PathFactor& statement)
    : context_(context) {
  auto auxData =
      std::make_unique<syntax_analyzer::PathVariableReferenceScopeAuxData>();
  auxData_ = auxData.get();
  statement.auxData = std::move(auxData);
  parentScopeAuxData_ = context_.currentVariableReferenceScope_;
  context_.currentVariableReferenceScope_ = auxData_;
}

VariableReferenceScope::VariableReferenceScope(
    GraphPatternContext& context,
    ast::PathPatternExpression& statement)
    : context_(context) {
  auto auxData =
      std::make_unique<syntax_analyzer::PathVariableReferenceScopeAuxData>();
  auxData_ = auxData.get();
  statement.auxData = std::move(auxData);
  parentScopeAuxData_ = context_.currentVariableReferenceScope_;
  context_.currentVariableReferenceScope_ = auxData_;
}

VariableReferenceScope::~VariableReferenceScope() {
  context_.currentVariableReferenceScope_ = parentScopeAuxData_;
}

ParenthesizedPathPatternExpressionScope::
    ParenthesizedPathPatternExpressionScope(GraphPatternContext& context,
                                            bool hasSubpathVariable,
                                            const ast::Node& node)
    : context_(context), node_(node), hasSubpathVariable_(hasSubpathVariable) {
  context_.EnterParenthesizedPathPatternExpression();
}

ParenthesizedPathPatternExpressionScope::
    ~ParenthesizedPathPatternExpressionScope() noexcept(false) {
  if (!std::uncaught_exceptions()) {
    context_.ExitParenthesizedPathPatternExpression(hasSubpathVariable_, node_);
  }
}

PathModeScope::PathModeScope(GraphPatternContext& context, ast::PathMode mode)
    : context_(context) {
  context_.EnterPathMode(mode);
}

PathModeScope::~PathModeScope() noexcept(false) {
  if (!std::uncaught_exceptions()) {
    context_.ExitPathMode();
  }
}

QuantifiedPathPrimaryScope::QuantifiedPathPrimaryScope(
    GraphPatternContext& context,
    const ast::Node& node)
    : context_(context), node_(node) {}

QuantifiedPathPrimaryScope::~QuantifiedPathPrimaryScope() noexcept(false) {
  if (isActive_ && !std::uncaught_exceptions()) {
    context_.ExitQuantifiedPathPrimary(node_, bounded_, lowerBound_);
  }
}

void QuantifiedPathPrimaryScope::Activate(bool bounded, int lowerBound) {
  bounded_ = bounded;
  lowerBound_ = lowerBound;

  assert(!isActive_);
  isActive_ = true;
  context_.EnterQuantifiedPathPrimary(node_, bounded_);
}

QuestionedPathPrimaryScope::QuestionedPathPrimaryScope(
    GraphPatternContext& context,
    const ast::Node& node)
    : context_(context), node_(node) {}

QuestionedPathPrimaryScope::~QuestionedPathPrimaryScope() noexcept(false) {
  if (isActive_ && !std::uncaught_exceptions()) {
    context_.ExitQuestionedPathPrimary(node_);
  }
}

void QuestionedPathPrimaryScope::Activate() {
  assert(!isActive_);
  isActive_ = true;
  context_.EnterQuestionedPathPrimary();
}

PathPatternUnionScope::PathPatternUnionScope(GraphPatternContext& context)
    : context_(context) {}

PathPatternUnionScope::~PathPatternUnionScope() noexcept(false) {
  if (isActive_ && !std::uncaught_exceptions()) {
    context_.ExitPathPatternUnion();
  }
}

void PathPatternUnionScope::Activate() {
  assert(!isActive_);
  isActive_ = true;
  context_.EnterPathPatternUnion();
}

PathPatternUnionOperandScope::PathPatternUnionOperandScope(
    GraphPatternContext& context)
    : context_(context) {}

PathPatternUnionOperandScope::~PathPatternUnionOperandScope() noexcept(false) {
  if (isActive_ && !std::uncaught_exceptions()) {
    context_.ExitPathPatternUnionOperand();
  }
}

void PathPatternUnionOperandScope::Activate() {
  assert(!isActive_);
  isActive_ = true;
  context_.EnterPathPatternUnionOperand();
}

NodePatternScope::NodePatternScope(GraphPatternContext& context)
    : context_(context) {
  context_.EnterNodePattern();
}

NodePatternScope::~NodePatternScope() noexcept(false) {
  if (!std::uncaught_exceptions()) {
    context_.ExitNodePattern();
  }
}

EdgePatternScope::EdgePatternScope(GraphPatternContext& context)
    : context_(context) {
  context_.EnterEdgePattern();
}

EdgePatternScope::~EdgePatternScope() noexcept(false) {
  if (!std::uncaught_exceptions()) {
    context_.ExitEdgePattern();
  }
}

}  // namespace gql::detail