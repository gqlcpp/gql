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

#include <assert.h>
#include <algorithm>
#include <unordered_set>

#include "common/formatted_error.h"
#include "gql/ast/algorithm.h"
#include "gql/ast/ast.h"
#include "gql/syntax_analyzer/aux_data.h"
#include "type_helpers.h"

namespace gql {

ast::ValueType SyntaxAnalyzer::ProcessValueExpression(
    ast::ValueExpression& expr,
    const ExecutionContext& inContext,
    bool isAggregatingValueExpression) {
  auto context = inContext.MakeCopy();
  if (expr.isValueExpressionRule && !isAggregatingValueExpression) {
    context.isInsideValueExpressionNotAggregating = true;
  }
  return ast::variant_switch(
      expr.option, [&](auto& value) { return Process(value, context); },
      [&](ast::ValueExpression::Unary& value) {
        return Process(value, expr, context);
      },
      [&](ast::ValueExpression::Binary& value) {
        return Process(value, expr, context);
      },
      [&](ast::UnsignedLiteral& value) {
        return Process(value, expr, context);
      },
      [&](ast::AggregateFunction& value) {
        return Process(value, expr, context);
      },
      [&](ast::ProcedureBodyPtr& value) {
        auto childContext = context.MakeCopyForProcedureBody();
        auto optTableType =
            Process(*value, CallProcedureKind::Query, childContext);
        if (!optTableType) {
          return MakeValueType(ast::SimplePredefinedType::Null);
        }
        // TODO: Implement
        return ast::ValueType{};
      });
}

ast::ValueType SyntaxAnalyzer::ProcessBooleanValueExpression(
    ast::ValueExpression& expr,
    const ExecutionContext& context) {
  auto type = ProcessValueExpression(expr, context);
  if (!IsCastableTo(type, MakeValueType(ast::SimplePredefinedType::Boolean))) {
    throw FormattedError(expr, ErrorCode::E0026, "Expected boolean value");
  }
  return type;
}

ast::ValueType SyntaxAnalyzer::ProcessStringValueExpression(
    ast::ValueExpression& expr,
    const ExecutionContext& context) {
  auto type = ProcessValueExpression(expr, context);
  if (!IsCastableTo(type, MakeValueType(ast::StringType::Kind::CHAR)) &&
      !IsCastableTo(type, MakeValueType(ast::StringType::Kind::BYTES))) {
    throw FormattedError(expr, ErrorCode::E0021, "Expected string value");
  }
  return type;
}

ast::ValueType SyntaxAnalyzer::ProcessPropertyValueExpression(
    ast::ValueExpression& expr,
    const ExecutionContext& context) {
  auto type = ProcessValueExpression(expr, context);
  // TODO: Specific type?
  if (!IsCastableTo(
          type, MakeValueType(ast::SimplePredefinedType::AnyProperty, false))) {
    throw FormattedError(expr, ErrorCode::E0034,
                         "Expected supported property value type");
  }
  return type;
}

ast::ValueType SyntaxAnalyzer::ProcessAggregatingValueExpression(
    ast::ValueExpression& expr,
    const ExecutionContext& context) {
  auto childContext = context.MakeCopy();
  childContext.isInsideAggregatingValueExpression = true;
  return ProcessValueExpression(expr, childContext, true);
}

ast::ValueType SyntaxAnalyzer::ProcessNumericValueExpression(
    ast::ValueExpression& expr,
    const ExecutionContext& context) {
  // TODO: Check for type
  return ProcessValueExpression(expr, context);
}

ast::ValueType SyntaxAnalyzer::ProcessCharacterStringValueExpression(
    ast::ValueExpression& expr,
    const ExecutionContext& context) {
  auto type = ProcessValueExpression(expr, context);
  if (!IsCastableTo(type, MakeValueType(ast::StringType::Kind::CHAR))) {
    throw FormattedError(expr, ErrorCode::E0025,
                         "Expected character string value");
  }
  return type;
}

ast::ValueType SyntaxAnalyzer::ProcessByteStringValueExpression(
    ast::ValueExpression& expr,
    const ExecutionContext& context) {
  auto type = ProcessValueExpression(expr, context);
  if (!IsCastableTo(type, MakeValueType(ast::StringType::Kind::BYTES))) {
    throw FormattedError(expr, ErrorCode::E0024, "Expected byte string value");
  }
  return type;
}

ast::ValueType SyntaxAnalyzer::ProcessPathValueExpression(
    ast::ValueExpression& expr,
    const ExecutionContext& context) {
  ThrowIfFeatureNotSupported(standard::Feature::GV55, expr);

  auto type = ProcessValueExpression(expr, context);
  if (!IsCastableTo(type, MakeValueType(ast::SimplePredefinedType::Path))) {
    throw FormattedError(expr, ErrorCode::E0041, "Expected path value");
  }
  return type;
}

ast::ValueType SyntaxAnalyzer::ProcessListValueExpression(
    ast::ValueExpression& expr,
    const ExecutionContext& context) {
  auto type = ProcessValueExpression(expr, context);
  if (!IsCastableTo(type, MakeValueType(ast::ValueType::List{}))) {
    throw FormattedError(expr, ErrorCode::E0040, "Expected list value");
  }
  return type;
}

ast::ValueType SyntaxAnalyzer::Process(ast::ValueExpression::Comparison& expr,
                                       const ExecutionContext& context) {
  auto left = ProcessValueExpression(*expr.left, context);
  auto right = ProcessValueExpression(*expr.right, context);
  if (!IsComparable(left, right)) {
    ThrowIfFeatureNotSupported(standard::Feature::GA04, *expr.left);
  }
  switch (expr.op) {
    case ast::CompOp::Equals:
    case ast::CompOp::NotEquals:
      if (IsPathType(left)) {
        ThrowIfFeatureNotSupported(standard::Feature::GA09, *expr.left);
      }
      if (IsPathType(right)) {
        ThrowIfFeatureNotSupported(standard::Feature::GA09, *expr.right);
      }
      break;
    case ast::CompOp::LessThan:
    case ast::CompOp::LessThanOrEquals:
    case ast::CompOp::GreaterThan:
    case ast::CompOp::GreaterThanOrEquals:
      // TODO: 22.14 Ordering operations
      break;
  }

  return MakeValueType(ast::SimplePredefinedType::Boolean);
}

ast::ValueType SyntaxAnalyzer::Process(ast::Predicate& expr,
                                       const ExecutionContext& context) {
  return ast::variant_switch(
      expr, [&](auto& value) { return Process(value, context); });
}

ast::ValueType SyntaxAnalyzer::Process(ast::ExistsPredicate& expr,
                                       const ExecutionContext& context) {
  if (auto* proc = std::get_if<ast::ProcedureBodyPtr>(&expr.option)) {
    auto childContext = context.MakeCopyForProcedureBody();
    Process(**proc, CallProcedureKind::Query, childContext);
  } else {
    throw FormattedError(expr, ErrorCode::E0112,
                         "EXIST must be rewritten to procedure call");
  }

  return MakeValueType(ast::SimplePredefinedType::Boolean);
}

ast::ValueType SyntaxAnalyzer::Process(ast::NullPredicate& expr,
                                       const ExecutionContext& context) {
  ProcessValueExpression(*expr.expr, context);

  return MakeValueType(ast::SimplePredefinedType::Boolean);
}

ast::ValueType SyntaxAnalyzer::Process(ast::ValueTypePredicate& expr,
                                       const ExecutionContext& context) {
  ThrowIfFeatureNotSupported(standard::Feature::GA06, expr);

  ProcessValueExpression(*expr.expr, context);
  Process(expr.type, context);

  return MakeValueType(ast::SimplePredefinedType::Boolean);
}

ast::ValueType SyntaxAnalyzer::Process(ast::NormalizedPredicate& expr,
                                       const ExecutionContext& context) {
  ProcessStringValueExpression(*expr.expr, context);

  return MakeValueType(ast::SimplePredefinedType::Boolean);
}

ast::ValueType SyntaxAnalyzer::Process(ast::DirectedPredicate& expr,
                                       const ExecutionContext& context) {
  ThrowIfFeatureNotSupported(standard::Feature::G110, expr);

  auto type = ProcessSingleton(expr.element, context);
  AssertEdgeReferenceType(type, expr.element);

  return MakeValueType(ast::SimplePredefinedType::Boolean);
}

ast::ValueType SyntaxAnalyzer::Process(ast::LabeledPredicate& expr,
                                       const ExecutionContext& context) {
  ThrowIfFeatureNotSupported(standard::Feature::G111, expr);

  ProcessSingleton(expr.element, context);

  return MakeValueType(ast::SimplePredefinedType::Boolean);
}

ast::ValueType SyntaxAnalyzer::Process(ast::SourceDestinationPredicate& expr,
                                       const ExecutionContext& context) {
  ThrowIfFeatureNotSupported(standard::Feature::G112, expr);

  auto nodeType = ProcessSingleton(expr.node, context);
  auto edgeType = ProcessSingleton(expr.edge, context);
  AssertEdgeReferenceType(edgeType, expr.edge);
  AssertNodeReferenceType(nodeType, expr.node);

  return MakeValueType(ast::SimplePredefinedType::Boolean);
}

ast::ValueType SyntaxAnalyzer::Process(ast::AllElementsPredicate& expr,
                                       const ExecutionContext& context) {
  switch (expr.kind) {
    case ast::AllElementsPredicate::Kind::AllDifferent:
      ThrowIfFeatureNotSupported(standard::Feature::G113, expr);
      break;
    case ast::AllElementsPredicate::Kind::Same:
      ThrowIfFeatureNotSupported(standard::Feature::G114, expr);
      break;
  }

  for (auto& element : expr.elements) {
    ProcessSingleton(element, context);
  }
  return MakeValueType(ast::SimplePredefinedType::Boolean);
}

ast::ValueType SyntaxAnalyzer::Process(ast::PropertyExistsPredicate& expr,
                                       const ExecutionContext& context) {
  ThrowIfFeatureNotSupported(standard::Feature::G115, expr);

  auto type = ProcessSingleton(expr.element, context);

  return MakeValueType(ast::SimplePredefinedType::Boolean, type.notNull);
}

ast::ValueType SyntaxAnalyzer::Process(ast::GraphExpression& expr,
                                       const ExecutionContext& context) {
  // TODO: Handle closed graph types.
  return ast::variant_switch(
      expr.option,
      [&](ast::ValueExpressionPtr& value) {
        auto type = ProcessValueExpression(*value, context);
        AssertGraphReferenceType(type, *value);
        AssertMaterialType(type, *value);
        return type;
      },
      [&](const ast::GraphReference& value) {
        ast::variant_switch(
            value.option, [&](const auto&) {},
            [&](const ast::ReferenceParameterSpecification& value) {
              Process(value, context);
            });
        return MakeValueType(ast::GraphReferenceValueType{});
      },
      [&](const ast::ObjectNameOrBindingVariable& value) {
        // TODO: Implement 11.1 Syntax Rule 2.a
        return MakeValueType(ast::GraphReferenceValueType{});
      },
      [&](const ast::CurrentGraph&) {
        return MakeValueType(ast::GraphReferenceValueType{});
      });
}

ast::ValueType SyntaxAnalyzer::Process(ast::BindingTableExpression& expr,
                                       const ExecutionContext& context) {
  return ast::variant_switch(
      expr.option,
      [&](ast::ValueExpressionPtr& value) {
        auto type = ProcessValueExpression(*value, context);
        AssertBindingTableReferenceType(type, *value);
        AssertMaterialType(type, *value);
        return type;
      },
      [&](const ast::BindingTableReference& value) {
        ast::variant_switch(
            value.option, [&](const auto&) {},
            [&](const ast::ReferenceParameterSpecification& value) {
              Process(value, context);
            });
        return MakeValueType(ast::BindingTableReferenceValueType{});
      },
      [&](ast::ProcedureBodyPtr& value) {
        ThrowIfFeatureNotSupported(standard::Feature::GV61, *value);

        auto childContext = context.MakeCopyForProcedureBody();
        auto bindingTableType =
            Process(*value, CallProcedureKind::Query, childContext);
        if (!bindingTableType) {
          throw FormattedError(*value, ErrorCode::E0039,
                               "Expected material binding table type");
        }
        ast::BindingTableReferenceValueType refType;
        refType.type.fields = std::move(*bindingTableType);
        return MakeValueType(refType);
      },
      [&](const ast::ObjectNameOrBindingVariable& value) {
        // TODO: Implement 11.2 Syntax Rule 3.a
        return MakeValueType(ast::BindingTableReferenceValueType{});
      });
}

ast::ValueType SyntaxAnalyzer::Process(ast::PropertyReference& expr,
                                       const ExecutionContext& context) {
  auto sourceType = ProcessValueExpression(*expr.element, context);

  return ast::variant_switch(
      sourceType.typeOption,
      [&](const auto&) -> ast::ValueType {
        throw FormattedError(expr, ErrorCode::E0038, "Property not found");
      },
      [&](const ast::NodeReferenceValueType& nodeType) {
        // TODO: Check for closed node type
        return MakeValueType(ast::SimplePredefinedType::AnyProperty,
                             sourceType.notNull);
      },
      [&](const ast::EdgeReferenceValueType& edgeType) {
        // TODO: Check for closed edge type
        return MakeValueType(ast::SimplePredefinedType::AnyProperty,
                             sourceType.notNull);
      },
      [&](const ast::RecordType& recordType) {
        ThrowIfFeatureNotSupported(standard::Feature::GV45, recordType);

        if (recordType.fields) {
          auto fieldIt = std::find_if(
              recordType.fields->fields.begin(),
              recordType.fields->fields.end(), [&](const auto& field) {
                return field.name.name == expr.property.name;
              });
          if (fieldIt == recordType.fields->fields.end()) {
            throw FormattedError(expr, ErrorCode::E0037, "Field not found");
          }

          auto propertyType = *fieldIt->type;
          propertyType.notNull &= sourceType.notNull;
          return propertyType;
        } else {
          auto propertyType = MakeValueType(ast::SimplePredefinedType::Any);
          propertyType.notNull = sourceType.notNull;
          return propertyType;
        }
      });
}

ast::ValueType SyntaxAnalyzer::Process(ast::ValueExpression::Unary& expr,
                                       const ast::Node& node,
                                       const ExecutionContext& context) {
  switch (expr.op) {
    case ast::ValueExpression::Unary::Op::Positive:
    case ast::ValueExpression::Unary::Op::Negative:
      return ProcessNumericValueExpression(*expr.expr, context);
    case ast::ValueExpression::Unary::Op::BoolNot:
      ProcessBooleanValueExpression(*expr.expr, context);
      return MakeValueType(ast::SimplePredefinedType::Boolean);
    case ast::ValueExpression::Unary::Op::Sin:
    case ast::ValueExpression::Unary::Op::Cos:
    case ast::ValueExpression::Unary::Op::Tan:
    case ast::ValueExpression::Unary::Op::Cot:
    case ast::ValueExpression::Unary::Op::Sinh:
    case ast::ValueExpression::Unary::Op::Cosh:
    case ast::ValueExpression::Unary::Op::Tanh:
    case ast::ValueExpression::Unary::Op::Asin:
    case ast::ValueExpression::Unary::Op::Acos:
    case ast::ValueExpression::Unary::Op::Atan:
    case ast::ValueExpression::Unary::Op::Degrees:
    case ast::ValueExpression::Unary::Op::Radians:
      ThrowIfFeatureNotSupported(standard::Feature::GF02, node);

      ProcessNumericValueExpression(*expr.expr, context);
      return MakeValueType(ast::SimpleNumericType::Double);
    case ast::ValueExpression::Unary::Op::Floor:
    case ast::ValueExpression::Unary::Op::Ceiling:
      ThrowIfFeatureNotSupported(standard::Feature::GF01, node);

      ProcessNumericValueExpression(*expr.expr, context);
      // TODO: Implement 20.22 Syntax Rule 11
      return MakeValueType(ast::SimpleNumericType::Double);
    case ast::ValueExpression::Unary::Op::SquareRoot:
      ThrowIfFeatureNotSupported(standard::Feature::GF01, node);
      [[fallthrough]];
    case ast::ValueExpression::Unary::Op::Exponential:
    case ast::ValueExpression::Unary::Op::NaturalLogarithm:
    case ast::ValueExpression::Unary::Op::CommonLogarithm:
      ThrowIfFeatureNotSupported(standard::Feature::GF03, node);

      ProcessNumericValueExpression(*expr.expr, context);
      return MakeValueType(ast::SimpleNumericType::Double);
    case ast::ValueExpression::Unary::Op::AbsoluteValue:
      ThrowIfFeatureNotSupported(standard::Feature::GF01, node);

      return ProcessNumericValueExpression(*expr.expr, context);
    case ast::ValueExpression::Unary::Op::CharLength:
      ProcessCharacterStringValueExpression(*expr.expr, context);
      return MakeValueType(ast::SimpleNumericType::UInt64);
    case ast::ValueExpression::Unary::Op::ByteLength:
      ThrowIfFeatureNotSupported(standard::Feature::GV35, node);

      ProcessByteStringValueExpression(*expr.expr, context);
      return MakeValueType(ast::SimpleNumericType::UInt64);
    case ast::ValueExpression::Unary::Op::PathLength:
      ThrowIfFeatureNotSupported(standard::Feature::GF04, node);

      ProcessPathValueExpression(*expr.expr, context);
      return MakeValueType(ast::SimpleNumericType::UInt64);
    case ast::ValueExpression::Unary::Op::Size: {
      ThrowIfFeatureNotSupported(standard::Feature::GF13, node);

      auto argType = ProcessListValueExpression(*expr.expr, context);
      auto resType = MakeValueType(ast::SimpleNumericType::UInt64);
      resType.notNull = argType.notNull;
      return resType;
    }
    case ast::ValueExpression::Unary::Op::Cardinality: {
      ThrowIfFeatureNotSupported(standard::Feature::GF12, node);

      auto argType = ProcessValueExpression(*expr.expr, context);
      // TODO: Check for acceptable types
      auto resType = MakeValueType(ast::SimpleNumericType::UInt64);
      resType.notNull = argType.notNull;
      return resType;
    }
    case ast::ValueExpression::Unary::Op::Elements: {
      ThrowIfFeatureNotSupported(standard::Feature::GV50, node);
      ThrowIfFeatureNotSupported(standard::Feature::GF04, node);
      auto argType = ProcessPathValueExpression(*expr.expr, context);

      // Creating a list type that holds node and edge reference types.
      ast::ValueType::Union elementType;
      elementType.types.emplace_back() =
          MakeValueType(ast::NodeReferenceValueType{});
      elementType.types.emplace_back() =
          MakeValueType(ast::EdgeReferenceValueType{});
      ast::ValueType::List listType;
      listType.valueType.emplace() = MakeValueType(elementType);
      return MakeValueType(listType, !IsNullableType(argType));
    }
    default:
      assert(false);
      return {};
  }
}

ast::ValueType SyntaxAnalyzer::Process(ast::ValueExpression::Binary& expr,
                                       const ast::Node& node,
                                       const ExecutionContext& context) {
  switch (expr.op) {
    case ast::ValueExpression::Binary::Op::Multiply:
    case ast::ValueExpression::Binary::Op::Divide:
    case ast::ValueExpression::Binary::Op::Add:
    case ast::ValueExpression::Binary::Op::Subtract:
    case ast::ValueExpression::Binary::Op::Concatenate:
      // TODO:
    case ast::ValueExpression::Binary::Op::BoolAnd:
    case ast::ValueExpression::Binary::Op::BoolOr:
    case ast::ValueExpression::Binary::Op::BoolXor: {
      auto type1 = ProcessBooleanValueExpression(*expr.left, context);
      auto type2 = ProcessBooleanValueExpression(*expr.right, context);
      return MakeValueType(ast::SimplePredefinedType::Boolean,
                           type1.notNull && type2.notNull);
    }
    case ast::ValueExpression::Binary::Op::Power:
    case ast::ValueExpression::Binary::Op::GeneralLogarithm:
      ThrowIfFeatureNotSupported(standard::Feature::GF03, node);

      ProcessNumericValueExpression(*expr.left, context);
      ProcessNumericValueExpression(*expr.right, context);
      return MakeValueType(ast::SimpleNumericType::Double);
    case ast::ValueExpression::Binary::Op::Modulus:
      ProcessNumericValueExpression(*expr.left, context);
      return ProcessNumericValueExpression(*expr.right, context);
    case ast::ValueExpression::Binary::Op::TrimList:
      // TODO:
      ThrowIfFeatureNotSupported(standard::Feature::GV50, node);

      ProcessNumericValueExpression(*expr.right, context);
      break;
  }
  return {};
}

ast::ValueType SyntaxAnalyzer::Process(ast::ValueExpression::Is& expr,
                                       const ExecutionContext& context) {
  ProcessBooleanValueExpression(*expr.expr, context);
  return MakeValueType(ast::SimplePredefinedType::Boolean);
}

ast::ValueType SyntaxAnalyzer::Process(ast::ValueFunction& expr,
                                       const ExecutionContext& context) {
  return ast::variant_switch(
      expr,
      [&](const ast::DatetimeSubtraction&) {
        // TODO:
        return ast::ValueType{};
      },
      [&](const ast::DateTimeFunction&) {
        // TODO:
        return ast::ValueType{};
      },
      [&](ast::SubCharacterOrByteString& func) {
        auto lengthType = ProcessNumericValueExpression(*func.length, context);
        auto sourceType = ProcessStringValueExpression(*func.expr, context);
        sourceType.notNull &= lengthType.notNull;
        return sourceType;
      },
      [&](ast::TrimSingleCharacterOrByteString& func) {
        auto sourceType = ProcessStringValueExpression(*func.source, context);
        bool isCharString = IsCastableTo(
            sourceType, MakeValueType(ast::StringType::Kind::CHAR));
        if (!isCharString) {
          ThrowIfFeatureNotSupported(standard::Feature::GF07, func);
        }
        if (func.trimString) {
          ThrowIfFeatureNotSupported(standard::Feature::GF06, func);

          auto trimType = ProcessValueExpression(**func.trimString, context);
          if (isCharString) {
            if (!IsCastableTo(trimType,
                              MakeValueType(ast::StringType::Kind::CHAR))) {
              throw FormattedError(**func.trimString, ErrorCode::E0022,
                                   "Expected character string value");
            }
          } else {
            if (!IsCastableTo(trimType,
                              MakeValueType(ast::StringType::Kind::BYTES))) {
              throw FormattedError(**func.trimString, ErrorCode::E0023,
                                   "Expected byte string value");
            }
          }

          AssertComparableTypes(trimType, sourceType);
          sourceType.notNull &= trimType.notNull;
        }
        return sourceType;
      },
      [&](ast::FoldCharacterString& func) {
        return ProcessCharacterStringValueExpression(*func.expr, context);
      },
      [&](const ast::TrimMultiCharacterCharacterString&) {
        // TODO:
        return ast::ValueType{};
      },
      [&](ast::NormalizeCharacterString& func) {
        return ProcessCharacterStringValueExpression(*func.expr, context);
      });
}

ast::ValueType SyntaxAnalyzer::ProcessAggregateFunctionOperand(
    ast::ValueExpression& expr,
    const ExecutionContext& context,
    syntax_analyzer::AggregateFunctionAuxData& auxData) {
  if (context.isInsideValueExpressionNotAggregating) {
    // Aggregate group binding variables.
    std::optional<std::string> groupListVar;
    ast::ForEachNodeOfType<ast::BindingVariableReference>(
        expr, [&](ast::BindingVariableReference& var) {
          if (auto* fieldType = HasField(context.workingRecord, var.name)) {
            if (IsGroupListType(*fieldType->type)) {
              if (groupListVar) {
                throw FormattedError(
                    var, ErrorCode::E0114,
                    "Multiple group list variables in aggregating "
                    "expression are not allowed");
              }
              groupListVar = var.name;
            }
          }
          return ast::VisitorResult::kContinue;
        });
    if (!groupListVar) {
      throw FormattedError(
          expr, ErrorCode::E0115,
          "Group list variable reference is expected in aggregate "
          "function");
    }
    auxData.groupListVariable = *groupListVar;
    auto childContext = context.MakeCopy();
    childContext.workingTable.clear();
    auto* fieldType = HasField(childContext.workingRecord, *groupListVar);
    auto& listType =
        std::get<ast::ValueType::List>(fieldType->type->typeOption);
    fieldType->type = *listType.valueType;
    return ProcessValueExpression(expr, childContext);
  } else {
    auto childContext = context.MakeAmended();
    return ProcessValueExpression(expr, childContext);
  }
}

ast::ValueType SyntaxAnalyzer::Process(ast::AggregateFunction& expr,
                                       const ast::Node& node,
                                       const ExecutionContext& context) {
  if (auto* proc = ast::FindFirstNodeOfType<ast::ProcedureBodyPtr>(expr)) {
    throw FormattedError(**proc, ErrorCode::E0031,
                         "Aggregate function cannot contain procedure body");
  }
  if (ast::FindFirstDescendantNodeOfType<ast::AggregateFunction>(expr)) {
    throw FormattedError(
        node, ErrorCode::E0032,
        "Aggregate function cannot contain aggregate function");
  }

  return ast::variant_switch(
      expr,
      [&](const ast::CountAsteriskValue&) {
        if (!context.isInsideAggregatingValueExpression) {
          throw FormattedError(node, ErrorCode::E0033,
                               "COUNT(*) function shall be contained in an "
                               "aggregating value expression");
        }
        return MakeValueType(ast::SimpleNumericType::UInt64);
      },
      [&](ast::GeneralSetFunction& func) {
        switch (func.type) {
          case ast::GeneralSetFunctionType::COLLECT_LIST:
          case ast::GeneralSetFunctionType::STDDEV_SAMP:
          case ast::GeneralSetFunctionType::STDDEV_POP:
            ThrowIfFeatureNotSupported(standard::Feature::GF10, func);
            break;
          default:
            break;
        }
        auto auxData =
            std::make_shared<syntax_analyzer::AggregateFunctionAuxData>();
        func.auxData = auxData;
        auto operandType =
            ProcessAggregateFunctionOperand(*func.value, context, *auxData);
        // TODO: Implement Syntax Rules
        switch (func.type) {
          case ast::GeneralSetFunctionType::COLLECT_LIST: {
            ast::ValueType::List listType;
            listType.valueType.emplace() = operandType;
            return MakeValueType(listType);
          }
          case ast::GeneralSetFunctionType::STDDEV_SAMP:
          case ast::GeneralSetFunctionType::STDDEV_POP:
            return MakeValueType(ast::SimpleNumericType::Double);
          case ast::GeneralSetFunctionType::COUNT:
            return MakeValueType(ast::SimpleNumericType::UInt64);
          case ast::GeneralSetFunctionType::AVG:
          case ast::GeneralSetFunctionType::MIN:
          case ast::GeneralSetFunctionType::MAX:
          case ast::GeneralSetFunctionType::SUM:
            // Not exactly what the standard says, but for now it's okay.
            return operandType;
          default:
            assert(false);
            return ast::ValueType{};
        }
      },
      [&](ast::BinarySetFunction& func) {
        ThrowIfFeatureNotSupported(standard::Feature::GF11, func);
        ProcessNumericValueExpression(*func.independent, context.MakeAmended());
        auto auxData =
            std::make_shared<syntax_analyzer::AggregateFunctionAuxData>();
        func.auxData = auxData;
        auto operandType = ProcessAggregateFunctionOperand(*func.dependentValue,
                                                           context, *auxData);
        // TODO: Check for numeric result type
        return MakeValueType(ast::SimpleNumericType::Double);
      });
}

ast::ValueType SyntaxAnalyzer::Process(ast::UnsignedLiteral& expr,
                                       const ast::Node& node,
                                       const ExecutionContext& context) {
  // TODO:
  return ast::variant_switch(
      expr,
      [&](const ast::UnsignedNumericLiteral& literal) {
        return ast::variant_switch(
            literal,
            [&](double) {
              return MakeValueType(ast::SimpleNumericType::Double);
            },
            [&](const ast::ExactNumericLiteral& lit) {
              if (lit.scale == 0) {
                return MakeValueType(ast::SimpleNumericType::Int64);
              } else {
                return MakeValueType(
                    ast::ScaleNumericType{ast::ScaleNumericType::Type::Decimal,
                                          /* precision */ 19, lit.scale});
              }
            });
      },
      [&](ast::GeneralLiteral& literal) {
        return ast::variant_switch(
            literal,
            [&](const ast::TruthValue& val) {
              return MakeValueType(ast::SimplePredefinedType::Boolean,
                                   val != ast::TruthValue::UNKNOWN);
            },
            [&](const ast::CharacterStringLiteral& lit) {
              return MakeValueType(ast::StringType::Kind::CHAR);
            },
            [&](const ast::ByteStringLiteral& lit) {
              return MakeValueType(ast::StringType::Kind::BYTES);
            },
            [&](const ast::TemporalLiteral&) {
              // TODO:
              return MakeValueType(ast::SimplePredefinedType::Boolean);
            },
            [&](const ast::DurationLiteral&) {
              // TODO:
              return MakeValueType(ast::SimplePredefinedType::Boolean);
            },
            [&](const ast::NullLiteral&) {
              // TODO:
              return MakeValueType(ast::SimplePredefinedType::Boolean);
            },
            [&](ast::ListLiteral& listLiteral) {
              ThrowIfFeatureNotSupported(standard::Feature::GV50, listLiteral);

              // TODO: 20.17
              for (auto& item : listLiteral.elements) {
                ProcessValueExpression(*item, context);
              }
              return MakeValueType(ast::SimplePredefinedType::Boolean);
            },
            [&](ast::RecordLiteral& recordLiteral) {
              ThrowIfFeatureNotSupported(standard::Feature::GV45, node);
              if (!recordLiteral.empty()) {
                ThrowIfFeatureNotSupported(standard::Feature::GV46, node);
              }

              ast::RecordType type;
              for (auto& f : recordLiteral) {
                if (!type.fields) {
                  type.fields.emplace();
                }
                auto& fieldType = type.fields->fields.emplace_back();
                fieldType.name = f.name;
                fieldType.type = ProcessValueExpression(*f.value, context);
              }
              return MakeValueType(type);
            });
      });
}

ast::ValueType SyntaxAnalyzer::Process(ast::GeneralParameterReference&,
                                       const ExecutionContext&) {
  // TODO:
  return {};
}

ast::ValueType SyntaxAnalyzer::Process(ast::SessionUserValue&,
                                       const ExecutionContext&) {
  // TODO:
  return {};
}

ast::ValueType SyntaxAnalyzer::Process(ast::PathValueConstructor& expr,
                                       const ExecutionContext& context) {
  ThrowIfFeatureNotSupported(standard::Feature::GE06, expr);
  for (auto& node : expr.nodes) {
    auto type = ProcessValueExpression(*node, context);
    AssertNodeReferenceType(type, *node);
  }
  for (auto& edge : expr.edges) {
    auto type = ProcessValueExpression(*edge, context);
    AssertEdgeReferenceType(type, *edge);
  }
  return MakeValueType(ast::SimplePredefinedType::Path);
}

ast::ValueType SyntaxAnalyzer::Process(ast::CaseExpression& expr,
                                       const ExecutionContext& context) {
  // TODO:
  ast::variant_switch(
      expr, [&](const ast::NullIfCaseAbbreviation&) {},
      [&](const ast::CoalesceCaseAbbreviation&) {},
      [&](const ast::SimpleCase&) {}, [&](const ast::SearchedCase&) {});
  return {};
}

ast::ValueType SyntaxAnalyzer::Process(ast::CastSpecification& castExpr,
                                       const ExecutionContext& context) {
  auto destType = Process(castExpr.target, context);
  if (IsImmaterialType(destType)) {
    throw FormattedError(castExpr, ErrorCode::E0107,
                         "Cannot cast to immaterial type");
  }
  ast::variant_switch(
      castExpr.operand,
      [&](ast::ValueExpressionPtr& expr) {
        auto srcType = ProcessValueExpression(*expr, context);
        if (!IsCastableTo(srcType, destType)) {
          throw FormattedError(castExpr, ErrorCode::E0108,
                               "Invalid cast specification");
        }
      },
      [&](const ast::NullLiteral&) {
        if (!IsNullableType(destType)) {
          throw FormattedError(castExpr, ErrorCode::E0106,
                               "Cannot cast NULL to non-nullable type");
        }
      });
  return destType;
}

ast::ValueType SyntaxAnalyzer::Process(ast::ElementIdFunction& expr,
                                       const ExecutionContext& context) {
  ThrowIfFeatureNotSupported(standard::Feature::G100, expr);

  auto type = ProcessSingleton(expr.variable, context);
  return MakeValueType(ast::SimpleNumericType::UInt64, type.notNull);
}

ast::ValueType SyntaxAnalyzer::Process(ast::LetValueExpression& expr,
                                       const ExecutionContext& context) {
  ThrowIfFeatureNotSupported(standard::Feature::GE03, expr);

  auto childContext = context.MakeChild();
  std::unordered_set<std::string> varNames;
  for (auto& var : expr.definitions) {
    if (varNames.insert(var.var.name).second == false) {
      throw FormattedError(var, ErrorCode::E0037,
                           "Duplicate variable name \"{0}\"", var.var.name);
    }

    if (HasField(context.workingRecord, var.var.name)) {
      throw FormattedError(var, ErrorCode::E0036,
                           "Variable \"{0}\" is already declared",
                           var.var.name);
    }

    auto& newField = childContext.workingRecord.emplace_back();
    newField.name.name = var.var.name;
    auto implicitType = ProcessValueExpression(*var.expr, context);
    if (var.type) {
      auto explicitType = Process(*var.type, context);
      AssertAssignableTo(implicitType, explicitType);
      newField.type = explicitType;
    } else {
      newField.type = implicitType;
    }
  }
  return ProcessValueExpression(*expr.expression, childContext);
}

}  // namespace gql