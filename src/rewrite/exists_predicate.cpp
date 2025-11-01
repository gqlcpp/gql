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

#include "gql/rewrite.h"

#include "gql/ast/algorithm.h"

namespace gql::rewrite {

void RewriteExistsPredicate(ast::ExistsPredicate& expr) {
  // There should be check for GQ22 feature but we don't have information about
  // supported features here.
  if (std::holds_alternative<ast::ProcedureBodyPtr>(expr.option)) {
    return;
  }

  ast::MatchStatementBlock msb;
  if (auto msbPtr = std::get_if<ast::MatchStatementBlockPtr>(&expr.option)) {
    msb = **msbPtr;
  } else {
    msb.statements.emplace_back().emplace<ast::SimpleMatchStatement>().pattern =
        *std::get<ast::GraphPatternPtr>(expr.option);
  }
  ast::ProcedureBodyPtr proc;
  auto& linearQuery = proc->statements.firstStatement->option
                          .emplace<ast::CompositeQueryStatement>()
                          .queries.emplace_back()
                          .emplace<ast::LinearQueryStatementOption>();
  auto& simpleLinearQuery =
      linearQuery.statements.emplace<ast::SimpleLinearQueryStatement>();
  for (auto& stmt : msb.statements) {
    simpleLinearQuery.emplace_back(std::move(stmt));
  }
  auto& returnItem =
      linearQuery.result.option.emplace<ast::PrimitiveResultStatement::Return>()
          .stmt.items.emplace()
          .emplace_back();
  returnItem.alias.emplace().name = "gql_gen_return";
  returnItem.aggregate.option.emplace<ast::UnsignedLiteral>()
      .emplace<ast::GeneralLiteral>() = ast::TruthValue::TRUE;
  expr.option = std::move(proc);
}

void RewriteExistsPredicate(ast::GQLProgram& program) {
  ast::ForEachNodeOfType<ast::ExistsPredicate>(
      program, [](ast::ExistsPredicate& node) {
        RewriteExistsPredicate(node);
        return ast::VisitorResult::kContinue;
      });
}

}  // namespace gql::rewrite