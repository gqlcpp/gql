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
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include "gql/ast/detail/reflection.h"
#include "gql/ast/detail/value_ptr.h"
#include "gql/ast/nodes/base.h"

namespace gql::ast {

struct ValueExpression;
using ValueExpressionPtr = value_ptr<ValueExpression>;

struct ValueType;
using ValueTypePtr = value_ptr<ValueType>;

struct ProcedureBody;
using ProcedureBodyPtr = value_ptr<ProcedureBody>;

struct GraphPattern;
using GraphPatternPtr = value_ptr<GraphPattern>;

struct MatchStatementBlock;
using MatchStatementBlockPtr = value_ptr<MatchStatementBlock>;

// truthValue
//    : BOOLEAN_LITERAL
//    ;
// BOOLEAN_LITERAL
//    : 'TRUE'
//    | 'FALSE'
//    | 'UNKNOWN'
//    ;
enum class TruthValue { TRUE, FALSE, UNKNOWN };

struct NameNodeCommonBase {
  std::string name;

  const std::string& operator()() const { return name; }
};

struct IdentifierBase : NameNodeCommonBase {};

struct Identifier : IdentifierBase, NodeBase<Identifier> {};
GQL_AST_STRUCT(Identifier, name)

struct DirectoryName : IdentifierBase, NodeBase<DirectoryName> {};
GQL_AST_STRUCT(DirectoryName, name)

struct EdgeTypeName : IdentifierBase, NodeBase<EdgeTypeName> {};
GQL_AST_STRUCT(EdgeTypeName, name)

struct FieldName : IdentifierBase, NodeBase<FieldName> {};
GQL_AST_STRUCT(FieldName, name)

struct GraphTypeName : IdentifierBase, NodeBase<GraphTypeName> {};
GQL_AST_STRUCT(GraphTypeName, name)

struct LabelName : IdentifierBase, NodeBase<LabelName> {};
GQL_AST_STRUCT(LabelName, name)

struct NodeTypeName : IdentifierBase, NodeBase<NodeTypeName> {};
GQL_AST_STRUCT(NodeTypeName, name)

struct ObjectName : IdentifierBase, NodeBase<ObjectName> {};
GQL_AST_STRUCT(ObjectName, name)

struct ProcedureName : IdentifierBase, NodeBase<ProcedureName> {};
GQL_AST_STRUCT(ProcedureName, name)

struct PropertyName : IdentifierBase, NodeBase<PropertyName> {};
GQL_AST_STRUCT(PropertyName, name)

struct SchemaName : IdentifierBase, NodeBase<SchemaName> {};
GQL_AST_STRUCT(SchemaName, name)

// regularIdentifier
//    : REGULAR_IDENTIFIER
//    | nonReservedWords
//    ;
struct RegularIdentifier : NameNodeCommonBase, NodeBase<RegularIdentifier> {};
GQL_AST_STRUCT(RegularIdentifier, name)

struct ObjectNameOrBindingVariable : NameNodeCommonBase,
                                     NodeBase<ObjectNameOrBindingVariable> {};
GQL_AST_STRUCT(ObjectNameOrBindingVariable, name)

// graphName
//    : regularIdentifier
//    | delimitedGraphName
//    ;
struct GraphName : IdentifierBase, NodeBase<GraphName> {};
GQL_AST_STRUCT(GraphName, name)

struct BindingVariableBase : NameNodeCommonBase {};

// bindingVariable
//    : regularIdentifier
//    ;
struct BindingVariable : BindingVariableBase, NodeBase<BindingVariable> {};
GQL_AST_STRUCT(BindingVariable, name)

// bindingVariableReference
//    : bindingVariable
//    ;
struct BindingVariableReference : BindingVariableBase,
                                  NodeBase<BindingVariableReference> {};
GQL_AST_STRUCT(BindingVariableReference, name)

// pathVariable
//    : bindingVariable
struct PathVariable : BindingVariableBase, NodeBase<PathVariable> {};
GQL_AST_STRUCT(PathVariable, name)

// elementVariableReference
//    : bindingVariableReference
struct ElementVariableReference : BindingVariableBase,
                                  NodeBase<ElementVariableReference> {};
GQL_AST_STRUCT(ElementVariableReference, name)

// edgeReference
//    : elementVariableReference
//    ;
struct EdgeReference : BindingVariableBase, NodeBase<EdgeReference> {};
GQL_AST_STRUCT(EdgeReference, name)

// unsignedInteger
//    : UNSIGNED_DECIMAL_INTEGER
//    | UNSIGNED_HEXADECIMAL_INTEGER
//    | UNSIGNED_OCTAL_INTEGER
//    | UNSIGNED_BINARY_INTEGER
//    ;
using UnsignedInteger = uint64_t;

// BYTE_STRING_LITERAL
//    : 'X' QUOTE SPACE* (HEX_DIGIT SPACE* HEX_DIGIT SPACE*)* QUOTE
//    ;
using ByteStringLiteral = std::vector<uint8_t>;

// dateLiteral
//    : DATE dateString
//      ;
struct DateLiteral : NodeBase<DateLiteral> {
  std::string value;
};
GQL_AST_STRUCT(DateLiteral, value)

// timeLiteral
//    : TIME timeString
//      ;
struct TimeLiteral : NodeBase<TimeLiteral> {
  std::string value;
};
GQL_AST_STRUCT(TimeLiteral, value)

// datetimeLiteral
//    : (DATETIME | TIMESTAMP) datetimeString
//      ;
struct DatetimeLiteral : NodeBase<DatetimeLiteral> {
  std::string value;
};
GQL_AST_STRUCT(DatetimeLiteral, value)

// temporalLiteral
//    : dateLiteral
//    | timeLiteral
//    | datetimeLiteral
//    // | sqlDatetimeLiteral
//    ;
using TemporalLiteral = std::variant<DateLiteral, TimeLiteral, DatetimeLiteral>;

// characterStringLiteral
//    : SINGLE_QUOTED_CHARACTER_SEQUENCE
//    | DOUBLE_QUOTED_CHARACTER_SEQUENCE
//    ;
struct CharacterStringLiteral : NameNodeCommonBase,
                                NodeBase<CharacterStringLiteral> {};
GQL_AST_STRUCT(CharacterStringLiteral, name)

struct ExactNumericLiteral {
  int64_t value;
  unsigned int scale;
};
GQL_AST_STRUCT(ExactNumericLiteral, value, scale)

// unsignedNumericLiteral
//    : exactNumericLiteral
//    | approximateNumericLiteral
//    ;
using UnsignedNumericLiteral = std::variant<double, ExactNumericLiteral>;

//  nullLiteral
//    : NULL_KW
//    ;
struct NullLiteral {};
GQL_AST_VALUE(NullLiteral)

// durationLiteral
//    : DURATION durationString
//    // | sqlIntervalLiteral
//    ;
struct DurationLiteral : NodeBase<DurationLiteral> {
  std::string value;
};
GQL_AST_STRUCT(DurationLiteral, value)

// listElement : valueExpression ;

// listElementList : listElement (COMMA listElement)* ;

// listValueConstructorByEnumeration
//     : listValueTypeName? LEFT_BRACKET listElementList? RIGHT_BRACKET
//     ;
struct ListValueConstructorByEnumeration
    : NodeBase<ListValueConstructorByEnumeration> {
  std::vector<ValueExpressionPtr> elements;
};
GQL_AST_STRUCT(ListValueConstructorByEnumeration, elements)

// listLiteral
//    : listValueConstructorByEnumeration
//    ;
using ListLiteral = ListValueConstructorByEnumeration;

// field : fieldName COLON valueExpression ;
struct Field : NodeBase<Field> {
  FieldName name;
  ValueExpressionPtr value;
};
GQL_AST_STRUCT(Field, name, value)

// fieldList : field (COMMA field)* ;

// fieldsSpecification : LEFT_BRACE fieldList? RIGHT_BRACE ;

// recordConstructor : RECORD? fieldsSpecification ;
using RecordConstructor = std::vector<Field>;

// recordLiteral
//    : recordConstructor
//    ;
using RecordLiteral = RecordConstructor;

// generalLiteral
//    : BOOLEAN_LITERAL
//    | characterStringLiteral
//    | BYTE_STRING_LITERAL
//    | temporalLiteral
//    | durationLiteral
//    | nullLiteral
//    | listLiteral
//    | recordLiteral
//    ;
using GeneralLiteral = std::variant<TruthValue,
                                    CharacterStringLiteral,
                                    ByteStringLiteral,
                                    TemporalLiteral,
                                    DurationLiteral,
                                    NullLiteral,
                                    ListLiteral,
                                    RecordLiteral>;

// unsignedLiteral
//    : unsignedNumericLiteral
//    | generalLiteral
//    ;
using UnsignedLiteral = std::variant<UnsignedNumericLiteral, GeneralLiteral>;

// GENERAL_PARAMETER_REFERENCE
//    : DOLLAR_SIGN PARAMETER_NAME
//    ;
struct GeneralParameterReference : NodeBase<GeneralParameterReference> {
  std::string name;
};
GQL_AST_STRUCT(GeneralParameterReference, name)

// SUBSTITUTED_PARAMETER_REFERENCE
//    : DOUBLE_DOLLAR_SIGN PARAMETER_NAME
//    ;

//  referenceParameterSpecification
//    : SUBSTITUTED_PARAMETER_REFERENCE
//    ;
struct ReferenceParameterSpecification
    : NodeBase<ReferenceParameterSpecification> {
  std::string name;
};
GQL_AST_STRUCT(ReferenceParameterSpecification, name)

// delimitedGraphName
//    : DOUBLE_QUOTED_CHARACTER_SEQUENCE
//    | ACCENT_QUOTED_CHARACTER_SEQUENCE
//    ;
struct DelimitedGraphName : NameNodeCommonBase, NodeBase<DelimitedGraphName> {};
GQL_AST_STRUCT(DelimitedGraphName, name)

// delimitedBindingTableName
//    : DOUBLE_QUOTED_CHARACTER_SEQUENCE
//    | ACCENT_QUOTED_CHARACTER_SEQUENCE
struct DelimitedBindingTableName : NameNodeCommonBase,
                                   NodeBase<DelimitedBindingTableName> {};
GQL_AST_STRUCT(DelimitedBindingTableName, name)

// bindingTableName
//    : regularIdentifier
//    | delimitedBindingTableName
struct BindingTableName : IdentifierBase, NodeBase<BindingTableName> {};
GQL_AST_STRUCT(BindingTableName, name)

// nonNegativeIntegerSpecification
//     : unsignedInteger
//     | dynamicParameterSpecification
//     ;
using NonNegativeIntegerSpecification =
    std::variant<UnsignedInteger,
                 GeneralParameterReference  // Feature GS16
                 >;

}  // namespace gql::ast
