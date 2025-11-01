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

SyntaxAnalyzer::OptBindingTableType SyntaxAnalyzer::Process(
    ast::CompositeQueryStatement& stmt,
    ExecutionContext& context) {
  if (stmt.queries.size() > 1) {
    ast::variant_switch(
        stmt.conjunction,
        [&](const ast::SetOperator& statement) {
          switch (statement.kind) {
            case ast::SetOperator::Kind::UNION:
              ThrowIfFeatureNotSupported(standard::Feature::GQ03, statement);
              break;
            case ast::SetOperator::Kind::EXCEPT:
              ThrowIfFeatureNotSupported(
                  statement.quantifier == ast::SetQuantifier::DISTINCT
                      ? standard::Feature::GQ04
                      : standard::Feature::GQ05,
                  statement);
              break;
            case ast::SetOperator::Kind::INTERSECT:
              ThrowIfFeatureNotSupported(
                  statement.quantifier == ast::SetQuantifier::DISTINCT
                      ? standard::Feature::GQ06
                      : standard::Feature::GQ07,
                  statement);
              break;
          }
        },
        [&](const ast::OtherwiseConjunctionValue&) {
          ThrowIfFeatureNotSupported(standard::Feature::GQ02, stmt);
        });
  }

  bool requireComparableCols = false;
  if (stmt.queries.size() > 1) {
    if (auto* setOp = std::get_if<ast::SetOperator>(&stmt.conjunction)) {
      if ((setOp->kind == ast::SetOperator::Kind::UNION &&
           setOp->quantifier == ast::SetQuantifier::DISTINCT) ||
          setOp->kind == ast::SetOperator::Kind::EXCEPT ||
          setOp->kind == ast::SetOperator::Kind::INTERSECT) {
        requireComparableCols = true;
      }
    }
  }

  BindingTableType combinedTable;
  bool firstQuery = true;
  for (auto& query : stmt.queries) {
    auto childContext = context.MakeCopy();
    // TODO: What about childContext?
    Process(query, context);
    if (firstQuery) {
      combinedTable = context.workingTable;
      firstQuery = false;
    } else {
      AssertColumnNameEqual(combinedTable, context.workingTable);
      combinedTable = CombineColumns(combinedTable, context.workingTable);
      if (requireComparableCols) {
        // 14.2 Syntax Rule 10.a.vii.1
        AssertComparableTypesInFields(combinedTable, stmt);
      }
    }
  }

  context.workingTable = combinedTable;
  return combinedTable;
}

void SyntaxAnalyzer::Process(ast::CompositeQueryPrimary& query,
                             ExecutionContext& context) {
  ast::variant_switch(
      query,
      [&](ast::LinearQueryStatementOption& statement) {
        ast::variant_switch(
            statement.statements,
            [&](ast::SimpleLinearQueryStatement& statement) {
              for (auto& simpleStmt : statement) {
                Process(simpleStmt, context);
              }
            },
            [&](std::vector<ast::FocusedLinearQueryStatementPart>& parts) {
              ThrowIfFeatureNotSupported(standard::Feature::GQ01, statement);

              for (auto& part : parts) {
                ProcessFallback(part.useGraph, context);
                for (auto& simpleStmt : part.statements) {
                  Process(simpleStmt, context);
                }
              }
            });
        Process(statement.result, context);
      },
      [&](ast::NestedQuerySpecification& statement) {
        if (statement.useGraph) {
          ThrowIfFeatureNotSupported(standard::Feature::GQ01,
                                     *statement.useGraph);
        }

        if (statement.useGraph) {
          ProcessFallback(*statement.useGraph, context);
        }
        Process(statement.procedure, CallProcedureKind::Query, context);
      },
      [&](ast::FocusedPrimitiveResultStatement& statement) {
        ThrowIfFeatureNotSupported(standard::Feature::GQ01, statement);

        ProcessFallback(statement.useGraph, context);
        Process(statement.result, context);
      },
      [&](const ast::SelectStatement& statement) {
        StatementRewriteException exc;
        exc.statements = Rewrite(statement, context);
        throw exc;
      });
}

void SyntaxAnalyzer::Process(ast::SimpleQueryStatement& simpleStmt,
                             ExecutionContext& context) {
  ast::variant_switch(
      simpleStmt,
      [&](ast::MatchStatement& statement) {
        ast::variant_switch(
            statement,
            [&](ast::SimpleMatchStatement& statement) {
              Process(statement, context);
            },
            [&](const ast::OptionalMatchStatement& optMatch) {
              if (optMatch.statements->statements.size() > 1) {
                ThrowIfFeatureNotSupported(standard::Feature::GQ21, optMatch);
              }

              ast::SimpleQueryStatement newSimpleStmt =
                  Rewrite(optMatch, context);
              Process(newSimpleStmt, context);
              if (config_.rewriteOptionalMatchStatement) {
                simpleStmt = std::move(newSimpleStmt);
              }
            });
      },
      [&](const ast::LetStatement& statement) {
        ThrowIfFeatureNotSupported(standard::Feature::GQ09, statement);

        ast::SimpleQueryStatement newSimpleStmt = Rewrite(statement, context);
        Process(newSimpleStmt, context);
        if (config_.rewriteLetStatement) {
          simpleStmt = std::move(newSimpleStmt);
        }
      },
      [&](ast::ForStatement& statement) { Process(statement, context); },
      [&](ast::FilterStatement& statement) {
        ThrowIfFeatureNotSupported(standard::Feature::GQ08, statement);

        auto childContext = context.MakeAmended();
        ProcessBooleanValueExpression(*statement.condition, childContext);
      },
      [&](ast::OrderByAndPageStatement& statement) {
        Process(statement, context);
      },
      [&](ast::CallProcedureStatement& statement) {
        Process(statement, CallProcedureKind::Query, context);
      });
}

ast::CallProcedureStatement SyntaxAnalyzer::Rewrite(
    const ast::LetStatement& statement,
    const ExecutionContext& context) const {
  ast::CallProcedureStatement callProc;
  auto& proc = callProc.call.emplace<ast::InlineProcedureCall>();
  auto& varScope = proc.vars.emplace();
  for (auto& f : context.workingTable) {
    varScope.emplace_back().name = f.name.name;
  }
  for (auto& f : context.workingRecord) {
    varScope.emplace_back().name = f.name.name;
  }
  auto& procBody = *proc.spec;
  ast::ReturnItemList& returnItems =
      procBody.statements.firstStatement->option
          .emplace<ast::CompositeQueryStatement>()
          .queries.emplace_back()
          .emplace<ast::LinearQueryStatementOption>()
          .result.option.emplace<ast::PrimitiveResultStatement::Return>()
          .stmt.items.emplace();
  for (auto& letVar : statement.definitions) {
    auto& procVar =
        procBody.vars.emplace_back().emplace<ast::ValueVariableDefinition>();
    procVar.var.name = letVar.var.name;
    procVar.initializer.initializer = letVar.expr;
    procVar.initializer.type = letVar.type;
    returnItems.emplace_back()
        .aggregate.option.emplace<ast::BindingVariableReference>()
        .name = letVar.var.name;
  }
  return callProc;
}

void SyntaxAnalyzer::Process(ast::ForStatement& statement,
                             ExecutionContext& context) {
  if (statement.ordinalityOrOffset) {
    if (statement.ordinalityOrOffset->isOrdinality) {
      ThrowIfFeatureNotSupported(standard::Feature::GQ11, statement);
    } else {
      ThrowIfFeatureNotSupported(standard::Feature::GQ24, statement);
    }
  }

  BindingTableType outWorkingTable = context.workingRecord;
  if (statement.ordinalityOrOffset) {
    if (statement.ordinalityOrOffset->var.name == statement.alias.name) {
      throw FormattedError(statement.ordinalityOrOffset->var, ErrorCode::E0056,
                           "FOR and WITH variables cannot have same name");
    }
    if (HasField(context.workingRecord,
                 statement.ordinalityOrOffset->var.name)) {
      throw FormattedError(
          statement.ordinalityOrOffset->var, ErrorCode::E0057,
          "WITH variable \"{0}\" cannot reuse name of incoming variable",
          statement.ordinalityOrOffset->var.name);
    }

    auto& newField = outWorkingTable.emplace_back();
    newField.name.name = statement.ordinalityOrOffset->var.name;
    newField.type = MakeValueType(ast::SimpleNumericType::UInt64);
  }

  if (HasField(context.workingRecord, statement.alias.name)) {
    throw FormattedError(
        statement.alias, ErrorCode::E0058,
        "FOR variable \"{0}\" cannot reuse name of incoming variable",
        statement.alias.name);
  }

  auto childContext = context.MakeAmended();
  auto sourceType = ProcessValueExpression(statement.source, childContext);
  if (auto* tableType = std::get_if<ast::BindingTableReferenceValueType>(
          &sourceType.typeOption)) {
    ThrowIfFeatureNotSupported(standard::Feature::GQ23, statement);

    auto& newField = outWorkingTable.emplace_back();
    newField.name.name = statement.alias.name;
    ast::RecordType recordType;
    recordType.fields = tableType->type;
    newField.type = MakeValueType(recordType);
  } else if (auto* listType =
                 std::get_if<ast::ValueType::List>(&sourceType.typeOption)) {
    ThrowIfFeatureNotSupported(standard::Feature::GQ10, statement);

    auto& newField = outWorkingTable.emplace_back();
    newField.name.name = statement.alias.name;
    if (listType->valueType) {
      newField.type = *listType->valueType;
    } else {
      newField.type = MakeValueType(ast::SimplePredefinedType::Any);
    }
  } else {
    throw FormattedError(statement.source, ErrorCode::E0059,
                         "List or binding table reference expected");
  }
  context.workingTable = outWorkingTable;
}

}  // namespace gql