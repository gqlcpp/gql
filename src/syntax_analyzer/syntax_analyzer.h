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

#include <initializer_list>
#include <stack>
#include <unordered_map>

#include "gql/ast/algorithm.h"
#include "gql/ast/ast.h"
#include "gql/standard/features.h"
#include "gql/syntax_analyzer/syntax_analyzer.h"

namespace gql {

namespace detail {
class GraphPatternContext;
}

namespace syntax_analyzer {
struct AggregateFunctionAuxData;
}

template <typename, typename>
struct SyntaxAnalyzerHasProcessFuncForType;

class SyntaxAnalyzer {
 public:
  using BindingTableType = ast::FieldTypeList;
  using OptBindingTableType = std::optional<ast::FieldTypeList>;

  using Config = SyntaxAnalyzerConfig;

  SyntaxAnalyzer(const Config* = nullptr);

  OptBindingTableType Process(ast::GQLProgram&);

 private:
  enum class CallProcedureKind { Any, Query, DataModifying, CatalogModifying };

  class ExecutionContext;
  struct StatementRewriteException;

  void ThrowIfFeatureNotSupported(standard::Feature, const ast::Node&) const;

  // Procedure statements
  OptBindingTableType Process(ast::ProcedureBody&,
                              CallProcedureKind,
                              ExecutionContext&);
  OptBindingTableType Process(ast::Statement&,
                              CallProcedureKind,
                              ExecutionContext&);
  BindingTableType Process(const ast::YieldClause&, ExecutionContext&);
  void Process(ast::CallProcedureStatement&,
               CallProcedureKind,
               ExecutionContext&);
  OptBindingTableType Process(ast::InlineProcedureCall&,
                              CallProcedureKind,
                              ExecutionContext&);
  void Process(std::vector<ast::BindingVariableDefinition>&, ExecutionContext&);

  // Query statements
  OptBindingTableType Process(ast::CompositeQueryStatement&, ExecutionContext&);
  void Process(ast::CompositeQueryPrimary&, ExecutionContext&);
  void Process(ast::SimpleQueryStatement&, ExecutionContext&);
  ast::CallProcedureStatement Rewrite(const ast::LetStatement&,
                                      const ExecutionContext&) const;
  void Process(ast::ForStatement&, ExecutionContext&);

  std::vector<ast::StatementPtr> Rewrite(ast::SelectStatement select,
                                         const ExecutionContext&);

  // Match statements
  void Process(ast::SimpleMatchStatement&, ExecutionContext&);
  ast::CallProcedureStatement Rewrite(const ast::OptionalMatchStatement&,
                                      const ExecutionContext&) const;

  // Graph pattern statements
  BindingTableType ProcessGraphPatternBindingTable(ast::SimpleMatchStatement&,
                                                   ExecutionContext&);

  // Path pattern statements
  void Process(ast::PathPatternExpression&,
               detail::GraphPatternContext&,
               ExecutionContext&);
  void Process(ast::PathMode, const ast::Node&, ExecutionContext&) const;
  void Process(const ast::PathPatternPrefix&, ExecutionContext&) const;
  void Process(const ast::LabelExpression&, ExecutionContext&) const;
  void Process(ast::PropertyKeyValuePair&, ExecutionContext&);

  // Data-modifying statements
  void Process(ast::LinearDataModifyingStatement&, ExecutionContext&);
  void Process(ast::LinearDataModifyingStatementBody&, ExecutionContext&);
  void Process(ast::PrimitiveDataModifyingStatement&, ExecutionContext&);
  void Process(ast::InsertStatement&, ExecutionContext&);

  void Process(ast::LinearCatalogModifyingStatement&, ExecutionContext&);

  // Result statements
  void Process(ast::PrimitiveResultStatement&, ExecutionContext&);
  void Process(ast::ReturnStatement&, ExecutionContext&);
  ast::ReturnStatement Rewrite(const ast::ReturnStatement&,
                               const ExecutionContext&) const;
  ast::PrimitiveResultStatement::Return RewriteOrderByClause(
      const ast::PrimitiveResultStatement::Return&,
      const ExecutionContext&) const;
  void Process(ast::OrderByAndPageStatement&, ExecutionContext&);

  void Process(ast::SessionSetParameterClause&, ExecutionContext&);

  ast::ValueType Process(ast::BindingTableExpression&, const ExecutionContext&);
  ast::ValueType Process(ast::GraphExpression&, const ExecutionContext&);

  template <typename ContextT>
  ast::ValueType Process(ast::ValueExpression&, ContextT&) {
    // |ValueExpression| is handled differently depending on the calling site
    // (see Process*ValueExpression functions). This function protects against
    // accidentally calling it directly or indirectly by ProcessFallback.
    static_assert(false);
    return {};
  }

  ast::ValueType ProcessValueExpression(
      ast::ValueExpression&,
      const ExecutionContext&,
      bool isAggregatingValueExpression = false);
  ast::ValueType ProcessPropertyValueExpression(ast::ValueExpression&,
                                                const ExecutionContext&);
  ast::ValueType ProcessStringValueExpression(ast::ValueExpression&,
                                              const ExecutionContext&);
  ast::ValueType ProcessBooleanValueExpression(ast::ValueExpression&,
                                               const ExecutionContext&);
  ast::ValueType ProcessAggregatingValueExpression(ast::ValueExpression&,
                                                   const ExecutionContext&);
  ast::ValueType ProcessNumericValueExpression(ast::ValueExpression&,
                                               const ExecutionContext&);
  ast::ValueType ProcessCharacterStringValueExpression(ast::ValueExpression&,
                                                       const ExecutionContext&);
  ast::ValueType ProcessByteStringValueExpression(ast::ValueExpression&,
                                                  const ExecutionContext&);
  ast::ValueType ProcessPathValueExpression(ast::ValueExpression&,
                                            const ExecutionContext&);
  ast::ValueType ProcessListValueExpression(ast::ValueExpression&,
                                            const ExecutionContext&);

  ast::ValueType ProcessBindingVariableReference(
      const ast::BindingVariableBase&,
      const ast::Node&,
      const ExecutionContext&) const;
  ast::ValueType Process(const ast::BindingVariableReference&,
                         const ExecutionContext&) const;
  ast::ValueType ProcessSingleton(const ast::ElementVariableReference&,
                                  const ExecutionContext&) const;

  ast::ValueType Process(ast::ValueExpression::Comparison&,
                         const ExecutionContext&);
  ast::ValueType Process(ast::ValueExpression::Unary&,
                         const ast::Node&,
                         const ExecutionContext&);
  ast::ValueType Process(ast::ValueExpression::Binary&,
                         const ast::Node&,
                         const ExecutionContext&);
  ast::ValueType Process(ast::ValueExpression::Is&, const ExecutionContext&);
  ast::ValueType Process(ast::ValueFunction&, const ExecutionContext&);
  ast::ValueType Process(ast::AggregateFunction&,
                         const ast::Node&,
                         const ExecutionContext&);
  ast::ValueType ProcessAggregateFunctionOperand(
      ast::ValueExpression&,
      const ExecutionContext&,
      syntax_analyzer::AggregateFunctionAuxData&);
  ast::ValueType Process(ast::UnsignedLiteral&,
                         const ast::Node&,
                         const ExecutionContext&);
  ast::ValueType Process(ast::GeneralParameterReference&,
                         const ExecutionContext&);
  ast::ValueType Process(ast::SessionUserValue&, const ExecutionContext&);
  ast::ValueType Process(ast::PathValueConstructor&, const ExecutionContext&);
  ast::ValueType Process(ast::PropertyReference&, const ExecutionContext&);
  ast::ValueType Process(ast::CaseExpression&, const ExecutionContext&);
  ast::ValueType Process(ast::CastSpecification&, const ExecutionContext&);
  ast::ValueType Process(ast::ElementIdFunction&, const ExecutionContext&);
  ast::ValueType Process(ast::LetValueExpression&, const ExecutionContext&);

  // Predicates
  ast::ValueType Process(ast::Predicate&, const ExecutionContext&);
  ast::ValueType Process(ast::ExistsPredicate&, const ExecutionContext&);
  ast::ValueType Process(ast::NullPredicate&, const ExecutionContext&);
  ast::ValueType Process(ast::ValueTypePredicate&, const ExecutionContext&);
  ast::ValueType Process(ast::NormalizedPredicate&, const ExecutionContext&);
  ast::ValueType Process(ast::DirectedPredicate&, const ExecutionContext&);
  ast::ValueType Process(ast::LabeledPredicate&, const ExecutionContext&);
  ast::ValueType Process(ast::SourceDestinationPredicate&,
                         const ExecutionContext&);
  ast::ValueType Process(ast::AllElementsPredicate&, const ExecutionContext&);
  ast::ValueType Process(ast::PropertyExistsPredicate&,
                         const ExecutionContext&);

  void Process(const ast::ReferenceParameterSpecification&,
               const ExecutionContext&) const;

  ast::ValueType Process(const ast::ValueType&, const ExecutionContext&) const;
  ast::ValueType Process(const ast::GraphReferenceValueType&,
                         const ExecutionContext&) const;
  ast::ValueType Process(const ast::BindingTableReferenceValueType&,
                         const ast::Node&,
                         const ExecutionContext&) const;
  void Process(const ast::FieldTypesSpecification&,
               const ExecutionContext&) const;

  bool IsCastableTo(ast::ValueType src, const ast::ValueType& dest) const;

  Config config_;
  std::vector<bool> unsupportedFeatures_;
  bool isInsideReadOnlyTransaction_ = false;

  template <typename NodeType>
  void ProcessFallback(NodeType&, ExecutionContext&);

  static CallProcedureKind GetProcedureKind(const ast::StatementBlock&,
                                            const ast::Node**);

  template <typename, typename>
  friend struct SyntaxAnalyzerHasProcessFuncForType;
};

class SyntaxAnalyzer::ExecutionContext {
 public:
  ExecutionContext() = default;
  ExecutionContext(ExecutionContext&&) = default;
  ExecutionContext& operator=(ExecutionContext&&) = default;

  // 4.11.5
  enum class InaccessibleReason {
    // An operand OP of <path pattern union> or <path multiset alternation> U
    // may only reference element variables declared in OP, or outside of U
    ReferenceToTheAdjacentUnionOperand,
    // A non-local reference shall have singleton degree of reference
    NonLocalReferenceWithGroupDegreeOfReference,
    // A selective <path pattern> SPP shall not reference a graph pattern
    // variable that is not declared in SPP.
    ReferenceFromSelectivePathPattern,
  };

  BindingTableType workingTable;
  BindingTableType workingRecord;
  std::unordered_map<std::string, InaccessibleReason> inaccessibleVariables;

  // For 20.1 Syntax Rule 7 and 20.9. This flag is only managed inside value
  // expressions. Tracks if current node is directly contained in a
  // <value expression> that is immediately contained in an <aggregating value
  // expression>.
  bool isInsideAggregatingValueExpression = false;
  // Used to handle 20.9 General Rule 5.a. This flag is only managed inside
  // value expressions. Flag tracks if current node is directly contained in a
  // <value expression> that is not immediately contained in an <aggregating
  // value expression>.
  bool isInsideValueExpressionNotAggregating = false;

  // Keep working record only.
  ExecutionContext MakeChild() const;
  // Keep working table and record.
  ExecutionContext MakeCopy() const;
  // Amend working record with working table. Don't inherit working table.
  ExecutionContext MakeAmended() const;
  // "A1 directly contains B1 if A1 contains B1 without an intervening instance
  // of <procedure body>."
  ExecutionContext MakeCopyForProcedureBody() const;

 private:
  ExecutionContext(const ExecutionContext&) = default;
  ExecutionContext& operator=(const ExecutionContext&) = default;
};

struct SyntaxAnalyzer::StatementRewriteException {
  std::vector<ast::StatementPtr> statements;
};

template <typename, typename = void>
struct SyntaxAnalyzerHasProcessFuncForType : std::false_type {};

template <typename T>
struct SyntaxAnalyzerHasProcessFuncForType<
    T,
    std::void_t<decltype(std::declval<SyntaxAnalyzer>().Process(
        std::declval<T&>(),
        std::declval<SyntaxAnalyzer::ExecutionContext&>()))>> : std::true_type {
};

template <typename NodeType>
void SyntaxAnalyzer::ProcessFallback(NodeType& node,
                                     ExecutionContext& context) {
  if constexpr (SyntaxAnalyzerHasProcessFuncForType<
                    std::decay_t<NodeType>>::value) {
    Process(node, context);
  } else {
    ast::EnumerateChildren<NodeType>::ForEachChild(
        node, [this, &context](auto& node) {
          this->ProcessFallback(node, context);
          return true;
        });
  }
}

}  // namespace gql