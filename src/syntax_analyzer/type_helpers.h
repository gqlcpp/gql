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

#include "gql/ast/nodes/types.h"

namespace gql {

template <typename T>
ast::ValueType MakeValueType(T&& v, bool notNull = true) {
  ast::ValueType t;
  t.typeOption = std::forward<T>(v);
  t.notNull = notNull;
  return t;
}

inline ast::ValueType MakeValueType(ast::StringType::Kind kind,
                                    bool notNull = true) {
  ast::StringType stringType;
  stringType.kind = kind;
  return MakeValueType(stringType, notNull);
}

const ast::FieldType* HasField(const ast::FieldTypeList&, const std::string&);
ast::FieldType* HasField(ast::FieldTypeList&, const std::string&);

bool IsComparable(const ast::ValueType&, const ast::ValueType&);
bool IsPathType(const ast::ValueType&);
// Union has special rule for nullability
bool IsNullableType(const ast::ValueType&);
bool IsImmaterialType(const ast::ValueType&);
bool IsGroupListType(const ast::ValueType&);

ast::FieldTypeList CombineColumns(const ast::FieldTypeList&,
                                  const ast::FieldTypeList&);
// Implements check in 14.2 Syntax Rule 10.a.vii.1
void AssertComparableTypesInFields(const ast::FieldTypeList&, const ast::Node&);
void AssertColumnNameEqual(const ast::FieldTypeList&,
                           const ast::FieldTypeList&);

void AssertGraphElementReferenceType(const ast::ValueType&, const ast::Node&);
void AssertNodeReferenceType(const ast::ValueType&, const ast::Node&);
void AssertEdgeReferenceType(const ast::ValueType&, const ast::Node&);
void AssertGraphReferenceType(const ast::ValueType&, const ast::Node&);
void AssertBindingTableReferenceType(const ast::ValueType&, const ast::Node&);
void AssertMaterialType(const ast::ValueType&, const ast::Node&);
void AssertAssignableTo(const ast::ValueType&, const ast::ValueType&);
void AssertComparableTypes(const ast::ValueType&, const ast::ValueType&);

}  // namespace gql