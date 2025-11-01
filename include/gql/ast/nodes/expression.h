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

#include "gql/ast/nodes/references.h"
#include "gql/ast/nodes/types.h"

namespace gql::ast {

using NumericValueExpression = ValueExpressionPtr;

// compOp
//    : EQUALS_OPERATOR
//    | NOT_EQUALS_OPERATOR
//    | LESS_THAN_OPERATOR
//    | GREATER_THAN_OPERATOR
//    | LESS_THAN_OR_EQUALS_OPERATOR
//    | GREATER_THAN_OR_EQUALS_OPERATOR
enum class CompOp {
  Equals,
  NotEquals,
  LessThan,
  GreaterThan,
  LessThanOrEquals,
  GreaterThanOrEquals
};

// normalForm : NFC | NFD | NFKC | NFKD ;
enum class NormalForm { NFC, NFD, NFKC, NFKD };

// characterStringValueExpression : valueExpression ;
using CharacterStringValueExpression = ValueExpressionPtr;

// byteStringValueExpression : valueExpression ;
using ByteStringValueExpression = ValueExpressionPtr;

// stringLength
//    : numericValueExpression
//    ;
using StringLength = NumericValueExpression;

// datetimeValueExpression : valueExpression ;
using DatetimeValueExpression = ValueExpressionPtr;

// booleanValueExpression
//    : valueExpression
//    ;
using BooleanValueExpression = ValueExpressionPtr;

//  searchCondition
//    : booleanValueExpression
//    ;
using SearchCondition = BooleanValueExpression;

// subCharacterOrByteString
//     : (LEFT | RIGHT) LEFT_PAREN valueExpression COMMA stringLength
//     RIGHT_PAREN
//     ;
struct SubCharacterOrByteString : NodeBase<SubCharacterOrByteString> {
  enum class Direction { Left, Right };

  Direction direction = Direction::Left;
  ValueExpressionPtr expr;
  StringLength length;
};
GQL_AST_STRUCT(SubCharacterOrByteString, direction, expr, length)

// trimSpecification : LEADING | TRAILING | BOTH ;
enum class TrimSpecification { LEADING, TRAILING, BOTH };

// trimOperands : (trimSpecification? trimCharacterOrByteString? FROM)?
// trimCharacterOrByteStringSource ;

// trimSingleCharacterOrByteString
//     : TRIM LEFT_PAREN trimOperands RIGHT_PAREN
//     ;
struct TrimSingleCharacterOrByteString
    : NodeBase<TrimSingleCharacterOrByteString> {
  TrimSpecification specification =
      TrimSpecification::BOTH;  // BOTH is implicit value.
  std::optional<ValueExpressionPtr> trimString;
  ValueExpressionPtr source;
};
GQL_AST_STRUCT(TrimSingleCharacterOrByteString,
               specification,
               trimString,
               source)

// foldCharacterString
//     : (UPPER | LOWER) LEFT_PAREN valueExpression RIGHT_PAREN
//     ;
struct FoldCharacterString : NodeBase<FoldCharacterString> {
  enum class Case { Upper, Lower };

  Case case_ = Case::Upper;
  ValueExpressionPtr expr;
};
GQL_AST_STRUCT(FoldCharacterString, case_, expr)

// trimMultiCharacterCharacterString
//     : (BTRIM | LTRIM | RTRIM) LEFT_PAREN valueExpression (COMMA
//     valueExpression)? RIGHT_PAREN
//     ;
struct TrimMultiCharacterCharacterString
    : NodeBase<TrimMultiCharacterCharacterString> {
  enum class TrimType { BTrim, LTrim, RTrim };

  TrimType type = TrimType::BTrim;
  ValueExpressionPtr source;
  std::optional<ValueExpressionPtr> trimString;
};
GQL_AST_STRUCT(TrimMultiCharacterCharacterString, type, source, trimString)

// normalizeCharacterString
//     : NORMALIZE LEFT_PAREN valueExpression (COMMA normalForm)? RIGHT_PAREN
//     ;
struct NormalizeCharacterString : NodeBase<NormalizeCharacterString> {
  ValueExpressionPtr expr;
  NormalForm form = NormalForm::NFC;  // NFC is implicit value.
};
GQL_AST_STRUCT(NormalizeCharacterString, expr, form)

// characterOrByteStringFunction
//    : subCharacterOrByteString
//    | trimSingleCharacterOrByteString
//    | foldCharacterString
//    | trimMultiCharacterCharacterString
//    | normalizeCharacterString
//    ;

// nodeReferenceValueExpression
//     : valueExpressionPrimary
//     ;
using NodeReferenceValueExpression = ValueExpressionPtr;

// edgeReferenceValueExpression
//     : valueExpressionPrimary
//     ;
using EdgeReferenceValueExpression = ValueExpressionPtr;

// pathValueConstructorByEnumeration
//     : PATH LEFT_BRACKET pathElementList RIGHT_BRACKET
//     ;

// pathElementList
//     : pathElementListStart pathElementListStep*
//     ;

// pathElementListStart
//     : nodeReferenceValueExpression
//     ;

// pathElementListStep
//     : COMMA edgeReferenceValueExpression COMMA nodeReferenceValueExpression
//     ;

// pathValueConstructor
//    : pathValueConstructorByEnumeration
//    ;
struct PathValueConstructor : NodeBase<PathValueConstructor> {
  std::vector<NodeReferenceValueExpression> nodes;
  std::vector<EdgeReferenceValueExpression> edges;  // One less than nodes
};
GQL_AST_STRUCT(PathValueConstructor, nodes, edges)

// aggregatingValueExpression
//     : valueExpression
//     ;
using AggregatingValueExpression = ValueExpression;

struct PropertyReference : NodeBase<PropertyReference> {
  ValueExpressionPtr element;
  PropertyName property;
};
GQL_AST_STRUCT(PropertyReference, element, property)

// parenthesizedValueExpression : LEFT_PAREN valueExpression RIGHT_PAREN ;

struct SessionUserValue {};
GQL_AST_VALUE(SessionUserValue)

// generalValueSpecification
//     : dynamicParameterSpecification
//     | SESSION_USER
//     ;

// unsignedValueSpecification
//     : unsignedLiteral
//     | generalValueSpecification
//     ;

// dynamicParameterSpecification : GENERAL_PARAMETER_REFERENCE ;

// valueInitializer
//    : EQUALS_OPERATOR valueExpression

// valueVariableDefinition
//     : VALUE bindingVariable optTypedValueInitializer
//     ;

// optTypedValueInitializer
//     : (typed? valueType)? valueInitializer
//     ;

// letVariableDefinition
//    : valueVariableDefinition
//    | bindingVariable EQUALS_OPERATOR valueExpression
struct LetVariableDefinition : NodeBase<LetVariableDefinition> {
  BindingVariable var;
  std::optional<ValueType> type;
  ValueExpressionPtr expr;
};
GQL_AST_STRUCT(LetVariableDefinition, var, type, expr)

// letVariableDefinitionList
//     : letVariableDefinition (COMMA letVariableDefinition)*
//     ;

// letValueExpression : LET letVariableDefinitionList IN valueExpression END ;
struct LetValueExpression : NodeBase<LetValueExpression> {
  std::vector<LetVariableDefinition> definitions;
  ValueExpressionPtr expression;
};
GQL_AST_STRUCT(LetValueExpression, definitions, expression)

// valueQueryExpression : VALUE nestedQuerySpecification ;

// labelExpression
//    : EXCLAMATION_MARK labelExpression
//    | labelExpression AMPERSAND labelExpression
//    | labelExpression VERTICAL_BAR labelExpression
//    | labelName
//    | PERCENT
//    | LEFT_PAREN labelExpression RIGHT_PAREN
struct LabelExpression;
using LabelExpressionPtr = value_ptr<LabelExpression>;
struct LabelExpression : NodeBase<LabelExpression> {
  struct Negation {
    LabelExpressionPtr expr;
  };
  struct Logical {
    enum class Op { And, Or };

    Op op = Op::And;
    std::vector<LabelExpressionPtr> terms;
  };
  struct Wildcard {};
  std::variant<Negation, Logical, LabelName, Wildcard> option;
};
GQL_AST_STRUCT(LabelExpression, option)
GQL_AST_STRUCT(LabelExpression::Negation, expr)
GQL_AST_STRUCT(LabelExpression::Logical, terms)
GQL_AST_VALUE(LabelExpression::Wildcard)

// nullPredicatePart2
//     : IS NOT? NULL_KW
//     ;

// valueTypePredicatePart2
//     : IS NOT? typed valueType
//     ;

// normalizedPredicatePart2
//     : IS NOT? normalForm? NORMALIZED
//     ;

// directedPredicatePart2
//     : IS NOT? DIRECTED
//     ;

// labeledPredicatePart2
//     : isLabeledOrColon labelExpression
//     ;

// sourcePredicatePart2
//     : IS NOT? SOURCE OF edgeReference
//     ;

// destinationPredicatePart2
//     : IS NOT? DESTINATION OF edgeReference
//     ;

// whenOperand
//     : nonParenthesizedValueExpressionPrimary
//     | compOp valueExpression
//     | nullPredicatePart2
//     | valueTypePredicatePart2
//     | normalizedPredicatePart2
//     | directedPredicatePart2
//     | labeledPredicatePart2
//     | sourcePredicatePart2
//     | destinationPredicatePart2
//     ;
struct WhenOperand : NodeBase<WhenOperand> {
  struct Comparison {
    CompOp op = CompOp::Equals;
    ValueExpressionPtr value;
  };
  struct IsNull {};
  struct IsDirected {};
  struct IsTyped {
    ValueType type;
  };
  struct IsNormalized {
    NormalForm form = NormalForm::NFC;
  };
  struct IsLabeled {
    LabelExpression label;
  };
  struct IsSourceOrDestinationOf {
    enum class Direction { Source, Destination };
    Direction direction = Direction::Source;
    EdgeReference edge;
  };

  bool isNot = false;
  std::variant<ValueExpressionPtr,
               Comparison,
               IsNull,
               IsTyped,
               IsNormalized,
               IsDirected,
               IsLabeled,
               IsSourceOrDestinationOf>
      option;
};
GQL_AST_STRUCT(WhenOperand, isNot, option)
GQL_AST_STRUCT(WhenOperand::Comparison, op, value)
GQL_AST_STRUCT(WhenOperand::IsTyped, type)
GQL_AST_STRUCT(WhenOperand::IsNormalized, form)
GQL_AST_STRUCT(WhenOperand::IsLabeled, label)
GQL_AST_STRUCT(WhenOperand::IsSourceOrDestinationOf, direction, edge)
GQL_AST_VALUE(WhenOperand::IsNull)
GQL_AST_VALUE(WhenOperand::IsDirected)

// caseAbbreviation
//     : NULLIF LEFT_PAREN valueExpression COMMA valueExpression RIGHT_PAREN
//     | COALESCE LEFT_PAREN valueExpression (COMMA valueExpression)+
//     RIGHT_PAREN
//     ;

struct NullIfCaseAbbreviation : NodeBase<NullIfCaseAbbreviation> {
  ValueExpressionPtr first;
  ValueExpressionPtr second;
};
GQL_AST_STRUCT(NullIfCaseAbbreviation, first, second)

struct CoalesceCaseAbbreviation : NodeBase<CoalesceCaseAbbreviation> {
  std::vector<ValueExpressionPtr> expressions;
};
GQL_AST_STRUCT(CoalesceCaseAbbreviation, expressions)

// resultExpression : valueExpression ;
using ResultExpression = ValueExpressionPtr;

// result
//     : resultExpression
//     | nullLiteral
//     ;
using Result = std::variant<ResultExpression, NullLiteral>;

// whenOperandList
//     : whenOperand (COMMA whenOperand)*
//     ;

// simpleWhenClause : WHEN whenOperandList THEN result ;
struct SimpleWhenClause : NodeBase<SimpleWhenClause> {
  std::vector<WhenOperand> operands;
  Result result;
};
GQL_AST_STRUCT(SimpleWhenClause, operands, result)

// caseOperand
//     : nonParenthesizedValueExpressionPrimary
//     | elementVariableReference
//     ;
using CaseOperand = std::variant<ValueExpressionPtr, ElementVariableReference>;

// elseClause : ELSE result ;
using ElseClause = Result;

// simpleCase
//     : CASE caseOperand simpleWhenClause+ elseClause? END
//     ;
struct SimpleCase : NodeBase<SimpleCase> {
  CaseOperand operand;
  std::vector<SimpleWhenClause> when;
  std::optional<ElseClause> else_;
};
GQL_AST_STRUCT(SimpleCase, operand, when, else_)

// searchedWhenClause : WHEN searchCondition THEN result ;
struct SearchedWhenClause : NodeBase<SearchedWhenClause> {
  SearchCondition condition;
  Result result;
};
GQL_AST_STRUCT(SearchedWhenClause, condition, result)

// searchedCase
//     : CASE searchedWhenClause+ elseClause? END
//     ;
struct SearchedCase : NodeBase<SearchedCase> {
  std::vector<SearchedWhenClause> when;
  std::optional<ElseClause> else_;
};
GQL_AST_STRUCT(SearchedCase, when, else_)

// caseSpecification
//     : simpleCase
//     | searchedCase
//     ;

// caseExpression
//     : caseAbbreviation
//     | caseSpecification
//     ;
using CaseExpression = std::variant<NullIfCaseAbbreviation,
                                    CoalesceCaseAbbreviation,
                                    SimpleCase,
                                    SearchedCase>;

// castOperand
//     : valueExpression
//     | nullLiteral
//     ;
using CastOperand = std::variant<ValueExpressionPtr, NullLiteral>;

// castTarget : valueType ;
using CastTarget = ValueType;

// castSpecification : CAST LEFT_PAREN castOperand AS castTarget RIGHT_PAREN ;
struct CastSpecification : NodeBase<CastSpecification> {
  CastOperand operand;
  CastTarget target;
};
GQL_AST_STRUCT(CastSpecification, operand, target)

struct CountAsteriskValue {};
GQL_AST_VALUE(CountAsteriskValue)

// generalSetFunctionType
//     : AVG | COUNT | MAX | MIN | SUM | COLLECT_LIST | STDDEV_SAMP | STDDEV_POP
//     ;
enum class GeneralSetFunctionType {
  AVG,
  COUNT,
  MAX,
  MIN,
  SUM,
  COLLECT_LIST,  // GF10 feature
  STDDEV_SAMP,   // GF10 feature
  STDDEV_POP     // GF10 feature
};

// setQuantifier : DISTINCT | ALL ;
enum class SetQuantifier { DISTINCT, ALL };

// generalSetFunction
//     : generalSetFunctionType LEFT_PAREN setQuantifier? valueExpression
//     RIGHT_PAREN
//     ;
struct GeneralSetFunction : NodeBase<GeneralSetFunction> {
  GeneralSetFunctionType type = GeneralSetFunctionType::AVG;
  SetQuantifier quantifier = SetQuantifier::ALL;  // ALL is implicit value.
  ValueExpressionPtr value;
};
GQL_AST_STRUCT(GeneralSetFunction, type, quantifier, value)

// binarySetFunctionType : PERCENTILE_CONT | PERCENTILE_DISC ;
enum class BinarySetFunctionType { PERCENTILE_CONT, PERCENTILE_DISC };

// dependentValueExpression : setQuantifier? numericValueExpression ;

// independentValueExpression : numericValueExpression ;
using IndependentValueExpression = NumericValueExpression;

// binarySetFunction
//     : binarySetFunctionType LEFT_PAREN dependentValueExpression COMMA
//     independentValueExpression RIGHT_PAREN
//     ;
struct BinarySetFunction : NodeBase<BinarySetFunction> {
  BinarySetFunctionType type = BinarySetFunctionType::PERCENTILE_CONT;
  SetQuantifier quantifier = SetQuantifier::ALL;  // ALL is implicit value.
  NumericValueExpression dependentValue;
  IndependentValueExpression independent;
};
GQL_AST_STRUCT(BinarySetFunction, type, quantifier, dependentValue, independent)

// aggregateFunction
//     : COUNT LEFT_PAREN ASTERISK RIGHT_PAREN
//     | generalSetFunction
//     | binarySetFunction
//     ;
using AggregateFunction = std::variant<CountAsteriskValue,
                                       GeneralSetFunction,
                                       BinarySetFunction  // GF11 feature
                                       >;

// element_idFunction : ELEMENT_ID LEFT_PAREN elementVariableReference
// RIGHT_PAREN ;
struct ElementIdFunction : NodeBase<ElementIdFunction> {
  ElementVariableReference variable;
};
GQL_AST_STRUCT(ElementIdFunction, variable)

// listValueConstructor : listValueConstructorByEnumeration ;
using ListValueConstructor = ListValueConstructorByEnumeration;

// cardinalityExpression
//     : CARDINALITY LEFT_PAREN cardinalityExpressionArgument RIGHT_PAREN
//     | SIZE LEFT_PAREN listValueExpression RIGHT_PAREN
//     ;

//  cardinalityExpressionArgument
//    : valueExpression
//    ;

// charLengthExpression
//     : (CHAR_LENGTH | CHARACTER_LENGTH) LEFT_PAREN
//     characterStringValueExpression RIGHT_PAREN
//     ;

// byteLengthExpression
//     : (BYTE_LENGTH | OCTET_LENGTH) LEFT_PAREN byteStringValueExpression
//     RIGHT_PAREN
//     ;

// pathLengthExpression : PATH_LENGTH LEFT_PAREN pathValueExpression RIGHT_PAREN
// ;

// lengthExpression
//     : charLengthExpression
//     | byteLengthExpression
//     | pathLengthExpression
//     ;

// absoluteValueExpression : ABS LEFT_PAREN valueExpression RIGHT_PAREN ;

// modulusExpression : MOD LEFT_PAREN numericValueExpressionDividend COMMA
// numericValueExpressionDivisor RIGHT_PAREN ;

// trigonometricFunctionName
//     : SIN | COS | TAN | COT | SINH | COSH | TANH | ASIN | ACOS | ATAN |
//     DEGREES | RADIANS
//     ;

// trigonometricFunction : trigonometricFunctionName LEFT_PAREN
// numericValueExpression RIGHT_PAREN ;

// generalLogarithmFunction : LOG_KW LEFT_PAREN generalLogarithmBase COMMA
// generalLogarithmArgument RIGHT_PAREN ;

// commonLogarithm : LOG10 LEFT_PAREN numericValueExpression RIGHT_PAREN ;

// naturalLogarithm : LN LEFT_PAREN numericValueExpression RIGHT_PAREN ;

// exponentialFunction : EXP LEFT_PAREN numericValueExpression RIGHT_PAREN ;

// powerFunction : POWER LEFT_PAREN numericValueExpressionBase COMMA
// numericValueExpressionExponent RIGHT_PAREN ;

// squareRoot : SQRT LEFT_PAREN numericValueExpression RIGHT_PAREN ;

// floorFunction : FLOOR LEFT_PAREN numericValueExpression RIGHT_PAREN ;

// ceilingFunction : (CEIL | CEILING) LEFT_PAREN numericValueExpression
// RIGHT_PAREN ;

// numericValueFunction
//     : lengthExpression
//     | cardinalityExpression
//     | absoluteValueExpression
//     | modulusExpression
//     | trigonometricFunction
//     | generalLogarithmFunction
//     | commonLogarithm
//     | naturalLogarithm
//     | exponentialFunction
//     | powerFunction
//     | squareRoot
//     | floorFunction
//     | ceilingFunction
//     ;

// numericValueExpression
//     : sign = (PLUS_SIGN | MINUS_SIGN) numericValueExpression
//     | numericValueExpression operator = (ASTERISK | SOLIDUS)
//     numericValueExpression | numericValueExpression operator = (PLUS_SIGN |
//     MINUS_SIGN) numericValueExpression | valueExpressionPrimary |
//     numericValueFunction
//     ;

// durationString
//    : characterStringLiteral
//    ;

// dateString
//    : characterStringLiteral
//    ;

// timeString
//    : characterStringLiteral
//    ;

// datetimeString
//    : characterStringLiteral
//    ;

// dateFunctionParameters
//     : dateString
//     | recordConstructor
//     ;

// timeFunctionParameters
//     : timeString
//     | recordConstructor
//     ;

// datetimeFunctionParameters
//     : datetimeString
//     | recordConstructor
//     ;

// dateFunction
//     : CURRENT_DATE
//     | DATE LEFT_PAREN dateFunctionParameters? RIGHT_PAREN
//     ;

// timeFunction
//     : CURRENT_TIME
//     | ZONED_TIME LEFT_PAREN timeFunctionParameters? RIGHT_PAREN
//     ;

// localtimeFunction
//     : LOCAL_TIME (LEFT_PAREN timeFunctionParameters? RIGHT_PAREN)?
//     ;
struct DateTimeFunction : NodeBase<DateTimeFunction> {
  using Parameters = std::variant<CharacterStringLiteral, RecordConstructor>;
  enum class Function {
    LOCAL_TIME,
    ZONED_TIME,
    ZONED_DATETIME,
    LOCAL_DATETIME,
    DATE,
    DURATION
  };

  Function function = Function::LOCAL_TIME;
  std::optional<Parameters> parameters;
};
GQL_AST_STRUCT(DateTimeFunction, function, parameters)

// datetimeFunction
//     : CURRENT_TIMESTAMP
//     | ZONED_DATETIME LEFT_PAREN datetimeFunctionParameters? RIGHT_PAREN
//     ;

// localdatetimeFunction
//     : LOCAL_TIMESTAMP
//     | LOCAL_DATETIME LEFT_PAREN datetimeFunctionParameters? RIGHT_PAREN
//     ;

// datetimeValueFunction
//     : dateFunction
//     | timeFunction
//     | datetimeFunction
//     | localtimeFunction
//     | localdatetimeFunction
//     ;

// dateFunctionParameters
// durationValueExpression : valueExpression ;

// datetimeSubtractionParameters
//     : datetimeValueExpression1 COMMA datetimeValueExpression2
//     ;

// datetimeSubtraction
//     : DURATION_BETWEEN LEFT_PAREN datetimeSubtractionParameters RIGHT_PAREN
//     temporalDurationQualifier?
//     ;
struct DatetimeSubtraction : NodeBase<DatetimeSubtraction> {
  DatetimeValueExpression param1;
  DatetimeValueExpression param2;
  TemporalDurationQualifier qualifier =
      TemporalDurationQualifier::DayToSecond;  // DAY TO SECOND is implicit
                                               // value.
};
GQL_AST_STRUCT(DatetimeSubtraction, param1, param2, qualifier)

// durationFunctionParameters
//     : durationString
//     | recordConstructor
//     ;

// durationFunction
//     : DURATION LEFT_PAREN durationFunctionParameters RIGHT_PAREN
//     ;

// durationValueFunction
//     : durationFunction
//     | absoluteValueExpression
//     ;

// trimListFunction
//     : TRIM LEFT_PAREN listValueExpression COMMA numericValueExpression
//     RIGHT_PAREN
//     ;

// elementsFunction
//     : ELEMENTS LEFT_PAREN pathValueExpression RIGHT_PAREN
//     ;

// listValueFunction
//     : trimListFunction
//     | elementsFunction
//     ;

// valueFunction
//    : numericValueFunction
//    | datetimeSubtraction
//    | datetimeValueFunction
//    | durationValueFunction
//    | characterOrByteStringFunction
//    | listValueFunction
//    ;
using ValueFunction = std::variant<DatetimeSubtraction,
                                   DateTimeFunction,
                                   SubCharacterOrByteString,
                                   TrimSingleCharacterOrByteString,
                                   FoldCharacterString,
                                   TrimMultiCharacterCharacterString,
                                   NormalizeCharacterString>;

// nonParenthesizedValueExpressionPrimarySpecialCase
//     : aggregateFunction
//     | unsignedValueSpecification
//     | pathValueConstructor
//     | valueExpressionPrimary PERIOD propertyName
//     | valueQueryExpression
//     | caseExpression
//     | castSpecification
//     | element_idFunction
//     | letValueExpression
//     ;

// nonParenthesizedValueExpressionPrimary
//     : nonParenthesizedValueExpressionPrimarySpecialCase
//     | bindingVariableReference
//     ;

// objectExpressionPrimary
//    : VARIABLE valueExpressionPrimary
//    | parenthesizedValueExpression
//    | nonParenthesizedValueExpressionPrimarySpecialCase

// normalizedPredicate
//    : valueExpression IS NOT? normalForm? NORMALIZED
struct NormalizedPredicate : NodeBase<NormalizedPredicate> {
  ValueExpressionPtr expr;
  bool isNot = false;
  NormalForm form = NormalForm::NFC;
};
GQL_AST_STRUCT(NormalizedPredicate, expr, isNot, form)

// valueExpressionPrimary
//    : parenthesizedValueExpression
//    | aggregateFunction
//    | unsignedValueSpecification
//// List and Record literals are reduntantly/abiguously part of the literal
/// production /    | listValueConstructor /    | recordConstructor
//    | pathValueConstructor
//    | valueExpressionPrimary PERIOD propertyName      // <propertyReference
//    | valueQueryExpression
//    | caseExpression
//    | castSpecification
//    | element_idFunction
//    | letValueExpression
//    | bindingVariableReference
//    ;

// bindingTableExpression
//    : nestedBindingTableQuerySpecification
//    | objectExpressionPrimary
//    | bindingTableReference
//    | objectNameOrBindingVariable
struct BindingTableExpression : NodeBase<BindingTableExpression> {
  std::variant<ProcedureBodyPtr,
               ValueExpressionPtr,
               BindingTableReference,
               ObjectNameOrBindingVariable>
      option;
};
GQL_AST_STRUCT(BindingTableExpression, option)

// currentGraph
//    : CURRENT_PROPERTY_GRAPH
//    | CURRENT_GRAPH
struct CurrentGraph {};
GQL_AST_VALUE(CurrentGraph)

// graphExpression
//    : objectExpressionPrimary
//    | graphReference
//    | objectNameOrBindingVariable
//    | currentGraph
struct GraphExpression : NodeBase<GraphExpression> {
  std::variant<ValueExpressionPtr,
               GraphReference,
               ObjectNameOrBindingVariable,
               CurrentGraph>
      option;
};
GQL_AST_STRUCT(GraphExpression, option)

// existsPredicate
//     : EXISTS (LEFT_BRACE graphPattern RIGHT_BRACE | LEFT_PAREN graphPattern
//     RIGHT_PAREN | LEFT_BRACE matchStatementBlock RIGHT_BRACE | LEFT_PAREN
//     matchStatementBlock RIGHT_PAREN | nestedQuerySpecification)
//     ;
struct ExistsPredicate : NodeBase<ExistsPredicate> {
  std::variant<GraphPatternPtr,         // May be rewritten
               MatchStatementBlockPtr,  // May be rewritten
               ProcedureBodyPtr>
      option;
};
GQL_AST_STRUCT(ExistsPredicate, option)

// nullPredicatePart2
//     : IS NOT? NULL_KW
//     ;

// nullPredicate
//     : valueExpressionPrimary nullPredicatePart2
//     ;
struct NullPredicate : NodeBase<NullPredicate> {
  ValueExpressionPtr expr;
  bool isNot = false;
};
GQL_AST_STRUCT(NullPredicate, expr, isNot)

// valueTypePredicate
//     : valueExpressionPrimary valueTypePredicatePart2
//     ;

// valueTypePredicatePart2
//     : IS NOT? typed valueType
//     ;
struct ValueTypePredicate : NodeBase<ValueTypePredicate> {
  ValueExpressionPtr expr;
  bool isNot = false;
  ValueType type;
};
GQL_AST_STRUCT(ValueTypePredicate, expr, isNot, type)

// directedPredicate
//     : elementVariableReference directedPredicatePart2
//     ;

// directedPredicatePart2
//     : IS NOT? DIRECTED
//     ;
struct DirectedPredicate : NodeBase<DirectedPredicate> {
  ElementVariableReference element;
  bool isNot = false;
};
GQL_AST_STRUCT(DirectedPredicate, element, isNot)

// labeledPredicate
//     : elementVariableReference labeledPredicatePart2
//     ;

// labeledPredicatePart2
//     : isLabeledOrColon labelExpression
//     ;

// isLabeledOrColon
//     : IS NOT? LABELED
//     | COLON
//     ;

struct LabeledPredicate : NodeBase<LabeledPredicate> {
  ElementVariableReference element;
  bool isNot = false;
  LabelExpression label;
};
GQL_AST_STRUCT(LabeledPredicate, element, isNot, label)

// sourceDestinationPredicate
//     : nodeReference sourcePredicatePart2
//     | nodeReference destinationPredicatePart2
//     ;

// nodeReference
//     : elementVariableReference
//     ;

// sourcePredicatePart2
//     : IS NOT? SOURCE OF edgeReference
//     ;

// destinationPredicatePart2
//     : IS NOT? DESTINATION OF edgeReference
//     ;

// edgeReference
//     : elementVariableReference
//     ;

struct SourceDestinationPredicate : NodeBase<SourceDestinationPredicate> {
  enum class Kind { NodeIsSourceOfEdge, NodeIsDestinationOfEdge };

  Kind kind = Kind::NodeIsSourceOfEdge;
  bool isNot = false;
  ElementVariableReference node;
  ElementVariableReference edge;
};

GQL_AST_STRUCT(SourceDestinationPredicate, kind, isNot, node, edge)

// all_differentPredicate
//     : ALL_DIFFERENT LEFT_PAREN elementVariableReference COMMA
//     elementVariableReference (COMMA elementVariableReference)* RIGHT_PAREN
//     ;

// samePredicate
//     : SAME LEFT_PAREN elementVariableReference COMMA elementVariableReference
//     (COMMA elementVariableReference)* RIGHT_PAREN
//     ;

struct AllElementsPredicate : NodeBase<AllElementsPredicate> {
  enum class Kind { AllDifferent, Same };

  Kind kind = Kind::AllDifferent;
  std::vector<ElementVariableReference> elements;
};
GQL_AST_STRUCT(AllElementsPredicate, kind, elements)

// property_existsPredicate
//     : PROPERTY_EXISTS LEFT_PAREN elementVariableReference COMMA propertyName
//     RIGHT_PAREN
//     ;

struct PropertyExistsPredicate : NodeBase<PropertyExistsPredicate> {
  ElementVariableReference element;
  PropertyName property;
};
GQL_AST_STRUCT(PropertyExistsPredicate, element, property)

// predicate
//     : existsPredicate
//     | nullPredicate
//     | valueTypePredicate
//     | directedPredicate
//     | labeledPredicate
//     | sourceDestinationPredicate
//     | all_differentPredicate
//     | samePredicate
//     | property_existsPredicate
//     ;
using Predicate = std::variant<ExistsPredicate,
                               NullPredicate,
                               ValueTypePredicate,
                               DirectedPredicate,
                               LabeledPredicate,
                               SourceDestinationPredicate,
                               AllElementsPredicate,
                               PropertyExistsPredicate>;

// This version of valueExpression sucks up rules broken out in the standard to
// a single production. This eliminates ambiguity in multiple rules specifying
// valueExpressionPrimary.

// valueExpression
//    // Numeric, datetime and duration types all support roughly the same
//    expressions. So here
//    // we define a rule that deals with all of them. It is up to the
//    implementation to post
//    // process the sytnax tree and flag invalid type and function
//    combinations. : sign = (PLUS_SIGN | MINUS_SIGN) valueExpression
//    #signedExprAlt | valueExpression operator = (ASTERISK | SOLIDUS)
//    valueExpression       #multDivExprAlt | valueExpression operator =
//    (PLUS_SIGN | MINUS_SIGN) valueExpression   #addSubtractExprAlt
//    // Character strings, byte strings, lists and paths all support the same
//    concatenation
//    // operator. So here we define a rule that deals with all of them. Of
//    course the types
//    // cannot be combined. So it is up to implementation to post process the
//    sytax tree
//    // and flag invalid type and function combinations.
//    | valueExpression CONCATENATION_OPERATOR valueExpression
//    #concatenationExprAlt
//    // The comparisonPredicate productions moved here to eliminate left mutual
//    recursion. | valueExpression compOp valueExpression #comparisonExprAlt
//    //comparisonPredicatePart2 | predicate #predicateExprAlt
//    // The normalizedPredicate productions moved here to eliminate left mutual
//    recursion. | valueExpression normalizedPredicatePart2
//    #normalizedPredicateExprAlt
//    // Boolean value expression included here.
//    | NOT valueExpression #notExprAlt | valueExpression IS NOT? truthValue
//    #isNotExprAlt | valueExpression AND valueExpression #conjunctiveExprAlt |
//    valueExpression operator = (OR | XOR) valueExpression #disjunctiveExprAlt
//    | PROPERTY? GRAPH graphExpression #propertyGraphExprAlt | BINDING? TABLE
//    bindingTableExpression #bindingTableExprAlt | valueFunction
//    #valueFunctionExprAlt | valueExpressionPrimary #primaryExprAlt
//    ;
struct ValueExpression : NodeBase<ValueExpression> {
  struct Unary {
    enum class Op {
      Positive,
      Negative,
      BoolNot,
      Sin,
      Cos,
      Tan,
      Cot,
      Sinh,
      Cosh,
      Tanh,
      Asin,
      Acos,
      Atan,
      Degrees,
      Radians,
      Floor,
      Ceiling,
      SquareRoot,
      Exponential,
      NaturalLogarithm,
      CommonLogarithm,
      AbsoluteValue,
      CharLength,
      ByteLength,
      PathLength,
      Cardinality,
      Size,  // Same as Cardinality but applies only to lists.
      Elements,
    };

    Op op = Op::Positive;
    ValueExpressionPtr expr;
  };
  struct Binary {
    enum class Op {
      Multiply,
      Divide,
      Add,
      Subtract,
      Concatenate,
      BoolAnd,
      BoolOr,
      BoolXor,
      Power,
      GeneralLogarithm,
      Modulus,
      TrimList,
    };

    Op op = Op::Multiply;
    ValueExpressionPtr left, right;
  };
  struct Comparison {
    CompOp op = CompOp::Equals;
    ValueExpressionPtr left, right;
  };
  struct Is {
    ValueExpressionPtr expr;
    bool isNot = false;
    TruthValue value = TruthValue::UNKNOWN;
  };

  std::variant<BindingVariableReference,
               Unary,
               Binary,
               Comparison,
               Is,
               NormalizedPredicate,
               GraphExpression,
               BindingTableExpression,
               ValueFunction,
               AggregateFunction,
               UnsignedLiteral,
               GeneralParameterReference,
               SessionUserValue,
               PathValueConstructor,
               PropertyReference,
               ProcedureBodyPtr,  // valueQueryExpression
               CaseExpression,
               CastSpecification,
               ElementIdFunction,
               LetValueExpression,
               Predicate>
      option;

  // This class is used not only for <value expression> but also for
  // <value expression primary> and other rules. This flag indicates whether
  // this instance was created exactly by <value expression> rule.
  // This is used in only place to handle 20.9 General Rule 5.a
  bool isValueExpressionRule = false;
};
GQL_AST_STRUCT(ValueExpression, option)
GQL_AST_STRUCT(ValueExpression::Unary, op, expr)
GQL_AST_STRUCT(ValueExpression::Binary, op, left, right)
GQL_AST_STRUCT(ValueExpression::Comparison, left, op, right)
GQL_AST_STRUCT(ValueExpression::Is, expr, isNot, value)

}  // namespace gql::ast
