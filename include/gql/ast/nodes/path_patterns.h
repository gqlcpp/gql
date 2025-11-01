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

#include "gql/ast/nodes/expression.h"

namespace gql::ast {

struct SimplifiedTertiary;
using SimplifiedTertiaryPtr = value_ptr<SimplifiedTertiary>;

struct ParenthesizedPathPatternExpression;
using ParenthesizedPathPatternExpressionPtr =
    value_ptr<ParenthesizedPathPatternExpression>;

// elementVariable
//    : bindingVariable

// elementVariableDeclaration
//    : TEMP? elementVariable
// NOTE 249 — An <element variable declaration> containing TEMP is a
// specification device and is not syntax available to the user.
struct ElementVariableDeclaration : BindingVariableBase,
                                    NodeBase<ElementVariableDeclaration> {
  bool isTemp =
      false;  // Never set to true by parser, only by program transformatons.
};
GQL_AST_STRUCT(ElementVariableDeclaration, name, isTemp)

// isLabelExpression
//    : isOrColon labelExpression
using IsLabelExpression = LabelExpression;

// elementPatternWhereClause
//    : WHERE searchCondition
struct ElementPatternWhereClause : NodeBase<ElementPatternWhereClause> {
  SearchCondition condition;
};
GQL_AST_STRUCT(ElementPatternWhereClause, condition)

// propertyKeyValuePair
//    : propertyName COLON valueExpression
struct PropertyKeyValuePair : NodeBase<PropertyKeyValuePair> {
  PropertyName name;
  ValueExpression value;
};
GQL_AST_STRUCT(PropertyKeyValuePair, name, value)

// propertyKeyValuePairList
//    : propertyKeyValuePair (COMMA propertyKeyValuePair)*
using PropertyKeyValuePairList = std::vector<PropertyKeyValuePair>;

// elementPropertySpecification
//    : LEFT_BRACE propertyKeyValuePairList RIGHT_BRACE
struct ElementPropertySpecification : NodeBase<ElementPropertySpecification> {
  PropertyKeyValuePairList props;
};
GQL_AST_STRUCT(ElementPropertySpecification, props)

// elementPatternPredicate
//    : elementPatternWhereClause
//    | elementPropertySpecification
using ElementPatternPredicate =
    std::variant<ElementPatternWhereClause,  // May be rewritten
                 ElementPropertySpecification>;

// elementPatternFiller
//    : elementVariableDeclaration? isLabelExpression? elementPatternPredicate?
struct ElementPatternFiller : NodeBase<ElementPatternFiller> {
  std::optional<ElementVariableDeclaration> var;
  std::optional<IsLabelExpression> labelExpr;
  std::optional<ElementPatternPredicate> predicate;

  bool MaybeNotSet() const { return !var && !labelExpr && !predicate; }
};
GQL_AST_STRUCT(ElementPatternFiller, var, labelExpr, predicate)

// fullEdgePattern
//    : fullEdgePointingLeft
//    | fullEdgeUndirected
//    | fullEdgePointingRight
//    | fullEdgeLeftOrUndirected
//    | fullEdgeUndirectedOrRight
//    | fullEdgeLeftOrRight
//    | fullEdgeAnyDirection

// fullEdgePointingLeft
//    : LEFT_ARROW_BRACKET elementPatternFiller RIGHT_BRACKET_MINUS

// fullEdgeUndirected
//    : TILDE_LEFT_BRACKET elementPatternFiller RIGHT_BRACKET_TILDE

// fullEdgePointingRight
//    : MINUS_LEFT_BRACKET elementPatternFiller BRACKET_RIGHT_ARROW

// fullEdgeLeftOrUndirected
//    : LEFT_ARROW_TILDE_BRACKET elementPatternFiller RIGHT_BRACKET_TILDE

// fullEdgeUndirectedOrRight
//    : TILDE_LEFT_BRACKET elementPatternFiller BRACKET_TILDE_RIGHT_ARROW

// fullEdgeLeftOrRight
//    : LEFT_ARROW_BRACKET elementPatternFiller BRACKET_RIGHT_ARROW

// fullEdgeAnyDirection
//    : MINUS_LEFT_BRACKET elementPatternFiller RIGHT_BRACKET_MINUS

// abbreviatedEdgePattern
//    : LEFT_ARROW
//    | TILDE
//    | RIGHT_ARROW
//    | LEFT_ARROW_TILDE
//    | TILDE_RIGHT_ARROW
//    | LEFT_MINUS_RIGHT
//    | MINUS_SIGN

// nodePattern
//    : LEFT_PAREN elementPatternFiller RIGHT_PAREN
struct NodePattern : NodeBase<NodePattern> {
  ElementPatternFiller filler;
};
GQL_AST_STRUCT(NodePattern, filler)

enum class EdgeDirectionPattern {
  Left,
  Undirected,
  Right,
  LeftOrUndirected,
  UndirectedOrRight,
  LeftOrRight,
  AnyDirection
};

// edgePattern
//    : fullEdgePattern
//    | abbreviatedEdgePattern
struct EdgePattern : NodeBase<EdgePattern> {
  EdgeDirectionPattern direction = EdgeDirectionPattern::Left;
  ElementPatternFiller filler;
};
GQL_AST_STRUCT(EdgePattern, direction, filler)

// elementPattern
//    : nodePattern
//    | edgePattern
using ElementPattern = std::variant<NodePattern, EdgePattern>;

// parenthesizedPathPatternWhereClause
//    : WHERE searchCondition
struct ParenthesizedPathPatternWhereClause
    : NodeBase<ParenthesizedPathPatternWhereClause> {
  SearchCondition condition;
};
GQL_AST_STRUCT(ParenthesizedPathPatternWhereClause, condition)

using SubpathVariable = RegularIdentifier;

// subpathVariableDeclaration
//    : subpathVariable EQUALS_OPERATOR
using SubpathVariableDeclaration = SubpathVariable;

// pathMode
//    : WALK
//    | TRAIL
//    | SIMPLE
//    | ACYCLIC
enum class PathMode { WALK, TRAIL, SIMPLE, ACYCLIC };

// pathModePrefix
//    : pathMode pathOrPaths?

// fixedQuantifier
//    : LEFT_BRACE unsignedInteger RIGHT_BRACE

// lowerBound
//    : unsignedInteger
using LowerBound = UnsignedInteger;

// upperBound
//    : unsignedInteger
using UpperBound = UnsignedInteger;

// generalQuantifier
//    : LEFT_BRACE lowerBound? COMMA upperBound? RIGHT_BRACE
struct GeneralQuantifier {
  LowerBound lower = 0u;  // 0 is implicit value.
  std::optional<UpperBound> upper;
};
GQL_AST_STRUCT(GeneralQuantifier, lower, upper)

// graphPatternQuantifier
//    : ASTERISK
//    | PLUS_SIGN
//    | fixedQuantifier
//    | generalQuantifier
using GraphPatternQuantifier = GeneralQuantifier;

// simplifiedNodePattern
//    : elementVariableDeclaration? labelSetSpecification?
//    elementPropertySpecification?
struct SimplifiedNodePattern : NodeBase<SimplifiedNodePattern> {
  std::optional<ElementVariableDeclaration> var;
  LabelSetSpecification labels;
  PropertyKeyValuePairList props;
};
GQL_AST_STRUCT(SimplifiedNodePattern, var, labels, props)

// simplifiedEdgePattern
//    : elementVariableDeclaration? labelSetSpecification?
//    elementPropertySpecification? graphPatternQuantifier?
struct SimplifiedEdgePattern : NodeBase<SimplifiedEdgePattern> {
  std::optional<ElementVariableDeclaration> var;
  LabelSetSpecification labels;
  PropertyKeyValuePairList props;
  std::optional<GraphPatternQuantifier> quantifier;
};
GQL_AST_STRUCT(SimplifiedEdgePattern, var, labels, props, quantifier)

// simplifiedPathPatternPrimary
//    : simplifiedNodePattern
//    | simplifiedEdgePattern
using SimplifiedPathPatternPrimary =
    std::variant<SimplifiedNodePattern, SimplifiedEdgePattern>;

// simplifiedQuantified
//     : simplifiedTertiary graphPatternQuantifier
//     ;

// simplifiedQuestioned
//     : simplifiedTertiary QUESTION_MARK
//     ;

// simplifiedFactorHigh
//     : simplifiedTertiary
//     | simplifiedQuantified
//     | simplifiedQuestioned
//     ;
struct SimplifiedFactorHigh : NodeBase<SimplifiedFactorHigh> {
  struct NoQuantifier {};
  struct Optional {};
  using Quantifier =
      std::variant<NoQuantifier, Optional, GraphPatternQuantifier>;

  SimplifiedTertiaryPtr tertiary;
  Quantifier quantifier;
};
GQL_AST_STRUCT(SimplifiedFactorHigh, tertiary, quantifier)

GQL_AST_VALUE(SimplifiedFactorHigh::NoQuantifier)
GQL_AST_VALUE(SimplifiedFactorHigh::Optional)

// simplifiedFactorLow
//     : simplifiedFactorHigh #simplifiedFactorHighLabel | simplifiedFactorLow
//     AMPERSAND simplifiedFactorHigh #simplifiedConjunctionLabel
//     ;
struct SimplifiedFactorLow : NodeBase<SimplifiedFactorLow> {
  std::vector<SimplifiedFactorHigh> factors;  // Conjunction
};
GQL_AST_STRUCT(SimplifiedFactorLow, factors)

// simplifiedTerm
//     : simplifiedFactorLow                        #simplifiedFactorLowLabel
//     | simplifiedTerm simplifiedFactorLow      #simplifiedConcatenationLabel
//     ;
struct SimplifiedTerm : NodeBase<SimplifiedTerm> {
  std::vector<SimplifiedFactorLow> factors;  // Concatenation
};
GQL_AST_STRUCT(SimplifiedTerm, factors)

// simplifiedPathUnion
//     : simplifiedTerm VERTICAL_BAR simplifiedTerm (VERTICAL_BAR
//     simplifiedTerm)*
//     ;

// simplifiedMultisetAlternation
//     : simplifiedTerm MULTISET_ALTERNATION_OPERATOR simplifiedTerm
//     (MULTISET_ALTERNATION_OPERATOR simplifiedTerm)*
//     ;

// simplifiedContents
//     : simplifiedTerm
//     | simplifiedPathUnion
//     | simplifiedMultisetAlternation
//     ;
struct SimplifiedContents : NodeBase<SimplifiedContents> {
  enum class Op { Union, MultisetAlternation };

  Op op = Op::Union;
  std::vector<SimplifiedTerm> terms;
};
GQL_AST_STRUCT(SimplifiedContents, op, terms)

// simplifiedPrimary
//     : labelName
//     | LEFT_PAREN simplifiedContents RIGHT_PAREN
//     ;
using SimplifiedPrimary = std::variant<LabelName, SimplifiedContents>;

// simplifiedNegation
//     : EXCLAMATION_MARK simplifiedPrimary
//     ;

// simplifiedSecondary
//     : simplifiedPrimary
//     | simplifiedNegation
//     ;

// simplifiedDefaultingLeft
//     : LEFT_MINUS_SLASH simplifiedContents SLASH_MINUS
//     ;

// simplifiedDefaultingUndirected
//     : TILDE_SLASH simplifiedContents SLASH_TILDE
//     ;

// simplifiedDefaultingRight
//     : MINUS_SLASH simplifiedContents SLASH_MINUS_RIGHT
//     ;

// simplifiedDefaultingLeftOrUndirected
//     : LEFT_TILDE_SLASH simplifiedContents SLASH_TILDE
//     ;

// simplifiedDefaultingUndirectedOrRight
//     : TILDE_SLASH simplifiedContents SLASH_TILDE_RIGHT
//     ;

// simplifiedDefaultingLeftOrRight
//     : LEFT_MINUS_SLASH simplifiedContents SLASH_MINUS_RIGHT
//     ;

// simplifiedDefaultingAnyDirection
//     : MINUS_SLASH simplifiedContents SLASH_MINUS
//     ;

// simplifiedOverrideLeft
//     : LEFT_ANGLE_BRACKET simplifiedSecondary
//     ;

// simplifiedOverrideUndirected
//     : TILDE simplifiedSecondary
//     ;

// simplifiedOverrideRight
//     : simplifiedSecondary RIGHT_ANGLE_BRACKET
//     ;

// simplifiedOverrideLeftOrUndirected
//     : LEFT_ARROW_TILDE simplifiedSecondary
//     ;

// simplifiedOverrideUndirectedOrRight
//     : TILDE simplifiedSecondary RIGHT_ANGLE_BRACKET
//     ;

// simplifiedOverrideLeftOrRight
//     : LEFT_ANGLE_BRACKET simplifiedSecondary RIGHT_ANGLE_BRACKET
//     ;

// simplifiedOverrideAnyDirection
//     : MINUS_SIGN simplifiedSecondary
//     ;

// simplifiedDirectionOverride
//     : simplifiedOverrideLeft
//     | simplifiedOverrideUndirected
//     | simplifiedOverrideRight
//     | simplifiedOverrideLeftOrUndirected
//     | simplifiedOverrideUndirectedOrRight
//     | simplifiedOverrideLeftOrRight
//     | simplifiedOverrideAnyDirection
//     ;

// simplifiedTertiary
//     : simplifiedDirectionOverride
//     | simplifiedSecondary
//     ;
struct SimplifiedTertiary : NodeBase<SimplifiedTertiary> {
  std::optional<EdgeDirectionPattern> direction;
  SimplifiedPrimary primary;
  bool isNegation = false;
};
GQL_AST_STRUCT(SimplifiedTertiary, direction, primary, isNegation)

// simplifiedPathPatternExpression
//     : simplifiedDefaultingLeft
//     | simplifiedDefaultingUndirected
//     | simplifiedDefaultingRight
//     | simplifiedDefaultingLeftOrUndirected
//     | simplifiedDefaultingUndirectedOrRight
//     | simplifiedDefaultingLeftOrRight
//     | simplifiedDefaultingAnyDirection
//     ;
struct SimplifiedPathPatternExpression
    : NodeBase<SimplifiedPathPatternExpression> {
  EdgeDirectionPattern direction = EdgeDirectionPattern::Left;
  SimplifiedContents contents;
};
GQL_AST_STRUCT(SimplifiedPathPatternExpression, direction, contents)

// pathPrimary
//    : elementPattern
//    | parenthesizedPathPatternExpression
//    | simplifiedPathPatternExpression
using PathPrimary = std::variant<
    ElementPattern,
    ParenthesizedPathPatternExpressionPtr,
    SimplifiedPathPatternExpression  // Eliminated by
                                     // rewrite::RewriteSimplifiedPathPattern
    >;

// pathFactor
//    : pathPrimary
//    | pathPrimary graphPatternQuantifier
//    | pathPrimary QUESTION_MARK
struct PathFactor : NodeBase<PathFactor> {
  using NoQuantifier = SimplifiedFactorHigh::NoQuantifier;
  using Optional = SimplifiedFactorHigh::Optional;
  using Quantifier = SimplifiedFactorHigh::Quantifier;

  PathPrimary pattern;
  Quantifier quantifier;
};
GQL_AST_STRUCT(PathFactor, pattern, quantifier)

// pathTerm
//    : pathFactor+
using PathTerm = std::vector<PathFactor>;

// pathPatternExpression
//    : pathTerm
//    | pathTerm (MULTISET_ALTERNATION_OPERATOR pathTerm)+
//    | pathTerm (VERTICAL_BAR pathTerm)+
struct PathPatternExpression : NodeBase<PathPatternExpression> {
  using Op = SimplifiedContents::Op;

  Op op = Op::Union;
  std::vector<PathTerm> terms;
};
GQL_AST_STRUCT(PathPatternExpression, op, terms)

// parenthesizedPathPatternExpression
//    : LEFT_PAREN subpathVariableDeclaration? pathModePrefix?
//    pathPatternExpression parenthesizedPathPatternWhereClause? RIGHT_PAREN
struct ParenthesizedPathPatternExpression
    : NodeBase<ParenthesizedPathPatternExpression> {
  std::optional<SubpathVariableDeclaration> var;
  PathMode pathMode = PathMode::WALK;  // WALK is implicit value.
  PathPatternExpression pattern;
  std::optional<ParenthesizedPathPatternWhereClause> where;
};
GQL_AST_STRUCT(ParenthesizedPathPatternExpression,
               var,
               pathMode,
               pattern,
               where)

}  // namespace gql::ast
