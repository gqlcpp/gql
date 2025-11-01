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

namespace gql {

void SyntaxAnalyzer::Process(ast::LinearCatalogModifyingStatement& statement,
                             ExecutionContext& context) {
  if (isInsideReadOnlyTransaction_) {
    throw FormattedError(
        statement, ErrorCode::E0010,
        "Catalog-modifying statement cannot be executed inside read-only "
        "transaction");
  }

  for (auto& stmt : statement.statements) {
    ast::variant_switch(
        stmt,
        [&](const ast::CreateSchemaStatement& statement) {
          ThrowIfFeatureNotSupported(standard::Feature::GC01, statement);
          if (statement.ifNotExists) {
            ThrowIfFeatureNotSupported(standard::Feature::GC02, statement);
          }

          ProcessFallback(statement, context);
        },
        [&](const ast::DropSchemaStatement& statement) {
          ThrowIfFeatureNotSupported(standard::Feature::GC01, statement);
          if (statement.ifExists) {
            ThrowIfFeatureNotSupported(standard::Feature::GC02, statement);
          }

          ProcessFallback(statement, context);
        },
        [&](ast::CreateGraphStatement& statement) {
          ThrowIfFeatureNotSupported(standard::Feature::GC04, statement);
          if (statement.createType ==
              ast::CreateGraphStatement::CreateType::IfNotExists) {
            ThrowIfFeatureNotSupported(standard::Feature::GC05, statement);
          }
          if (statement.graphType) {
            ThrowIfFeatureNotSupported(standard::Feature::GG02, statement);
            ast::variant_switch(
                *statement.graphType,
                [&](const ast::GraphExpression& statement) {
                  ThrowIfFeatureNotSupported(standard::Feature::GG04,
                                             statement);
                },
                [&](const ast::GraphTypeReference& statement) {},
                [&](const ast::GraphTypeSpecificationBody& statement) {
                  ThrowIfFeatureNotSupported(standard::Feature::GG03,
                                             statement);
                });
          } else {
            ThrowIfFeatureNotSupported(standard::Feature::GG01, statement);
          }
          if (statement.source) {
            ThrowIfFeatureNotSupported(standard::Feature::GG05, statement);
          }

          if (statement.graphType) {
            ProcessFallback(*statement.graphType, context);
          }
          if (statement.source) {
            Process(*statement.source, context);
          }
        },
        [&](const ast::DropGraphStatement& statement) {
          ThrowIfFeatureNotSupported(standard::Feature::GC04, statement);
          if (statement.ifExists) {
            ThrowIfFeatureNotSupported(standard::Feature::GC05, statement);
          }

          ProcessFallback(statement, context);
        },
        [&](ast::CreateGraphTypeStatement& statement) {
          ThrowIfFeatureNotSupported(standard::Feature::GC02, statement);
          if (statement.createType ==
              ast::CreateGraphTypeStatement::CreateType::IfNotExists) {
            ThrowIfFeatureNotSupported(standard::Feature::GC05, statement);
          }

          ProcessFallback(statement.source, context);
        },
        [&](const ast::DropGraphTypeStatement& statement) {
          ThrowIfFeatureNotSupported(standard::Feature::GC02, statement);
          if (statement.ifExists) {
            ThrowIfFeatureNotSupported(standard::Feature::GC03, statement);
          }

          ProcessFallback(statement, context);
        },
        [&](ast::CallProcedureStatement& statement) {
          Process(statement, CallProcedureKind::CatalogModifying, context);
        });
  }
}

}  // namespace gql