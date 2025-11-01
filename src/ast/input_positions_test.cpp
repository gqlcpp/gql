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

#include "input_positions_test.h"

#include <gtest/gtest.h>

#include "gql/ast/algorithm.h"
#include "gql/ast/ast.h"

namespace gql::ast {

namespace {
template <typename Class, typename Enabled = void>
struct has_MaybeNotSet : std::false_type {};

template <typename Class>
struct has_MaybeNotSet<Class,
                       std::enable_if_t<std::is_member_function_pointer_v<
                           decltype(&Class::MaybeNotSet)>>> : std::true_type {};

template <typename NodeType>
struct ChildrenEnumeratedInOrder {
  static constexpr bool value = true;
};

template <>
struct ChildrenEnumeratedInOrder<gql::ast::PathValueConstructor> {
  static constexpr bool value = false;
};

template <>
struct ChildrenEnumeratedInOrder<gql::ast::InsertPathPattern> {
  static constexpr bool value = false;
};

template <>
struct ChildrenEnumeratedInOrder<gql::ast::LinearDataModifyingStatement> {
  static constexpr bool value = false;
};

template <>
struct ChildrenEnumeratedInOrder<gql::ast::EdgeTypeSpecification> {
  static constexpr bool value = false;
};

template <>
struct ChildrenEnumeratedInOrder<gql::ast::NodeTypeSpecification> {
  static constexpr bool value = false;
};
}  // namespace

template <typename NodeType>
void CheckInputPositions(const NodeType& node,
                         gql::ast::InputPosition& minStart,
                         bool ignorePositionOrder) {
  if constexpr (std::is_base_of_v<gql::ast::Node, NodeType>) {
    bool maybeNotSet = false;
    if constexpr (has_MaybeNotSet<NodeType>::value) {
      maybeNotSet = node.MaybeNotSet();
    }

    if (!node.inputPosition().IsSet() && !maybeNotSet) {
      ADD_FAILURE() << "Node position is not set";
    }

    if (node.inputPosition().IsSet()) {
      if (!ignorePositionOrder && node.inputPosition() < minStart) {
        ADD_FAILURE() << "Node " << node.inputPosition().line << ":"
                      << node.inputPosition().col << " is before "
                      << minStart.line << ":" << minStart.col;
      } else {
        minStart = node.inputPosition();
      }
    }
  }

  if constexpr (ChildrenEnumeratedInOrder<NodeType>::value) {
    gql::ast::ForEachChild(
        node, [&minStart, ignorePositionOrder](auto& childNode) {
          CheckInputPositions(childNode, minStart, ignorePositionOrder);
          return true;
        });
  } else {
    const auto parentStart = minStart;
    gql::ast::ForEachChild(
        node, [&minStart, parentStart, ignorePositionOrder](auto& childNode) {
          gql::ast::InputPosition childMinStart = parentStart;
          CheckInputPositions(childNode, childMinStart, ignorePositionOrder);
          if (childMinStart > minStart) {
            minStart = childMinStart;
          }
          return true;
        });
  }
}

void CheckInputPositions(const gql::ast::GQLProgram& program,
                         bool ignorePositionOrder) {
  gql::ast::InputPosition startPosition{0, 0};
  CheckInputPositions(program, startPosition, ignorePositionOrder);
}

}  // namespace gql::ast