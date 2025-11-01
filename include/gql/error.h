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

#include <stdexcept>
#include <string>

#include "error_codes.h"
#include "gql/ast/nodes/node.h"
#include "gql/error_codes.h"
#include "gql/gql_export.h"
#include "gql/standard/features.h"

namespace gql {

class GQL_EXPORT ParserError : public std::runtime_error {
 public:
  ast::InputPosition inputPosition() const { return inputPosition_; }
  ErrorCode errorCode() const { return errorCode_; }
  const std::string& message() const { return msg_; }

 protected:
  static const char* GetFormatString(ErrorCode errorCode,
                                     const char* defaultFormatString);

  ParserError(ast::InputPosition, ErrorCode errorCode, const std::string& msg);

 private:
  const ast::InputPosition inputPosition_;
  const ErrorCode errorCode_;
  const std::string msg_;
};

class UnsupportedFeatureError : public ParserError {
 public:
  UnsupportedFeatureError(standard::Feature, const ast::Node&);

  standard::Feature feature() const { return feature_; }

 private:
  standard::Feature feature_;
};

GQL_EXPORT std::string FormatErrorMessageInQuery(const std::string& query,
                                                 gql::ast::InputPosition pos,
                                                 const std::string& message);

}  // namespace gql