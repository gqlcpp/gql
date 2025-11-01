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

#include "syntax_analyzer.h"

#include <unordered_set>
#include "common/formatted_error.h"
#include "gql/ast/algorithm.h"
#include "gql/ast/ast.h"
#include "type_helpers.h"

namespace gql {

ast::ValueType SyntaxAnalyzer::Process(const ast::ValueType& type,
                                       const ExecutionContext& context) const {
  // TODO: Implement 18.9
  if (type.notNull) {
    ThrowIfFeatureNotSupported(standard::Feature::GV90, type);
  }
  ast::variant_switch(
      type.typeOption,
      [&](const ast::SimplePredefinedType& value) {
        switch (value) {
          case ast::SimplePredefinedType::Boolean:
            break;
          case ast::SimplePredefinedType::Localdatetime:
          case ast::SimplePredefinedType::Date:
          case ast::SimplePredefinedType::Localtime:
            ThrowIfFeatureNotSupported(standard::Feature::GV39, type);
            break;
          case ast::SimplePredefinedType::Datetime:
          case ast::SimplePredefinedType::Time:
            ThrowIfFeatureNotSupported(standard::Feature::GV40, type);
            break;
          case ast::SimplePredefinedType::DurationYearToMonth:
          case ast::SimplePredefinedType::DurationDayToSecond:
            ThrowIfFeatureNotSupported(standard::Feature::GV41, type);
            break;
          case ast::SimplePredefinedType::Null:
            ThrowIfFeatureNotSupported(standard::Feature::GV71, type);
            break;
          case ast::SimplePredefinedType::Empty:
            ThrowIfFeatureNotSupported(standard::Feature::GV72, type);
            break;
          case ast::SimplePredefinedType::Any:
            ThrowIfFeatureNotSupported(standard::Feature::GV66, type);
            break;
          case ast::SimplePredefinedType::AnyProperty:
            ThrowIfFeatureNotSupported(standard::Feature::GV68, type);
            break;
          case ast::SimplePredefinedType::Path:
            ThrowIfFeatureNotSupported(standard::Feature::GV55, type);
            break;
        }
      },
      [&](const ast::SimpleNumericType& value) {
        switch (value) {
          case ast::SimpleNumericType::Int:
          case ast::SimpleNumericType::UInt:
            break;
          case ast::SimpleNumericType::Int8:
            ThrowIfFeatureNotSupported(standard::Feature::GV02, type);
            break;
          case ast::SimpleNumericType::Int16:
            ThrowIfFeatureNotSupported(standard::Feature::GV04, type);
            break;
          case ast::SimpleNumericType::Int32:
            ThrowIfFeatureNotSupported(standard::Feature::GV07, type);
            break;
          case ast::SimpleNumericType::Int64:
            ThrowIfFeatureNotSupported(standard::Feature::GV12, type);
            break;
          case ast::SimpleNumericType::Int128:
            ThrowIfFeatureNotSupported(standard::Feature::GV14, type);
            break;
          case ast::SimpleNumericType::Int256:
            ThrowIfFeatureNotSupported(standard::Feature::GV16, type);
            break;
          case ast::SimpleNumericType::SmallInt:
            ThrowIfFeatureNotSupported(standard::Feature::GV18, type);
            break;
          case ast::SimpleNumericType::BigInt:
            ThrowIfFeatureNotSupported(standard::Feature::GV19, type);
            break;
          case ast::SimpleNumericType::UInt8:
            ThrowIfFeatureNotSupported(standard::Feature::GV01, type);
            break;
          case ast::SimpleNumericType::UInt16:
            ThrowIfFeatureNotSupported(standard::Feature::GV03, type);
            break;
          case ast::SimpleNumericType::UInt32:
            ThrowIfFeatureNotSupported(standard::Feature::GV06, type);
            break;
          case ast::SimpleNumericType::UInt64:
            ThrowIfFeatureNotSupported(standard::Feature::GV11, type);
            break;
          case ast::SimpleNumericType::UInt128:
            ThrowIfFeatureNotSupported(standard::Feature::GV13, type);
            break;
          case ast::SimpleNumericType::UInt256:
            ThrowIfFeatureNotSupported(standard::Feature::GV15, type);
            break;
          case ast::SimpleNumericType::USmallInt:
            ThrowIfFeatureNotSupported(standard::Feature::GV05, type);
            break;
          case ast::SimpleNumericType::UBigInt:
            ThrowIfFeatureNotSupported(standard::Feature::GV10, type);
            break;
          case ast::SimpleNumericType::Float16:
            ThrowIfFeatureNotSupported(standard::Feature::GV20, type);
            break;
          case ast::SimpleNumericType::Float32:
            ThrowIfFeatureNotSupported(standard::Feature::GV21, type);
            break;
          case ast::SimpleNumericType::Float64:
            ThrowIfFeatureNotSupported(standard::Feature::GV24, type);
            break;
          case ast::SimpleNumericType::Float128:
            ThrowIfFeatureNotSupported(standard::Feature::GV25, type);
            break;
          case ast::SimpleNumericType::Float256:
            ThrowIfFeatureNotSupported(standard::Feature::GV26, type);
            break;
          case ast::SimpleNumericType::Real:
            ThrowIfFeatureNotSupported(standard::Feature::GV23, type);
            break;
          case ast::SimpleNumericType::Double:
            ThrowIfFeatureNotSupported(standard::Feature::GV23, type);
            break;
        }
      },
      [&](const ast::BinaryExactUserNumericType& value) {
        ThrowIfFeatureNotSupported(standard::Feature::GV09, type);
        if (!value.isSigned) {
          ThrowIfFeatureNotSupported(standard::Feature::GV08, type);
        }
      },
      [&](const ast::ScaleNumericType& value) {
        switch (value.type) {
          case ast::ScaleNumericType::Type::Decimal:
            ThrowIfFeatureNotSupported(standard::Feature::GV17, type);
            break;
          case ast::ScaleNumericType::Type::Float:
            if (value.precision || value.scale) {
              ThrowIfFeatureNotSupported(standard::Feature::GV22, type);
            }
            break;
        }
      },
      [&](const ast::StringType& value) {
        switch (value.kind) {
          case ast::StringType::Kind::BYTES:
            ThrowIfFeatureNotSupported(standard::Feature::GV35, type);
            if (value.minLength > 0) {
              ThrowIfFeatureNotSupported(standard::Feature::GV36, type);
            }
            if (value.maxLength) {
              ThrowIfFeatureNotSupported(standard::Feature::GV37, type);
              if (value.minLength == *value.maxLength) {
                ThrowIfFeatureNotSupported(standard::Feature::GV38, type);
              }
            }
            break;
          case ast::StringType::Kind::CHAR:
            if (value.minLength > 0) {
              ThrowIfFeatureNotSupported(standard::Feature::GV30, type);
            }
            if (value.maxLength) {
              ThrowIfFeatureNotSupported(standard::Feature::GV31, type);
              if (value.minLength == *value.maxLength) {
                ThrowIfFeatureNotSupported(standard::Feature::GV32, type);
              }
            }
            break;
        }
        if (value.maxLength) {
          if (*value.maxLength == 0) {
            throw FormattedError(
                type, ErrorCode::E0095,
                "String type cannot have maximum length of zero");
          }
          if (value.minLength >= *value.maxLength) {
            throw FormattedError(type, ErrorCode::E0096,
                                 "String maximum length must be greater than "
                                 "or equal to the minimum length");
          }
        }
      },
      [&](const ast::GraphReferenceValueType& value) {
        Process(value, context);
      },
      [&](const ast::BindingTableReferenceValueType& value) {
        Process(value, type, context);
      },
      [&](const ast::NodeReferenceValueType& value) {},
      [&](const ast::EdgeReferenceValueType& value) {},
      [&](const ast::ValueType::List& value) {
        ThrowIfFeatureNotSupported(standard::Feature::GV50, type);
        if (!value.valueType) {
          ThrowIfFeatureNotSupported(standard::Feature::GV66, type);
        }

        if (value.isGroup) {
          if (!value.valueType ||
              !(std::holds_alternative<ast::NodeReferenceValueType>(
                    (*value.valueType)->typeOption) ||
                std::holds_alternative<ast::EdgeReferenceValueType>(
                    (*value.valueType)->typeOption))) {
            throw FormattedError(
                type, ErrorCode::E0097,
                "Group list element type must be node or edge reference type");
          }
        }
        if (value.valueType) {
          Process(**value.valueType, context);
        }
      },
      [&](const ast::RecordType& value) {
        ThrowIfFeatureNotSupported(standard::Feature::GV45, type);
        if (value.fields) {
          ThrowIfFeatureNotSupported(standard::Feature::GV46, type);
        } else {
          ThrowIfFeatureNotSupported(standard::Feature::GV47, type);
        }

        if (value.fields) {
          Process(*value.fields, context);
        }
      },
      [&](const ast::ValueType::Union& value) {
        ThrowIfFeatureNotSupported(standard::Feature::GV67, type);

        if (value.types.size() == 1) {
          throw FormattedError(
              type, ErrorCode::E0035,
              "Union type cannot contain exactly one component");
        }
        std::optional<bool> nullable;
        for (auto& t : value.types) {
          Process(*t, context);
          if (nullable && *nullable != t->notNull) {
            throw FormattedError(
                type, ErrorCode::E0098,
                "Union type components must have the same nullability");
          }
          nullable = t->notNull;
        }
      });
  return type;
}

ast::ValueType SyntaxAnalyzer::Process(const ast::GraphReferenceValueType& type,
                                       const ExecutionContext&) const {
  ThrowIfFeatureNotSupported(standard::Feature::GV60, type);

  return MakeValueType(type);
}

ast::ValueType SyntaxAnalyzer::Process(
    const ast::BindingTableReferenceValueType& type,
    const ast::Node& node,
    const ExecutionContext& context) const {
  ThrowIfFeatureNotSupported(standard::Feature::GV61, node);

  Process(type.type, context);
  return MakeValueType(type);
}

void SyntaxAnalyzer::Process(const ast::FieldTypesSpecification& type,
                             const ExecutionContext& context) const {
  std::unordered_set<std::string> fieldNames;
  for (auto& f : type.fields) {
    if (auto* rec =
            ast::FindFirstDescendantNodeOfType<ast::RecordType>(*f.type)) {
      ThrowIfFeatureNotSupported(standard::Feature::GV48, *rec);
    }

    if (!fieldNames.insert(f.name.name).second) {
      throw FormattedError(type, ErrorCode::E0099,
                           "Field names must be unique");
    }
    Process(*f.type, context);
  }
}

}  // namespace gql