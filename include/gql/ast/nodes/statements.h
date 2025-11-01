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

#include "gql/ast/nodes/path_patterns.h"

namespace gql::ast {

struct Statement;
using StatementPtr = value_ptr<Statement>;

// yieldItemName
//     : fieldName
//     ;
using YieldItemName = FieldName;

// yieldItemAlias
//     : AS bindingVariable
//     ;
using YieldItemAlias = BindingVariable;

// yieldItem
//     : (yieldItemName yieldItemAlias?)
//     ;
struct YieldItem : NodeBase<YieldItem> {
  YieldItemName name;
  std::optional<YieldItemAlias> alias;  // Rewrite may make |alias| non-optional
};
GQL_AST_STRUCT(YieldItem, name, alias)

// yieldItemList
//     : yieldItem (COMMA yieldItem)*
//     ;

// yieldClause
//     : YIELD yieldItemList
//     ;
using YieldClause = std::vector<YieldItem>;

// bindingVariableReferenceList
//    : bindingVariableReference (COMMA bindingVariableReference)*

// variableScopeClause
//    : LEFT_PAREN bindingVariableReferenceList? RIGHT_PAREN

// inlineProcedureCall
//    : variableScopeClause? nestedProcedureSpecification
struct InlineProcedureCall : NodeBase<InlineProcedureCall> {
  std::optional<std::vector<BindingVariableReference>> vars;
  ProcedureBodyPtr spec;
};
GQL_AST_STRUCT(InlineProcedureCall, vars, spec)

// procedureArgument
//    : valueExpression

// procedureArgumentList
//    : procedureArgument (COMMA procedureArgument)*
using ProcedureArgumentList = std::vector<ValueExpression>;

// namedProcedureCall
//    : procedureReference LEFT_PAREN procedureArgumentList? RIGHT_PAREN
//    yieldClause?
struct NamedProcedureCall : NodeBase<NamedProcedureCall> {
  ProcedureReference proc;
  ProcedureArgumentList args;
  YieldClause yield;
};
GQL_AST_STRUCT(NamedProcedureCall, proc, args, yield)

// procedureCall
//    : inlineProcedureCall
//    | namedProcedureCall
using ProcedureCall = std::variant<InlineProcedureCall,
                                   NamedProcedureCall  // GP04 feature
                                   >;

// callProcedureStatement
//    : OPTIONAL? CALL procedureCall
struct CallProcedureStatement : NodeBase<CallProcedureStatement> {
  bool isOptional = false;
  ProcedureCall call;
};
GQL_AST_STRUCT(CallProcedureStatement, isOptional, call)

// sessionSetSchemaClause
//    : SCHEMA schemaReference
struct SessionSetSchemaClause : NodeBase<SessionSetSchemaClause> {
  SchemaReference schema;
};
GQL_AST_STRUCT(SessionSetSchemaClause, schema)

// sessionSetGraphClause
//    : PROPERTY? GRAPH graphExpression
struct SessionSetGraphClause : NodeBase<SessionSetGraphClause> {
  GraphExpression graph;
};
GQL_AST_STRUCT(SessionSetGraphClause, graph)

// sessionSetTimeZoneClause
//    : TIME ZONE setTimeZoneValue
struct SessionSetTimeZoneClause : NodeBase<SessionSetTimeZoneClause> {
  std::string timeZoneString;
};
GQL_AST_STRUCT(SessionSetTimeZoneClause, timeZoneString)

// sessionSetParameterName
//    : (IF NOT EXISTS)? sessionParameterSpecification
struct SessionSetParameterName : NodeBase<SessionSetParameterName> {
  bool ifNotExists = false;
  GeneralParameterReference param;
};
GQL_AST_STRUCT(SessionSetParameterName, ifNotExists, param)

// optTypedGraphInitializer
//    : (typed? graphReferenceValueType)? graphInitializer
struct OptTypedGraphInitializer : NodeBase<OptTypedGraphInitializer> {
  std::optional<GraphReferenceValueType> type;
  GraphExpression initializer;
};
GQL_AST_STRUCT(OptTypedGraphInitializer, type, initializer)

// optTypedBindingTableInitializer
//    : (typed? bindingTableReferenceValueType)? bindingTableInitializer
struct OptTypedBindingTableInitializer
    : NodeBase<OptTypedBindingTableInitializer> {
  std::optional<BindingTableReferenceValueType> type;
  BindingTableExpression initializer;
};
GQL_AST_STRUCT(OptTypedBindingTableInitializer, type, initializer)

// sessionSetGraphParameterClause
//    : PROPERTY? GRAPH sessionSetParameterName optTypedGraphInitializer
struct SessionSetGraphParameterClause
    : NodeBase<SessionSetGraphParameterClause> {
  SessionSetParameterName paramName;
  OptTypedGraphInitializer initializer;
};
GQL_AST_STRUCT(SessionSetGraphParameterClause, paramName, initializer)

// sessionSetBindingTableParameterClause
//    : BINDING? TABLE sessionSetParameterName optTypedBindingTableInitializer
struct SessionSetBindingTableParameterClause
    : NodeBase<SessionSetBindingTableParameterClause> {
  SessionSetParameterName paramName;
  OptTypedBindingTableInitializer initializer;
};
GQL_AST_STRUCT(SessionSetBindingTableParameterClause, paramName, initializer)

// optTypedValueInitializer
//    : (typed? valueType)? valueInitializer
struct OptTypedValueInitializer : NodeBase<OptTypedValueInitializer> {
  std::optional<ValueType> type;
  ValueExpressionPtr initializer;
};
GQL_AST_STRUCT(OptTypedValueInitializer, type, initializer)

// sessionSetValueParameterClause
//    : VALUE sessionSetParameterName optTypedValueInitializer
struct SessionSetValueParameterClause
    : NodeBase<SessionSetValueParameterClause> {
  SessionSetParameterName paramName;
  OptTypedValueInitializer initializer;
};
GQL_AST_STRUCT(SessionSetValueParameterClause, paramName, initializer)

// sessionSetParameterClause
//    : sessionSetGraphParameterClause
//    | sessionSetBindingTableParameterClause
//    | sessionSetValueParameterClause
using SessionSetParameterClause =
    std::variant<SessionSetGraphParameterClause,         // GS01 feature
                 SessionSetBindingTableParameterClause,  // GS02 feature
                 SessionSetValueParameterClause          // GS03 feature
                 >;

// sessionSetCommand
//    : SESSION SET (sessionSetSchemaClause | sessionSetGraphClause |
//    sessionSetTimeZoneClause | sessionSetParameterClause)
using SessionSetCommand =
    std::variant<SessionSetSchemaClause,
                 SessionSetGraphClause,
                 SessionSetTimeZoneClause,  // GS15 feature
                 SessionSetParameterClause>;

// sessionResetArguments
//    : ALL? (PARAMETERS | CHARACTERISTICS)
//    | SCHEMA
//    | PROPERTY? GRAPH
//    | TIME ZONE
//    | PARAMETER? sessionParameterSpecification
enum class SessionResetArguments {
  Parameters,
  Characteristics,
  Schema,
  Graph,
  TimeZone,
};

// sessionResetCommand
//    : SESSION RESET sessionResetArguments?
struct SessionResetCommand : NodeBase<SessionResetCommand> {
  std::variant<SessionResetArguments, GeneralParameterReference> arguments;
};
GQL_AST_STRUCT(SessionResetCommand, arguments)

// sessionActivity
//    : sessionResetCommand+
//    | sessionSetCommand+ sessionResetCommand*
struct SessionActivity : NodeBase<SessionActivity> {
  std::vector<SessionSetCommand> setCommands;
  std::vector<SessionResetCommand> resetCommands;
};
GQL_AST_STRUCT(SessionActivity, setCommands, resetCommands)

// sessionCloseCommand
//    : SESSION CLOSE

// transactionAccessMode
//    : READ ONLY
//    | READ WRITE
enum class TransactionAccessMode { ReadOnly, ReadWrite };

// transactionCharacteristics
//    : transactionMode (COMMA transactionMode)*
//    ;

// startTransactionCommand
//    : START TRANSACTION transactionCharacteristics?
struct StartTransactionCommand : NodeBase<StartTransactionCommand> {
  std::optional<TransactionAccessMode> accessMode;
};
GQL_AST_STRUCT(StartTransactionCommand, accessMode)

// rollbackCommand
//    : ROLLBACK

// commitCommand
//    : COMMIT

// endTransactionCommand
//    : rollbackCommand
//    | commitCommand
enum class EndTransactionCommand { ROLLBACK, COMMIT };

// nestedProcedureSpecification
//    : LEFT_BRACE procedureSpecification RIGHT_BRACE

// graphVariableDefinition
//    : PROPERTY? GRAPH bindingVariable optTypedGraphInitializer
struct GraphVariableDefinition : NodeBase<GraphVariableDefinition> {
  BindingVariable var;
  OptTypedGraphInitializer initializer;
};
GQL_AST_STRUCT(GraphVariableDefinition, var, initializer)

// graphInitializer
//    : EQUALS_OPERATOR graphExpression

// bindingTableVariableDefinition
//    : BINDING? TABLE bindingVariable optTypedBindingTableInitializer
struct BindingTableVariableDefinition
    : NodeBase<BindingTableVariableDefinition> {
  BindingVariable var;
  OptTypedBindingTableInitializer initializer;
};
GQL_AST_STRUCT(BindingTableVariableDefinition, var, initializer)

// valueVariableDefinition
//    : VALUE bindingVariable optTypedValueInitializer
struct ValueVariableDefinition : NodeBase<ValueVariableDefinition> {
  BindingVariable var;
  OptTypedValueInitializer initializer;
};
GQL_AST_STRUCT(ValueVariableDefinition, var, initializer)

// bindingVariableDefinition
//    : graphVariableDefinition
//    | bindingTableVariableDefinition
//    | valueVariableDefinition
using BindingVariableDefinition =
    std::variant<GraphVariableDefinition,         // GP11 feature
                 BindingTableVariableDefinition,  // GP08 feature
                 ValueVariableDefinition          // GP05 feature
                 >;

// bindingVariableDefinitionBlock
//    : bindingVariableDefinition+

// nextStatement
//    : NEXT yieldClause? statement
struct NextStatement {
  YieldClause yield;  // Container is empty if clause is not present.
  StatementPtr statement;
};
GQL_AST_STRUCT(NextStatement, yield, statement)

// statementBlock
//    : statement nextStatement*
struct StatementBlock : NodeBase<StatementBlock> {
  StatementPtr firstStatement;
  std::vector<NextStatement> nextStatements;  // GQ20 feature
};
GQL_AST_STRUCT(StatementBlock, firstStatement, nextStatements)

// atSchemaClause
//    : AT schemaReference
using AtSchemaClause = SchemaReference;

// <catalog-modifying procedure specification>, <data-modifying procedure
// specification> and <query specification> are identical productions. The
// specification distinguishes them in the BNF, but in the implementation, the
// distinction has to be made sematically, in code, based on the kind of
// statements contained in the <procedure specification>.

// procedureBody
//    : atSchemaClause? bindingVariableDefinitionBlock? statementBlock
struct ProcedureBody : NodeBase<ProcedureBody> {
  std::optional<AtSchemaClause> schema;         // GP16 feature
  std::vector<BindingVariableDefinition> vars;  // GP17 feature
  StatementBlock statements;
};
GQL_AST_STRUCT(ProcedureBody, schema, vars, statements)

// bindingTableInitializer
//    : EQUALS_OPERATOR bindingTableExpression

// createSchemaStatement
//    : CREATE SCHEMA (IF NOT EXISTS)? catalogSchemaParentAndName
struct CreateSchemaStatement : NodeBase<CreateSchemaStatement> {
  bool ifNotExists = false;
  CatalogSchemaParentAndName schema;
};
GQL_AST_STRUCT(CreateSchemaStatement, ifNotExists, schema)

// dropSchemaStatement
//    : DROP SCHEMA (IF EXISTS)? catalogSchemaParentAndName
struct DropSchemaStatement : NodeBase<DropSchemaStatement> {
  bool ifExists = false;
  CatalogSchemaParentAndName schema;
};
GQL_AST_STRUCT(DropSchemaStatement, ifExists, schema)

// openGraphType
//    : typed? ANY (PROPERTY? GRAPH)?

// graphTypeLikeGraph
//    : LIKE graphExpression

// ofGraphType
//    : graphTypeLikeGraph
//    | typed? graphTypeReference
//    | typed? (PROPERTY? GRAPH)? nestedGraphTypeSpecification
using OfGraphType = std::
    variant<GraphExpression, GraphTypeReference, GraphTypeSpecificationBody>;

// graphSource
//    : AS COPY OF graphExpression

// createGraphStatement
//    : CREATE (PROPERTY? GRAPH (IF NOT EXISTS)? | OR REPLACE PROPERTY? GRAPH)
//    catalogGraphParentAndName (openGraphType | ofGraphType) graphSource?
struct CreateGraphStatement : NodeBase<CreateGraphStatement> {
  enum class CreateType {
    Default,  // Shall not identify an existing graph.
    IfNotExists,
    OrReplace,
  } createType = CreateType::Default;

  CatalogGraphParentAndName graph;
  std::optional<OfGraphType> graphType;
  std::optional<GraphExpression> source;
};
GQL_AST_STRUCT(CreateGraphStatement, createType, graph, graphType, source)

// dropGraphStatement
//    : DROP PROPERTY? GRAPH (IF EXISTS)? catalogGraphParentAndName
struct DropGraphStatement : NodeBase<DropGraphStatement> {
  bool ifExists = false;
  CatalogGraphParentAndName graph;
};
GQL_AST_STRUCT(DropGraphStatement, ifExists, graph)

// copyOfGraphType
//    : COPY OF graphTypeReference
using CopyOfGraphType = GraphTypeReference;

// graphTypeSource
//    : AS? copyOfGraphType
//    | graphTypeLikeGraph
//    | AS? nestedGraphTypeSpecification
using GraphTypeSource =
    std::variant<CopyOfGraphType, GraphExpression, GraphTypeSpecificationBody>;

// createGraphTypeStatement
//    : CREATE (PROPERTY? GRAPH TYPE (IF NOT EXISTS)? | OR REPLACE PROPERTY?
//    GRAPH TYPE) catalogGraphTypeParentAndName graphTypeSource
struct CreateGraphTypeStatement : NodeBase<CreateGraphTypeStatement> {
  enum class CreateType {
    Default,  // Shall not identify an existing graph type.
    IfNotExists,
    OrReplace,
  } createType = CreateType::Default;

  CatalogGraphTypeParentAndName graphType;
  GraphTypeSource source;
};
GQL_AST_STRUCT(CreateGraphTypeStatement, createType, graphType, source)

// dropGraphTypeStatement
//    : DROP PROPERTY? GRAPH TYPE (IF EXISTS)? catalogGraphTypeParentAndName
struct DropGraphTypeStatement : NodeBase<DropGraphTypeStatement> {
  bool ifExists = false;
  CatalogGraphTypeParentAndName graphType;
};
GQL_AST_STRUCT(DropGraphTypeStatement, ifExists, graphType)

// primitiveCatalogModifyingStatement
//    : createSchemaStatement
//    | dropSchemaStatement
//    | createGraphStatement
//    | dropGraphStatement
//    | createGraphTypeStatement
//    | dropGraphTypeStatement

// simpleCatalogModifyingStatement
//    : primitiveCatalogModifyingStatement
//    | callCatalogModifyingProcedureStatement
using SimpleCatalogModifyingStatement =
    std::variant<CreateSchemaStatement,     // GC01 feature
                 DropSchemaStatement,       // GC01 feature
                 CreateGraphStatement,      // GC04 feature
                 DropGraphStatement,        // GC04 feature
                 CreateGraphTypeStatement,  // GC02 feature
                 DropGraphTypeStatement,    // GC02 feature
                 CallProcedureStatement>;

// linearCatalogModifyingStatement
//    : simpleCatalogModifyingStatement+
struct LinearCatalogModifyingStatement
    : NodeBase<LinearCatalogModifyingStatement> {
  std::vector<SimpleCatalogModifyingStatement> statements;
};
GQL_AST_STRUCT(LinearCatalogModifyingStatement, statements)

// useGraphClause
//    : USE graphExpression
using UseGraphClause = GraphExpression;

// labelAndPropertySetSpecification
//    : isOrColon labelSetSpecification elementPropertySpecification?
//    | (isOrColon labelSetSpecification)? elementPropertySpecification

// insertElementPatternFiller
//    : elementVariableDeclaration labelAndPropertySetSpecification?
//    | elementVariableDeclaration? labelAndPropertySetSpecification
struct InsertElementPatternFiller : NodeBase<InsertElementPatternFiller> {
  std::optional<ElementVariableDeclaration> var;
  LabelSetSpecification labels;
  PropertyKeyValuePairList props;

  bool MaybeNotSet() const {
    return !var && labels.labels.empty() && props.empty();
  }
};
GQL_AST_STRUCT(InsertElementPatternFiller, var, labels, props)

// insertNodePattern
//    : LEFT_PAREN insertElementPatternFiller? RIGHT_PAREN
struct InsertNodePattern : NodeBase<InsertNodePattern> {
  InsertElementPatternFiller filler;
};
GQL_AST_STRUCT(InsertNodePattern, filler)

// insertEdgePointingLeft
//    : LEFT_ARROW_BRACKET insertElementPatternFiller? RIGHT_BRACKET_MINUS

// insertEdgePointingRight
//    : MINUS_LEFT_BRACKET insertElementPatternFiller? BRACKET_RIGHT_ARROW

// insertEdgeUndirected
//    : TILDE_LEFT_BRACKET insertElementPatternFiller? RIGHT_BRACKET_TILDE

// insertEdgePattern
//    : insertEdgePointingLeft
//    | insertEdgePointingRight
//    | insertEdgeUndirected
struct InsertEdgePattern : NodeBase<InsertEdgePattern> {
  enum class Direction { Left, Right, Undirected };

  Direction direction = Direction::Left;
  InsertElementPatternFiller filler;
};
GQL_AST_STRUCT(InsertEdgePattern, direction, filler)

// insertPathPattern
//    : insertNodePattern (insertEdgePattern insertNodePattern)*
struct InsertPathPattern : NodeBase<InsertPathPattern> {
  std::vector<InsertNodePattern> nodes;
  std::vector<InsertEdgePattern> edges;  // One less than the number of nodes
};
GQL_AST_STRUCT(InsertPathPattern, nodes, edges)

// insertPathPatternList
//    : insertPathPattern (COMMA insertPathPattern)*

// insertGraphPattern
//    : insertPathPatternList
using InsertGraphPattern = std::vector<InsertPathPattern>;

// insertStatement
//    : INSERT insertGraphPattern
struct InsertStatement : NodeBase<InsertStatement> {
  InsertGraphPattern paths;
};
GQL_AST_STRUCT(InsertStatement, paths)

// setPropertyItem
//    : bindingVariableReference PERIOD propertyName EQUALS_OPERATOR
//    valueExpression
struct SetPropertyItem : NodeBase<SetPropertyItem> {
  BindingVariableReference var;
  PropertyName property;
  ValueExpression value;
};
GQL_AST_STRUCT(SetPropertyItem, var, property, value)

// setAllPropertiesItem
//    : bindingVariableReference EQUALS_OPERATOR LEFT_BRACE
//    propertyKeyValuePairList? RIGHT_BRACE
struct SetAllPropertiesItem : NodeBase<SetAllPropertiesItem> {
  BindingVariableReference var;
  PropertyKeyValuePairList properties;
};
GQL_AST_STRUCT(SetAllPropertiesItem, var, properties)

// setLabelItem
//    : bindingVariableReference isOrColon labelName
struct SetLabelItem : NodeBase<SetLabelItem> {
  BindingVariableReference var;
  LabelName label;
};
GQL_AST_STRUCT(SetLabelItem, var, label)

// setItem
//    : setPropertyItem
//    | setAllPropertiesItem
//    | setLabelItem
using SetItem = std::variant<SetPropertyItem,
                             SetAllPropertiesItem,
                             SetLabelItem  // GD02 feature
                             >;

// setItemList
//    : setItem (COMMA setItem)*

// setStatement
//    : SET setItemList
struct SetStatement : NodeBase<SetStatement> {
  std::vector<SetItem> items;
};
GQL_AST_STRUCT(SetStatement, items)

// removePropertyItem
//    : bindingVariableReference PERIOD propertyName
struct RemovePropertyItem : NodeBase<RemovePropertyItem> {
  BindingVariableReference var;
  PropertyName property;
};
GQL_AST_STRUCT(RemovePropertyItem, var, property)

// removeLabelItem
//    : bindingVariableReference isOrColon labelName
struct RemoveLabelItem : NodeBase<RemoveLabelItem> {
  BindingVariableReference var;
  LabelName label;
};
GQL_AST_STRUCT(RemoveLabelItem, var, label)

// removeItem
//    : removePropertyItem
//    | removeLabelItem
using RemoveItem = std::variant<RemovePropertyItem,
                                RemoveLabelItem  // GD02 feature
                                >;

// removeItemList
//    : removeItem (COMMA removeItem)*

// removeStatement
//    : REMOVE removeItemList
struct RemoveStatement : NodeBase<RemoveStatement> {
  std::vector<RemoveItem> items;
};
GQL_AST_STRUCT(RemoveStatement, items)

// deleteItemList
//    : deleteItem (COMMA deleteItem)*

// deleteItem
//    : valueExpression
using DeleteItem = ValueExpression;

// deleteStatement
//    : (DETACH | NODETACH)? DELETE deleteItemList
struct DeleteStatement : NodeBase<DeleteStatement> {
  bool detach = false;  // NODETACH is implicit value if omitted.
  std::vector<DeleteItem> items;
};
GQL_AST_STRUCT(DeleteStatement, detach, items)

// primitiveDataModifyingStatement
//    : insertStatement
//    | setStatement
//    | removeStatement
//    | deleteStatement
using PrimitiveDataModifyingStatement = std::
    variant<InsertStatement, SetStatement, RemoveStatement, DeleteStatement>;

struct OtherwiseConjunctionValue {};
GQL_AST_VALUE(OtherwiseConjunctionValue)

// setOperator
//    : UNION setQuantifier?
//    | EXCEPT setQuantifier?
//    | INTERSECT setQuantifier?
struct SetOperator : NodeBase<SetOperator> {
  enum class Kind { UNION, EXCEPT, INTERSECT };

  Kind kind = Kind::UNION;
  SetQuantifier quantifier =
      SetQuantifier::DISTINCT;  // DISTINCT is implicit value.

  bool MaybeNotSet() const {
    // QueryConjunction is not initialized in CompositeQueryExpression if there
    // is just one query.
    return kind == Kind::UNION && quantifier == SetQuantifier::DISTINCT;
  }
};
GQL_AST_STRUCT(SetOperator, kind, quantifier)

// queryConjunction
//    : setOperator
//    | OTHERWISE
using QueryConjunction = std::variant<SetOperator, OtherwiseConjunctionValue>;

// elementBindingsOrElements
//    : ELEMENT BINDINGS?
//    | ELEMENTS

// repeatableElementsMatchMode
//    : REPEATABLE elementBindingsOrElements

// edgeBindingsOrEdges
//    : edgeSynonym BINDINGS?
//    | edgesSynonym

// differentEdgesMatchMode
//    : DIFFERENT edgeBindingsOrEdges

// matchMode
//    : repeatableElementsMatchMode
//    | differentEdgesMatchMode
enum class MatchMode {
  RepeatableElements,
  DifferentEdges,
};

// numberOfPaths
//    : nonNegativeIntegerSpecification

// allPathSearch
//    : ALL pathMode? pathOrPaths?

// anyPathSearch
//    : ANY numberOfPaths? pathMode? pathOrPaths?

// allShortestPathSearch
//    : ALL SHORTEST pathMode? pathOrPaths?

// anyShortestPathSearch
//    : ANY SHORTEST pathMode? pathOrPaths?

// countedShortestPathSearch
//    : SHORTEST numberOfPaths pathMode? pathOrPaths?

// numberOfGroups
//    : nonNegativeIntegerSpecification
using NumberOfGroups = NonNegativeIntegerSpecification;

// countedShortestGroupSearch
//    : SHORTEST numberOfGroups? pathMode? pathOrPaths? (GROUP | GROUPS)

// shortestPathSearch
//    : allShortestPathSearch
//    | anyShortestPathSearch
//    | countedShortestPathSearch
//    | countedShortestGroupSearch

// pathSearchPrefix
//    : allPathSearch
//    | anyPathSearch
//    | shortestPathSearch
struct PathSearchPrefix {
  enum class Search {
    All,
    Any,
    CountedShortestPath,
    CountedShortestGroup,
  };

  Search search = Search::All;
  NumberOfGroups number =
      1u;  // Number of Paths or Groups. 1 is implicit value.
};
GQL_AST_STRUCT(PathSearchPrefix, search, number)

// pathPatternPrefix
//    : pathModePrefix
//    | pathSearchPrefix
struct PathPatternPrefix : NodeBase<PathPatternPrefix> {
  PathMode mode = PathMode::WALK;  // WALK is implicit value.
  std::optional<PathSearchPrefix> pathSearchPrefix;
};
GQL_AST_STRUCT(PathPatternPrefix, mode, pathSearchPrefix)

// pathVariableDeclaration
//    : pathVariable EQUALS_OPERATOR

// pathPattern
//    : pathVariableDeclaration? pathPatternPrefix? pathPatternExpression
struct PathPattern : NodeBase<PathPattern> {
  std::optional<PathVariable> var;
  std::optional<PathPatternPrefix> prefix;
  PathPatternExpression expr;
};
GQL_AST_STRUCT(PathPattern, var, prefix, expr)

// keepClause
//    : KEEP pathPatternPrefix
using KeepClause = PathPatternPrefix;

// graphPatternWhereClause
//    : WHERE searchCondition
struct GraphPatternWhereClause : NodeBase<GraphPatternWhereClause> {
  SearchCondition condition;
};
GQL_AST_STRUCT(GraphPatternWhereClause, condition)

// pathPatternList
//    : pathPattern (COMMA pathPattern)*
using PathPatternList = std::vector<PathPattern>;

// graphPattern
//    : matchMode? pathPatternList keepClause? graphPatternWhereClause?
struct GraphPattern : NodeBase<GraphPattern> {
  std::optional<MatchMode> matchMode;
  PathPatternList paths;
  std::optional<KeepClause>
      keep;  // Eliminated by rewrite::RewritePathPatternPrefix
  std::optional<GraphPatternWhereClause> where;
};
GQL_AST_STRUCT(GraphPattern, matchMode, paths, keep, where)

// graphPatternYieldClause
//    : YIELD graphPatternYieldItemList

// graphPatternYieldItemList
//    : graphPatternYieldItem (COMMA graphPatternYieldItem)*
//    | NO BINDINGS

// graphPatternYieldItem
//    : bindingVariableReference
using GraphPatternYieldItem = BindingVariableReference;

// graphPatternBindingTable
//    : graphPattern graphPatternYieldClause?

// simpleMatchStatement
//    : MATCH graphPatternBindingTable
struct SimpleMatchStatement : NodeBase<SimpleMatchStatement> {
  GraphPattern pattern;
  std::optional<std::vector<GraphPatternYieldItem>>
      yield;  // Not set when YIELD clause is missing. Empty container means NO
              // BINDINGS.
};
GQL_AST_STRUCT(SimpleMatchStatement, pattern, yield)

struct MatchStatementBlock;
using MatchStatementBlockPtr = value_ptr<MatchStatementBlock>;

// optionalOperand
//    : simpleMatchStatement
//    | LEFT_BRACE matchStatementBlock RIGHT_BRACE
//    | LEFT_PAREN matchStatementBlock RIGHT_PAREN

// optionalMatchStatement
//    : OPTIONAL optionalOperand
struct OptionalMatchStatement : NodeBase<OptionalMatchStatement> {
  MatchStatementBlockPtr statements;
};
GQL_AST_STRUCT(OptionalMatchStatement, statements)

// matchStatement
//    : simpleMatchStatement
//    | optionalMatchStatement
using MatchStatement =
    std::variant<SimpleMatchStatement, OptionalMatchStatement>;

// matchStatementBlock
//    : matchStatement+
struct MatchStatementBlock : NodeBase<MatchStatementBlock> {
  std::vector<MatchStatement> statements;

  bool MaybeNotSet() const {
    return statements.size() == 1 &&
           std::holds_alternative<SimpleMatchStatement>(statements[0]);
  }
};
GQL_AST_STRUCT(MatchStatementBlock, statements)

// letVariableDefinitionList
//    : letVariableDefinition (COMMA letVariableDefinition)*

// letStatement
//    : LET letVariableDefinitionList
struct LetStatement : NodeBase<LetStatement> {
  std::vector<LetVariableDefinition> definitions;
};
GQL_AST_STRUCT(LetStatement, definitions)

// forItemAlias
//    : bindingVariable IN
using ForItemAlias = BindingVariable;

// forItemSource
//    : valueExpression
using ForItemSource = ValueExpression;

// forItem
//    : forItemAlias forItemSource

// forOrdinalityOrOffset
//    : WITH (ORDINALITY | OFFSET) bindingVariable
struct ForOrdinalityOrOffset : NodeBase<ForOrdinalityOrOffset> {
  bool isOrdinality = false;  // true=ORDINALITY, false=OFFSET
  BindingVariable var;
};
GQL_AST_STRUCT(ForOrdinalityOrOffset, isOrdinality, var)

// forStatement
//    : FOR forItem forOrdinalityOrOffset?
struct ForStatement : NodeBase<ForStatement> {
  ForItemAlias alias;
  ForItemSource source;
  std::optional<ForOrdinalityOrOffset> ordinalityOrOffset;
};
GQL_AST_STRUCT(ForStatement, alias, source, ordinalityOrOffset)

// orderingSpecification
//     : ASC
//     | ASCENDING
//     | DESC
//     | DESCENDING
//     ;
enum class OrderingSpecification { ASCENDING, DESCENDING };

// nullOrdering
//     : NULLS FIRST
//     | NULLS LAST
//     ;
enum class NullOrdering { NULLS_FIRST, NULLS_LAST };

// sortSpecification
//     : sortKey orderingSpecification? nullOrdering?
//     ;
struct SortSpecification : NodeBase<SortSpecification> {
  ValueExpression sortKey;
  OrderingSpecification ordering =
      OrderingSpecification::ASCENDING;  // ASCENDING is implicit value.
  std::optional<NullOrdering> nullOrder;
};
GQL_AST_STRUCT(SortSpecification, sortKey, ordering, nullOrder)

// limitClause
//     : LIMIT nonNegativeIntegerSpecification
//     ;

// offsetClause
//     : offsetSynonym nonNegativeIntegerSpecification
//     ;

// sortSpecificationList
//     : sortSpecification (COMMA sortSpecification)*
//     ;
using SortSpecificationList = std::vector<SortSpecification>;

// orderByClause
//     : ORDER BY sortSpecificationList
//     ;

// orderByAndPageStatement
//    : orderByClause offsetClause? limitClause?
//    | offsetClause limitClause?
//    | limitClause
struct OrderByAndPageStatement : NodeBase<OrderByAndPageStatement> {
  SortSpecificationList orderBy;
  std::optional<NonNegativeIntegerSpecification> offset;  // GQ12 feature
  std::optional<NonNegativeIntegerSpecification> limit;   // GQ13 feature
};
GQL_AST_STRUCT(OrderByAndPageStatement, orderBy, offset, limit)

struct FinishValue {};
GQL_AST_VALUE(FinishValue)

// returnItemAlias
//    : AS identifier
using ReturnItemAlias = Identifier;

// returnItem
//    : aggregatingValueExpression returnItemAlias?
struct ReturnItem : NodeBase<ReturnItem> {
  ValueExpression aggregate;
  std::optional<ReturnItemAlias> alias;
};
GQL_AST_STRUCT(ReturnItem, aggregate, alias)

// returnItemList
//    : returnItem (COMMA returnItem)*
using ReturnItemList = std::vector<ReturnItem>;

// groupingElement
//     : bindingVariableReference
//     ;
using GroupingElement = BindingVariableReference;

// groupByClause
//     : GROUP BY groupingElementList
//     ;

// groupingElementList
//     : groupingElement (COMMA groupingElement)*
//     | emptyGroupingSet
//     ;
using GroupingElementList = std::vector<GroupingElement>;

struct AsteriskValue {};
GQL_AST_VALUE(AsteriskValue)

// returnStatementBody
//    : setQuantifier? (ASTERISK | returnItemList) groupByClause?
//    | NO BINDINGS
struct ReturnStatementBody : NodeBase<ReturnStatementBody> {
  SetQuantifier quantifier = SetQuantifier::ALL;  // ALL is implicit value.
  std::optional<ReturnItemList>
      items;  // Not set means ASTERISK; empty list means NO BINDINGS
              // Asterisk rewrite may make |items| non-optional.
  GroupingElementList groupBy;  // GQ15 feature
};
GQL_AST_STRUCT(ReturnStatementBody, quantifier, items, groupBy)

// returnStatement
//    : RETURN returnStatementBody
using ReturnStatement = ReturnStatementBody;

// primitiveResultStatement
//    : returnStatement orderByAndPageStatement?
//    | FINISH
struct PrimitiveResultStatement : NodeBase<PrimitiveResultStatement> {
  struct Return {
    ReturnStatement stmt;
    std::optional<OrderByAndPageStatement> orderByAndPage;
  };
  std::variant<FinishValue, Return> option;

  bool MaybeNotSet() const { return option.index() == 0; }
};
GQL_AST_STRUCT(PrimitiveResultStatement, option)
GQL_AST_STRUCT(PrimitiveResultStatement::Return, stmt, orderByAndPage)

// selectItemAlias
//    : AS identifier
using SelectItemAlias = Identifier;

// selectItem
//    : aggregatingValueExpression selectItemAlias?
struct SelectItem : NodeBase<SelectItem> {
  ValueExpression expr;
  std::optional<SelectItemAlias> alias;
};
GQL_AST_STRUCT(SelectItem, expr, alias)

// selectItemList
//    : selectItem (COMMA selectItem)*
using SelectItemList = std::vector<SelectItem>;

// havingClause
//    : HAVING searchCondition
//    ;
using HavingClause = SearchCondition;

// selectGraphMatch
//    : graphExpression matchStatement
struct SelectGraphMatch : NodeBase<SelectGraphMatch> {
  GraphExpression graph;
  MatchStatement match;
};
GQL_AST_STRUCT(SelectGraphMatch, graph, match)

// selectGraphMatchList
//    : selectGraphMatch (COMMA selectGraphMatch)*
struct SelectGraphMatchList : NodeBase<SelectGraphMatchList> {
  std::vector<SelectGraphMatch> matches;
};
GQL_AST_STRUCT(SelectGraphMatchList, matches)

// selectQuerySpecification
//    : nestedQuerySpecification
//    | graphExpression nestedQuerySpecification
struct SelectQuerySpecification : NodeBase<SelectQuerySpecification> {
  std::optional<GraphExpression> graph;
  ProcedureBody query;
};
GQL_AST_STRUCT(SelectQuerySpecification, graph, query)

// selectStatementBody
//    : FROM (selectGraphMatchList | selectQuerySpecification)
using SelectStatementBody =
    std::variant<SelectGraphMatchList, SelectQuerySpecification>;

// whereClause
//     : WHERE searchCondition
//     ;
using WhereClause = SearchCondition;

// emptyGroupingSet
//     : LEFT_PAREN RIGHT_PAREN
//     ;

// selectStatement
//    : SELECT setQuantifier? (ASTERISK | selectItemList) (selectStatementBody
//    whereClause? groupByClause? havingClause? orderByClause? offsetClause?
//    limitClause?)?
struct SelectStatement : NodeBase<SelectStatement> {
  SetQuantifier quantifier = SetQuantifier::ALL;  // ALL is implicit value.
  std::variant<AsteriskValue, SelectItemList> items;
  std::optional<SelectStatementBody> body;
  std::optional<WhereClause> where;
  GroupingElementList groupBy;
  std::optional<HavingClause> having;
  SortSpecificationList orderBy;
  std::optional<NonNegativeIntegerSpecification> offset;
  std::optional<NonNegativeIntegerSpecification> limit;
};
GQL_AST_STRUCT(SelectStatement,
               quantifier,
               items,
               body,
               where,
               groupBy,
               having,
               orderBy,
               offset,
               limit)

// filterStatement
//    : FILTER (whereClause | searchCondition)
struct FilterStatement : NodeBase<FilterStatement> {
  SearchCondition condition;
};
GQL_AST_STRUCT(FilterStatement, condition)

// primitiveQueryStatement
//    : matchStatement
//    | letStatement
//    | forStatement
//    | filterStatement
//    | orderByAndPageStatement

// simpleQueryStatement
//    : primitiveQueryStatement
//    | callQueryStatement
using SimpleQueryStatement =
    std::variant<MatchStatement,
                 LetStatement,     // GQ09 feature. May be rewritten
                 ForStatement,     // GQ10 or GQ23 feature
                 FilterStatement,  // GQ08 feature
                 OrderByAndPageStatement,
                 CallProcedureStatement>;

// simpleLinearQueryStatement
//    : simpleQueryStatement+
using SimpleLinearQueryStatement = std::vector<SimpleQueryStatement>;

// focusedNestedQuerySpecification
//    : useGraphClause nestedQuerySpecification
struct NestedQuerySpecification : NodeBase<NestedQuerySpecification> {
  std::optional<UseGraphClause> useGraph;  // GQ01 feature
  ProcedureBody procedure;
};
GQL_AST_STRUCT(NestedQuerySpecification, useGraph, procedure)

// focusedLinearQueryStatementPart
//    : useGraphClause simpleLinearQueryStatement
struct FocusedLinearQueryStatementPart
    : NodeBase<FocusedLinearQueryStatementPart> {
  UseGraphClause useGraph;
  SimpleLinearQueryStatement statements;
};
GQL_AST_STRUCT(FocusedLinearQueryStatementPart, useGraph, statements)

// focusedLinearQueryAndPrimitiveResultStatementPart
//    : useGraphClause simpleLinearQueryStatement primitiveResultStatement

// focusedPrimitiveResultStatement
//    : useGraphClause primitiveResultStatement

// focusedLinearQueryStatement
//    : focusedLinearQueryStatementPart*
//    focusedLinearQueryAndPrimitiveResultStatementPart |
//    focusedPrimitiveResultStatement | focusedNestedQuerySpecification |
//    selectStatement
//    ;

// ambientLinearQueryStatement
//    : simpleLinearQueryStatement? primitiveResultStatement
//    | nestedQuerySpecification

// linearQueryStatement
//    : focusedLinearQueryStatement
//    | ambientLinearQueryStatement

// Made as a combination of Focused* and Ambient* grammar rules
struct LinearQueryStatementOption : NodeBase<LinearQueryStatementOption> {
  std::variant<SimpleLinearQueryStatement,
               std::vector<FocusedLinearQueryStatementPart>  // GQ01 feature
               >
      statements;
  PrimitiveResultStatement result;
};
GQL_AST_STRUCT(LinearQueryStatementOption, statements, result)

struct FocusedPrimitiveResultStatement
    : NodeBase<FocusedPrimitiveResultStatement> {
  UseGraphClause useGraph;
  PrimitiveResultStatement result;
};
GQL_AST_STRUCT(FocusedPrimitiveResultStatement, useGraph, result)

using LinearQueryStatement =
    std::variant<LinearQueryStatementOption,
                 NestedQuerySpecification,
                 FocusedPrimitiveResultStatement,  // GQ01 feature
                 SelectStatement>;

// compositeQueryPrimary
//    : linearQueryStatement
using CompositeQueryPrimary = LinearQueryStatement;

// compositeQueryExpression
//    : compositeQueryExpression queryConjunction compositeQueryPrimary
//    | compositeQueryPrimary
struct CompositeQueryExpression : NodeBase<CompositeQueryExpression> {
  QueryConjunction conjunction;
  std::vector<CompositeQueryPrimary> queries;
};
GQL_AST_STRUCT(CompositeQueryExpression, conjunction, queries)

// compositeQueryStatement
//    : compositeQueryExpression
using CompositeQueryStatement = CompositeQueryExpression;

// simpleDataModifyingStatement
//    : primitiveDataModifyingStatement
//    | callDataModifyingProcedureStatement

// simpleLinearDataAccessingStatement
//    : simpleDataAccessingStatement+

// simpleDataAccessingStatement
//    : simpleQueryStatement
//    | simpleDataModifyingStatement
using SimpleDataAccessingStatement =
    std::variant<SimpleQueryStatement,
                 PrimitiveDataModifyingStatement,  // GD01 feature
                 CallProcedureStatement            // GD01 feature
                 >;

// focusedLinearDataModifyingStatementBody
//    : useGraphClause simpleLinearDataAccessingStatement
//    primitiveResultStatement?

// focusedNestedDataModifyingProcedureSpecification
//    : useGraphClause nestedDataModifyingProcedureSpecification

// ambientLinearDataModifyingStatementBody
//    : simpleLinearDataAccessingStatement primitiveResultStatement?
struct LinearDataModifyingStatementBody
    : NodeBase<LinearDataModifyingStatementBody> {
  std::vector<SimpleDataAccessingStatement> statements;
  std::optional<PrimitiveResultStatement> result;
};
GQL_AST_STRUCT(LinearDataModifyingStatementBody, statements, result)

// ambientLinearDataModifyingStatement
//    : ambientLinearDataModifyingStatementBody
//    | nestedDataModifyingProcedureSpecification

// focusedLinearDataModifyingStatement
//    : focusedLinearDataModifyingStatementBody
//    | focusedNestedDataModifyingProcedureSpecification

// linearDataModifyingStatement
//    : focusedLinearDataModifyingStatement
//    | ambientLinearDataModifyingStatement
struct LinearDataModifyingStatement : NodeBase<LinearDataModifyingStatement> {
  std::optional<UseGraphClause> useGraph;  // GP01 feature
  std::variant<LinearDataModifyingStatementBody, ProcedureBody> option;
};
GQL_AST_STRUCT(LinearDataModifyingStatement, useGraph, option)

// statement
//    : linearCatalogModifyingStatement
//    | linearDataModifyingStatement
//    | compositeQueryStatement
struct Statement : NodeBase<Statement> {
  std::variant<LinearCatalogModifyingStatement,
               LinearDataModifyingStatement,
               CompositeQueryStatement>
      option;
};
GQL_AST_STRUCT(Statement, option)

// transactionActivity
//    : startTransactionCommand (procedureSpecification endTransactionCommand?)?
//    | procedureSpecification endTransactionCommand?
//    | endTransactionCommand
struct TransactionActivity : NodeBase<TransactionActivity> {
  std::optional<StartTransactionCommand> startCmd;  // GT01 feature
  std::optional<ProcedureBody> procedure;
  std::optional<EndTransactionCommand> endCmd;  // GT01 feature
};
GQL_AST_STRUCT(TransactionActivity, startCmd, procedure, endCmd)

// programActivity
//    : sessionActivity
//    | transactionActivity
//    ;

// gqlProgram
//    : programActivity sessionCloseCommand? EOF
//    | sessionCloseCommand EOF
struct GQLProgram : NodeBase<GQLProgram> {
  std::variant<SessionActivity, TransactionActivity> programActivity;
  bool sessionCloseCommand = false;
};
GQL_AST_STRUCT(GQLProgram, programActivity, sessionCloseCommand)

}  // namespace gql::ast