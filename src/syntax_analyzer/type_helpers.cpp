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

#include "type_helpers.h"

#include <algorithm>

#include "common/formatted_error.h"

namespace gql {

ast::FieldType* HasField(ast::FieldTypeList& record, const std::string& name) {
  auto it = std::find_if(record.begin(), record.end(), [&](const auto& field) {
    return field.name.name == name;
  });
  if (it == record.end()) {
    return nullptr;
  }
  return &*it;
}

const ast::FieldType* HasField(const ast::FieldTypeList& record,
                               const std::string& name) {
  auto it = std::find_if(record.begin(), record.end(), [&](const auto& field) {
    return field.name.name == name;
  });
  if (it == record.end()) {
    return nullptr;
  }
  return &*it;
}

bool IsComparable(const ast::ValueType&, const ast::ValueType&) {
  // TODO: Implement
  return true;
}

bool IsPathType(const ast::ValueType& type) {
  if (auto* simpleType =
          std::get_if<ast::SimplePredefinedType>(&type.typeOption)) {
    return *simpleType == ast::SimplePredefinedType::Path;
  }
  return false;
}

bool IsNullableType(const ast::ValueType& type) {
  // TODO: Recheck
  if (auto* unionType = std::get_if<ast::ValueType::Union>(&type.typeOption)) {
    return std::any_of(unionType->types.begin(), unionType->types.end(),
                       [](const auto& t) { return IsNullableType(*t); });
  }
  return !type.notNull;
}

bool IsImmaterialType(const ast::ValueType& type) {
  if (auto* simpleType =
          std::get_if<ast::SimplePredefinedType>(&type.typeOption)) {
    return *simpleType == ast::SimplePredefinedType::Null ||
           *simpleType == ast::SimplePredefinedType::Empty;
  }
  return false;
}

bool IsGroupListType(const ast::ValueType& type) {
  if (auto* listType = std::get_if<ast::ValueType::List>(&type.typeOption)) {
    return listType->isGroup;
  }
  return false;
}

ast::FieldTypeList CombineColumns(const ast::FieldTypeList&,
                                  const ast::FieldTypeList&) {
  // TODO: Implement
  // May throw if not combinable
  return {};
}

void AssertComparableTypesInFields(const ast::FieldTypeList& record,
                                   const ast::Node& node) {
  for (auto& col : record) {
    if (auto* unionType =
            std::get_if<ast::ValueType::Union>(&col.type->typeOption)) {
      for (size_t i = 0; i < unionType->types.size(); i++) {
        for (size_t j = i + 1; j < unionType->types.size(); j++) {
          if (!IsComparable(*unionType->types[i], *unionType->types[j])) {
            throw FormattedError(node, ErrorCode::E0030,
                                 "Types in column \"{0}\" must be comparable",
                                 col.name.name);
          }
        }
      }
    }
  }
}

void AssertColumnNameEqual(const ast::FieldTypeList&,
                           const ast::FieldTypeList&) {
  // TODO: Implement
  // May throw
}

void AssertGraphElementReferenceType(const ast::ValueType& type,
                                     const ast::Node& node) {
  if (!std::holds_alternative<ast::NodeReferenceValueType>(type.typeOption) &&
      !std::holds_alternative<ast::EdgeReferenceValueType>(type.typeOption)) {
    throw FormattedError(node, ErrorCode::E0048,
                         "Expected graph element reference type");
  }
}

void AssertNodeReferenceType(const ast::ValueType& type,
                             const ast::Node& node) {
  if (!std::holds_alternative<ast::NodeReferenceValueType>(type.typeOption)) {
    throw FormattedError(node, ErrorCode::E0046,
                         "Expected node reference type");
  }
}

void AssertEdgeReferenceType(const ast::ValueType& type,
                             const ast::Node& node) {
  if (!std::holds_alternative<ast::EdgeReferenceValueType>(type.typeOption)) {
    throw FormattedError(node, ErrorCode::E0045,
                         "Expected edge reference type");
  }
}

void AssertGraphReferenceType(const ast::ValueType& type,
                              const ast::Node& node) {
  if (!std::holds_alternative<ast::GraphReferenceValueType>(type.typeOption)) {
    throw FormattedError(node, ErrorCode::E0044,
                         "Expected graph reference type");
  }
}

void AssertBindingTableReferenceType(const ast::ValueType& type,
                                     const ast::Node& node) {
  if (!std::holds_alternative<ast::BindingTableReferenceValueType>(
          type.typeOption)) {
    throw FormattedError(node, ErrorCode::E0043,
                         "Expected binding table reference type");
  }
}

void AssertMaterialType(const ast::ValueType& type, const ast::Node& node) {
  if (!type.notNull) {
    throw FormattedError(node, ErrorCode::E0042, "Material type expected");
  }
}

void AssertAssignableTo(const ast::ValueType&, const ast::ValueType&) {
  // TODO:
}

void AssertComparableTypes(const ast::ValueType&, const ast::ValueType&) {
  // TODO:
}

}  // namespace gql