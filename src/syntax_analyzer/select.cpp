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
#include <unordered_map>

#include "common/formatted_error.h"
#include "gql/ast/algorithm.h"
#include "gql/ast/ast.h"
#include "helpers.h"
#include "match_utils.h"

namespace gql {

std::vector<ast::StatementPtr> SyntaxAnalyzer::Rewrite(
    ast::SelectStatement select,
    const ExecutionContext& context) {
  if (auto* items = std::get_if<ast::SelectItemList>(&select.items)) {
    for (auto& item : *items) {
      if (!item.alias) {
        if (auto* bindVar =
                std::get_if<ast::BindingVariableReference>(&item.expr.option)) {
          item.alias.emplace().name = bindVar->name;
        } else {
          throw FormattedError(item, ErrorCode::E0082,
                               "Select item must have an alias");
        }
      }
    }
  }

  std::vector<ast::StatementPtr> newStatements;
  ast::LinearQueryStatementOption* linearQuery =
      &newStatements.emplace_back()
           ->option.emplace<ast::CompositeQueryStatement>()
           .queries.emplace_back()
           .emplace<ast::LinearQueryStatementOption>();

  if (!select.body) {
    // 14.12 Syntax Rule 27.a
    // RETURN SETQ XOISIL
    ast::ReturnStatementBody& ret =
        linearQuery->result.option
            .emplace<ast::PrimitiveResultStatement::Return>()
            .stmt;
    ret.quantifier = select.quantifier;
    ast::variant_switch(
        select.items, [&](const ast::AsteriskValue&) {},
        [&](const ast::SelectItemList& items) {
          auto& outItems = ret.items.emplace();
          for (auto& item : items) {
            auto& outItem = outItems.emplace_back();
            outItem.alias = item.alias;
            outItem.aggregate = item.expr;
          }
        });
    return newStatements;
  }

  ast::SimpleLinearQueryStatement* simpleLinearQuery =
      &linearQuery->statements.emplace<ast::SimpleLinearQueryStatement>();

  ast::LinearQueryStatementOption& funcLinearQuery =
      simpleLinearQuery->emplace_back()
          .emplace<ast::CallProcedureStatement>()
          .call.emplace<ast::InlineProcedureCall>()
          .spec->statements.firstStatement->option
          .emplace<ast::CompositeQueryStatement>()
          .queries.emplace_back()
          .emplace<ast::LinearQueryStatementOption>();

  // 14.12 Syntax Rule 9
  std::optional<ast::SearchCondition> filter;
  if (select.where) {
    filter = *select.where;
  } else {
    if (auto* matchList =
            std::get_if<ast::SelectGraphMatchList>(&*select.body)) {
      if (auto* match = std::get_if<ast::SimpleMatchStatement>(
              &matchList->matches.back().match)) {
        if (match->pattern.where && !match->yield) {
          filter = match->pattern.where->condition;
          match->pattern.where.reset();
        }
      }
    }
  }

  if (filter) {
    simpleLinearQuery->emplace_back()
        .emplace<ast::FilterStatement>()
        .condition = std::move(*filter);
  }

  std::unordered_set<std::string> matchCols;
  ast::variant_switch(
      *select.body,
      [&](const ast::SelectGraphMatchList& statement) {
        // 14.12 Syntax Rule 26.a
        for (const auto& match : statement.matches) {
          ast::FocusedLinearQueryStatementPart& focusedPart =
              funcLinearQuery.statements
                  .emplace<std::vector<ast::FocusedLinearQueryStatementPart>>()
                  .emplace_back();
          focusedPart.useGraph = match.graph;
          focusedPart.statements.emplace_back(match.match);

          detail::CollectMatchOutputColumns(match.match, matchCols);
        }
      },
      [&](ast::SelectQuerySpecification& query) {
        // 14.12 Syntax Rule 26.b
        // Check conditions in 14.12 Syntax Rule 26.b.ii
        if (!query.query.statements.nextStatements.empty()) {
          throw FormattedError(
              *query.query.statements.nextStatements.front().statement,
              ErrorCode::E0102, "Single query statement expected");
        }
        if (auto* queryStatement = std::get_if<ast::CompositeQueryStatement>(
                &query.query.statements.firstStatement->option)) {
          if (queryStatement->queries.size() > 1) {
            throw FormattedError(*queryStatement, ErrorCode::E0102,
                                 "Single query statement expected");
          }
          std::optional<bool> isFocused = ast::variant_switch(
              queryStatement->queries.front(),
              [&](const ast::LinearQueryStatementOption& statement)
                  -> std::optional<bool> {
                return !std::holds_alternative<ast::SimpleLinearQueryStatement>(
                    statement.statements);
              },
              [&](const ast::NestedQuerySpecification&) -> std::optional<bool> {
                return {};
              },
              [&](const ast::FocusedPrimitiveResultStatement&)
                  -> std::optional<bool> { return true; },
              [&](const ast::SelectStatement&) -> std::optional<bool> {
                return true;
              });
          if (isFocused && *isFocused != !!query.graph) {
            if (!!query.graph) {
              throw FormattedError(query.query, ErrorCode::E0104,
                                   "Focused query expected");

            } else {
              throw FormattedError(query.query, ErrorCode::E0105,
                                   "Ambient query expected");
            }
          }
        } else {
          throw FormattedError(query.query, ErrorCode::E0103,
                               "Query statement expected");
        }
        ast::SimpleLinearQueryStatement* statements;
        if (query.graph) {
          ast::FocusedLinearQueryStatementPart& focusedPart =
              funcLinearQuery.statements
                  .emplace<std::vector<ast::FocusedLinearQueryStatementPart>>()
                  .emplace_back();
          focusedPart.useGraph = *query.graph;
          statements = &focusedPart.statements;
        } else {
          statements = &funcLinearQuery.statements
                            .emplace<ast::SimpleLinearQueryStatement>();
        }
        statements->emplace_back()
            .emplace<ast::CallProcedureStatement>()
            .call.emplace<ast::InlineProcedureCall>()
            .spec = query.query;
        auto contextCopy = context.MakeCopy();
        auto bindingTable =
            Process(query.query, CallProcedureKind::Query, contextCopy);
        if (bindingTable) {
          for (auto& f : *bindingTable) {
            matchCols.insert(f.name.name);
          }
        }
      });

  ast::ReturnItemList& funcReturnItems =
      funcLinearQuery.result.option
          .emplace<ast::PrimitiveResultStatement::Return>()
          .stmt.items.emplace();
  for (auto& var : matchCols) {
    funcReturnItems.emplace_back()
        .aggregate.option.emplace<ast::BindingVariableReference>()
        .name = var;
  }

  const bool hasAggregatingItems =
      detail::FindDirectlyContainedDescendant<ast::AggregateFunction>(
          select.items);

  int lastGeneratedId = 0;
  auto generateId = [&lastGeneratedId]() {
    return fmt::format("gql_gen_select{}", ++lastGeneratedId);
  };

  // BODY
  // FILTERSTM
  // RETURN SETQ RETURN_FIRST
  // // RETURN_FIRST = GKRISET, SARISET, ARISET, and HARISET
  // GROUP_BY
  // NEXT
  // HAVING_FILTER
  // LETSTM
  // RETURN SETQ RETURN_LAST
  // // RETURN_LAST = NONAGGREGATING_ITEMS and PARISET
  // ORDER_BY OFFCL LIMCL
  ast::ReturnStatement returnFirst;
  returnFirst.quantifier = select.quantifier;
  returnFirst.items.emplace();
  ast::PrimitiveResultStatement::Return returnLast;
  returnLast.stmt.quantifier = select.quantifier;

  // 14.12 Syntax Rule 12
  // GKRISET
  std::unordered_map<std::string, std::string> gkriset;
  for (auto& group : select.groupBy) {
    auto generatedId = generateId();
    // There is probably an error in Syntax Rule 12.a.iv
    returnFirst.groupBy.emplace_back().name = generatedId;
    auto& outReturnItem = returnFirst.items->emplace_back();
    outReturnItem.aggregate.option.emplace<ast::BindingVariableReference>()
        .name = group.name;
    outReturnItem.alias.emplace().name = generatedId;
    gkriset[group.name] = generatedId;
  }

  // 14.12 Syntax Rule 13.a.i.1 and 20.a.i.2.B.I
  auto replaceGkriset = [&gkriset](auto& rootNode) {
    using NodeType = std::decay_t<decltype(rootNode)>;
    static_assert(std::is_same_v<NodeType, ast::ReturnItem> ||
                  std::is_same_v<NodeType, ast::SortSpecification>);
    detail::ForEachSimplyContainedDescendant<ast::BindingVariableReference,
                                             NodeType>(
        rootNode, [&gkriset](ast::BindingVariableReference& var) {
          if (auto it = gkriset.find(var.name); it != gkriset.end()) {
            var.name = it->second;
          }
          return ast::VisitorResult::kContinue;
        });
  };

  if (auto* items = std::get_if<ast::SelectItemList>(&select.items)) {
    for (auto& item : *items) {
      if (!returnLast.stmt.items) {
        returnLast.stmt.items.emplace();
      }
      auto& returnLastItem = returnLast.stmt.items->emplace_back();
      returnLastItem.alias.emplace().name = item.alias->name;
      if (detail::FindDirectlyContainedDescendant<ast::AggregateFunction>(
              item.expr.option)) {
        // 14.12 Syntax Rule 15
        // ARISET and PARISET
        auto generatedId = generateId();
        auto& returnFirstItem = returnFirst.items->emplace_back();
        returnFirstItem.aggregate = item.expr;
        returnFirstItem.alias.emplace().name = generatedId;
        returnLastItem.aggregate.option.emplace<ast::BindingVariableReference>()
            .name = generatedId;
      } else {
        // 14.12 Syntax Rule 13
        // NONAGGREGATING_ITEMS
        returnLastItem.aggregate = item.expr;
        replaceGkriset(returnLastItem);
      }
    }
  }

  // 14.12 Syntax Rule 14
  // SARISET
  // 14.12 Syntax Rule 17
  // LET_ITEMS
  ast::LetStatement letStatement;
  for (auto& sortItem : select.orderBy) {
    if (detail::FindDirectlyContainedDescendant<ast::AggregateFunction>(
            sortItem.sortKey.option)) {
      // 14.12 Syntax Rule 14
      auto generatedId = generateId();
      auto& outReturnItem = returnFirst.items->emplace_back();
      outReturnItem.aggregate = sortItem.sortKey;
      outReturnItem.alias.emplace().name = generatedId;

      // 14.12 Syntax Rule 20.a.i.2.A
      if (!returnLast.orderByAndPage) {
        returnLast.orderByAndPage.emplace();
      }
      auto& sortSpec = returnLast.orderByAndPage->orderBy.emplace_back();
      sortSpec.sortKey.option.emplace<ast::BindingVariableReference>().name =
          generatedId;
    } else if (!hasAggregatingItems &&
               !std::holds_alternative<ast::BindingVariableReference>(
                   sortItem.sortKey.option)) {
      // 14.12 Syntax Rule 17
      auto generatedId = generateId();
      auto& letVar = letStatement.definitions.emplace_back();
      letVar.var.name = generatedId;
      letVar.expr = sortItem.sortKey;

      // 14.12 Syntax Rule 20.a.i.2.C
      if (!returnLast.orderByAndPage) {
        returnLast.orderByAndPage.emplace();
      }
      auto& sortSpec = returnLast.orderByAndPage->orderBy.emplace_back();
      sortSpec.sortKey.option.emplace<ast::BindingVariableReference>().name =
          generatedId;

      // This part is not in the spec, but it seems to be the only way to
      // get the column from LET through the RETURN_LAST to the ORDER BY
      auto& returnLastItem = returnLast.stmt.items->emplace_back();
      returnLastItem.alias.emplace().name = generatedId;
      returnLastItem.aggregate.option.emplace<ast::BindingVariableReference>()
          .name = generatedId;
    } else {
      // 14.12 Syntax Rule 20.a.i.2.D
      if (!returnLast.orderByAndPage) {
        returnLast.orderByAndPage.emplace();
      }
      returnLast.orderByAndPage->orderBy.push_back(sortItem);
      replaceGkriset(returnLast.orderByAndPage->orderBy.back());
    }
  }

  std::optional<ast::FilterStatement> havingFilter;
  // 14.12 Syntax Rule 16
  // HARISET
  if (select.having) {
    auto generatedId = generateId();
    auto& outReturnItem = returnFirst.items->emplace_back();
    outReturnItem.aggregate = **select.having;
    outReturnItem.alias.emplace().name = generatedId;
    havingFilter.emplace()
        .condition->option.emplace<ast::BindingVariableReference>()
        .name = generatedId;
  }

  // 14.12 Syntax Rule 19
  if (hasAggregatingItems || !select.groupBy.empty()) {
    for (auto& sortItem : select.orderBy) {
      if (auto* bindVar = std::get_if<ast::BindingVariableReference>(
              &sortItem.sortKey.option)) {
        if (auto* selectItems =
                std::get_if<ast::SelectItemList>(&select.items)) {
          if (std::any_of(selectItems->begin(), selectItems->end(),
                          [bindVar](const ast::SelectItem& item) {
                            return item.alias->name == bindVar->name;
                          })) {
            continue;
          }
        }
        if (std::any_of(select.groupBy.begin(), select.groupBy.end(),
                        [bindVar](const ast::GroupingElement& item) {
                          return item.name == bindVar->name;
                        })) {
          continue;
        }
        throw FormattedError(sortItem, ErrorCode::E0083,
                             "Sort item must reference a variable");
      } else {
        // 14.12 Syntax Rule 19.b
        detail::ForEachDirectlyContainedDescendant<
            ast::BindingVariableReference>(
            sortItem.sortKey, [&](const ast::BindingVariableReference& var) {
              if (std::none_of(select.groupBy.begin(), select.groupBy.end(),
                               [&var](const ast::GroupingElement& item) {
                                 return item.name == var.name;
                               })) {
                throw FormattedError(
                    var, ErrorCode::E0101,
                    "Variable in sort item must reference a GROUP BY variable");
              }
              return ast::VisitorResult::kContinue;
            });
      }
    }
  }

  if (!returnFirst.items->empty()) {
    linearQuery->result.option.emplace<ast::PrimitiveResultStatement::Return>()
        .stmt = std::move(returnFirst);

    linearQuery = &newStatements.emplace_back()
                       ->option.emplace<ast::CompositeQueryStatement>()
                       .queries.emplace_back()
                       .emplace<ast::LinearQueryStatementOption>();
    simpleLinearQuery =
        &linearQuery->statements.emplace<ast::SimpleLinearQueryStatement>();
  }

  if (havingFilter) {
    simpleLinearQuery->push_back(std::move(*havingFilter));
  }

  if (!letStatement.definitions.empty()) {
    simpleLinearQuery->push_back(std::move(letStatement));
  }

  if (select.limit) {
    if (!returnLast.orderByAndPage) {
      returnLast.orderByAndPage.emplace();
    }
    returnLast.orderByAndPage->limit = *select.limit;
  }
  if (select.offset) {
    if (!returnLast.orderByAndPage) {
      returnLast.orderByAndPage.emplace();
    }
    returnLast.orderByAndPage->offset = *select.offset;
  }

  linearQuery->result.option.emplace<ast::PrimitiveResultStatement::Return>() =
      std::move(returnLast);

  return newStatements;
}

}  // namespace gql