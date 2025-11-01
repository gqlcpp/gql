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

#include "common/formatted_error.h"
#include "gql/ast/nodes/types_comparison.h"
#include "type_helpers.h"

namespace gql {

std::optional<ast::FieldTypeList> AnalyzeSyntax(
    ast::GQLProgram& prog,
    const SyntaxAnalyzerConfig& config) {
  return SyntaxAnalyzer(&config).Process(prog);
}

SyntaxAnalyzer::SyntaxAnalyzer(const Config* config)
    : unsupportedFeatures_(static_cast<int>(standard::Feature::Count)) {
  if (config) {
    config_ = *config;
    for (auto f : config->unsupportedFeatures) {
      unsupportedFeatures_[static_cast<int>(f)] = true;
    }
  }
  if (config_.supportedPropertyTypes.empty()) {
    // 24.2.3 Minimum conformance
    // TODO: Check types
    config_.supportedPropertyTypes = {
        MakeValueType(ast::StringType::Kind::CHAR),
        MakeValueType(ast::SimplePredefinedType::Boolean),
        MakeValueType(ast::SimpleNumericType::Int64),
        MakeValueType(ast::SimpleNumericType::Double),
        MakeValueType(
            ast::ScaleNumericType{ast::ScaleNumericType::Type::Float})};
  }
}

void SyntaxAnalyzer::ThrowIfFeatureNotSupported(standard::Feature feature,
                                                const ast::Node& node) const {
  if (!unsupportedFeatures_[static_cast<int>(feature)])
    return;

  throw UnsupportedFeatureError(feature, node);
}

namespace {

// TODO: Implement
struct IsCastableVisitor {
  IsCastableVisitor(ast::ValueType::Union propertyUnionType)
      : propertyUnionType(propertyUnionType) {}
  IsCastableVisitor(const IsCastableVisitor&) = delete;
  void operator=(const IsCastableVisitor&) = delete;

  bool Visit(const ast::ValueType& src, const ast::ValueType& dest) const {
    return std::visit(*this, MaybeReplace(src).typeOption,
                      MaybeReplace(dest).typeOption);
  }

  ast::ValueType MaybeReplace(const ast::ValueType& type) const {
    if (auto simpleType =
            std::get_if<ast::SimplePredefinedType>(&type.typeOption)) {
      if (*simpleType == ast::SimplePredefinedType::AnyProperty) {
        return MakeValueType(propertyUnionType);
      }
    }
    return type;
  }

  template <typename Src, typename Dest>
  bool operator()(const Src&, const Dest&) const {
    return false;
  }

  bool operator()(const ast::SimplePredefinedType& src,
                  const ast::SimplePredefinedType& dest) const {
    return src == dest;
  }

  bool operator()(const ast::StringType& src,
                  const ast::StringType& dest) const {
    return src.kind == dest.kind;
  }

  bool operator()(const ast::SimpleNumericType& src,
                  const ast::SimpleNumericType& dest) const {
    return src == dest;
  }

  bool operator()(const ast::BinaryExactUserNumericType& src,
                  const ast::BinaryExactUserNumericType& dest) const {
    return src == dest;
  }

  bool operator()(const ast::ScaleNumericType& src,
                  const ast::ScaleNumericType& dest) const {
    return src == dest;
  }

  bool operator()(const ast::GraphReferenceValueType& src,
                  const ast::GraphReferenceValueType& dest) const {
    return src == dest;
  }

  bool operator()(const ast::BindingTableReferenceValueType& src,
                  const ast::BindingTableReferenceValueType& dest) const {
    return src == dest;
  }

  bool operator()(const ast::NodeReferenceValueType& src,
                  const ast::NodeReferenceValueType& dest) const {
    return src == dest;
  }

  bool operator()(const ast::EdgeReferenceValueType& src,
                  const ast::EdgeReferenceValueType& dest) const {
    return src == dest;
  }

  bool operator()(const ast::ValueType::List& src,
                  const ast::ValueType::List& dest) const {
    return Visit(src.valueType ? **src.valueType
                               : MakeValueType(ast::SimplePredefinedType::Any),
                 dest.valueType
                     ? **dest.valueType
                     : MakeValueType(ast::SimplePredefinedType::Any));
  }

  bool operator()(const ast::RecordType& src,
                  const ast::RecordType& dest) const {
    return src == dest;
  }

  bool operator()(const ast::ValueType::Union& src,
                  const ast::ValueType::Union& dest) const {
    return (*this)(src, dest, nullptr);
  }

  template <typename Dest>
  bool operator()(const ast::ValueType::Union& src,
                  const Dest& dest,
                  void* = nullptr) const {
    for (const auto& t : src.types) {
      if (std::visit(
              [&](const auto& typeOption) { return (*this)(typeOption, dest); },
              MaybeReplace(*t).typeOption)) {
        return true;
      }
    }
    return false;
  }

  template <typename Src>
  bool operator()(const Src& src, const ast::ValueType::Union& dest) const {
    for (const auto& t : dest.types) {
      if (std::visit(
              [&](const auto& typeOption) { return (*this)(src, typeOption); },
              MaybeReplace(*t).typeOption)) {
        return true;
      }
    }
    return false;
  }

 private:
  const ast::ValueType::Union propertyUnionType;
};

}  // namespace

bool SyntaxAnalyzer::IsCastableTo(ast::ValueType src,
                                  const ast::ValueType& dest) const {
  ast::ValueType::Union propertyUnionType;
  for (auto& t : config_.supportedPropertyTypes) {
    propertyUnionType.types.emplace_back() = t;
  }

  return IsCastableVisitor{propertyUnionType}.Visit(src, dest);
}

SyntaxAnalyzer::ExecutionContext SyntaxAnalyzer::ExecutionContext::MakeChild()
    const {
  ExecutionContext newContext = *this;
  newContext.workingTable.clear();
  return newContext;
}

SyntaxAnalyzer::ExecutionContext SyntaxAnalyzer::ExecutionContext::MakeCopy()
    const {
  return *this;
}

SyntaxAnalyzer::ExecutionContext SyntaxAnalyzer::ExecutionContext::MakeAmended()
    const {
  ExecutionContext newContext = *this;
  newContext.workingTable.clear();
  newContext.workingRecord.insert(newContext.workingRecord.end(),
                                  workingTable.begin(), workingTable.end());
  return newContext;
}

SyntaxAnalyzer::ExecutionContext
SyntaxAnalyzer::ExecutionContext::MakeCopyForProcedureBody() const {
  ExecutionContext newContext = *this;
  newContext.isInsideAggregatingValueExpression = false;
  newContext.isInsideValueExpressionNotAggregating = false;
  return newContext;
}

}  // namespace gql