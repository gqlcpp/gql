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

#include <cctype>
#include <sstream>
#include <string>

#include "gql/ast/print/output_stream.h"

namespace gql::ast::print {

OutputStreamBase& OutputStreamBase::operator<<(const NoBreak& b) {
  if (b.token.empty() || (lastMark && b.token == lastMark)) {
    noBreak = true;
  }
  lastMark.reset();
  return *this;
}

OutputStreamBase& OutputStreamBase::operator<<(const MarkSymbol& symbol) {
  lastMark = symbol.token;
  return *this;
}

OutputStreamBase& OutputStreamBase::operator<<(const char* str) {
  if (*str) {
    MaybeSpace(*str) << str;
  }
  return *this;
}

OutputStreamBase& OutputStreamBase::operator<<(int64_t num) {
  MaybeSpace('1') << num;
  return *this;
}

OutputStreamBase& OutputStreamBase::operator<<(uint64_t num) {
  MaybeSpace('1') << num;
  return *this;
}

OutputStreamBase& OutputStreamBase::operator<<(double num) {
  MaybeSpace('1') << num << "d";
  return *this;
}

OutputStreamBase& OutputStreamBase::operator<<(QuotedString str) {
  std::string escaped = "\"";
  for (char c : str.str) {
    switch (c) {
      case '"':
        escaped += "\\\"";
        break;
      case '\n':
        escaped += "\\n";
        break;
      case '\r':
        escaped += "\\r";
        break;
      case '\t':
        escaped += "\\t";
        break;
      case '\b':
        escaped += "\\b";
        break;
      case '\f':
        escaped += "\\f";
        break;
      case '\\':
        escaped += "\\\\";
        break;
      default:
        escaped += c;
        break;
    }
  }
  escaped += '"';
  MaybeSpace('"') << escaped;
  return *this;
}

std::string OutputStreamBase::str() const {
  return os.str();
}

void OutputStreamBase::ResetNoBreak() {
  noBreak = false;
  lastMark.reset();
}

char OutputStreamBase::LastChar() {
  auto pos = os.tellp();
  if (pos > 0) {
    os.seekg(pos - std::streamoff(1));
    return static_cast<char>(os.get());
  }
  return '\0';
}

std::ostream& OutputStreamBase::MaybeSpace(char nextChar) {
  if (nextChar == '\0') {
    return os;
  }

  if (noBreak) {
    noBreak = false;
    return os;
  }

  if (nextChar != ' ' && nextChar != ')' && nextChar != '}' &&
      nextChar != ']' && nextChar != ',') {
    char last = LastChar();
    if (last != '\0' && !std::isspace(last) && last != '(' && last != '{' &&
        last != '[') {
      os << " ";
    }
  }
  return os;
}

}  // namespace gql::ast::print
