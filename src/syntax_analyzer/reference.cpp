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

#include "common/formatted_error.h"
#include "gql/ast/algorithm.h"
#include "type_helpers.h"

namespace gql {

void SyntaxAnalyzer::Process(
    const ast::ReferenceParameterSpecification& statement,
    const ExecutionContext&) const {
  ThrowIfFeatureNotSupported(standard::Feature::GE08, statement);
}

ast::ValueType SyntaxAnalyzer::ProcessBindingVariableReference(
    const ast::BindingVariableBase& var,
    const ast::Node& node,
    const ExecutionContext& context) const {
  auto* field = HasField(context.workingRecord, var.name);
  if (!field) {
    auto reason = context.inaccessibleVariables.find(var.name);
    if (reason != context.inaccessibleVariables.end()) {
      switch (reason->second) {
        case ExecutionContext::InaccessibleReason::
            ReferenceToTheAdjacentUnionOperand:
          throw FormattedError(
              node, ErrorCode::E0051,
              "Cannot reference variable in the adjacent union operand");
        case ExecutionContext::InaccessibleReason::
            NonLocalReferenceWithGroupDegreeOfReference:
          throw FormattedError(
              node, ErrorCode::E0052,
              "Cannot reference non-local variable with group degree of "
              "reference");
        case ExecutionContext::InaccessibleReason::
            ReferenceFromSelectivePathPattern:
          throw FormattedError(
              node, ErrorCode::E0053,
              "Cannot reference variables in other path patterns from "
              "selective path pattern");
      }
    }
    if (HasField(context.workingTable, var.name)) {
      throw FormattedError(
          node, ErrorCode::E0113,
          "There is no field \"{0}\" in current working "
          "record. Probably, aggregating expression is missing",
          var.name);
    } else {
      throw FormattedError(node, ErrorCode::E0054,
                           "Reference to unknown field \"{0}\"", var.name);
    }
  }
  return *field->type;
}

ast::ValueType SyntaxAnalyzer::Process(const ast::BindingVariableReference& var,
                                       const ExecutionContext& context) const {
  return ProcessBindingVariableReference(var, var, context);
}

ast::ValueType SyntaxAnalyzer::ProcessSingleton(
    const ast::ElementVariableReference& var,
    const ExecutionContext& context) const {
  auto type = ProcessBindingVariableReference(var, var, context);
  if (auto* listType = std::get_if<ast::ValueType::List>(&type.typeOption)) {
    throw FormattedError(var, ErrorCode::E0055,
                         "Expected singleton degree of reference");
  }
  AssertGraphElementReferenceType(type, var);
  return type;
}

}  // namespace gql