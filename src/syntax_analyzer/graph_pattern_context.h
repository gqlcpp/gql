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

#include <list>
#include <stack>
#include <string>
#include <unordered_map>
#include <unordered_set>

#include "gql/ast/ast.h"
#include "gql/syntax_analyzer/defs.h"

namespace gql::detail {

class GraphPatternContext {
 public:
  explicit GraphPatternContext(bool differentEdgesMatchMode);
  ~GraphPatternContext();

  struct Variable;
  struct VariableScope;

  using Variables = std::unordered_map<std::string, Variable>;
  Variables variables() const;

  struct SearchConditionScope;
  using SearchConditionScopes = std::vector<SearchConditionScope>;
  const SearchConditionScopes& searchConditionScopes() const;

  void Finalize();

  void DeclarePathVariable(const ast::PathVariable&);
  void DeclareSubpathVariable(const ast::SubpathVariable&);
  void DeclareNodeVariable(const ast::ElementVariableDeclaration&);
  void DeclareEdgeVariable(const ast::ElementVariableDeclaration&);

  void EnterPathPattern(bool isSelectivePattern);
  void ExitPathPattern(ast::PathPattern&);

  void EnterParenthesizedPathPatternExpression();
  void ExitParenthesizedPathPatternExpression(bool hasSubpathVariable,
                                              const ast::Node&);

  void EnterVariableScope();
  void ExitVariableScope();

  void EnterPathMode(ast::PathMode);
  void ExitPathMode();

  void EnterPathPatternUnion();
  void ExitPathPatternUnion();

  void EnterPathPatternUnionOperand();
  void ExitPathPatternUnionOperand();

  void EnterQuantifiedPathPrimary(const ast::Node&, bool bounded);
  void ExitQuantifiedPathPrimary(const ast::Node&,
                                 bool bounded,
                                 int lowerBound);

  void EnterQuestionedPathPrimary();
  void ExitQuestionedPathPrimary(const ast::Node&);

  void EnterNodePattern();
  void ExitNodePattern();

  void EnterEdgePattern();
  void ExitEdgePattern();

  // Store search conditions from parenthesized path pattern and graph pattern
  // where clause for later use together with theirs scope.
  void AddSearchCondition(ast::ParenthesizedPathPatternWhereClause*);
  void AddSearchCondition(ast::GraphPatternWhereClause*);

 private:
  struct ExposedVariable;
  struct VariableDeclaration;
  using CountedVariableDeclarations = std::unordered_map<std::string, int>;

  struct Union {
    // Used to fill |SearchConditionScope::inaccessibleVariables_|
    std::vector<size_t> indexOfFirstSearchConditionScopeInOperands_;
    std::vector<CountedVariableDeclarations> declarationsInOperands_;

    bool isFirstOperand() const { return declarationsInOperands_.empty(); }
  };

  void DeclareVariable(const std::string& name,
                       const ast::Node&,
                       GraphPatternVariableType,
                       bool isTemp = false);
  void AppendExposedVariables();
  // Expose new variable or in a new reference context.
  void ExposeNewVariable(const std::string& name, ExposedVariable);
  void ExposeVariable(const std::string& name, ExposedVariable);
  bool IsInsideRestrictiveSearch() const;

  bool isInsideQuantifiedPathPrimary_ = false;
  bool isInsideSelectivePattern_ = false;

  bool differentEdgesMatchMode_ = false;

  std::stack<bool> isRestrictivePathMode_;

  // 4.11.5 Variable's locality is used in references to a graph pattern
  // variable in search conditions.
  std::list<VariableScope> variableScopes_;
  std::stack<VariableScope*> variableScopeStack_;

  using ExposedVariables = std::unordered_map<std::string, ExposedVariable>;
  std::stack<ExposedVariables> exposedVariables_;

  std::unordered_map<std::string, VariableDeclaration> variableDeclarations_;
  ast::InputPosition lastVariableDeclarationPosition_;

  std::stack<CountedVariableDeclarations> declarationsInUnions_;
  SearchConditionScopes searchConditionScopes_;
  size_t indexOfFirstSearchConditionScopeInPathPattern_ = 0;

  // For 16.6 Syntax Rule 5
  bool expectingLeftBoundaryVariable_ = false;
  std::optional<std::string> leftBoundaryVariable_;
  std::optional<std::string> possibleRightBoundaryVariable_;

  std::stack<Union> pathPatternUnion_;

  // For 16.7 Syntax Rule 8
  std::stack<int> minimumPathLength_;
  // For 16.7 Syntax Rule 15
  std::stack<bool> nonZeroNodeCount_;

  syntax_analyzer::PathVariableReferenceScopeAuxData*
      currentVariableReferenceScope_ = nullptr;

  friend class VariableReferenceScope;
};

struct GraphPatternContext::Variable {
  GraphPatternVariableType type;
  ast::InputPosition declarationPosition_;
  bool isTemp = false;
  VariableDegreeOfExposure degree =
      VariableDegreeOfExposure::UnconditionalSingleton;
};

struct GraphPatternContext::ExposedVariable : Variable {
  bool isStrictInterior_ = false;  // For 16.6 Syntax Rule 5
};

struct GraphPatternContext::VariableScope {
  using Variables = std::unordered_map<std::string, Variable>;

  VariableScope* parent = nullptr;
  Variables localVariables;
};

struct GraphPatternContext::VariableDeclaration {
  GraphPatternVariableType type_;
  int declarationOrder_;
  ast::InputPosition firstDeclarationPosition_;
};

struct GraphPatternContext::SearchConditionScope {
  ast::ValueExpression* condition;
  std::shared_ptr<const syntax_analyzer::GraphPatternWhereClauseAuxData>*
      auxData;
  const VariableScope* variableScope = nullptr;

  // A selective <path pattern> SPP shall not reference a graph pattern
  // variable that is not declared in SPP |scope_| is set to the set of
  // variables declared in the selective path pattern
  std::optional<std::unordered_set<std::string>> scope;

  // A <path term> PPUOP simply contained in a <path pattern union> PSD
  // shall not contain a reference to an element variable that is not
  // declared in PPUOP or outside of PSD.
  //
  // In |inaccessibleVariables_| we count the number of times a variable is
  // declared inside union operands adjacent to the one containing the search
  // condition. After entire graph pattern is processed we check (in
  // |Finalize()|) if the total number of declarations of a variable is
  // greater than this count. If it is, then the variable is accessible,
  // otherwise it isn't.
  CountedVariableDeclarations inaccessibleVariables;
};

class PathPatternScope {
 public:
  PathPatternScope(GraphPatternContext&,
                   bool isSelectivePattern,
                   ast::PathPattern&);
  ~PathPatternScope() noexcept(false);

 private:
  GraphPatternContext& context_;
  ast::PathPattern& node_;
};

class VariableReferenceScope {
 public:
  VariableReferenceScope(GraphPatternContext&, ast::PathFactor&);
  VariableReferenceScope(GraphPatternContext&, ast::PathPatternExpression&);
  ~VariableReferenceScope();

 private:
  GraphPatternContext& context_;
  syntax_analyzer::PathVariableReferenceScopeAuxData* auxData_;
  syntax_analyzer::PathVariableReferenceScopeAuxData* parentScopeAuxData_;
};

class ParenthesizedPathPatternExpressionScope {
 public:
  ParenthesizedPathPatternExpressionScope(GraphPatternContext&,
                                          bool hasSubpathVariable,
                                          const ast::Node&);
  ~ParenthesizedPathPatternExpressionScope() noexcept(false);

 private:
  GraphPatternContext& context_;
  const ast::Node& node_;
  const bool hasSubpathVariable_;
};

class PathModeScope {
 public:
  PathModeScope(GraphPatternContext&, ast::PathMode);
  ~PathModeScope() noexcept(false);

 private:
  GraphPatternContext& context_;
};

class QuantifiedPathPrimaryScope {
 public:
  QuantifiedPathPrimaryScope(GraphPatternContext&, const ast::Node&);
  ~QuantifiedPathPrimaryScope() noexcept(false);

  void Activate(bool bounded, int lowerBound);

 private:
  GraphPatternContext& context_;
  const ast::Node& node_;
  bool bounded_;
  int lowerBound_;
  bool isActive_ = false;
};

class QuestionedPathPrimaryScope {
 public:
  QuestionedPathPrimaryScope(GraphPatternContext&, const ast::Node&);
  ~QuestionedPathPrimaryScope() noexcept(false);

  void Activate();

 private:
  GraphPatternContext& context_;
  const ast::Node& node_;
  bool isActive_ = false;
};

class PathPatternUnionScope {
 public:
  explicit PathPatternUnionScope(GraphPatternContext&);
  ~PathPatternUnionScope() noexcept(false);

  void Activate();
  bool isActive() const { return isActive_; }

 private:
  GraphPatternContext& context_;
  bool isActive_ = false;
};

class PathPatternUnionOperandScope {
 public:
  explicit PathPatternUnionOperandScope(GraphPatternContext&);
  ~PathPatternUnionOperandScope() noexcept(false);

  void Activate();

 private:
  GraphPatternContext& context_;
  bool isActive_ = false;
};

class NodePatternScope {
 public:
  explicit NodePatternScope(GraphPatternContext&);
  ~NodePatternScope() noexcept(false);

 private:
  GraphPatternContext& context_;
};

class EdgePatternScope {
 public:
  explicit EdgePatternScope(GraphPatternContext&);
  ~EdgePatternScope() noexcept(false);

 private:
  GraphPatternContext& context_;
};

}  // namespace gql::detail