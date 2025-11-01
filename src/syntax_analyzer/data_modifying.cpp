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

#include <set>
#include <unordered_set>
#include <vector>

#include "common/formatted_error.h"
#include "gql/ast/algorithm.h"
#include "type_helpers.h"

namespace gql {

void SyntaxAnalyzer::Process(
    ast::LinearDataModifyingStatement& dataModifyingStmt,
    ExecutionContext& context) {
  if (dataModifyingStmt.useGraph) {
    ThrowIfFeatureNotSupported(standard::Feature::GP01,
                               *dataModifyingStmt.useGraph);
  }

  if (isInsideReadOnlyTransaction_) {
    throw FormattedError(
        dataModifyingStmt, ErrorCode::E0011,
        "Data-modifying statement cannot be executed inside read-only "
        "transaction");
  }
  ast::variant_switch(
      dataModifyingStmt.option,
      [&](ast::LinearDataModifyingStatementBody& statement) {
        Process(statement, context);
      },
      [&](ast::ProcedureBody& statement) {
        Process(statement, CallProcedureKind::DataModifying, context);
      });
}

void SyntaxAnalyzer::Process(ast::LinearDataModifyingStatementBody& statement,
                             ExecutionContext& context) {
  for (auto& stmt : statement.statements) {
    ast::variant_switch(
        stmt,
        [&](ast::SimpleQueryStatement& query) { Process(query, context); },
        [&](ast::PrimitiveDataModifyingStatement& prim) {
          ThrowIfFeatureNotSupported(standard::Feature::GD01, statement);

          Process(prim, context);
        },
        [&](ast::CallProcedureStatement& callProc) {
          ThrowIfFeatureNotSupported(standard::Feature::GD01, statement);

          Process(callProc, CallProcedureKind::DataModifying, context);
        });
  }
  if (statement.result) {
    Process(*statement.result, context);
  }
}

void SyntaxAnalyzer::Process(ast::PrimitiveDataModifyingStatement& statement,
                             ExecutionContext& context) {
  ast::variant_switch(
      statement,
      [&](ast::InsertStatement& statement) { Process(statement, context); },
      [&](ast::SetStatement& statement) {
        std::unordered_set<std::string> setAllVars;
        std::set<std::pair<std::string, std::string>> setProps;
        for (auto& item : statement.items) {
          ast::variant_switch(
              item,
              [&](ast::SetPropertyItem& statement) {
                if (!setProps
                         .emplace(statement.var.name, statement.property.name)
                         .second) {
                  // 13.3 Syntax Rule 4
                  throw FormattedError(
                      statement.var, ErrorCode::E0011,
                      "Two set property items cannot reference same variable "
                      "and property");
                }

                AssertGraphElementReferenceType(Process(statement.var, context),
                                                statement.var);
                ProcessPropertyValueExpression(statement.value, context);
              },
              [&](ast::SetAllPropertiesItem& statement) {
                if (!setAllVars.insert(statement.var.name).second) {
                  // 13.3 Syntax Rule 5
                  throw FormattedError(
                      statement.var, ErrorCode::E0012,
                      "Two set all properties items cannot reference same "
                      "variable");
                }

                AssertGraphElementReferenceType(Process(statement.var, context),
                                                statement.var);
                for (auto& p : statement.properties) {
                  ProcessPropertyValueExpression(p.value, context);
                }
              },
              [&](const ast::SetLabelItem& statement) {
                ThrowIfFeatureNotSupported(standard::Feature::GD02, statement);

                AssertGraphElementReferenceType(Process(statement.var, context),
                                                statement.var);
              });
        }
      },
      [&](const ast::RemoveStatement& statement) {
        for (const auto& item : statement.items) {
          ast::variant_switch(
              item,
              [&](const ast::RemovePropertyItem& statement) {
                AssertGraphElementReferenceType(Process(statement.var, context),
                                                statement.var);
              },
              [&](const ast::RemoveLabelItem& statement) {
                ThrowIfFeatureNotSupported(standard::Feature::GD02, statement);

                AssertGraphElementReferenceType(Process(statement.var, context),
                                                statement.var);
              });
        }
      },
      [&](ast::DeleteStatement& statement) {
        for (auto& item : statement.items) {
          if (!std::holds_alternative<ast::BindingVariableReference>(
                  item.option)) {
            ThrowIfFeatureNotSupported(standard::Feature::GD04, item);
          }
          if (auto* proc = ast::FindFirstNodeOfType<ast::ProcedureBodyPtr>(
                  item.option)) {
            ThrowIfFeatureNotSupported(standard::Feature::GD03, **proc);
          }

          AssertGraphElementReferenceType(ProcessValueExpression(item, context),
                                          item);
        }
      });
}

void SyntaxAnalyzer::Process(ast::InsertStatement& statement,
                             ExecutionContext& context) {
  std::vector<ast::FieldType> fieldsToInsert;

  auto processProperties = [&](ast::PropertyKeyValuePairList& props) {
    for (auto& p : props) {
      auto childContext = context.MakeAmended();
      ProcessPropertyValueExpression(p.value, childContext);
    }
  };

  std::unordered_set<std::string> insertedNodes;
  for (auto& path : statement.paths) {
    for (auto& node : path.nodes) {
      bool is_bound_node = false;
      if (node.filler.var) {
        auto* fieldType =
            HasField(context.workingRecord, node.filler.var->name);
        if (!fieldType) {
          fieldType = HasField(context.workingTable, node.filler.var->name);
        }
        if (fieldType) {
          is_bound_node = true;
          AssertNodeReferenceType(*fieldType->type, *node.filler.var);
        }
      }

      if (is_bound_node) {
        if (!node.filler.labels.labels.empty()) {
          throw FormattedError(node.filler.labels, ErrorCode::E0013,
                               "Bound node cannot define labels");
        }
        if (!node.filler.props.empty()) {
          throw FormattedError(node.filler, ErrorCode::E0014,
                               "Bound node cannot define properties");
        }
      }

      if (node.filler.var && !is_bound_node) {
        if (insertedNodes.insert(node.filler.var->name).second) {
          // Defining pattern
          auto& f = fieldsToInsert.emplace_back();
          f.name.name = node.filler.var->name;
          f.type = MakeValueType(ast::NodeReferenceValueType{});
        } else {
          if (!node.filler.labels.labels.empty()) {
            throw FormattedError(node.filler.labels, ErrorCode::E0015,
                                 "Only first node declaration with the same "
                                 "name can define labels");
          }
          if (!node.filler.props.empty()) {
            throw FormattedError(node.filler, ErrorCode::E0016,
                                 "Only first node declaration with the same "
                                 "name can define properties");
          }
        }
      }

      processProperties(node.filler.props);
    }

    std::unordered_set<std::string> insertedEdges;
    for (auto& edge : path.edges) {
      if (edge.filler.var) {
        if (HasField(context.workingRecord, edge.filler.var->name) ||
            HasField(context.workingTable, edge.filler.var->name)) {
          throw FormattedError(edge.filler, ErrorCode::E0017,
                               "Edge cannot be bound");
        }
        if (insertedNodes.count(edge.filler.var->name)) {
          throw FormattedError(edge.filler, ErrorCode::E0018,
                               "Edge cannot have same variable name as node");
        }
        if (!insertedEdges.insert(edge.filler.var->name).second) {
          throw FormattedError(edge.filler, ErrorCode::E0019,
                               "Edge variable name cannot be duplicated");
        }

        auto& f = fieldsToInsert.emplace_back();
        f.name.name = edge.filler.var->name;
        f.type = MakeValueType(ast::EdgeReferenceValueType{});

        processProperties(edge.filler.props);
      }
    }
  }

  context.workingRecord.insert(context.workingRecord.end(),
                               fieldsToInsert.begin(), fieldsToInsert.end());
}

}  // namespace gql