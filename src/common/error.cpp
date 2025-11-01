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

#include "common/formatted_error.h"

#include <sstream>
#include <unordered_map>

namespace gql {

namespace {

std::string FormatParserError(ast::InputPosition inputPosition,
                              const std::string& msg) {
  std::stringstream str;
  if (inputPosition.IsSet()) {
    str << "At line " << inputPosition.line << ":" << inputPosition.col;
  } else {
    str << "At unknown position";
  }
  str << " " << msg;
  return str.str();
}

using ErrorFormatStrings = std::unordered_map<ErrorCode, std::string>;
ErrorFormatStrings& GetErrorFormatOverrides() {
  static ErrorFormatStrings errorFormatStrings;
  return errorFormatStrings;
}

}  // namespace

// static
const char* ParserError::GetFormatString(ErrorCode errorCode,
                                         const char* defaultFormatString) {
  auto& ovr = GetErrorFormatOverrides();
  if (auto it = ovr.find(errorCode); it != ovr.end()) {
    return it->second.c_str();
  } else {
    return defaultFormatString;
  }
}

ParserError::ParserError(ast::InputPosition inputPosition,
                         ErrorCode errorCode,
                         const std::string& msg)
    : std::runtime_error(FormatParserError(inputPosition, msg)),
      inputPosition_(inputPosition),
      errorCode_(errorCode),
      msg_(msg) {}

UnsupportedFeatureError::UnsupportedFeatureError(standard::Feature feature,
                                                 const ast::Node& node)
    : ParserError(
          node.inputPosition(),
          ErrorCode::UnsupportedFeature,
          fmt::format(GetFormatString(ErrorCode::UnsupportedFeature,
                                      "Unsupported feature {0} \"{1}\""),
                      standard::GetFeatureCodeString(feature),
                      standard::GetFeatureDescription(feature))),
      feature_(feature) {}

std::string FormatErrorMessageInQuery(const std::string& query,
                                      gql::ast::InputPosition pos,
                                      const std::string& message) {
  size_t offset = 0;
  for (size_t line = 0; line < pos.line; line++) {
    offset = query.find('\n', offset);
    if (offset == std::string::npos) {
      if (line == pos.line - 1) {
        break;
      } else {
        throw std::runtime_error("Can't find the line in the query");
      }
    }
    offset++;
  }
  std::stringstream str;
  if (offset == std::string::npos) {
    str << query << "\n";
  } else {
    str << query.substr(0, offset);
  }
  str << std::string(pos.col, ' ') << "^ " << message;
  if (offset != std::string::npos) {
    str << "\n" << query.substr(offset);
  }
  return str.str();
}

}  // namespace gql