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

#include <unordered_map>

#include "common/formatted_error.h"
#include "gql/ast/algorithm.h"
#include "gql/rewrite.h"
#include "type_helpers.h"

namespace gql {

SyntaxAnalyzer::OptBindingTableType SyntaxAnalyzer::Process(
    ast::GQLProgram& program) {
  if (config_.rewriteExistsPredicate) {
    rewrite::RewriteExistsPredicate(program);
  }
  if (config_.rewritePathPatternPrefix) {
    rewrite::RewritePathPatternPrefix(program);
  }
  if (config_.rewriteSimplifiedPathPattern) {
    rewrite::RewriteSimplifiedPathPattern(program);
  }
  if (config_.rewriteElementPatterns) {
    rewrite::RewriteElementPatterns(program);
  }
  if (config_.rewriteElementPatternWhereClause) {
    rewrite::RewriteElementPatternWhereClause(program);
  }
  if (config_.rewriteElementPropertyPredicate) {
    rewrite::RewriteElementPropertyPredicate(program);
  }

  OptBindingTableType outcome;
  ExecutionContext context;
  ast::variant_switch(
      program.programActivity,
      [&](ast::SessionActivity& value) {
        for (auto& setCmd : value.setCommands) {
          ast::variant_switch(
              setCmd,
              [&](const ast::SessionSetSchemaClause&) {
                // TODO: Implement
              },
              [&](ast::SessionSetGraphClause& value) {
                ThrowIfFeatureNotSupported(standard::Feature::GS01, value);

                Process(value.graph, context);
              },
              [&](const ast::SessionSetTimeZoneClause& value) {
                ThrowIfFeatureNotSupported(standard::Feature::GS15, value);
              },
              [&](ast::SessionSetParameterClause& value) {
                Process(value, context);
              });
        }
        for (const auto& resetCmd : value.resetCommands) {
          ast::variant_switch(
              resetCmd.arguments,
              [&](ast::SessionResetArguments value) {
                switch (value) {
                  case ast::SessionResetArguments::Characteristics:
                  case ast::SessionResetArguments::Parameters:
                    ThrowIfFeatureNotSupported(standard::Feature::GS04,
                                               resetCmd);
                    break;
                  case ast::SessionResetArguments::Schema:
                    ThrowIfFeatureNotSupported(standard::Feature::GS05,
                                               resetCmd);
                    break;
                  case ast::SessionResetArguments::Graph:
                    ThrowIfFeatureNotSupported(standard::Feature::GS06,
                                               resetCmd);
                    break;
                  case ast::SessionResetArguments::TimeZone:
                    ThrowIfFeatureNotSupported(standard::Feature::GS07,
                                               resetCmd);
                    break;
                }
              },
              [&](const ast::GeneralParameterReference& value) {
                ThrowIfFeatureNotSupported(standard::Feature::GS16, value);
              });
        }
      },
      [&](ast::TransactionActivity& transaction) {
        // BEGIN feature checks
        if (transaction.startCmd) {
          ThrowIfFeatureNotSupported(standard::Feature::GT01,
                                     *transaction.startCmd);
          if (transaction.startCmd->accessMode) {
            ThrowIfFeatureNotSupported(standard::Feature::GT02, transaction);
          }
        }
        if (transaction.endCmd) {
          ThrowIfFeatureNotSupported(standard::Feature::GT01, transaction);
        }
        // END feature checks

        if (transaction.startCmd && transaction.startCmd->accessMode &&
            *transaction.startCmd->accessMode ==
                ast::TransactionAccessMode::ReadOnly) {
          isInsideReadOnlyTransaction_ = true;
        }
        if (transaction.procedure) {
          outcome =
              Process(*transaction.procedure, CallProcedureKind::Any, context);
        }
      });
  if (program.sessionCloseCommand) {
    outcome.reset();
  }
  return outcome;
}

void SyntaxAnalyzer::Process(ast::SessionSetParameterClause& value,
                             ExecutionContext& context) {
  // 7.1 Syntax Rule 6
  std::unordered_map<std::string, ast::ValueType> parameters;
  auto setParameter = [&](const std::string& name, const ast::ValueType& type) {
    auto [it, inserted] = parameters.emplace(name, type);
    if (!inserted) {
      AssertAssignableTo(type, it->second);
      it->second = type;
    }
  };
  ast::variant_switch(
      value,
      [&](ast::SessionSetGraphParameterClause& value) {
        ThrowIfFeatureNotSupported(standard::Feature::GS01, value);

        auto implicitType = Process(value.initializer.initializer, context);
        if (value.initializer.type) {
          auto explicitType = Process(*value.initializer.type, context);
          AssertAssignableTo(implicitType, explicitType);
          setParameter(value.paramName.param.name, explicitType);
        } else {
          setParameter(value.paramName.param.name, implicitType);
        }
      },
      [&](ast::SessionSetBindingTableParameterClause& value) {
        // BEGIN feature checks
        ThrowIfFeatureNotSupported(standard::Feature::GS02, value);
        if (std::holds_alternative<ast::ProcedureBodyPtr>(
                value.initializer.initializer.option)) {
          ThrowIfFeatureNotSupported(standard::Feature::GS10,
                                     value.initializer.initializer);
        }
        // END feature checks

        auto implicitType = Process(value.initializer.initializer, context);
        if (value.initializer.type) {
          auto explicitType =
              Process(*value.initializer.type, value.initializer, context);
          AssertAssignableTo(implicitType, explicitType);
          setParameter(value.paramName.param.name, explicitType);
        } else {
          setParameter(value.paramName.param.name, implicitType);
        }
      },
      [&](ast::SessionSetValueParameterClause& value) {
        // BEGIN feature checks
        ThrowIfFeatureNotSupported(standard::Feature::GS03, value);
        if (std::holds_alternative<ast::ProcedureBodyPtr>(
                value.initializer.initializer->option)) {
          ThrowIfFeatureNotSupported(standard::Feature::GS11,
                                     *value.initializer.initializer);
        }
        // END feature checks

        auto implicitType =
            ProcessValueExpression(*value.initializer.initializer, context);
        if (value.initializer.type) {
          auto explicitType = Process(*value.initializer.type, context);
          AssertAssignableTo(implicitType, explicitType);
          setParameter(value.paramName.param.name, explicitType);
        } else {
          setParameter(value.paramName.param.name, implicitType);
        }
      });
}

}  // namespace gql
