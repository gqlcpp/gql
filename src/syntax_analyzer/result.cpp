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

#include <algorithm>
#include <unordered_set>

#include "common/formatted_error.h"
#include "gql/ast/algorithm.h"
#include "helpers.h"
#include "type_helpers.h"

namespace gql {

void SyntaxAnalyzer::Process(ast::PrimitiveResultStatement& result,
                             ExecutionContext& context) {
  ast::variant_switch(
      result.option, [&](const ast::FinishValue&) {},
      [&](ast::PrimitiveResultStatement::Return& statement) {
        auto origContext = context.MakeCopy();

        statement.stmt = Rewrite(statement.stmt, context);
        Process(statement.stmt, context);
        if (statement.orderByAndPage &&
            !statement.orderByAndPage->orderBy.empty()) {
          Process(*statement.orderByAndPage, context);
          // Rewrite above made return item list non-optional.
          const ast::ReturnItemList& returnItemList = *statement.stmt.items;
          for (auto& sortItem : statement.orderByAndPage->orderBy) {
            if (auto* bindingVar = std::get_if<ast::BindingVariableReference>(
                    &sortItem.sortKey.option)) {
              if (std::none_of(returnItemList.begin(), returnItemList.end(),
                               [bindingVar](const ast::ReturnItem& ril) {
                                 return ril.alias->name == bindingVar->name;
                               })) {
                ThrowIfFeatureNotSupported(standard::Feature::GQ16,
                                           sortItem.sortKey);
              }
            } else {
              ThrowIfFeatureNotSupported(standard::Feature::GQ14,
                                         sortItem.sortKey);

              // It says "the <value expression> immediately contained in a
              // <sort key> shall not simply contain an <aggregate function>",
              // but I check for "directly contains" instead. I hope it's fine
              if (detail::FindDirectlyContainedDescendant<
                      ast::AggregateFunction>(sortItem.sortKey.option)) {
                ThrowIfFeatureNotSupported(standard::Feature::GF20,
                                           sortItem.sortKey);
              }

              if (auto* proc = ast::FindFirstNodeOfType<ast::ProcedureBodyPtr>(
                      sortItem.sortKey)) {
                // 14.10 Syntax Rule 4.c.i.2.A.I
                throw FormattedError(
                    **proc, ErrorCode::E0020,
                    "Sort key cannot contain query specification");
              }
            }
            // TODO: Check GA07 feature
          }

          // Call rewrite independently from the |rewriteResultOrderByClause|
          // option value to perform checks inside the function, but drop
          // rewrite result if option is disabled.
          auto rewrittenOrderBy = RewriteOrderByClause(statement, context);
          // Processing rewritten statement shouldn't be necessary, but we do it
          // anyway to check for errors in the code.
          Process(rewrittenOrderBy.stmt, origContext);
          Process(*rewrittenOrderBy.orderByAndPage, origContext);
          if (config_.rewriteResultOrderByClause) {
            statement = std::move(rewrittenOrderBy);
          }
        }
      });
}

// Rewrite asterisk to the list of all columns and assign explicit aliases for
// all return items.
ast::ReturnStatement SyntaxAnalyzer::Rewrite(
    const ast::ReturnStatement& retStatement,
    const ExecutionContext& context) const {
  auto newRetStatement = retStatement;
  if (!retStatement.items) {
    // Asterisk
    if (context.workingTable.empty()) {
      throw FormattedError(
          retStatement, ErrorCode::E0050,
          "Asterisk cannot be used without any columns in the working "
          "table");
    }

    if (!retStatement.groupBy.empty()) {
      throw FormattedError(retStatement, ErrorCode::E0049,
                           "GROUP BY cannot be used with * selector");
    }

    newRetStatement.items.emplace();
    for (auto& col : context.workingTable) {
      auto& newRetItem = newRetStatement.items->emplace_back();
      newRetItem.alias.emplace().name = col.name.name;
      newRetItem.aggregate.option.emplace<ast::BindingVariableReference>()
          .name = col.name.name;
    }
    return newRetStatement;
  }

  for (auto& item : *newRetStatement.items) {
    if (!item.alias) {
      if (auto* var = std::get_if<ast::BindingVariableReference>(
              &item.aggregate.option)) {
        item.alias.emplace().name = var->name;
      } else {
        throw FormattedError(item, ErrorCode::E0048,
                             "Explicit alias required for the return item");
      }
    }
  }
  return newRetStatement;
}

void SyntaxAnalyzer::Process(ast::ReturnStatement& retStatement,
                             ExecutionContext& context) {
  if (!retStatement.groupBy.empty()) {
    ThrowIfFeatureNotSupported(standard::Feature::GQ15,
                               retStatement.groupBy.front());
  }

  if (retStatement.groupBy.empty()) {
    // No GROUP BY
    ast::FieldTypeList outWorkingTable;
    // Child context is not as required by 14.11 Syntax Rule 10.b, I consider
    // this as a mistake in the standard. I made it by 14.11 General Rule 3.b.
    auto childContext = context.MakeAmended();
    for (auto& item : *retStatement.items) {
      auto& newField = outWorkingTable.emplace_back();
      newField.name.name = item.alias->name;
      newField.type =
          ProcessAggregatingValueExpression(item.aggregate, childContext);
    }
    context.workingTable = std::move(outWorkingTable);
    return;
  }

  // Handle GROUP BY 14.11 Syntax Rule 10
  auto groupingItemContext = context.MakeAmended();
  std::unordered_map<std::string, ast::ValueType> groupCols;
  for (auto& groupItem : retStatement.groupBy) {
    auto retItemIt =
        std::find_if(retStatement.items->begin(), retStatement.items->end(),
                     [&groupItem](const ast::ReturnItem& item) {
                       return item.alias->name == groupItem.name;
                     });
    if (retItemIt != retStatement.items->end()) {
      groupCols[groupItem.name] = ProcessAggregatingValueExpression(
          retItemIt->aggregate, groupingItemContext);
    } else {
      groupCols[groupItem.name] = Process(groupItem, groupingItemContext);
    }
  }

  auto nonGroupingItemContext = context.MakeCopy();
  nonGroupingItemContext.workingTable.erase(
      std::remove_if(nonGroupingItemContext.workingTable.begin(),
                     nonGroupingItemContext.workingTable.end(),
                     [&groupCols](const ast::FieldType& field) {
                       return groupCols.count(field.name.name);
                     }),
      nonGroupingItemContext.workingTable.end());
  for (auto& col : groupCols) {
    auto& newField = nonGroupingItemContext.workingRecord.emplace_back();
    newField.name.name = col.first;
    newField.type = col.second;
  }

  ast::FieldTypeList outWorkingTable;
  for (auto& item : *retStatement.items) {
    auto& newField = outWorkingTable.emplace_back();
    newField.name.name = item.alias->name;
    auto groupColIt = groupCols.find(item.alias->name);
    if (groupColIt != groupCols.end()) {
      newField.type = groupColIt->second;
    } else {
      newField.type = ProcessAggregatingValueExpression(item.aggregate,
                                                        nonGroupingItemContext);
    }
  }
  context.workingTable = std::move(outWorkingTable);
}

void SyntaxAnalyzer::Process(ast::OrderByAndPageStatement& statement,
                             ExecutionContext& context) {
  if (statement.offset) {
    ThrowIfFeatureNotSupported(standard::Feature::GQ12, statement);
  }
  if (statement.limit) {
    ThrowIfFeatureNotSupported(standard::Feature::GQ13, statement);
  }

  auto childContext = context.MakeAmended();
  for (auto& s : statement.orderBy) {
    if (s.nullOrder) {
      ThrowIfFeatureNotSupported(standard::Feature::GA03, s);
    }

    ProcessValueExpression(s.sortKey, childContext);
  }
}

namespace {

// 14.10 Syntax Rule 4.c.i.A.IV: Check if node contains <binding variable
// reference> without an intervening instance of <aggregate function>.
struct BindingVariableWithoutAggregateFunctionVisitor {
  template <typename NodeType>
  auto operator()(NodeType*) {
    return ast::VisitorResult::kContinue;
  }

  auto operator()(ast::AggregateFunction*) {
    return ast::VisitorResult::kSkipChildren;
  }

  auto operator()(ast::BindingVariableReference* node) {
    result = node;
    return ast::VisitorResult::kStop;
  }

  ast::BindingVariableReference* result = nullptr;
};

}  // namespace

// 14.10 Syntax Rule 4.c.i.A
ast::PrimitiveResultStatement::Return SyntaxAnalyzer::RewriteOrderByClause(
    const ast::PrimitiveResultStatement::Return& origStatement,
    const ExecutionContext& context) const {
  auto statement = origStatement;
  auto& orderBy = statement.orderByAndPage->orderBy;

  std::unordered_set<std::string> returnIds, orderRefs;
  for (auto& retItem : *statement.stmt.items) {
    returnIds.insert(retItem.alias->name);
  }
  orderRefs = returnIds;

  bool retItemContainsAggregateFunction =
      ast::FindFirstNodeOfType<ast::AggregateFunction>(*statement.stmt.items) !=
      nullptr;
  if (!statement.stmt.groupBy.empty()) {
    for (auto& groupItem : statement.stmt.groupBy) {
      orderRefs.insert(groupItem.name);
    }
  } else if (statement.stmt.quantifier != ast::SetQuantifier::DISTINCT &&
             !retItemContainsAggregateFunction) {
    for (auto& f : context.workingTable) {
      orderRefs.insert(f.name.name);
    }
    for (auto& f : context.workingRecord) {
      orderRefs.insert(f.name.name);
    }
  }

  // 14.10 Syntax Rule 4.c.i.A.V: Check if node contains an <aggregate function>
  // without an intervening instance of <procedure body>.
  const bool retItemContainsAggregateFunctionWithoutProcedureBody =
      detail::FindDirectlyContainedDescendant<ast::AggregateFunction>(
          *statement.stmt.items);

  int lastGeneratedId = 0;
  auto generateId = [&lastGeneratedId]() {
    return fmt::format("gql_gen_sort{}", ++lastGeneratedId);
  };

  for (auto& sortItem : orderBy) {
    // 14.10 Syntax Rule 4.c.i.A.IV
    BindingVariableWithoutAggregateFunctionVisitor visitor;
    ast::ForEachNodeInTree(sortItem.sortKey, visitor);
    if (visitor.result) {
      if (orderRefs.count(visitor.result->name) == 0) {
        throw FormattedError(*visitor.result, ErrorCode::E0027,
                             "Variable cannot be used in sort key");
      }
    }

    if (ast::FindFirstNodeOfType<ast::AggregateFunction>(sortItem.sortKey)) {
      // 14.10 Syntax Rule 4.c.i.A.V
      if (statement.stmt.groupBy.empty() ||
          !retItemContainsAggregateFunctionWithoutProcedureBody) {
        throw FormattedError(sortItem.sortKey, ErrorCode::E0028,
                             "Sort key cannot contain aggregate function");
      }

      auto alias = generateId();
      auto& newRetItem = statement.stmt.items->emplace_back();
      newRetItem.alias.emplace().name = alias;
      sortItem.sortKey.option.emplace<ast::BindingVariableReference>().name =
          alias;
    }
  }

  // 14.10 Syntax Rule 4.c.i.A.VIII
  for (auto& item : orderRefs) {
    if (returnIds.count(item) == 0) {
      auto& newRetItem = statement.stmt.items->emplace_back();
      newRetItem.alias.emplace().name = item;
      newRetItem.aggregate.option.emplace<ast::BindingVariableReference>()
          .name = item;
    }
  }

  return statement;
}

}  // namespace gql