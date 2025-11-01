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
#include "gql/ast/ast.h"

namespace gql::detail {

template <typename TargetNodeType, typename BoundaryNodeType, typename Func>
struct ForEachSimplyContainedDescendantVisitor {
  explicit ForEachSimplyContainedDescendantVisitor(Func& func) : func(func) {}

  template <typename NodeType>
  auto operator()(NodeType*) const {
    return ast::VisitorResult::kContinue;
  }

  auto operator()(const BoundaryNodeType*) const {
    return ast::VisitorResult::kSkipChildren;
  }

  ast::VisitorResult operator()(TargetNodeType* node) const {
    return func(*node);
  }

 private:
  Func& func;
};

template <typename TargetNodeType,
          typename BoundaryNodeType,
          typename RootType,
          typename Func>
void ForEachSimplyContainedDescendant(RootType& node, Func&& func) {
  ForEachSimplyContainedDescendantVisitor<TargetNodeType, BoundaryNodeType,
                                          Func>
      visitor(func);
  ast::ForEachDescendantNodeInTree(node, visitor);
}

template <typename TargetNodeType,
          typename BoundaryNodeType,
          typename RootType,
          typename Func>
void ForEachSimplyContainedDescendant(const RootType& node, Func&& func) {
  ForEachSimplyContainedDescendantVisitor<const TargetNodeType,
                                          BoundaryNodeType, Func>
      visitor(func);
  ast::ForEachDescendantNodeInTree(node, visitor);
}

template <typename TargetNodeType, typename RootType, typename Func>
void ForEachDirectlyContainedDescendant(const RootType& node, Func&& func) {
  ForEachSimplyContainedDescendant<TargetNodeType, ast::ProcedureBodyPtr>(node,
                                                                          func);
}

template <typename TargetNodeType, typename RootType>
const TargetNodeType* FindDirectlyContainedDescendant(const RootType& node) {
  const TargetNodeType* result = nullptr;
  ForEachDirectlyContainedDescendant<TargetNodeType>(
      node, [&result](const TargetNodeType& node) {
        result = &node;
        return ast::VisitorResult::kStop;
      });
  return result;
}

}  // namespace gql::detail