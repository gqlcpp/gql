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
#include <unordered_set>

#include "common/formatted_error.h"
#include "gql/ast/algorithm.h"
#include "type_helpers.h"

namespace gql {

static void CheckLinearQueryStatements(const ast::StatementBlock& statements) {
  int linearQueryStatementCount = 0;
  int selectStatementCount = 0;
  int withPrimitiveResultStatementCount = 0;
  const ast::Node* nonSelectStatement = nullptr;
  const ast::Node* withoutPrimitiveResultStatement = nullptr;
  auto checkStatement = [&](const ast::Statement& statement) {
    ast::variant_switch(
        statement.option, [](const auto&) {},
        [&](const ast::CompositeQueryStatement& stmt) {
          for (auto& query : stmt.queries) {
            linearQueryStatementCount++;

            ast::variant_switch(
                query,
                [&](const ast::LinearQueryStatementOption& stmt) {
                  nonSelectStatement = &stmt;
                  withPrimitiveResultStatementCount++;
                },
                [&](const ast::NestedQuerySpecification& stmt) {
                  nonSelectStatement = &stmt;
                  withoutPrimitiveResultStatement = &stmt;
                },
                [&](const ast::FocusedPrimitiveResultStatement& stmt) {
                  nonSelectStatement = &stmt;
                  withPrimitiveResultStatementCount++;
                },
                [&](const ast::SelectStatement& stmt) {
                  selectStatementCount++;
                  withoutPrimitiveResultStatement = &stmt;
                });
          }
        });
  };

  // 9.2 Syntax Rule 6
  if (selectStatementCount > 0 &&
      linearQueryStatementCount != selectStatementCount) {
    throw FormattedError(
        *nonSelectStatement, ErrorCode::E0068,
        "If procedure contains SELECT statement then all queries must be "
        "SELECT queries");
  }

  // 9.2 Syntax Rule 7
  if (withPrimitiveResultStatementCount > 0 &&
      linearQueryStatementCount != withPrimitiveResultStatementCount) {
    throw FormattedError(
        *withoutPrimitiveResultStatement, ErrorCode::E0069,
        "If procedure contains primitive result statement then all queries "
        "must have primitive result statements");
  }

  checkStatement(*statements.firstStatement);
  for (const auto& stmt : statements.nextStatements) {
    checkStatement(*stmt.statement);
  }
}

// 9.1 Syntax Rule 4b
// static
SyntaxAnalyzer::CallProcedureKind SyntaxAnalyzer::GetProcedureKind(
    const ast::StatementBlock& statements,
    const ast::Node** definingStatement) {
  const ast::Node* catalogModifyingStatement = nullptr;
  const ast::Node* dataModifyingStatement = nullptr;
  auto checkStatement = [&](const ast::Statement& statement) {
    ast::variant_switch(
        statement.option,
        [&](const ast::LinearCatalogModifyingStatement& statement) {
          if (!catalogModifyingStatement) {
            catalogModifyingStatement = &statement;
          }
        },
        [&](const ast::LinearDataModifyingStatement& statement) {
          if (!dataModifyingStatement) {
            dataModifyingStatement = &statement;
          }
        },
        [&](const ast::CompositeQueryStatement&) {});
  };
  checkStatement(*statements.firstStatement);
  for (const auto& stmt : statements.nextStatements) {
    checkStatement(*stmt.statement);
  }
  if (catalogModifyingStatement) {
    *definingStatement = catalogModifyingStatement;
    return CallProcedureKind::CatalogModifying;
  }
  if (dataModifyingStatement) {
    *definingStatement = dataModifyingStatement;
    return CallProcedureKind::DataModifying;
  }
  *definingStatement = statements.firstStatement.get();
  return CallProcedureKind::Query;
}

SyntaxAnalyzer::OptBindingTableType SyntaxAnalyzer::Process(
    ast::ProcedureBody& procDef,
    CallProcedureKind kind,
    ExecutionContext& context) {
  // BEGIN feature checks
  if (procDef.schema) {
    ThrowIfFeatureNotSupported(standard::Feature::GP16, *procDef.schema);
  }
  if (!procDef.statements.nextStatements.empty()) {
    ThrowIfFeatureNotSupported(
        standard::Feature::GQ20,
        *procDef.statements.nextStatements.front().statement);
  }
  if (!procDef.vars.empty()) {
    ThrowIfFeatureNotSupported(standard::Feature::GP17, procDef);
  }
  // END feature checks

  Process(procDef.vars, context);

  const ast::Node* procDefiningStatement = nullptr;
  auto detectedKind =
      GetProcedureKind(procDef.statements, &procDefiningStatement);
  if (kind != CallProcedureKind::Any && detectedKind != kind) {
    throw FormattedError(
        *procDefiningStatement, ErrorCode::E0070,
        "Unexpected type of statement in the current type of procedure");
  }

  if (detectedKind == CallProcedureKind::CatalogModifying &&
      !procDef.statements.nextStatements.empty()) {
    // 9.2 Syntax Rule 4
    throw FormattedError(
        *procDef.statements.nextStatements.front().statement, ErrorCode::E0071,
        "Multiple catalog-modifying statements are not allowed in a procedure");
  }

  CheckLinearQueryStatements(procDef.statements);

  OptBindingTableType resultType;
  while (true) {
    try {
      auto contextClone = context.MakeCopy();
      resultType = Process(*procDef.statements.firstStatement, detectedKind,
                           contextClone);
      context = std::move(contextClone);
      break;
    } catch (StatementRewriteException e) {
      procDef.statements.firstStatement = std::move(e.statements.front());
      e.statements.erase(e.statements.begin());
      for (auto& stmt : e.statements) {
        auto& newStmt = *procDef.statements.nextStatements.emplace(
            procDef.statements.nextStatements.begin());
        newStmt.statement = std::move(stmt);
      }
    }
  }
  if (resultType) {
    context.workingTable = *resultType;
  } else {
    context.workingTable.clear();
  }
  for (auto stmtIt = procDef.statements.nextStatements.begin();
       stmtIt != procDef.statements.nextStatements.end(); stmtIt++) {
    if (!stmtIt->yield.empty()) {
      context.workingTable = Process(stmtIt->yield, context);
    }

    while (true) {
      try {
        auto contextClone = context.MakeCopy();
        resultType = Process(*stmtIt->statement, detectedKind, contextClone);
        context = std::move(contextClone);
        break;
      } catch (StatementRewriteException e) {
        assert(stmtIt->yield.empty());

        std::vector<ast::NextStatement> newNextStatements;
        for (auto& stmt : e.statements) {
          newNextStatements.emplace_back().statement = std::move(stmt);
        }
        stmtIt = procDef.statements.nextStatements.erase(stmtIt);
        stmtIt = procDef.statements.nextStatements.insert(
            stmtIt, newNextStatements.begin(), newNextStatements.end());
      }
    }
    if (resultType) {
      context.workingTable = *resultType;
    } else {
      context.workingTable.clear();
    }
  }
  return resultType;
}

void SyntaxAnalyzer::Process(std::vector<ast::BindingVariableDefinition>& vars,
                             ExecutionContext& parentContext) {
  auto context = parentContext.MakeChild();
  for (auto& var : vars) {
    ast::FieldType newField;
    ast::variant_switch(
        var,
        [&](ast::GraphVariableDefinition& statement) {
          // BEGIN feature checks
          ThrowIfFeatureNotSupported(standard::Feature::GP11, statement);
          if (auto* valueExpr = std::get_if<ast::ValueExpressionPtr>(
                  &statement.initializer.initializer.option)) {
            if (std::holds_alternative<ast::ProcedureBodyPtr>(
                    (*valueExpr)->option)) {
              ThrowIfFeatureNotSupported(standard::Feature::GP12,
                                         statement.initializer.initializer);
            }
          }
          // END feature checks

          if (HasField(context.workingRecord, statement.var.name)) {
            throw FormattedError(statement.var, ErrorCode::E0072,
                                 "Duplicate variable name");
          }

          auto implicitType =
              Process(statement.initializer.initializer, context);
          if (statement.initializer.type) {
            newField.type = Process(*statement.initializer.type, context);
            AssertAssignableTo(implicitType, *newField.type);
          } else {
            newField.type = implicitType;
          }
          newField.name.name = statement.var.name;
        },
        [&](ast::BindingTableVariableDefinition& statement) {
          // BEGIN feature checks
          ThrowIfFeatureNotSupported(standard::Feature::GP08, statement);
          if (std::holds_alternative<ast::ProcedureBodyPtr>(
                  statement.initializer.initializer.option)) {
            ThrowIfFeatureNotSupported(standard::Feature::GP10,
                                       statement.initializer.initializer);
          }
          // END feature checks

          if (HasField(context.workingRecord, statement.var.name)) {
            throw FormattedError(statement.var, ErrorCode::E0073,
                                 "Duplicate variable name");
          }

          auto implicitType =
              Process(statement.initializer.initializer, context);
          if (statement.initializer.type) {
            newField.type = Process(*statement.initializer.type,
                                    statement.initializer, context);
            AssertAssignableTo(implicitType, *newField.type);
          } else {
            newField.type = implicitType;
          }
          newField.name.name = statement.var.name;
        },
        [&](ast::ValueVariableDefinition& statement) {
          // BEGIN feature checks
          ThrowIfFeatureNotSupported(standard::Feature::GP05, statement);
          if (std::holds_alternative<ast::ProcedureBodyPtr>(
                  statement.initializer.initializer->option)) {
            ThrowIfFeatureNotSupported(standard::Feature::GP07,
                                       *statement.initializer.initializer);
          }
          // END feature checks

          if (HasField(context.workingRecord, statement.var.name)) {
            throw FormattedError(statement.var, ErrorCode::E0074,
                                 "Duplicate variable name");
          }

          auto implicitType = ProcessValueExpression(
              *statement.initializer.initializer, context);
          if (statement.initializer.type) {
            newField.type = Process(*statement.initializer.type, context);
            AssertAssignableTo(implicitType, *newField.type);
          } else {
            newField.type = implicitType;
          }
          newField.name.name = statement.var.name;
        });

    context.workingRecord.push_back(newField);
  }
  parentContext.workingRecord = std::move(context.workingRecord);
}

SyntaxAnalyzer::OptBindingTableType SyntaxAnalyzer::Process(
    ast::Statement& statement,
    CallProcedureKind kind,
    ExecutionContext& context) {
  return ast::variant_switch(
      statement.option,
      [&](ast::LinearCatalogModifyingStatement& statement)
          -> OptBindingTableType {
        switch (kind) {
          case CallProcedureKind::CatalogModifying:
            break;
          default:
            throw FormattedError(statement, ErrorCode::E0075,
                                 "Unexpected catalog-modifying statement");
        }
        Process(statement, context);
        return {};
      },
      [&](ast::LinearDataModifyingStatement& statement) -> OptBindingTableType {
        switch (kind) {
          case CallProcedureKind::DataModifying:
            break;
          default:
            throw FormattedError(statement, ErrorCode::E0076,
                                 "Unexpected data-modifying statement");
        }
        Process(statement, context);
        return {};
      },
      [&](ast::CompositeQueryStatement& statement) -> OptBindingTableType {
        // Execute composite query statement
        switch (kind) {
          case CallProcedureKind::DataModifying:
          case CallProcedureKind::Query:
            break;
          default:
            throw FormattedError(statement, ErrorCode::E0077,
                                 "Unexpected query statement");
        }

        return Process(statement, context);
      });
}

void SyntaxAnalyzer::Process(ast::CallProcedureStatement& callProc,
                             CallProcedureKind kind,
                             ExecutionContext& context) {
  auto childContext = context.MakeAmended();
  OptBindingTableType procResultType;
  ast::variant_switch(
      callProc.call,
      [&](ast::InlineProcedureCall& statement) {
        procResultType = Process(statement, kind, childContext);
      },
      [&](const ast::NamedProcedureCall& statement) {
        ThrowIfFeatureNotSupported(standard::Feature::GP04, statement);

        throw FormattedError(statement, ErrorCode::E0100,
                             "No named procedures defined");
      });

  if (procResultType) {
    for (auto& field : *procResultType) {
      // 15.1 Syntax Rule 9
      if (HasField(context.workingTable, field.name.name)) {
        throw FormattedError(callProc, ErrorCode::E0029,
                             "Procedure returns \"{0}\" field that duplicates "
                             "working table field name",
                             field.name.name);
      }
    }
    context.workingTable.insert(context.workingTable.end(),
                                procResultType->begin(), procResultType->end());
  }
}

SyntaxAnalyzer::OptBindingTableType SyntaxAnalyzer::Process(
    ast::InlineProcedureCall& statement,
    CallProcedureKind kind,
    ExecutionContext& context) {
  // BEGIN feature checks
  ThrowIfFeatureNotSupported(standard::Feature::GP01, statement);
  if (!statement.vars) {
    ThrowIfFeatureNotSupported(standard::Feature::GP02, statement);
  } else {
    ThrowIfFeatureNotSupported(standard::Feature::GP03, statement);
  }
  // END feature checks

  if (statement.vars) {
    std::unordered_set<std::string> referencedFields;
    ast::FieldTypeList newWorkingRecord;
    for (auto& item : *statement.vars) {
      if (!referencedFields.insert(item.name).second) {
        throw FormattedError(item, ErrorCode::E0078,
                             "Duplicate field name \"{0}\" in procedure call",
                             item.name);
      }
      if (auto* f = HasField(context.workingRecord, item.name)) {
        newWorkingRecord.push_back(*f);
      } else {
        throw FormattedError(item, ErrorCode::E0079,
                             "Field \"{0}\" not found in working record",
                             item.name);
      }
    }
    context.workingRecord = std::move(newWorkingRecord);
  }

  return Process(*statement.spec, kind, context);
}

SyntaxAnalyzer::BindingTableType SyntaxAnalyzer::Process(
    const ast::YieldClause& yield,
    ExecutionContext& context) {
  // TODO: Check NOTE 258 in 16.14
  std::unordered_set<std::string> referencedFields;
  ast::FieldTypeList newWorkingTable;
  for (const auto& item : yield) {
    if (!referencedFields.insert(item.name.name).second) {
      throw FormattedError(item, ErrorCode::E0080,
                           "Duplicate field name \"{0}\" in YIELD clause",
                           item.name.name);
    }
    auto* referencedField = HasField(context.workingTable, item.name.name);
    if (!referencedField) {
      throw FormattedError(item, ErrorCode::E0081,
                           "Field \"{0}\" not found in working table",
                           item.name.name);
    }

    auto& newField = newWorkingTable.emplace_back();
    newField.name.name = item.alias ? item.alias->name : item.name.name;
    newField.type = *referencedField->type;
  }
  return newWorkingTable;
}

}  // namespace gql