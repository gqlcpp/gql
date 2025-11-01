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

#include "gql/rewrite.h"

#include "gql/ast/algorithm.h"
#include "common/formatted_error.h"

namespace gql::rewrite {

void RewritePathPatternPrefix(ast::GraphPattern& graphPattern) {
  if (!graphPattern.keep) {
    return;
  }

  for (auto& pathPattern : graphPattern.paths) {
    if (!pathPattern.prefix) {
      pathPattern.prefix = *graphPattern.keep;
      continue;
    }

    if (pathPattern.prefix->pathSearchPrefix) {
      // 16.4.6.b.i
      throw FormattedError(
          *pathPattern.prefix, ErrorCode::E0090,
          "Path search prefix is not allowed in path pattern when "
          "graph pattern has KEEP clause");
    }

    pathPattern.prefix->pathSearchPrefix = graphPattern.keep->pathSearchPrefix;
  }
}

void RewritePathPatternPrefix(ast::GQLProgram& program) {
  ast::ForEachNodeOfType<ast::GraphPattern>(
      program, [](ast::GraphPattern& node) {
        RewritePathPatternPrefix(node);
        return ast::VisitorResult::kContinue;
      });
}

}  // namespace gql::rewrite