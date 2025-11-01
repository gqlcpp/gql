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

#include "gql/parser/parser.h"

#include <cstdio>

// "common/formatted_error.h" includes "fmt/format.h" which includes "cstdio"
// which defines "EOF" macro. ATNLR undefines it, so we need to include
// "common/formatted_error.h" first.
#include "common/formatted_error.h"

#include "ast_builder.h"
#include "generated/GQLLexer.h"
#include "generated/GQLParser.h"
#include "gql/ast/ast.h"

#define GQL_ASSERT(condition) assert(condition)

namespace gql::parser {

using namespace antlr4;

class GQLParserErrorListener : public BaseErrorListener {
 public:
  void syntaxError(Recognizer*,
                   Token* offendingSymbol,
                   size_t line,
                   size_t charPositionInLine,
                   const std::string& msg,
                   std::exception_ptr) override {
    throw FormattedError({line, charPositionInLine}, ErrorCode::SyntaxError,
                         "Parse error {0}", msg, offendingSymbol->getText());
  }
};

struct ParserCache::Impl {
  atn::PredictionContextCache predictionContextCache_;
  std::vector<dfa::DFA> decisionToDFA_;
};

ParserCache::ParserCache() : impl_(std::make_unique<Impl>()) {}

ParserCache::~ParserCache() = default;

ast::GQLProgram ParseProgram(const char* query, ParserCache* cache) {
  static_assert(
      std::is_default_constructible<ast::ValueExpression::Unary>::value);

  ast::ValueExpression::Unary unary;

  ANTLRInputStream input(query);
  GQLLexer lexer(&input);
  CommonTokenStream tokens(&lexer);

  GQLParserErrorListener errorListener;
  GQLParser parser(&tokens);

  if (cache) {
    auto& dfa = cache->impl_->decisionToDFA_;
    if (dfa.empty()) {
      const size_t count = parser.getATN().getNumberOfDecisions();
      dfa.reserve(count);
      for (size_t i = 0; i < count; i++) {
        dfa.emplace_back(parser.getATN().getDecisionState(i), i);
      }
    }

    parser.setInterpreter(new atn::ParserATNSimulator(
        &parser, parser.getATN(), cache->impl_->decisionToDFA_,
        cache->impl_->predictionContextCache_));
  }

  parser.removeErrorListeners();
  parser.addErrorListener(&errorListener);

  ast::GQLProgram program;
  ast_builder::BuildAST(parser.gqlProgram(), program);
  GQL_ASSERT(parser.getNumberOfSyntaxErrors() == 0);
  return program;
}

}  // namespace gql::parser
