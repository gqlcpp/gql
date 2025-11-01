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

#include <cstdio>

#include "gql/ast/nodes/expression.h"
#include "gql/ast/print/helpers.h"
#include "gql/ast/print/output_stream.h"

namespace gql::ast::print {

GQL_AST_ENUM_PRINTER_LITERAL(TruthValue, TRUE, FALSE, UNKNOWN)
GQL_AST_VALUE_PRINTER(NullLiteral, "NULL")

struct RegularIdentifierOrQuotedStringPrinter {
  template <typename OutputStream, typename T>
  static void Print(OutputStream& os, const T& v) {
    if (IsRegularIdentifier(v.name)) {
      os << v.name.c_str();
    } else {
      os << Quoted(v.name);
    }
  }

 private:
  static bool IsRegularIdentifier(const std::string& name) {
    if (name.empty())
      return false;
    if ('0' <= name[0] && name[0] <= '9')
      return false;
    for (char c : name) {
      if (('a' <= c && c <= 'z') || ('A' <= c && c <= 'Z') ||
          ('0' <= c && c <= '9') || c == '_')
        continue;
      return false;
    }
    return true;
  }
};

template <typename T>
struct Printer<T, std::enable_if_t<std::is_base_of_v<IdentifierBase, T>>>
    : RegularIdentifierOrQuotedStringPrinter {};

template <>
struct Printer<RegularIdentifier> {
  template <typename OutputStream>
  static void Print(OutputStream& os, const RegularIdentifier& v) {
    os << v.name.c_str();
  }
};

template <>
struct Printer<GraphName> : RegularIdentifierOrQuotedStringPrinter {};

template <>
struct Printer<ObjectNameOrBindingVariable> {
  template <typename OutputStream>
  static void Print(OutputStream& os, const ObjectNameOrBindingVariable& v) {
    os << v.name.c_str();
  }
};

template <typename T>
struct Printer<T, std::enable_if_t<std::is_base_of_v<BindingVariableBase, T>>> {
  template <typename OutputStream>
  static void Print(OutputStream& os, const T& v) {
    os << v.name.c_str();
  }
};

template <>
struct Printer<ExactNumericLiteral> {
  template <typename OutputStream>
  static void Print(OutputStream& os, const ExactNumericLiteral& v) {
    if (v.scale == 0) {
      os << v.value;
    } else {
      os << v.value << NoBreak() << "E-" << NoBreak()
         << static_cast<uint64_t>(v.scale) << NoBreak() << "M";
    }
  }
};

template <>
struct Printer<ByteStringLiteral> {
  template <typename OutputStream>
  static void Print(OutputStream& os, const ByteStringLiteral& v) {
    os << "X'";
    for (uint8_t byte : v) {
      char buf[5];
      std::snprintf(buf, sizeof(buf), "%02X", byte);
      os << NoBreak() << buf;
    }
    os << NoBreak() << "'";
  }
};

template <>
struct Printer<DateLiteral> {
  template <typename OutputStream>
  static void Print(OutputStream& os, const DateLiteral& v) {
    os << "DATE" << Quoted(v.value);
  }
};

template <>
struct Printer<TimeLiteral> {
  template <typename OutputStream>
  static void Print(OutputStream& os, const TimeLiteral& v) {
    os << "TIME" << Quoted(v.value);
  }
};

template <>
struct Printer<DatetimeLiteral> {
  template <typename OutputStream>
  static void Print(OutputStream& os, const DatetimeLiteral& v) {
    os << "DATETIME" << Quoted(v.value);
  }
};

template <>
struct Printer<DurationLiteral> {
  template <typename OutputStream>
  static void Print(OutputStream& os, const DurationLiteral& v) {
    os << "DURATION" << Quoted(v.value);
  }
};

template <>
struct Printer<ListValueConstructorByEnumeration> {
  template <typename OutputStream>
  static void Print(OutputStream& os,
                    const ListValueConstructorByEnumeration& v) {
    os << "LIST [" << Sequence(v.elements, ",") << "]";
  }
};

template <>
struct Printer<Field> {
  template <typename OutputStream>
  static void Print(OutputStream& os, const Field& v) {
    os << v.name << NoBreak() << ":" << *v.value;
  }
};

template <>
struct Printer<RecordConstructor> {
  template <typename OutputStream>
  static void Print(OutputStream& os, const RecordConstructor& v) {
    os << "RECORD {" << Sequence(v, ",") << "}";
  }
};

template <>
struct Printer<GeneralParameterReference> {
  template <typename OutputStream>
  static void Print(OutputStream& os, const GeneralParameterReference& v) {
    os << "$" << NoBreak();
    RegularIdentifierOrQuotedStringPrinter::Print(os, v);
  }
};

template <>
struct Printer<ReferenceParameterSpecification> {
  template <typename OutputStream>
  static void Print(OutputStream& os,
                    const ReferenceParameterSpecification& v) {
    os << "$$" << NoBreak();
    RegularIdentifierOrQuotedStringPrinter::Print(os, v);
  }
};

}  // namespace gql::ast::print
