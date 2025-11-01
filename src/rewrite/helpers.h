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

#include "gql/ast/algorithm.h"
#include "gql/ast/nodes/node.h"

namespace gql::rewrite {

namespace detail {

struct SetInputPositionVisitor {
  explicit SetInputPositionVisitor(const ast::InputPosition& position)
      : position(position) {}

  template <
      typename NodeType,
      typename = std::enable_if_t<!std::is_base_of_v<ast::Node, NodeType>>>
  auto operator()(NodeType*) const {
    return ast::VisitorResult::kContinue;
  }

  auto operator()(ast::Node* node) const {
    if (node->inputPosition().IsSet()) {
      return ast::VisitorResult::kSkipChildren;
    }
    node->SetInputPosition(position);
    return ast::VisitorResult::kContinue;
  }

 private:
  const ast::InputPosition position;
};

}  // namespace detail

template <typename NodeType>
inline void SetInputPositionRecursive(NodeType& node,
                                      const ast::InputPosition& position) {
  ast::ForEachNodeInTree(node, detail::SetInputPositionVisitor(position));
}

}  // namespace gql::rewrite