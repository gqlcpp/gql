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
#include "gql/ast/print/helpers.h"
#include "gql/ast/print/output_stream.h"

namespace gql::ast::print {

template <>
struct Printer<ElementPatternWhereClause> {
  template <typename OutputStream>
  static void Print(OutputStream& os, const ElementPatternWhereClause& v) {
    os << "WHERE" << v.condition;
  }
};

template <>
struct Printer<PropertyKeyValuePair> {
  template <typename OutputStream>
  static void Print(OutputStream& os, const PropertyKeyValuePair& v) {
    os << v.name << NoBreak() << ":" << ValueExpression(v.value);
  }
};

template <>
struct Printer<PropertyKeyValuePairList> {
  template <typename OutputStream>
  static void Print(OutputStream& os, const PropertyKeyValuePairList& v) {
    os << Sequence(v, ",");
  }
};

template <>
struct Printer<ElementPropertySpecification> {
  template <typename OutputStream>
  static void Print(OutputStream& os, const ElementPropertySpecification& v) {
    os << "{" << v.props << "}";
  }
};

template <>
struct Printer<ElementVariableDeclaration> {
  template <typename OutputStream>
  static void Print(OutputStream& os, const ElementVariableDeclaration& v) {
    if (v.isTemp)
      os << "TEMP";
    os << static_cast<BindingVariableBase>(v);
  }
};

template <>
struct Printer<ElementPatternFiller> {
  template <typename OutputStream>
  static void Print(OutputStream& os, const ElementPatternFiller& v) {
    os << NoBreak() << v.var;
    if (v.labelExpr)
      os << ":" << NoBreak() << v.labelExpr;
    os << v.predicate << NoBreak();
  }
};

inline constexpr const char* kEndOfNodePatternMark = "NodePattern";
inline constexpr const char* kEndOfEdgePatternMark = "EdgePattern";

template <>
struct Printer<NodePattern> {
  template <typename OutputStream>
  static void Print(OutputStream& os, const NodePattern& v) {
    os << NoBreak{kEndOfEdgePatternMark};
    os << "(" << v.filler << ")" << MarkSymbol{kEndOfNodePatternMark};
  }
};

template <>
struct Printer<EdgePattern> {
  constexpr static const char* kMark = "EdgePattern";

  template <typename OutputStream>
  static void Print(OutputStream& os, const EdgePattern& v) {
    os << NoBreak{kEndOfNodePatternMark};
    if (v.filler.var || v.filler.labelExpr || v.filler.predicate) {
      switch (v.direction) {
        case EdgeDirectionPattern::Left:
          os << "<-[" << v.filler << "]-";
          break;
        case EdgeDirectionPattern::Undirected:
          os << "~[" << v.filler << "]~";
          break;
        case EdgeDirectionPattern::Right:
          os << "-[" << v.filler << "]->";
          break;
        case EdgeDirectionPattern::LeftOrUndirected:
          os << "<~[" << v.filler << "]~";
          break;
        case EdgeDirectionPattern::UndirectedOrRight:
          os << "~[" << v.filler << "]~>";
          break;
        case EdgeDirectionPattern::LeftOrRight:
          os << "<-[" << v.filler << "]->";
          break;
        case EdgeDirectionPattern::AnyDirection:
          os << "-[" << v.filler << "]-";
          break;
      }
    } else {
      switch (v.direction) {
        case EdgeDirectionPattern::Left:
          os << "<-";
          break;
        case EdgeDirectionPattern::Undirected:
          os << "~";
          break;
        case EdgeDirectionPattern::Right:
          os << "->";
          break;
        case EdgeDirectionPattern::LeftOrUndirected:
          os << "<~";
          break;
        case EdgeDirectionPattern::UndirectedOrRight:
          os << "~>";
          break;
        case EdgeDirectionPattern::LeftOrRight:
          os << "<->";
          break;
        case EdgeDirectionPattern::AnyDirection:
          os << "-";
          break;
      }
    }
    os << MarkSymbol{kEndOfEdgePatternMark};
  }
};

GQL_AST_ENUM_PRINTER_LITERAL(PathMode, WALK, TRAIL, SIMPLE, ACYCLIC)

template <>
struct Printer<GeneralQuantifier> {
  template <typename OutputStream>
  static void Print(OutputStream& os, const GeneralQuantifier& v) {
    if (!v.upper && v.lower == 1)
      os << "+";
    else if (!v.upper && v.lower == 0)
      os << "*";
    else if (v.upper && v.lower == *v.upper)
      os << "{" << v.lower << "}";
    else
      os << "{" << v.lower << "," << v.upper << "}";
  }
};

template <>
struct Printer<SimplifiedNodePattern> {
  template <typename OutputStream>
  static void Print(OutputStream& os, const SimplifiedNodePattern& v) {
    os << v.var << v.labels;
    if (!v.props.empty())
      os << "{" << v.props << "}";
  }
};

template <>
struct Printer<SimplifiedEdgePattern> {
  template <typename OutputStream>
  static void Print(OutputStream& os, const SimplifiedEdgePattern& v) {
    os << v.var << v.labels;
    if (!v.props.empty())
      os << "{" << v.props << "}";
    os << v.quantifier;
  }
};

GQL_AST_VALUE_PRINTER(SimplifiedFactorHigh::NoQuantifier, "")
GQL_AST_VALUE_PRINTER(SimplifiedFactorHigh::Optional, "?");

template <>
struct Printer<SimplifiedFactorLow> {
  template <typename OutputStream>
  static void Print(OutputStream& os, const SimplifiedFactorLow& v) {
    os << Sequence(v.factors, " & ");
  }
};

template <>
struct Printer<SimplifiedContents> {
  template <typename OutputStream>
  static void Print(OutputStream& os, const SimplifiedContents& v) {
    switch (v.op) {
      case SimplifiedContents::Op::Union:
        os << Sequence(v.terms, " | ");
        break;
      case SimplifiedContents::Op::MultisetAlternation:
        os << Sequence(v.terms, " |+| ");
        break;
    }
  }
};

template <>
struct Printer<SimplifiedPrimary> {
  template <typename OutputStream>
  static void Print(OutputStream& os, const SimplifiedPrimary& v) {
    if (auto* contents = std::get_if<SimplifiedContents>(&v))
      os << "(" << *contents << ")";
    else
      os << std::get<LabelName>(v);
  }
};

template <>
struct Printer<SimplifiedTertiary> {
  template <typename OutputStream>
  static void Print(OutputStream& os, const SimplifiedTertiary& v) {
    if (v.direction) {
      switch (*v.direction) {
        case EdgeDirectionPattern::Left:
          os << "<" << (v.isNegation ? "!" : "") << v.primary;
          break;
        case EdgeDirectionPattern::Undirected:
          os << "~" << (v.isNegation ? "!" : "") << v.primary;
          break;
        case EdgeDirectionPattern::Right:
          os << (v.isNegation ? "!" : "") << v.primary << ">";
          break;
        case EdgeDirectionPattern::LeftOrUndirected:
          os << "<~" << (v.isNegation ? "!" : "") << v.primary;
          break;
        case EdgeDirectionPattern::UndirectedOrRight:
          os << "~" << (v.isNegation ? "!" : "") << v.primary << ">";
          break;
        case EdgeDirectionPattern::LeftOrRight:
          os << "<" << (v.isNegation ? "!" : "") << v.primary << ">";
          break;
        case EdgeDirectionPattern::AnyDirection:
          os << "-" << (v.isNegation ? "!" : "") << v.primary;
          break;
      }
    } else {
      os << (v.isNegation ? "!" : "") << v.primary;
    }
  }
};

template <>
struct Printer<SimplifiedPathPatternExpression> {
  template <typename OutputStream>
  static void Print(OutputStream& os,
                    const SimplifiedPathPatternExpression& v) {
    switch (v.direction) {
      case EdgeDirectionPattern::Left:
        os << "<-/" << v.contents << "/-";
        break;
      case EdgeDirectionPattern::Undirected:
        os << "~/" << v.contents << "/~";
        break;
      case EdgeDirectionPattern::Right:
        os << "-/" << v.contents << "/->";
        break;
      case EdgeDirectionPattern::LeftOrUndirected:
        os << "<~/" << v.contents << "/~";
        break;
      case EdgeDirectionPattern::UndirectedOrRight:
        os << "~/" << v.contents << "/~>";
        break;
      case EdgeDirectionPattern::LeftOrRight:
        os << "<-/" << v.contents << "/->";
        break;
      case EdgeDirectionPattern::AnyDirection:
        os << "-/" << v.contents << "/-";
        break;
    }
  }
};

template <>
struct Printer<PathPatternExpression> {
  template <typename OutputStream>
  static void Print(OutputStream& os, const PathPatternExpression& v) {
    switch (v.op) {
      case PathPatternExpression::Op::MultisetAlternation:
        os << Sequence(v.terms, " |+| ");
        break;
      case PathPatternExpression::Op::Union:
        os << Sequence(v.terms, " | ");
        break;
    }
  }
};

template <>
struct Printer<ParenthesizedPathPatternExpression> {
  template <typename OutputStream>
  static void Print(OutputStream& os,
                    const ParenthesizedPathPatternExpression& v) {
    os << "(";
    if (v.var)
      os << v.var << "=";
    if (v.pathMode != PathMode::WALK)
      os << v.pathMode << "PATH";
    os << v.pattern;
    if (v.where)
      os << "WHERE" << *v.where;
    os << ")";
  }
};

}  // namespace gql::ast::print
