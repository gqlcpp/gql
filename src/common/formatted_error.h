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

#include <fmt/format.h>

#include "gql/error.h"

namespace gql {

class FormattedError : public ParserError {
 public:
  template <typename... Args>
  FormattedError(ast::InputPosition pos,
                 ErrorCode errorCode,
                 const char* defaultFormatString,
                 Args&&... args)
      : ParserError(pos,
                    errorCode,
                    fmt::format(GetFormatString(errorCode, defaultFormatString),
                                std::forward<Args>(args)...)) {}

  template <typename... Args>
  FormattedError(const ast::Node& node,
                 ErrorCode errorCode,
                 const char* defaultFormatString,
                 Args&&... args)
      : FormattedError(node.inputPosition(),
                       errorCode,
                       defaultFormatString,
                       std::forward<Args>(args)...) {}
};

}  // namespace gql