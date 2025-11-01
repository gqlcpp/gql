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

#include <cstdint>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

#include "gql/gql_export.h"

namespace gql::ast::print {

template <typename T, class Enable = void>
struct Printer {};

template <typename T>
struct Printer<const T> : Printer<T> {};

struct NoBreak {
  std::string token;
};

struct MarkSymbol {
  std::string token;
};

template <typename T>
struct SequenceT {
  const std::vector<T>& items;
  const char* separator;
};

template <typename T>
SequenceT<T> Sequence(const std::vector<T>& items, const char* separator) {
  if (separator[0] == ',' && separator[1] == 0)
    separator = ", ";
  return {items, separator};
}

struct QuotedString {
  const std::string& str;
};

inline QuotedString Quoted(const std::string& str) {
  return {str};
}

class GQL_EXPORT OutputStreamBase {
 public:
  OutputStreamBase& operator<<(const NoBreak&);
  OutputStreamBase& operator<<(const MarkSymbol&);
  OutputStreamBase& operator<<(const char* str);
  OutputStreamBase& operator<<(int64_t num);
  OutputStreamBase& operator<<(uint64_t num);
  OutputStreamBase& operator<<(double num);
  OutputStreamBase& operator<<(QuotedString str);

  std::string str() const;

 protected:
  std::stringstream os;

  void ResetNoBreak();

 private:
  bool noBreak = false;
  std::optional<std::string> lastMark;

  char LastChar();
  std::ostream& MaybeSpace(char nextChar);
};

template <template <typename> class Printer>
class OutputStreamT : public OutputStreamBase {
 public:
  OutputStreamT& operator<<(const NoBreak& b) {
    OutputStreamBase::operator<<(b);
    return *this;
  }
  OutputStreamT& operator<<(const MarkSymbol& symbol) {
    OutputStreamBase::operator<<(symbol);
    return *this;
  }
  template <size_t N>
  OutputStreamT& operator<<(const char (&str)[N]) {
    OutputStreamBase::operator<<(str);
    return *this;
  }
  OutputStreamT& operator<<(const char* str) {
    OutputStreamBase::operator<<(str);
    return *this;
  }
  OutputStreamT& operator<<(int64_t num) {
    OutputStreamBase::operator<<(num);
    return *this;
  }
  OutputStreamT& operator<<(uint64_t num) {
    OutputStreamBase::operator<<(num);
    return *this;
  }
  OutputStreamT& operator<<(double num) {
    OutputStreamBase::operator<<(num);
    return *this;
  }
  OutputStreamT& operator<<(QuotedString str) {
    OutputStreamBase::operator<<(str);
    return *this;
  }
  template <typename T>
  OutputStreamT& operator<<(SequenceT<T> seq);

  template <typename NodeType>
  OutputStreamT& operator<<(const NodeType& node) {
    Printer<NodeType>::Print(*this, node);
    return *this;
  }
};

// Converting two-argument Printer template to one-argument template required by
// OutputStreamT.
template <typename T>
using PrinterAdapter = Printer<T>;

using OutputStream = OutputStreamT<PrinterAdapter>;

template <template <typename> class Printer>
template <typename T>
OutputStreamT<Printer>& OutputStreamT<Printer>::operator<<(SequenceT<T> seq) {
  if (seq.items.empty()) {
    return *this;
  }

  bool first = true;
  for (auto& item : seq.items) {
    if (first) {
      first = false;
    } else {
      os << seq.separator;
      *this << NoBreak();
    }
    *this << item;
    ResetNoBreak();
  }
  return *this;
}

}  // namespace gql::ast::print
