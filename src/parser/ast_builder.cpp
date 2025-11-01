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

// "common/formatted_error.h" includes "fmt/format.h" which includes "cstdio"
// which defines "EOF" macro. ATNLR undefines it, so we need to include
// "common/formatted_error.h" first.
#include "common/formatted_error.h"

#include "ast_builder.h"

#include "gql/ast/ast.h"
#include "unescape_string.h"

#define GQL_ASSERT(condition) assert(condition)

namespace gql::parser::ast_builder {

void BuildAST(GQLParser::ValueExpressionContext* ctx,
              ast::ValueExpression& value);
void BuildAST(GQLParser::ValueTypeContext* ctx, ast::ValueType& value);
void BuildAST(GQLParser::LabelExpressionContext* ctx,
              ast::LabelExpression& value);
void BuildAST(GQLParser::GraphExpressionContext* ctx,
              ast::GraphExpression& value);
void BuildAST(GQLParser::ProcedureBodyContext* ctx, ast::ProcedureBody& value);
void BuildAST(GQLParser::GraphPatternContext* ctx, ast::GraphPattern& value);
void BuildAST(GQLParser::MatchStatementBlockContext* ctx,
              ast::MatchStatementBlock& value);

namespace {
const char kValueOutOfRangeErrorFmt[] = "Value out of range {0}";
}

template <class Context, class Node>
void AssignInputPosition(Context* ctx, Node& value) {
  value.SetInputPosition(
      {ctx->start->getLine(), ctx->start->getCharPositionInLine()});
#ifndef NDEBUG
  value.parserContext = ctx;
#endif
}

template <class NodeType>
void BuildAST(GQLParser::RegularIdentifierContext* ctx, NodeType& value) {
  AssignInputPosition(ctx, value);
  value.name = ctx->getStart()->getText();
}

void BuildAST(GQLParser::CharacterStringLiteralContext* ctx,
              std::string& value) {
  value = UnescapeQuotedCharacterSequence(ctx->getStart()->getText());
}

void BuildAST(GQLParser::CharacterStringLiteralContext* ctx,
              ast::CharacterStringLiteral& value) {
  AssignInputPosition(ctx, value);
  BuildAST(ctx, value.name);
}

template <class NodeType>
void BuildAST(GQLParser::IdentifierContext* ctx, NodeType& value) {
  AssignInputPosition(ctx, value);
  if (auto ctx2 = ctx->regularIdentifier()) {
    BuildAST(ctx2, value);
  } else {
    value.name = UnescapeQuotedCharacterSequence(ctx->getStart()->getText());
  }
}

template <class NodeType>
void BuildAST(GQLParser::BindingVariableContext* ctx, NodeType& value) {
  AssignInputPosition(ctx, value);
  BuildAST(ctx->regularIdentifier(), value);
}

static unsigned long long ReadUnsignedInteger(antlr4::Token* token,
                                              const char* str,
                                              const char** str_end,
                                              int base = 10) {
  try {
    std::size_t pos;
    auto value = std::stoull(str, &pos, base);
    if (str_end) {
      *str_end = str + pos;
    }
    return value;
  } catch (const std::out_of_range& e) {
    throw FormattedError({token->getLine(), token->getCharPositionInLine()},
                         ErrorCode::ValueOutOfRange, kValueOutOfRangeErrorFmt,
                         e.what(), token->getText());
  }
}

void BuildAST(GQLParser::UnsignedDecimalIntegerContext* ctx,
              ast::UnsignedInteger& value) {
  auto token = ctx->UNSIGNED_DECIMAL_INTEGER()->getSymbol();
  std::string text = token->getText();
  text.erase(std::remove(text.begin(), text.end(), '_'), text.end());
  const char* str_end;
  value = ReadUnsignedInteger(token, text.c_str(), &str_end);
  GQL_ASSERT(str_end - text.c_str() == text.size());
}

void BuildAST(GQLParser::UnsignedIntegerContext* ctx,
              ast::UnsignedInteger& value) {
  int base;
  size_t start = 0;
  antlr4::Token* token;
  if (auto ctx2 = ctx->UNSIGNED_DECIMAL_INTEGER()) {
    base = 10;
    token = ctx2->getSymbol();
  } else if (auto ctx2 = ctx->UNSIGNED_HEXADECIMAL_INTEGER()) {
    base = 16;
    start = 2;
    token = ctx2->getSymbol();
  } else if (auto ctx2 = ctx->UNSIGNED_OCTAL_INTEGER()) {
    base = 8;
    start = 2;
    token = ctx2->getSymbol();
  } else {
    auto ctx3 = ctx->UNSIGNED_BINARY_INTEGER();
    base = 2;
    start = 2;
    token = ctx3->getSymbol();
  }
  std::string text = token->getText();
  text.erase(std::remove(text.begin(), text.end(), '_'), text.end());
  text.erase(0, start);
  const char* str_end;
  value = ReadUnsignedInteger(token, text.c_str(), &str_end, base);
  GQL_ASSERT(str_end - text.c_str() == text.size());
}

void BuildAST(GQLParser::ExactNumericLiteralContext* ctx,
              ast::ExactNumericLiteral& value) {
  value.scale = 0;
  if (auto ctx2 = ctx->unsignedInteger()) {
    ast::UnsignedInteger unsignedInteger;
    BuildAST(ctx2, unsignedInteger);
    value.value = unsignedInteger;
    return;
  }

  if (auto ctx3 = ctx->UNSIGNED_DECIMAL_INTEGER_WITH_EXACT_NUMBER_SUFFIX()) {
    auto token = ctx3->getSymbol();
    std::string text = token->getText();
    text.erase(text.size() - 1);
    text.erase(std::remove(text.begin(), text.end(), '_'), text.end());
    const char* str_end;
    value.value = ReadUnsignedInteger(token, text.c_str(), &str_end);
    GQL_ASSERT(str_end - text.c_str() == text.size());
    return;
  }

  if (auto ctx2 =
          ctx->UNSIGNED_DECIMAL_IN_SCIENTIFIC_NOTATION_WITH_EXACT_NUMBER_SUFFIX()) {
    auto token = ctx2->getSymbol();
    std::string text = token->getText();
    text.erase(text.size() - 1);
    text.erase(std::remove(text.begin(), text.end(), '_'), text.end());
    auto dotOrExpPos = text.find_first_of(".eE", 0, 3);
    GQL_ASSERT(dotOrExpPos != std::string::npos);
    bool hasDot = false;
    if (text[dotOrExpPos] == '.') {
      text.erase(dotOrExpPos, 1);
      hasDot = true;
    }
    const char* str_end;
    value.value = ReadUnsignedInteger(token, text.c_str(), &str_end);
    GQL_ASSERT(*str_end == 'e' || *str_end == 'E');
    int scale = 0;
    if (hasDot) {
      scale = str_end - text.c_str() - dotOrExpPos;
    }
    try {
      scale -= std::stol(str_end + 1, nullptr, 10);
    } catch (const std::out_of_range& e) {
      throw FormattedError({token->getLine(), token->getCharPositionInLine()},
                           ErrorCode::ValueOutOfRange, kValueOutOfRangeErrorFmt,
                           e.what(), token->getText());
    }
    while (scale < 0) {
      auto prevValue = value.value;
      value.value *= 10;
      scale += 1;
      if (prevValue > value.value) {
        // Integer overflow happened
        throw FormattedError(
            {token->getLine(), token->getCharPositionInLine()},
            ErrorCode::ValueOutOfRange, kValueOutOfRangeErrorFmt,
            "Value is out of supported range", token->getText());
      }
    }
    value.scale = scale;
    return;
  }

  antlr4::Token* token;
  std::string text;
  if (auto ctx2 =
          ctx->UNSIGNED_DECIMAL_IN_COMMON_NOTATION_WITH_EXACT_NUMBER_SUFFIX()) {
    token = ctx2->getSymbol();
    text = token->getText();
    text.erase(text.size() - 1);
  } else {
    auto ctx3 = ctx->UNSIGNED_DECIMAL_IN_COMMON_NOTATION_WITHOUT_SUFFIX();
    token = ctx3->getSymbol();
    text = token->getText();
  }

  text.erase(std::remove(text.begin(), text.end(), '_'), text.end());
  auto dotPos = text.find('.');
  GQL_ASSERT(dotPos != std::string::npos);
  text.erase(dotPos, 1);
  const char* str_end;
  value.value = ReadUnsignedInteger(token, text.c_str(), &str_end);
  GQL_ASSERT(str_end - text.c_str() == text.size());
  value.scale = text.size() - dotPos;
}

void BuildAST(GQLParser::ApproximateNumericLiteralContext* ctx, double& value) {
  size_t suffixLength = 0;
  std::string text;
  if (auto ctx2 =
          ctx->UNSIGNED_DECIMAL_IN_SCIENTIFIC_NOTATION_WITH_APPROXIMATE_NUMBER_SUFFIX()) {
    text = ctx2->getSymbol()->getText();
    suffixLength = 1;
  } else if (
      auto ctx2 =
          ctx->UNSIGNED_DECIMAL_IN_SCIENTIFIC_NOTATION_WITHOUT_SUFFIX()) {
    text = ctx2->getSymbol()->getText();
  } else if (
      auto ctx2 =
          ctx->UNSIGNED_DECIMAL_IN_COMMON_NOTATION_WITH_APPROXIMATE_NUMBER_SUFFIX()) {
    text = ctx2->getSymbol()->getText();
    suffixLength = 1;
  } else {
    text = ctx->UNSIGNED_DECIMAL_INTEGER_WITH_APPROXIMATE_NUMBER_SUFFIX()
               ->getSymbol()
               ->getText();
    suffixLength = 1;
  }
  text.erase(text.size() - suffixLength);
  text.erase(std::remove(text.begin(), text.end(), '_'), text.end());
  size_t pos;
  try {
    value = std::stod(text, &pos);
  } catch (const std::out_of_range& e) {
    auto token = ctx->getStart();
    throw FormattedError({token->getLine(), token->getCharPositionInLine()},
                         ErrorCode::ValueOutOfRange, kValueOutOfRangeErrorFmt,
                         e.what(), token->getText());
  }
  GQL_ASSERT(pos == text.size());
}

void BuildAST(GQLParser::UnsignedNumericLiteralContext* ctx,
              ast::UnsignedNumericLiteral& value) {
  if (auto ctx2 = ctx->exactNumericLiteral()) {
    BuildAST(ctx2, value.emplace<ast::ExactNumericLiteral>());
  } else {
    BuildAST(ctx->approximateNumericLiteral(), value.emplace<double>());
  }
}

void BuildAST(GQLParser::TemporalLiteralContext* ctx,
              ast::TemporalLiteral& value) {
  if (auto ctx2 = ctx->dateLiteral()) {
    auto& value2 = value.emplace<ast::DateLiteral>();
    AssignInputPosition(ctx2, value2);
    BuildAST(ctx2->dateString()->characterStringLiteral(), value2.value);
  } else if (auto ctx2 = ctx->timeLiteral()) {
    auto& value2 = value.emplace<ast::TimeLiteral>();
    AssignInputPosition(ctx2, value2);
    BuildAST(ctx2->timeString()->characterStringLiteral(), value2.value);
  } else {
    auto& value2 = value.emplace<ast::DatetimeLiteral>();
    AssignInputPosition(ctx, value2);
    BuildAST(ctx->datetimeLiteral()->datetimeString()->characterStringLiteral(),
             value2.value);
  }
}

void BuildAST(GQLParser::ListValueConstructorByEnumerationContext* ctx,
              ast::ListValueConstructorByEnumeration& value) {
  AssignInputPosition(ctx, value);
  if (auto ctx2 = ctx->listElementList()) {
    for (auto ctx3 : ctx2->listElement()) {
      BuildAST(ctx3->valueExpression(), *value.elements.emplace_back());
      value.elements.back()->isValueExpressionRule = true;
    }
  }
}

void ProcessByteStringLiteral(const std::string& text,
                              ast::ByteStringLiteral& value) {
  value.clear();
  value.reserve((text.size() - 3) / 2);
  char hex[3] = {};
  size_t pos = 0;
  for (size_t i = 2; i < text.size() - 1; i++) {
    if (text[i] == ' ')
      continue;
    hex[pos] = text[i];
    pos = 1 - pos;
    if (pos == 0) {
      char* end;
      value.push_back(static_cast<uint8_t>(std::strtoul(hex, &end, 16)));
      GQL_ASSERT(end == hex + 2);
    }
  }
}

template <class Context>
void ProcessGeneralParameterReference(Context* ctx,
                                      ast::GeneralParameterReference& value) {
  AssignInputPosition(ctx, value);
  AssignInputPosition(ctx, value);
  // Strip the dollar sign
  auto str =
      ctx->GENERAL_PARAMETER_REFERENCE()->getSymbol()->getText().substr(1);
  if (str.front() == '@' || str.front() == '"' || str.front() == '\'')
    value.name = UnescapeQuotedCharacterSequence(str);
  else
    value.name = str;
}

void BuildAST(GQLParser::ReferenceParameterSpecificationContext* ctx,
              ast::ReferenceParameterSpecification& value) {
  AssignInputPosition(ctx, value);
  // Strip double dollar sign
  auto str = ctx->SUBSTITUTED_PARAMETER_REFERENCE()->getText().substr(2);
  if (str.front() == '@' || str.front() == '"' || str.front() == '\'')
    value.name = UnescapeQuotedCharacterSequence(str);
  else
    value.name = str;
}

void BuildAST(GQLParser::RecordConstructorContext* ctx,
              ast::RecordConstructor& value) {
  if (auto ctx2 = ctx->fieldsSpecification()) {
    if (auto ctx3 = ctx2->fieldList()) {
      for (auto ctx4 : ctx3->field()) {
        auto& value2 = value.emplace_back();
        AssignInputPosition(ctx4, value2);
        BuildAST(ctx4->fieldName()->identifier(), value2.name);
        BuildAST(ctx4->valueExpression(), *value2.value);
        value2.value->isValueExpressionRule = true;
      }
    }
  }
}

void BuildAST(GQLParser::NonNegativeIntegerSpecificationContext* ctx,
              ast::NonNegativeIntegerSpecification& value) {
  if (auto ctx2 = ctx->unsignedInteger()) {
    BuildAST(ctx2, value.emplace<ast::UnsignedInteger>());
  } else {
    ProcessGeneralParameterReference(
        ctx->dynamicParameterSpecification(),
        value.emplace<ast::GeneralParameterReference>());
  }
}

void BuildAST(GQLParser::GeneralLiteralContext* ctx,
              ast::GeneralLiteral& value) {
  if (ctx->BOOLEAN_LITERAL()) {
    value = ast::TruthValue();
  } else if (auto ctx2 = ctx->characterStringLiteral()) {
    BuildAST(ctx2, value.emplace<ast::CharacterStringLiteral>());
  } else if (auto ctx2 = ctx->BYTE_STRING_LITERAL()) {
    ProcessByteStringLiteral(ctx2->getSymbol()->getText(),
                             value.emplace<ast::ByteStringLiteral>());
  } else if (auto ctx2 = ctx->temporalLiteral()) {
    BuildAST(ctx2, value.emplace<ast::TemporalLiteral>());
  } else if (auto ctx2 = ctx->durationLiteral()) {
    auto& value2 = value.emplace<ast::DurationLiteral>();
    AssignInputPosition(ctx2, value2);
    BuildAST(ctx2->durationString()->characterStringLiteral(), value2.value);
  } else if (ctx->nullLiteral()) {
    value = ast::NullLiteral();
  } else if (auto ctx2 = ctx->listLiteral()) {
    BuildAST(ctx2->listValueConstructorByEnumeration(),
             value.emplace<ast::ListLiteral>());
  } else {
    BuildAST(ctx->recordLiteral()->recordConstructor(),
             value.emplace<ast::RecordLiteral>());
  }
}

void BuildAST(GQLParser::UnsignedLiteralContext* ctx,
              ast::UnsignedLiteral& value) {
  if (auto ctx2 = ctx->unsignedNumericLiteral()) {
    BuildAST(ctx2, value.emplace<ast::UnsignedNumericLiteral>());
  } else {
    BuildAST(ctx->generalLiteral(), value.emplace<ast::GeneralLiteral>());
  }
}

void BuildAST(GQLParser::DelimitedGraphNameContext* ctx,
              ast::GraphName& value) {
  AssignInputPosition(ctx, value);
  value.name = UnescapeQuotedCharacterSequence(ctx->getStart()->getText());
}

void BuildAST(GQLParser::GraphNameContext* ctx, ast::GraphName& value) {
  AssignInputPosition(ctx, value);
  if (auto ctx2 = ctx->regularIdentifier()) {
    BuildAST(ctx2, value);
  } else {
    BuildAST(ctx->delimitedGraphName(), value);
  }
}

void BuildAST(GQLParser::DelimitedBindingTableNameContext* ctx,
              ast::BindingTableName& value) {
  AssignInputPosition(ctx, value);
  value.name = UnescapeQuotedCharacterSequence(ctx->getStart()->getText());
}

void BuildAST(GQLParser::BindingTableNameContext* ctx,
              ast::BindingTableName& value) {
  AssignInputPosition(ctx, value);
  if (auto ctx2 = ctx->regularIdentifier()) {
    BuildAST(ctx2, value);
  } else {
    BuildAST(ctx->delimitedBindingTableName(), value);
  }
}

void BuildAST(GQLParser::SimpleDirectoryPathContext* ctx,
              ast::SimpleDirectoryPath& value) {
  for (auto ctx2 : ctx->directoryName()) {
    BuildAST(ctx2->identifier(), value.emplace_back());
  }
}

void BuildAST(GQLParser::AbsoluteCatalogSchemaReferenceContext* ctx,
              ast::CatalogSchemaReference& value) {
  AssignInputPosition(ctx, value);
  if (auto ctx2 = ctx->absoluteDirectoryPath()) {
    if (auto ctx3 = ctx2->simpleDirectoryPath()) {
      BuildAST(ctx3, value.path);
    }
    BuildAST(ctx->schemaName()->identifier(), value.name.emplace());
  }
}

void BuildAST(GQLParser::PredefinedSchemaReferenceContext* ctx,
              ast::PredefinedSchemaReference& value) {
  if (ctx->HOME_SCHEMA() != nullptr) {
    value = ast::PredefinedSchemaReference::HOME_SCHEMA;
  } else {
    value = ast::PredefinedSchemaReference::CURRENT_SCHEMA;
  }
}

void BuildAST(GQLParser::RelativeCatalogSchemaReferenceContext* ctx,
              ast::SchemaReference& value) {
  AssignInputPosition(ctx, value);
  if (auto ctx2 = ctx->predefinedSchemaReference()) {
    BuildAST(ctx2, value.option.emplace<ast::PredefinedSchemaReference>());
  } else {
    auto& value2 = value.option.emplace<ast::CatalogSchemaReference>();
    AssignInputPosition(ctx, value2);
    value2.levelUpCount = static_cast<unsigned>(
        ctx->relativeDirectoryPath()->DOUBLE_PERIOD().size());
    if (auto ctx3 = ctx->relativeDirectoryPath()->simpleDirectoryPath()) {
      BuildAST(ctx3, value2.path);
    }
    BuildAST(ctx->schemaName()->identifier(), value2.name.emplace());
  }
}

void BuildAST(GQLParser::SchemaReferenceContext* ctx,
              ast::SchemaReference& value) {
  AssignInputPosition(ctx, value);
  if (auto ctx2 = ctx->absoluteCatalogSchemaReference()) {
    BuildAST(ctx2, value.option.emplace<ast::CatalogSchemaReference>());
  } else if (auto ctx2 = ctx->relativeCatalogSchemaReference()) {
    BuildAST(ctx2, value);
  } else {
    BuildAST(ctx->referenceParameterSpecification(),
             value.option.emplace<ast::ReferenceParameterSpecification>());
  }
}

void BuildAST(GQLParser::CatalogObjectParentReferenceContext* ctx,
              ast::CatalogObjectParentReference& value) {
  AssignInputPosition(ctx, value);
  if (auto ctx2 = ctx->schemaReference()) {
    BuildAST(ctx2, value.schema.emplace());
  }
  for (auto ctx3 : ctx->objectName()) {
    BuildAST(ctx3->identifier(), value.path.emplace_back());
  }
}

void BuildAST(GQLParser::BindingTableReferenceContext* ctx,
              ast::BindingTableReference& value) {
  AssignInputPosition(ctx, value);
  if (auto ctx2 = ctx->catalogObjectParentReference()) {
    auto& value2 =
        value.option.emplace<ast::BindingTableReference::ParentAndName>();
    BuildAST(ctx2, value2.parent.emplace());
    BuildAST(ctx->bindingTableName(), value2.name);
  } else if (auto ctx2 = ctx->delimitedBindingTableName()) {
    BuildAST(
        ctx2,
        value.option.emplace<ast::BindingTableReference::ParentAndName>().name);
  } else {
    BuildAST(ctx->referenceParameterSpecification(),
             value.option.emplace<ast::ReferenceParameterSpecification>());
  }
}

void BuildAST(GQLParser::GraphReferenceContext* ctx,
              ast::GraphReference& value) {
  AssignInputPosition(ctx, value);
  if (auto ctx2 = ctx->catalogObjectParentReference()) {
    auto& value2 = value.option.emplace<ast::GraphReference::ParentAndName>();
    BuildAST(ctx2, value2.parent.emplace());
    BuildAST(ctx->graphName(), value2.name);
  } else if (auto ctx2 = ctx->delimitedGraphName()) {
    auto& value2 = value.option.emplace<ast::GraphReference::ParentAndName>();
    BuildAST(ctx2, value2.name);
  } else if (ctx->homeGraph()) {
    value.option.emplace<ast::HomeGraph>();
  } else {
    BuildAST(ctx->referenceParameterSpecification(),
             value.option.emplace<ast::ReferenceParameterSpecification>());
  }
}

void BuildAST(GQLParser::CatalogSchemaParentAndNameContext* ctx,
              ast::CatalogSchemaParentAndName& value) {
  AssignInputPosition(ctx, value);
  if (auto ctx2 = ctx->absoluteDirectoryPath()->simpleDirectoryPath()) {
    BuildAST(ctx2, value.absolutePath);
  }
  BuildAST(ctx->schemaName()->identifier(), value.name);
}

void BuildAST(GQLParser::CatalogGraphParentAndNameContext* ctx,
              ast::CatalogGraphParentAndName& value) {
  AssignInputPosition(ctx, value);
  if (auto ctx2 = ctx->catalogObjectParentReference()) {
    BuildAST(ctx2, value.parent.emplace());
  }
  BuildAST(ctx->graphName(), value.name);
}

void BuildAST(GQLParser::CatalogGraphTypeParentAndNameContext* ctx,
              ast::CatalogGraphTypeParentAndName& value) {
  AssignInputPosition(ctx, value);
  if (auto ctx2 = ctx->catalogObjectParentReference()) {
    BuildAST(ctx2, value.parent.emplace());
  }
  BuildAST(ctx->graphTypeName()->identifier(), value.name);
}

void BuildAST(GQLParser::GraphTypeReferenceContext* ctx,
              ast::GraphTypeReference& value) {
  if (auto ctx2 = ctx->catalogGraphTypeParentAndName()) {
    BuildAST(ctx2, value.emplace<ast::CatalogGraphTypeParentAndName>());
  } else {
    BuildAST(ctx->referenceParameterSpecification(),
             value.emplace<ast::ReferenceParameterSpecification>());
  }
}

void BuildAST(GQLParser::CatalogProcedureParentAndNameContext* ctx,
              ast::CatalogProcedureParentAndName& value) {
  AssignInputPosition(ctx, value);
  if (auto ctx2 = ctx->catalogObjectParentReference()) {
    BuildAST(ctx2, value.parent.emplace());
  }
  BuildAST(ctx->procedureName()->identifier(), value.name);
}

void BuildAST(GQLParser::ProcedureReferenceContext* ctx,
              ast::ProcedureReference& value) {
  if (auto ctx2 = ctx->catalogProcedureParentAndName()) {
    BuildAST(ctx2, value.emplace<ast::CatalogProcedureParentAndName>());
  } else {
    BuildAST(ctx->referenceParameterSpecification(),
             value.emplace<ast::ReferenceParameterSpecification>());
  }
}

void BuildAST(GQLParser::LabelSetSpecificationContext* ctx,
              ast::LabelSetSpecification& value) {
  AssignInputPosition(ctx, value);
  for (auto ctx2 : ctx->labelName()) {
    BuildAST(ctx2->identifier(), value.labels.emplace_back());
  }
}

void BuildAST(GQLParser::LabelSetPhraseContext* ctx,
              ast::LabelSetSpecification& value) {
  AssignInputPosition(ctx, value);
  if (auto ctx2 = ctx->labelName()) {
    BuildAST(ctx2->identifier(), value.labels.emplace_back());
  } else {
    BuildAST(ctx->labelSetSpecification(), value);
  }
}

void BuildAST(GQLParser::PropertyTypesSpecificationContext* ctx,
              ast::PropertyTypesSpecification& value) {
  AssignInputPosition(ctx, value);
  if (auto ctx2 = ctx->propertyTypeList()) {
    for (auto ctx3 : ctx2->propertyType()) {
      auto& property = value.properties.emplace_back();
      AssignInputPosition(ctx3, property);
      BuildAST(ctx3->propertyName()->identifier(), property.name);
      BuildAST(ctx3->propertyValueType()->valueType(), *property.type);
    }
  }
}

void BuildAST(GQLParser::NodeTypeFillerContext* ctx,
              ast::ElementTypeFiller& value) {
  AssignInputPosition(ctx, value);
  if (auto ctx2 = ctx->nodeTypeKeyLabelSet()) {
    if (auto ctx3 = ctx2->labelSetPhrase()) {
      BuildAST(ctx3, value.keyLabels);
    }
  }
  if (auto ctx2 = ctx->nodeTypeImpliedContent()) {
    AssignInputPosition(ctx2, value.implied);
    if (auto ctx3 = ctx2->nodeTypeLabelSet()) {
      BuildAST(ctx3->labelSetPhrase(), value.implied.labels);
    }
    if (auto ctx3 = ctx2->nodeTypePropertyTypes()) {
      BuildAST(ctx3->propertyTypesSpecification(), value.implied.properties);
    }
  }
}

void BuildAST(GQLParser::NodeTypePatternContext* ctx,
              ast::NodeTypeSpecification& value) {
  AssignInputPosition(ctx, value);
  if (auto ctx2 = ctx->nodeTypeName()) {
    BuildAST(ctx2->identifier(), value.typeName.emplace());
  }
  if (auto ctx2 = ctx->localNodeTypeAlias()) {
    BuildAST(ctx2->regularIdentifier(), value.localAlias.emplace());
  }
  if (auto ctx2 = ctx->nodeTypeFiller()) {
    BuildAST(ctx2, value.filler);
  }
}

void BuildAST(GQLParser::NodeTypePhraseContext* ctx,
              ast::NodeTypeSpecification& value) {
  AssignInputPosition(ctx, value);
  auto ctx2 = ctx->nodeTypePhraseFiller();
  if (auto ctx3 = ctx2->nodeTypeName()) {
    BuildAST(ctx3->identifier(), value.typeName.emplace());
  }
  if (auto ctx3 = ctx2->nodeTypeFiller()) {
    BuildAST(ctx3, value.filler);
  }

  if (auto ctx3 = ctx->localNodeTypeAlias()) {
    BuildAST(ctx3->regularIdentifier(), value.localAlias.emplace());
  }
}

void BuildAST(GQLParser::NodeTypeSpecificationContext* ctx,
              ast::NodeTypeSpecification& value) {
  AssignInputPosition(ctx, value);
  if (auto ctx2 = ctx->nodeTypePattern()) {
    BuildAST(ctx2, value);
  } else if (auto ctx2 = ctx->nodeTypePhrase()) {
    BuildAST(ctx2, value);
  }
}

void BuildAST(GQLParser::EdgeTypeFillerContext* ctx,
              ast::ElementTypeFiller& value) {
  AssignInputPosition(ctx, value);
  if (auto ctx2 = ctx->edgeTypeKeyLabelSet()) {
    if (auto ctx3 = ctx2->labelSetPhrase()) {
      BuildAST(ctx3, value.keyLabels);
    }
  }
  if (auto ctx2 = ctx->edgeTypeImpliedContent()) {
    AssignInputPosition(ctx2, value.implied);
    if (auto ctx3 = ctx2->edgeTypeLabelSet()) {
      BuildAST(ctx3->labelSetPhrase(), value.implied.labels);
    }
    if (auto ctx3 = ctx2->edgeTypePropertyTypes()) {
      BuildAST(ctx3->propertyTypesSpecification(), value.implied.properties);
    }
  }
}

void BuildAST(GQLParser::SourceNodeTypeReferenceContext* ctx,
              std::optional<ast::NodeTypeReference>& value) {
  if (auto ctx2 = ctx->sourceNodeTypeAlias()) {
    BuildAST(ctx2->regularIdentifier(),
             value.emplace().emplace<ast::NodeTypeAlias>());
  } else if (auto ctx2 = ctx->nodeTypeFiller()) {
    BuildAST(ctx2, value.emplace().emplace<ast::ElementTypeFiller>());
  }
}

void BuildAST(GQLParser::DestinationNodeTypeReferenceContext* ctx,
              std::optional<ast::NodeTypeReference>& value) {
  if (auto ctx2 = ctx->destinationNodeTypeAlias()) {
    BuildAST(ctx2->regularIdentifier(),
             value.emplace().emplace<ast::NodeTypeAlias>());
  } else if (auto ctx2 = ctx->nodeTypeFiller()) {
    BuildAST(ctx2, value.emplace().emplace<ast::ElementTypeFiller>());
  }
}

void BuildAST(GQLParser::EdgeTypePatternContext* ctx,
              ast::EdgeTypeSpecification& value) {
  AssignInputPosition(ctx, value);
  if (auto ctx2 = ctx->edgeTypeName()) {
    BuildAST(ctx2->identifier(), value.typeName.emplace());
  }
  if (auto ctx2 = ctx->edgeTypePatternDirected()) {
    value.isDirected = true;
    if (auto ctx3 = ctx2->edgeTypePatternPointingRight()) {
      BuildAST(ctx3->sourceNodeTypeReference(), value.source);
      BuildAST(ctx3->destinationNodeTypeReference(), value.destination);
      BuildAST(ctx3->arcTypePointingRight()->edgeTypeFiller(), value.filler);
    } else if (auto ctx3 = ctx2->edgeTypePatternPointingLeft()) {
      BuildAST(ctx3->sourceNodeTypeReference(), value.source);
      BuildAST(ctx3->destinationNodeTypeReference(), value.destination);
      BuildAST(ctx3->arcTypePointingLeft()->edgeTypeFiller(), value.filler);
    }
  } else if (auto ctx2 = ctx->edgeTypePatternUndirected()) {
    value.isDirected = false;
    BuildAST(ctx2->sourceNodeTypeReference(), value.source);
    BuildAST(ctx2->destinationNodeTypeReference(), value.destination);
    BuildAST(ctx2->arcTypeUndirected()->edgeTypeFiller(), value.filler);
  }
}

void BuildAST(GQLParser::EdgeTypePhraseContext* ctx,
              ast::EdgeTypeSpecification& value) {
  AssignInputPosition(ctx, value);
  auto ctx2 = ctx->edgeTypePhraseFiller();
  if (auto ctx3 = ctx2->edgeTypeName()) {
    BuildAST(ctx3->identifier(), value.typeName.emplace());
  }
  if (auto ctx3 = ctx2->edgeTypeFiller()) {
    BuildAST(ctx3, value.filler);
  }

  auto ctx3 = ctx->endpointPairPhrase()->endpointPair();
  if (auto ctx4 = ctx3->endpointPairDirected()) {
    value.isDirected = true;
    if (auto ctx5 = ctx4->endpointPairPointingRight()) {
      BuildAST(ctx5->sourceNodeTypeAlias()->regularIdentifier(),
               value.source.emplace().emplace<ast::NodeTypeAlias>());
      BuildAST(ctx5->destinationNodeTypeAlias()->regularIdentifier(),
               value.destination.emplace().emplace<ast::NodeTypeAlias>());
    } else if (auto ctx5 = ctx4->endpointPairPointingLeft()) {
      BuildAST(ctx5->sourceNodeTypeAlias()->regularIdentifier(),
               value.source.emplace().emplace<ast::NodeTypeAlias>());
      BuildAST(ctx5->destinationNodeTypeAlias()->regularIdentifier(),
               value.destination.emplace().emplace<ast::NodeTypeAlias>());
    }
  } else if (auto ctx4 = ctx3->endpointPairUndirected()) {
    value.isDirected = false;
    BuildAST(ctx4->sourceNodeTypeAlias()->regularIdentifier(),
             value.source.emplace().emplace<ast::NodeTypeAlias>());
    BuildAST(ctx4->destinationNodeTypeAlias()->regularIdentifier(),
             value.destination.emplace().emplace<ast::NodeTypeAlias>());
  }
}

void BuildAST(GQLParser::EdgeTypeSpecificationContext* ctx,
              ast::EdgeTypeSpecification& value) {
  AssignInputPosition(ctx, value);
  if (auto ctx2 = ctx->edgeTypePattern()) {
    BuildAST(ctx2, value);
  } else {
    BuildAST(ctx->edgeTypePhrase(), value);
  }
}

void BuildAST(GQLParser::ElementTypeSpecificationContext* ctx,
              ast::ElementTypeSpecification& value) {
  if (auto ctx2 = ctx->nodeTypeSpecification()) {
    BuildAST(ctx2, value.emplace<ast::NodeTypeSpecification>());
  } else {
    BuildAST(ctx->edgeTypeSpecification(),
             value.emplace<ast::EdgeTypeSpecification>());
  }
}

void BuildAST(GQLParser::GraphTypeSpecificationBodyContext* ctx,
              ast::GraphTypeSpecificationBody& value) {
  AssignInputPosition(ctx, value);
  for (auto ctx2 : ctx->elementTypeList()->elementTypeSpecification()) {
    BuildAST(ctx2, value.elementTypes.emplace_back());
  }
}

void BuildAST(GQLParser::VerboseBinaryExactNumericTypeContext* ctx,
              ast::ValueType& value,
              bool isSigned) {
  value.notNull = !!ctx->notNull();

  if (ctx->INTEGER8()) {
    value.typeOption =
        isSigned ? ast::SimpleNumericType{ast::SimpleNumericType::Int8}
                 : ast::SimpleNumericType{ast::SimpleNumericType::UInt8};
  } else if (ctx->INTEGER16()) {
    value.typeOption =
        isSigned ? ast::SimpleNumericType{ast::SimpleNumericType::Int16}
                 : ast::SimpleNumericType{ast::SimpleNumericType::UInt16};
  } else if (ctx->INTEGER32()) {
    value.typeOption =
        isSigned ? ast::SimpleNumericType{ast::SimpleNumericType::Int32}
                 : ast::SimpleNumericType{ast::SimpleNumericType::UInt32};
  } else if (ctx->INTEGER64()) {
    value.typeOption =
        isSigned ? ast::SimpleNumericType{ast::SimpleNumericType::Int64}
                 : ast::SimpleNumericType{ast::SimpleNumericType::UInt64};
  } else if (ctx->INTEGER128()) {
    value.typeOption =
        isSigned ? ast::SimpleNumericType{ast::SimpleNumericType::Int128}
                 : ast::SimpleNumericType{ast::SimpleNumericType::UInt128};
  } else if (ctx->INTEGER256()) {
    value.typeOption =
        isSigned ? ast::SimpleNumericType{ast::SimpleNumericType::Int256}
                 : ast::SimpleNumericType{ast::SimpleNumericType::UInt256};
  } else if (ctx->SMALL()) {
    value.typeOption =
        isSigned ? ast::SimpleNumericType{ast::SimpleNumericType::SmallInt}
                 : ast::SimpleNumericType{ast::SimpleNumericType::USmallInt};
  } else if (ctx->BIG()) {
    value.typeOption =
        isSigned ? ast::SimpleNumericType{ast::SimpleNumericType::BigInt}
                 : ast::SimpleNumericType{ast::SimpleNumericType::UBigInt};
  } else {
    if (auto ctx2 = ctx->precision()) {
      auto& value2 =
          value.typeOption.emplace<ast::BinaryExactUserNumericType>();
      value2.isSigned = isSigned;
      BuildAST(ctx2->unsignedDecimalInteger(), value2.precision);
    } else {
      value.typeOption =
          isSigned ? ast::SimpleNumericType{ast::SimpleNumericType::UInt}
                   : ast::SimpleNumericType{ast::SimpleNumericType::UInt};
    }
  }
}

void BuildAST(GQLParser::NumericTypeContext* ctx, ast::ValueType& value) {
  AssignInputPosition(ctx, value);
  if (auto ctx2 = ctx->exactNumericType()) {
    if (auto ctx3 = ctx2->binaryExactNumericType()) {
      if (auto ctx4 = ctx3->signedBinaryExactNumericType()) {
        value.notNull = !!ctx4->notNull();

        if (ctx4->INT8()) {
          value.typeOption =
              ast::SimpleNumericType{ast::SimpleNumericType::Int8};
        } else if (ctx4->INT16()) {
          value.typeOption =
              ast::SimpleNumericType{ast::SimpleNumericType::Int16};
        } else if (ctx4->INT32()) {
          value.typeOption =
              ast::SimpleNumericType{ast::SimpleNumericType::Int32};
        } else if (ctx4->INT64()) {
          value.typeOption =
              ast::SimpleNumericType{ast::SimpleNumericType::Int64};
        } else if (ctx4->INT128()) {
          value.typeOption =
              ast::SimpleNumericType{ast::SimpleNumericType::Int128};
        } else if (ctx4->INT256()) {
          value.typeOption =
              ast::SimpleNumericType{ast::SimpleNumericType::Int256};
        } else if (ctx4->SMALLINT()) {
          value.typeOption =
              ast::SimpleNumericType{ast::SimpleNumericType::SmallInt};
        } else if (ctx4->INT()) {
          if (auto ctx5 = ctx4->precision()) {
            auto& value2 =
                value.typeOption.emplace<ast::BinaryExactUserNumericType>();
            value2.isSigned = true;
            BuildAST(ctx5->unsignedDecimalInteger(), value2.precision);
          } else {
            value.typeOption =
                ast::SimpleNumericType{ast::SimpleNumericType::Int};
          }
        } else if (ctx4->BIGINT()) {
          value.typeOption =
              ast::SimpleNumericType{ast::SimpleNumericType::BigInt};
        } else if (auto ctx5 = ctx4->verboseBinaryExactNumericType()) {
          BuildAST(ctx5, value, /*isSigned=*/true);
        }
      } else {
        auto ctx5 = ctx3->unsignedBinaryExactNumericType();
        value.notNull = !!ctx5->notNull();

        if (ctx5->UINT8()) {
          value.typeOption =
              ast::SimpleNumericType{ast::SimpleNumericType::UInt8};
        } else if (ctx5->UINT16()) {
          value.typeOption =
              ast::SimpleNumericType{ast::SimpleNumericType::UInt16};
        } else if (ctx5->UINT32()) {
          value.typeOption =
              ast::SimpleNumericType{ast::SimpleNumericType::UInt32};
        } else if (ctx5->UINT64()) {
          value.typeOption =
              ast::SimpleNumericType{ast::SimpleNumericType::UInt64};
        } else if (ctx5->UINT128()) {
          value.typeOption =
              ast::SimpleNumericType{ast::SimpleNumericType::UInt128};
        } else if (ctx5->UINT256()) {
          value.typeOption =
              ast::SimpleNumericType{ast::SimpleNumericType::UInt256};
        } else if (ctx5->USMALLINT()) {
          value.typeOption =
              ast::SimpleNumericType{ast::SimpleNumericType::USmallInt};
        } else if (ctx5->UINT()) {
          if (auto ctx6 = ctx5->precision()) {
            auto& value2 =
                value.typeOption.emplace<ast::BinaryExactUserNumericType>();
            value2.isSigned = false;
            BuildAST(ctx6->unsignedDecimalInteger(), value2.precision);
          } else {
            value.typeOption =
                ast::SimpleNumericType{ast::SimpleNumericType::UInt};
          }
        } else if (ctx5->UBIGINT()) {
          value.typeOption =
              ast::SimpleNumericType{ast::SimpleNumericType::UBigInt};
        } else if (auto ctx6 = ctx5->verboseBinaryExactNumericType()) {
          BuildAST(ctx6, value, /*isSigned=*/false);
        }
      }
    } else {
      auto ctx4 = ctx2->decimalExactNumericType();
      auto& value2 = value.typeOption.emplace<ast::ScaleNumericType>();
      value2.type = ast::ScaleNumericType::Type::Decimal;
      if (auto ctx5 = ctx4->precision()) {
        BuildAST(ctx5->unsignedDecimalInteger(), value2.precision.emplace());
      }
      if (auto ctx5 = ctx4->scale()) {
        BuildAST(ctx5->unsignedDecimalInteger(), value2.scale.emplace());
      }
      value.notNull = !!ctx4->notNull();
    }
  } else if (auto ctx2 = ctx->approximateNumericType()) {
    value.notNull = !!ctx2->notNull();

    if (ctx2->FLOAT16()) {
      value.typeOption =
          ast::SimpleNumericType{ast::SimpleNumericType::Float16};
    } else if (ctx2->FLOAT32()) {
      value.typeOption =
          ast::SimpleNumericType{ast::SimpleNumericType::Float32};
    } else if (ctx2->FLOAT64()) {
      value.typeOption =
          ast::SimpleNumericType{ast::SimpleNumericType::Float64};
    } else if (ctx2->FLOAT128()) {
      value.typeOption =
          ast::SimpleNumericType{ast::SimpleNumericType::Float128};
    } else if (ctx2->FLOAT256()) {
      value.typeOption =
          ast::SimpleNumericType{ast::SimpleNumericType::Float256};
    } else if (ctx2->FLOAT()) {
      auto& value2 = value.typeOption.emplace<ast::ScaleNumericType>();
      value2.type = ast::ScaleNumericType::Type::Float;
      if (auto ctx3 = ctx2->precision()) {
        BuildAST(ctx3->unsignedDecimalInteger(), value2.precision.emplace());
      }
      if (auto ctx3 = ctx2->scale()) {
        BuildAST(ctx3->unsignedDecimalInteger(), value2.scale.emplace());
      }
    } else if (ctx2->REAL()) {
      value.typeOption = ast::SimpleNumericType{ast::SimpleNumericType::Real};
    } else if (ctx2->DOUBLE()) {
      value.typeOption = ast::SimpleNumericType{ast::SimpleNumericType::Double};
    }
  }
}

void BuildAST(GQLParser::FieldTypeListContext* ctx,
              ast::FieldTypesSpecification& value) {
  AssignInputPosition(ctx, value);
  for (auto fieldTypeCtx : ctx->fieldType()) {
    auto& field = value.fields.emplace_back();
    AssignInputPosition(fieldTypeCtx, field);
    BuildAST(fieldTypeCtx->fieldName()->identifier(), field.name);
    BuildAST(fieldTypeCtx->valueType(), *field.type);
  }
}

void BuildAST(GQLParser::CharacterStringTypeContext* ctx,
              ast::StringType& value,
              bool* notNull) {
  value.kind = ast::StringType::Kind::CHAR;
  if (ctx->CHAR()) {
    if (auto ctx2 = ctx->fixedLength()) {
      BuildAST(ctx2->unsignedInteger(), value.minLength);
      value.maxLength = value.minLength;
    } else {
      value.minLength = 1;
      value.maxLength = 1;
    }
  } else {
    if (auto ctx2 = ctx->minLength()) {
      BuildAST(ctx2->unsignedInteger(), value.minLength);
    }
    if (auto ctx2 = ctx->maxLength()) {
      BuildAST(ctx2->unsignedInteger(), value.maxLength.emplace());
    }
  }

  *notNull = !!ctx->notNull();
}

void BuildAST(GQLParser::ByteStringTypeContext* ctx,
              ast::StringType& value,
              bool* notNull) {
  value.kind = ast::StringType::Kind::BYTES;
  if (ctx->BINARY()) {
    if (auto ctx2 = ctx->fixedLength()) {
      BuildAST(ctx2->unsignedInteger(), value.minLength);
      value.maxLength = value.minLength;
    } else {
      value.minLength = 1;
      value.maxLength = 1;
    }
  } else {
    if (auto ctx2 = ctx->minLength()) {
      BuildAST(ctx2->unsignedInteger(), value.minLength);
    }
    if (auto ctx2 = ctx->maxLength()) {
      BuildAST(ctx2->unsignedInteger(), value.maxLength.emplace());
    }
  }

  *notNull = !!ctx->notNull();
}

void BuildAST(GQLParser::TemporalTypeContext* ctx,
              ast::SimplePredefinedType& value,
              bool* notNull) {
  if (auto ctx2 = ctx->temporalInstantType()) {
    if (auto ctx3 = ctx2->datetimeType()) {
      value = ast::SimplePredefinedType::Datetime;
      *notNull = !!ctx3->notNull();
    } else if (auto ctx3 = ctx2->localdatetimeType()) {
      value = ast::SimplePredefinedType::Localdatetime;
      *notNull = !!ctx3->notNull();
    } else if (auto ctx3 = ctx2->dateType()) {
      value = ast::SimplePredefinedType::Date;
      *notNull = !!ctx3->notNull();
    } else if (auto ctx3 = ctx2->timeType()) {
      value = ast::SimplePredefinedType::Time;
      *notNull = !!ctx3->notNull();
    } else if (auto ctx3 = ctx2->localtimeType()) {
      value = ast::SimplePredefinedType::Localtime;
      *notNull = !!ctx3->notNull();
    }
  } else {
    auto ctx3 = ctx->temporalDurationType();
    if (ctx3->temporalDurationQualifier()->SECOND() != nullptr) {
      value = ast::SimplePredefinedType::DurationDayToSecond;
    } else {
      value = ast::SimplePredefinedType::DurationYearToMonth;
    }
    *notNull = !!ctx3->notNull();
  }
}

void BuildAST(GQLParser::ImmaterialValueTypeContext* ctx,
              ast::SimplePredefinedType& value) {
  if (ctx->nullType()) {
    value = ast::SimplePredefinedType::Null;
  } else {
    // Empty type is "NULL NOT NULL" itself.
    value = ast::SimplePredefinedType::Empty;
  }
}

void BuildAST(GQLParser::GraphReferenceValueTypeContext* ctx,
              ast::GraphReferenceValueType& value,
              bool* notNull = nullptr) {
  AssignInputPosition(ctx, value);
  if (notNull == nullptr) {
    notNull = &value.notNull;
  }

  if (auto ctx2 = ctx->openGraphReferenceValueType()) {
    *notNull = !!ctx2->notNull();
  } else {
    auto ctx3 = ctx->closedGraphReferenceValueType();
    *notNull = !!ctx3->notNull();

    BuildAST(ctx3->nestedGraphTypeSpecification()->graphTypeSpecificationBody(),
             value.type.emplace());
  }
}

void BuildAST(GQLParser::EdgeReferenceValueTypeContext* ctx,
              ast::EdgeReferenceValueType& value,
              bool* notNull) {
  AssignInputPosition(ctx, value);
  if (auto ctx2 = ctx->openEdgeReferenceValueType()) {
    *notNull = !!ctx2->notNull();
  } else {
    auto ctx3 = ctx->closedEdgeReferenceValueType();
    *notNull = !!ctx3->notNull();

    BuildAST(ctx3->edgeTypeSpecification(), value.type.emplace());
  }
}

void BuildAST(GQLParser::NodeReferenceValueTypeContext* ctx,
              ast::NodeReferenceValueType& value,
              bool* notNull) {
  AssignInputPosition(ctx, value);
  if (auto ctx2 = ctx->openNodeReferenceValueType()) {
    *notNull = !!ctx2->notNull();
  } else {
    auto ctx3 = ctx->closedNodeReferenceValueType();
    *notNull = !!ctx3->notNull();

    BuildAST(ctx3->nodeTypeSpecification(), value.type.emplace());
  }
}

void BuildAST(GQLParser::BindingTableReferenceValueTypeContext* ctx,
              ast::BindingTableReferenceValueType& value) {
  value.notNull = !!ctx->notNull();
  AssignInputPosition(ctx, value.type);
  if (auto ctx2 =
          ctx->bindingTableType()->fieldTypesSpecification()->fieldTypeList()) {
    BuildAST(ctx2, value.type);
  }
}

void BuildAST(GQLParser::ReferenceValueTypeContext* ctx,
              ast::ValueType& value) {
  AssignInputPosition(ctx, value);
  if (auto ctx2 = ctx->graphReferenceValueType()) {
    BuildAST(ctx2, value.typeOption.emplace<ast::GraphReferenceValueType>(),
             &value.notNull);
  } else if (auto ctx2 = ctx->edgeReferenceValueType()) {
    BuildAST(ctx2, value.typeOption.emplace<ast::EdgeReferenceValueType>(),
             &value.notNull);
  } else if (auto ctx2 = ctx->nodeReferenceValueType()) {
    BuildAST(ctx2, value.typeOption.emplace<ast::NodeReferenceValueType>(),
             &value.notNull);
  } else if (auto ctx2 = ctx->bindingTableReferenceValueType()) {
    auto& value2 =
        value.typeOption.emplace<ast::BindingTableReferenceValueType>();
    BuildAST(ctx2, value2);
    value.notNull = value2.notNull;
  }
}

void BuildAST(GQLParser::PredefinedTypeContext* ctx, ast::ValueType& value) {
  AssignInputPosition(ctx, value);
  if (auto ctx2 = ctx->booleanType()) {
    value.typeOption = ast::SimplePredefinedType::Boolean;
    value.notNull = !!ctx2->notNull();
  } else if (auto ctx2 = ctx->characterStringType()) {
    BuildAST(ctx2, value.typeOption.emplace<ast::StringType>(), &value.notNull);
  } else if (auto ctx2 = ctx->byteStringType()) {
    BuildAST(ctx2, value.typeOption.emplace<ast::StringType>(), &value.notNull);
  } else if (auto ctx2 = ctx->numericType()) {
    BuildAST(ctx2, value);
  } else if (auto ctx2 = ctx->temporalType()) {
    BuildAST(ctx2, value.typeOption.emplace<ast::SimplePredefinedType>(),
             &value.notNull);
  } else if (auto ctx2 = ctx->referenceValueType()) {
    BuildAST(ctx2, value);
  } else if (auto ctx2 = ctx->immaterialValueType()) {
    BuildAST(ctx2, value.typeOption.emplace<ast::SimplePredefinedType>());
  }
}

void BuildAST(GQLParser::ClosedDynamicUnionTypeAtl2Context* ctx,
              ast::ValueType::Union& value) {
  auto firstCtx = ctx->valueType(0);
  while (true) {
    if (auto innerCtx =
            dynamic_cast<GQLParser::ClosedDynamicUnionTypeAtl1Context*>(
                firstCtx)) {
      firstCtx = innerCtx->valueType(0);
      GQL_ASSERT(innerCtx->valueType(1) == nullptr);
    } else {
      break;
    }
  }
  if (auto ctx2 = dynamic_cast<GQLParser::ClosedDynamicUnionTypeAtl2Context*>(
          firstCtx)) {
    BuildAST(ctx2, value);
  } else {
    BuildAST(firstCtx, *value.types.emplace_back());
  }
  BuildAST(ctx->valueType(1), *value.types.emplace_back());
}

void BuildAST(GQLParser::ValueTypeContext* ctx, ast::ValueType& value) {
  AssignInputPosition(ctx, value);
  if (auto ctx2 = dynamic_cast<GQLParser::PredefinedTypeLabelContext*>(ctx)) {
    BuildAST(ctx2->predefinedType(), value);
  } else if (auto ctx2 =
                 dynamic_cast<GQLParser::PathValueTypeLabelContext*>(ctx)) {
    value.typeOption = ast::SimplePredefinedType::Path;
    value.notNull = !!ctx2->pathValueType()->notNull();
  } else if (auto ctx2 =
                 dynamic_cast<GQLParser::ListValueTypeAlt1Context*>(ctx)) {
    auto& value2 = value.typeOption.emplace<ast::ValueType::List>();
    value2.isGroup = !!ctx2->listValueTypeName()->GROUP();
    BuildAST(ctx2->valueType(), *value2.valueType.emplace());
    if (auto ctx3 = ctx2->maxLength()) {
      BuildAST(ctx3->unsignedInteger(), value2.maxLength.emplace());
    }
    value.notNull = !!ctx2->notNull();
  } else if (auto ctx2 =
                 dynamic_cast<GQLParser::ListValueTypeAlt2Context*>(ctx)) {
    auto& value2 = value.typeOption.emplace<ast::ValueType::List>();
    value2.isGroup = !!ctx2->listValueTypeName()->GROUP();
    BuildAST(ctx2->valueType(), *value2.valueType.emplace());
    if (auto ctx3 = ctx2->maxLength()) {
      BuildAST(ctx3->unsignedInteger(), value2.maxLength.emplace());
    }
    value.notNull = !!ctx2->notNull();
  } else if (auto ctx2 =
                 dynamic_cast<GQLParser::ListValueTypeAlt3Context*>(ctx)) {
    auto& value2 = value.typeOption.emplace<ast::ValueType::List>();
    value2.isGroup = !!ctx2->listValueTypeName()->GROUP();
    if (auto ctx3 = ctx2->maxLength()) {
      BuildAST(ctx3->unsignedInteger(), value2.maxLength.emplace());
    }
    value.notNull = !!ctx2->notNull();
  } else if (auto ctx2 =
                 dynamic_cast<GQLParser::RecordTypeLabelContext*>(ctx)) {
    auto& value2 = value.typeOption.emplace<ast::RecordType>();
    AssignInputPosition(ctx2, value2);
    if (auto ctx3 = ctx2->recordType()->fieldTypesSpecification()) {
      auto& fields = value2.fields.emplace();
      AssignInputPosition(ctx3, fields);
      if (auto ctx4 = ctx3->fieldTypeList()) {
        BuildAST(ctx4, fields);
      }
    }
    value.notNull = !!ctx2->recordType()->notNull();
  } else if (auto ctx2 =
                 dynamic_cast<GQLParser::OpenDynamicUnionTypeLabelContext*>(
                     ctx)) {
    value.typeOption = ast::SimplePredefinedType::Any;
    value.notNull = !!ctx2->notNull();
  } else if (auto ctx2 =
                 dynamic_cast<GQLParser::DynamicPropertyValueTypeLabelContext*>(
                     ctx)) {
    value.typeOption = ast::SimplePredefinedType::AnyProperty;
    value.notNull = !!ctx2->notNull();
  } else if (auto ctx2 =
                 dynamic_cast<GQLParser::ClosedDynamicUnionTypeAtl1Context*>(
                     ctx)) {
    // There are two options of nested content:
    // 1. Type list "Type1 | Type2". Will be handled by
    // ClosedDynamicUnionTypeAtl2. No need to create union node now.
    // 2. Single type "ANY <SomeType>". Union of single type is same as the
    // type. No need to create union type node at all.
    BuildAST(ctx2->valueType(0), value);
    GQL_ASSERT(ctx2->valueType(1) == nullptr);
  } else if (auto ctx2 =
                 dynamic_cast<GQLParser::ClosedDynamicUnionTypeAtl2Context*>(
                     ctx)) {
    BuildAST(ctx2, value.typeOption.emplace<ast::ValueType::Union>());
  } else {
    GQL_ASSERT(false);
  }
}

void BuildAST(
    GQLParser::NonParenthesizedValueExpressionPrimarySpecialCaseContext* ctx,
    ast::ValueExpression& value);

void BuildAST(GQLParser::ValueExpressionPrimaryContext* ctx,
              ast::ValueExpression& value);

void BuildAST(GQLParser::NumericValueFunctionContext* ctx,
              ast::ValueExpression& value);

template <class NodeType>
void BuildAST(GQLParser::ElementVariableReferenceContext* ctx,
              NodeType& value) {
  BuildAST(ctx->bindingVariableReference()->bindingVariable(), value);
}

void BuildAST(GQLParser::CompOpContext* ctx, ast::CompOp& value) {
  if (ctx->EQUALS_OPERATOR()) {
    value = ast::CompOp::Equals;
  } else if (ctx->NOT_EQUALS_OPERATOR()) {
    value = ast::CompOp::NotEquals;
  } else if (ctx->LEFT_ANGLE_BRACKET()) {
    value = ast::CompOp::LessThan;
  } else if (ctx->RIGHT_ANGLE_BRACKET()) {
    value = ast::CompOp::GreaterThan;
  } else if (ctx->LESS_THAN_OR_EQUALS_OPERATOR()) {
    value = ast::CompOp::LessThanOrEquals;
  } else if (ctx->GREATER_THAN_OR_EQUALS_OPERATOR()) {
    value = ast::CompOp::GreaterThanOrEquals;
  } else {
    GQL_ASSERT(false);
  }
}

void BuildAST(GQLParser::LabelExpressionDisjunctionContext* ctx,
              ast::LabelExpression::Logical& value) {
  value.op = ast::LabelExpression::Logical::Op::Or;
  auto firstCtx = ctx->labelExpression(0);
  while (true) {
    if (auto ctx2 =
            dynamic_cast<GQLParser::LabelExpressionParenthesizedContext*>(
                firstCtx)) {
      firstCtx = ctx2->labelExpression();
    } else {
      break;
    }
  }
  if (auto ctx2 = dynamic_cast<GQLParser::LabelExpressionDisjunctionContext*>(
          firstCtx)) {
    BuildAST(ctx2, value);
  } else {
    BuildAST(firstCtx, *value.terms.emplace_back());
  }
  BuildAST(ctx->labelExpression(1), *value.terms.emplace_back());
}

void BuildAST(GQLParser::LabelExpressionConjunctionContext* ctx,
              ast::LabelExpression::Logical& value) {
  value.op = ast::LabelExpression::Logical::Op::And;
  auto firstCtx = ctx->labelExpression(0);
  while (true) {
    if (auto ctx2 =
            dynamic_cast<GQLParser::LabelExpressionParenthesizedContext*>(
                firstCtx)) {
      firstCtx = ctx2->labelExpression();
    } else {
      break;
    }
  }
  if (auto ctx2 = dynamic_cast<GQLParser::LabelExpressionConjunctionContext*>(
          firstCtx)) {
    BuildAST(ctx2, value);
  } else {
    BuildAST(firstCtx, *value.terms.emplace_back());
  }
  BuildAST(ctx->labelExpression(1), *value.terms.emplace_back());
}

void BuildAST(GQLParser::LabelExpressionContext* ctx,
              ast::LabelExpression& value) {
  AssignInputPosition(ctx, value);
  if (auto ctx2 =
          dynamic_cast<GQLParser::LabelExpressionNegationContext*>(ctx)) {
    auto& value2 = value.option.emplace<ast::LabelExpression::Negation>();
    BuildAST(ctx2->labelExpression(), *value2.expr);
  } else if (auto ctx2 =
                 dynamic_cast<GQLParser::LabelExpressionDisjunctionContext*>(
                     ctx)) {
    BuildAST(ctx2, value.option.emplace<ast::LabelExpression::Logical>());
  } else if (auto ctx2 =
                 dynamic_cast<GQLParser::LabelExpressionConjunctionContext*>(
                     ctx)) {
    BuildAST(ctx2, value.option.emplace<ast::LabelExpression::Logical>());
  } else if (auto ctx2 =
                 dynamic_cast<GQLParser::LabelExpressionNameContext*>(ctx)) {
    auto& value2 = value.option.emplace<ast::LabelName>();
    BuildAST(ctx2->labelName()->identifier(), value2);
  } else if (dynamic_cast<GQLParser::LabelExpressionWildcardContext*>(ctx)) {
    value.option = ast::LabelExpression::Wildcard{};
  } else if (auto ctx2 =
                 dynamic_cast<GQLParser::LabelExpressionParenthesizedContext*>(
                     ctx)) {
    BuildAST(ctx2->labelExpression(), value);
  } else {
    GQL_ASSERT(false);
  }
}

void BuildAST(GQLParser::NumericValueExpressionContext* ctx,
              ast::ValueExpression& value) {
  AssignInputPosition(ctx, value);
  if (auto ctx2 = dynamic_cast<GQLParser::SignedExprAlt2Context*>(ctx)) {
    auto& value2 = value.option.emplace<ast::ValueExpression::Unary>();
    value2.op = (ctx2->PLUS_SIGN() != nullptr)
                    ? ast::ValueExpression::Unary::Op::Positive
                    : ast::ValueExpression::Unary::Op::Negative;
    BuildAST(ctx2->numericValueExpression(), *value2.expr);
  } else if (auto ctx2 =
                 dynamic_cast<GQLParser::MultDivExprAlt2Context*>(ctx)) {
    auto& value2 = value.option.emplace<ast::ValueExpression::Binary>();
    value2.op = (ctx2->ASTERISK() != nullptr)
                    ? ast::ValueExpression::Binary::Op::Multiply
                    : ast::ValueExpression::Binary::Op::Divide;
    BuildAST(ctx2->numericValueExpression(0), *value2.left);
    BuildAST(ctx2->numericValueExpression(1), *value2.right);
  } else if (auto ctx2 =
                 dynamic_cast<GQLParser::AddSubtractExprAlt2Context*>(ctx)) {
    auto& value2 = value.option.emplace<ast::ValueExpression::Binary>();
    value2.op = (ctx2->PLUS_SIGN() != nullptr)
                    ? ast::ValueExpression::Binary::Op::Add
                    : ast::ValueExpression::Binary::Op::Subtract;
    BuildAST(ctx2->numericValueExpression(0), *value2.left);
    BuildAST(ctx2->numericValueExpression(1), *value2.right);
  } else if (auto ctx2 =
                 dynamic_cast<GQLParser::PrimaryExprAlt2Context*>(ctx)) {
    BuildAST(ctx2->valueExpressionPrimary(), value);
  } else if (auto ctx2 =
                 dynamic_cast<GQLParser::NumericValueFunctionExprAltContext*>(
                     ctx)) {
    BuildAST(ctx2->numericValueFunction(), value);
  } else {
    GQL_ASSERT(false);
  }
}

void BuildAST(GQLParser::NormalFormContext* ctx, ast::NormalForm& value) {
  if (ctx->NFC() != nullptr) {
    value = ast::NormalForm::NFC;
  } else if (ctx->NFD() != nullptr) {
    value = ast::NormalForm::NFD;
  } else if (ctx->NFKC() != nullptr) {
    value = ast::NormalForm::NFKC;
  } else if (ctx->NFKD() != nullptr) {
    value = ast::NormalForm::NFKD;
  } else {
    GQL_ASSERT(false);
  }
}

void BuildAST(GQLParser::NormalizedPredicatePart2Context* ctx,
              ast::NormalizedPredicate& value) {
  AssignInputPosition(ctx, value);
  value.isNot = !!ctx->NOT();
  BuildAST(ctx->normalForm(), value.form);
}

void BuildAST(GQLParser::ObjectExpressionPrimaryContext* ctx,
              ast::ValueExpression& value) {
  AssignInputPosition(ctx, value);
  if (auto ctx2 = ctx->valueExpressionPrimary()) {
    BuildAST(ctx2, value);
  } else if (auto ctx2 = ctx->parenthesizedValueExpression()) {
    BuildAST(ctx2->valueExpression(), value);
  } else if (auto ctx2 =
                 ctx->nonParenthesizedValueExpressionPrimarySpecialCase()) {
    BuildAST(ctx2, value);
  } else {
    GQL_ASSERT(false);
  }
}

void BuildAST(GQLParser::BindingTableExpressionContext* ctx,
              ast::BindingTableExpression& value) {
  AssignInputPosition(ctx, value);
  if (auto ctx2 = ctx->nestedBindingTableQuerySpecification()) {
    BuildAST(ctx2->nestedQuerySpecification()->procedureBody(),
             *value.option.emplace<ast::ProcedureBodyPtr>());
  } else if (auto ctx2 = ctx->bindingTableReference()) {
    BuildAST(ctx2, value.option.emplace<ast::BindingTableReference>());
  } else if (auto ctx2 = ctx->objectExpressionPrimary()) {
    BuildAST(ctx2, *value.option.emplace<ast::ValueExpressionPtr>());
  } else if (auto ctx2 = ctx->objectNameOrBindingVariable()) {
    BuildAST(ctx2->regularIdentifier(),
             value.option.emplace<ast::ObjectNameOrBindingVariable>());
  } else {
    GQL_ASSERT(false);
  }
}

void BuildAST(GQLParser::SetQuantifierContext* ctx, ast::SetQuantifier& value) {
  value = (ctx->DISTINCT() != nullptr) ? ast::SetQuantifier::DISTINCT
                                       : ast::SetQuantifier::ALL;
}

void BuildAST(GQLParser::BinarySetFunctionContext* ctx,
              ast::BinarySetFunction& value) {
  AssignInputPosition(ctx, value);
  value.type = (ctx->binarySetFunctionType()->PERCENTILE_CONT() != nullptr)
                   ? ast::BinarySetFunctionType::PERCENTILE_CONT
                   : ast::BinarySetFunctionType::PERCENTILE_DISC;
  if (auto ctx2 = ctx->dependentValueExpression()->setQuantifier()) {
    BuildAST(ctx2, value.quantifier);
  }
  BuildAST(ctx->dependentValueExpression()->numericValueExpression(),
           *value.dependentValue);
  BuildAST(ctx->independentValueExpression()->numericValueExpression(),
           *value.independent);
}

void BuildAST(GQLParser::GeneralSetFunctionContext* ctx,
              ast::GeneralSetFunction& value) {
  AssignInputPosition(ctx, value);
  auto ctx2 = ctx->generalSetFunctionType();
  if (ctx2->AVG() != nullptr) {
    value.type = ast::GeneralSetFunctionType::AVG;
  } else if (ctx2->COUNT() != nullptr) {
    value.type = ast::GeneralSetFunctionType::COUNT;
  } else if (ctx2->MAX() != nullptr) {
    value.type = ast::GeneralSetFunctionType::MAX;
  } else if (ctx2->MIN() != nullptr) {
    value.type = ast::GeneralSetFunctionType::MIN;
  } else if (ctx2->SUM() != nullptr) {
    value.type = ast::GeneralSetFunctionType::SUM;
  } else if (ctx2->COLLECT_LIST() != nullptr) {
    value.type = ast::GeneralSetFunctionType::COLLECT_LIST;
  } else if (ctx2->STDDEV_SAMP() != nullptr) {
    value.type = ast::GeneralSetFunctionType::STDDEV_SAMP;
  } else if (ctx2->STDDEV_POP() != nullptr) {
    value.type = ast::GeneralSetFunctionType::STDDEV_POP;
  } else {
    GQL_ASSERT(false);
  }
  if (auto ctx2 = ctx->setQuantifier()) {
    BuildAST(ctx2, value.quantifier);
  }
  BuildAST(ctx->valueExpression(), *value.value);
  value.value->isValueExpressionRule = true;
}

void BuildAST(GQLParser::AggregateFunctionContext* ctx,
              ast::AggregateFunction& value) {
  if (auto ctx2 = ctx->generalSetFunction()) {
    BuildAST(ctx2, value.emplace<ast::GeneralSetFunction>());
  } else if (auto ctx2 = ctx->binarySetFunction()) {
    BuildAST(ctx2, value.emplace<ast::BinarySetFunction>());
  } else {
    value = ast::CountAsteriskValue();
  }
}

void BuildAST(GQLParser::PathElementListContext* ctx,
              ast::PathValueConstructor& value) {
  AssignInputPosition(ctx, value);
  BuildAST(ctx->pathElementListStart()
               ->nodeReferenceValueExpression()
               ->valueExpressionPrimary(),
           *value.nodes.emplace_back());
  for (auto step : ctx->pathElementListStep()) {
    BuildAST(step->edgeReferenceValueExpression()->valueExpressionPrimary(),
             *value.edges.emplace_back());
    BuildAST(step->nodeReferenceValueExpression()->valueExpressionPrimary(),
             *value.nodes.emplace_back());
  }
}

void BuildAST(GQLParser::UnsignedValueSpecificationContext* ctx,
              ast::ValueExpression& value) {
  AssignInputPosition(ctx, value);
  if (auto ctx2 = ctx->unsignedLiteral()) {
    BuildAST(ctx2, value.option.emplace<ast::UnsignedLiteral>());
  } else if (auto ctx2 = ctx->generalValueSpecification()
                             ->dynamicParameterSpecification()) {
    ProcessGeneralParameterReference(
        ctx2, value.option.emplace<ast::GeneralParameterReference>());
  } else {
    value.option.emplace<ast::SessionUserValue>();
  }
}

void BuildAST(GQLParser::CastSpecificationContext* ctx,
              ast::CastSpecification& value) {
  AssignInputPosition(ctx, value);
  if (ctx->castOperand()->nullLiteral()) {
    value.operand = ast::NullLiteral();
  } else {
    auto& operand = value.operand.emplace<ast::ValueExpressionPtr>();
    BuildAST(ctx->castOperand()->valueExpression(), *operand);
    operand->isValueExpressionRule = true;
  }
  BuildAST(ctx->castTarget()->valueType(), value.target);
}

void BuildAST(GQLParser::Element_idFunctionContext* ctx,
              ast::ElementIdFunction& value) {
  AssignInputPosition(ctx, value);
  BuildAST(ctx->elementVariableReference(), value.variable);
}

void BuildAST(GQLParser::LetVariableDefinitionContext* ctx,
              ast::LetVariableDefinition& value) {
  AssignInputPosition(ctx, value);
  if (auto ctx2 = ctx->valueVariableDefinition()) {
    BuildAST(ctx2->bindingVariable(), value.var);
    BuildAST(
        ctx2->optTypedValueInitializer()->valueInitializer()->valueExpression(),
        *value.expr);
    value.expr->isValueExpressionRule = true;
    if (auto ctx3 = ctx2->optTypedValueInitializer()->valueType()) {
      BuildAST(ctx3, value.type.emplace());
    }
  } else {
    BuildAST(ctx->bindingVariable(), value.var);
    BuildAST(ctx->valueExpression(), *value.expr);
    value.expr->isValueExpressionRule = true;
  }
}

void BuildAST(GQLParser::LetValueExpressionContext* ctx,
              ast::LetValueExpression& value) {
  AssignInputPosition(ctx, value);
  for (auto definition :
       ctx->letVariableDefinitionList()->letVariableDefinition()) {
    BuildAST(definition, value.definitions.emplace_back());
  }
  BuildAST(ctx->valueExpression(), *value.expression);
  value.expression->isValueExpressionRule = true;
}

void BuildAST(GQLParser::NonParenthesizedValueExpressionPrimaryContext* ctx,
              ast::ValueExpression& value) {
  AssignInputPosition(ctx, value);
  if (auto ctx2 = ctx->nonParenthesizedValueExpressionPrimarySpecialCase()) {
    BuildAST(ctx2, value);
  } else {
    BuildAST(ctx->bindingVariableReference()->bindingVariable(),
             value.option.emplace<ast::BindingVariableReference>());
  }
}

void BuildAST(GQLParser::WhenOperandContext* ctx, ast::WhenOperand& value) {
  AssignInputPosition(ctx, value);
  if (auto ctx2 = ctx->nonParenthesizedValueExpressionPrimary()) {
    BuildAST(ctx2, *value.option.emplace<ast::ValueExpressionPtr>());
  } else if (auto ctx2 = ctx->compOp()) {
    auto& value2 = value.option.emplace<ast::WhenOperand::Comparison>();
    BuildAST(ctx2, value2.op);
    BuildAST(ctx->valueExpression(), *value2.value);
  } else if (auto ctx2 = ctx->nullPredicatePart2()) {
    value.option = ast::WhenOperand::IsNull();
    value.isNot = ctx2->NOT() != nullptr;
  } else if (auto ctx2 = ctx->valueTypePredicatePart2()) {
    auto& value2 = value.option.emplace<ast::WhenOperand::IsTyped>();
    BuildAST(ctx2->valueType(), value2.type);
    value.isNot = ctx2->NOT() != nullptr;
  } else if (auto ctx2 = ctx->normalizedPredicatePart2()) {
    auto& value2 = value.option.emplace<ast::WhenOperand::IsNormalized>();
    if (auto ctx3 = ctx2->normalForm()) {
      BuildAST(ctx3, value2.form);
    }
    value.isNot = ctx2->NOT() != nullptr;
  } else if (auto ctx2 = ctx->directedPredicatePart2()) {
    value.option = ast::WhenOperand::IsDirected();
    value.isNot = ctx2->NOT() != nullptr;
  } else if (auto ctx2 = ctx->labeledPredicatePart2()) {
    auto& value2 = value.option.emplace<ast::WhenOperand::IsLabeled>();
    BuildAST(ctx2->labelExpression(), value2.label);
    value.isNot = ctx2->isLabeledOrColon()->NOT() != nullptr;
  } else if (auto ctx2 = ctx->sourcePredicatePart2()) {
    auto& value2 =
        value.option.emplace<ast::WhenOperand::IsSourceOrDestinationOf>();
    value2.direction =
        ast::WhenOperand::IsSourceOrDestinationOf::Direction::Source;
    BuildAST(ctx2->edgeReference()->elementVariableReference(), value2.edge);
    value.isNot = ctx2->NOT() != nullptr;
  } else if (auto ctx2 = ctx->destinationPredicatePart2()) {
    auto& value2 =
        value.option.emplace<ast::WhenOperand::IsSourceOrDestinationOf>();
    value2.direction =
        ast::WhenOperand::IsSourceOrDestinationOf::Direction::Destination;
    value.isNot = ctx2->NOT() != nullptr;
    BuildAST(ctx2->edgeReference()->elementVariableReference(), value2.edge);
  } else {
    GQL_ASSERT(false);
  }
}

void BuildAST(GQLParser::ResultContext* ctx, ast::Result& value) {
  if (ctx->nullLiteral()) {
    value = ast::NullLiteral();
  } else {
    auto& valueExpr = value.emplace<ast::ValueExpressionPtr>();
    BuildAST(ctx->resultExpression()->valueExpression(), *valueExpr);
    valueExpr->isValueExpressionRule = true;
  }
}

void BuildAST(GQLParser::SimpleWhenClauseContext* ctx,
              ast::SimpleWhenClause& value) {
  AssignInputPosition(ctx, value);
  for (auto operand : ctx->whenOperandList()->whenOperand()) {
    BuildAST(operand, value.operands.emplace_back());
  }
  BuildAST(ctx->result(), value.result);
}

void BuildAST(GQLParser::SimpleCaseContext* ctx, ast::SimpleCase& value) {
  AssignInputPosition(ctx, value);
  if (auto ctx2 = ctx->caseOperand()->elementVariableReference()) {
    BuildAST(ctx2, value.operand.emplace<ast::ElementVariableReference>());
  } else {
    BuildAST(ctx->caseOperand()->nonParenthesizedValueExpressionPrimary(),
             *value.operand.emplace<ast::ValueExpressionPtr>());
  }
  for (auto when : ctx->simpleWhenClause()) {
    BuildAST(when, value.when.emplace_back());
  }
  if (auto ctx2 = ctx->elseClause()) {
    BuildAST(ctx2->result(), value.else_.emplace());
  }
}

void BuildAST(GQLParser::SearchedCaseContext* ctx, ast::SearchedCase& value) {
  AssignInputPosition(ctx, value);
  for (auto when : ctx->searchedWhenClause()) {
    auto& value2 = value.when.emplace_back();
    AssignInputPosition(when, value2);
    BuildAST(
        when->searchCondition()->booleanValueExpression()->valueExpression(),
        *value2.condition);
    BuildAST(when->result(), value2.result);
  }
  if (auto ctx2 = ctx->elseClause()) {
    BuildAST(ctx2->result(), value.else_.emplace());
  }
}

void BuildAST(GQLParser::CaseExpressionContext* ctx,
              ast::CaseExpression& value) {
  if (auto ctx2 = ctx->caseAbbreviation()) {
    if (auto ctx3 = dynamic_cast<GQLParser::NullIfExprAltContext*>(ctx2)) {
      auto& value2 = value.emplace<ast::NullIfCaseAbbreviation>();
      AssignInputPosition(ctx3, value2);
      BuildAST(ctx3->valueExpression(0), *value2.first);
      BuildAST(ctx3->valueExpression(1), *value2.second);
      value2.first->isValueExpressionRule = true;
      value2.second->isValueExpressionRule = true;
    } else {
      auto& value2 = value.emplace<ast::CoalesceCaseAbbreviation>();
      AssignInputPosition(ctx2, value2);
      for (auto expr : dynamic_cast<GQLParser::CoalesceExprAltContext*>(ctx2)
                           ->valueExpression()) {
        BuildAST(expr, *value2.expressions.emplace_back());
        value2.expressions.back()->isValueExpressionRule = true;
      }
    }
  } else if (auto ctx2 = ctx->caseSpecification()) {
    if (auto ctx3 = ctx2->simpleCase()) {
      BuildAST(ctx3, value.emplace<ast::SimpleCase>());
    } else {
      BuildAST(ctx2->searchedCase(), value.emplace<ast::SearchedCase>());
    }
  }
}

template <class Context>
void ProcessNonParenthesizedValueExpressionPrimarySpecialCase(
    Context* ctx,
    ast::ValueExpression& value) {
  AssignInputPosition(ctx, value);
  if (auto ctx2 = ctx->aggregateFunction()) {
    BuildAST(ctx2, value.option.emplace<ast::AggregateFunction>());
  } else if (auto ctx2 = ctx->unsignedValueSpecification()) {
    BuildAST(ctx2, value);
  } else if (auto ctx2 = ctx->pathValueConstructor()) {
    BuildAST(ctx2->pathValueConstructorByEnumeration()->pathElementList(),
             value.option.emplace<ast::PathValueConstructor>());
  } else if (auto ctx2 = ctx->propertyName()) {
    auto& value2 = value.option.emplace<ast::PropertyReference>();
    AssignInputPosition(ctx, value2);
    BuildAST(ctx->valueExpressionPrimary(), *value2.element);
    BuildAST(ctx2->identifier(), value2.property);
  } else if (auto ctx2 = ctx->valueQueryExpression()) {
    BuildAST(ctx2->nestedQuerySpecification()->procedureBody(),
             *value.option.emplace<ast::ProcedureBodyPtr>());
  } else if (auto ctx2 = ctx->caseExpression()) {
    BuildAST(ctx2, value.option.emplace<ast::CaseExpression>());
  } else if (auto ctx2 = ctx->castSpecification()) {
    BuildAST(ctx2, value.option.emplace<ast::CastSpecification>());
  } else if (auto ctx2 = ctx->element_idFunction()) {
    BuildAST(ctx2, value.option.emplace<ast::ElementIdFunction>());
  } else if (auto ctx2 = ctx->letValueExpression()) {
    BuildAST(ctx2, value.option.emplace<ast::LetValueExpression>());
  } else {
    GQL_ASSERT(false);
  }
}

void BuildAST(
    GQLParser::NonParenthesizedValueExpressionPrimarySpecialCaseContext* ctx,
    ast::ValueExpression& value) {
  AssignInputPosition(ctx, value);
  ProcessNonParenthesizedValueExpressionPrimarySpecialCase(ctx, value);
}

void BuildAST(GQLParser::ValueExpressionPrimaryContext* ctx,
              ast::ValueExpression& value) {
  AssignInputPosition(ctx, value);
  if (auto ctx2 = ctx->parenthesizedValueExpression()) {
    BuildAST(ctx2->valueExpression(), value);
    value.isValueExpressionRule = true;
  } else if (auto ctx2 = ctx->bindingVariableReference()) {
    BuildAST(ctx2->bindingVariable(),
             value.option.emplace<ast::BindingVariableReference>());
  } else {
    ProcessNonParenthesizedValueExpressionPrimarySpecialCase(ctx, value);
  }
}

void BuildAST(GQLParser::DatetimeSubtractionContext* ctx,
              ast::DatetimeSubtraction& value) {
  AssignInputPosition(ctx, value);
  BuildAST(ctx->datetimeSubtractionParameters()
               ->datetimeValueExpression1()
               ->datetimeValueExpression()
               ->valueExpression(),
           *value.param1);
  BuildAST(ctx->datetimeSubtractionParameters()
               ->datetimeValueExpression2()
               ->datetimeValueExpression()
               ->valueExpression(),
           *value.param2);
  if (auto ctx2 = ctx->temporalDurationQualifier()) {
    value.qualifier = ctx2->YEAR() != nullptr
                          ? ast::TemporalDurationQualifier::YearToMonth
                          : ast::TemporalDurationQualifier::DayToSecond;
  }
}

void BuildAST(GQLParser::DatetimeValueFunctionContext* ctx,
              ast::DateTimeFunction& value) {
  AssignInputPosition(ctx, value);
  if (auto ctx2 = ctx->dateFunction()) {
    value.function = ast::DateTimeFunction::Function::DATE;
    if (auto ctx3 = ctx2->dateFunctionParameters()) {
      if (auto ctx4 = ctx3->dateString()) {
        BuildAST(
            ctx4->characterStringLiteral(),
            value.parameters.emplace().emplace<ast::CharacterStringLiteral>());
      } else {
        BuildAST(ctx3->recordConstructor(),
                 value.parameters.emplace().emplace<ast::RecordConstructor>());
      }
    }
  } else if (auto ctx2 = ctx->timeFunction()) {
    value.function = ast::DateTimeFunction::Function::ZONED_TIME;
    if (auto ctx3 = ctx2->timeFunctionParameters()) {
      if (auto ctx4 = ctx3->timeString()) {
        BuildAST(
            ctx4->characterStringLiteral(),
            value.parameters.emplace().emplace<ast::CharacterStringLiteral>());
      } else {
        BuildAST(ctx3->recordConstructor(),
                 value.parameters.emplace().emplace<ast::RecordConstructor>());
      }
    }
  } else if (auto ctx2 = ctx->datetimeFunction()) {
    value.function = ast::DateTimeFunction::Function::ZONED_DATETIME;
    if (auto ctx3 = ctx2->datetimeFunctionParameters()) {
      if (auto ctx4 = ctx3->datetimeString()) {
        BuildAST(
            ctx4->characterStringLiteral(),
            value.parameters.emplace().emplace<ast::CharacterStringLiteral>());
      } else {
        BuildAST(ctx3->recordConstructor(),
                 value.parameters.emplace().emplace<ast::RecordConstructor>());
      }
    }
  } else if (auto ctx2 = ctx->localtimeFunction()) {
    value.function = ast::DateTimeFunction::Function::LOCAL_TIME;
    if (auto ctx3 = ctx2->timeFunctionParameters()) {
      if (auto ctx4 = ctx3->timeString()) {
        BuildAST(
            ctx4->characterStringLiteral(),
            value.parameters.emplace().emplace<ast::CharacterStringLiteral>());
      } else {
        BuildAST(ctx3->recordConstructor(),
                 value.parameters.emplace().emplace<ast::RecordConstructor>());
      }
    }
  } else if (auto ctx2 = ctx->localdatetimeFunction()) {
    value.function = ast::DateTimeFunction::Function::LOCAL_DATETIME;
    if (auto ctx3 = ctx2->datetimeFunctionParameters()) {
      if (auto ctx4 = ctx3->datetimeString()) {
        BuildAST(
            ctx4->characterStringLiteral(),
            value.parameters.emplace().emplace<ast::CharacterStringLiteral>());
      } else {
        BuildAST(ctx3->recordConstructor(),
                 value.parameters.emplace().emplace<ast::RecordConstructor>());
      }
    }
  } else {
    GQL_ASSERT(false);
  }
}

void BuildAST(GQLParser::TrimListFunctionContext* ctx,
              ast::ValueExpression::Binary& value) {
  value.op = ast::ValueExpression::Binary::Op::TrimList;
  BuildAST(ctx->listValueExpression()->valueExpression(), *value.left);
  BuildAST(ctx->numericValueExpression(), *value.right);
}

void BuildAST(GQLParser::ElementsFunctionContext* ctx,
              ast::ValueExpression::Unary& value) {
  value.op = ast::ValueExpression::Unary::Op::Elements;
  BuildAST(ctx->pathValueExpression()->valueExpression(), *value.expr);
}

void BuildAST(GQLParser::CardinalityExpressionContext* ctx,
              ast::ValueExpression::Unary& value) {
  if (auto ctx2 = ctx->cardinalityExpressionArgument()) {
    value.op = ast::ValueExpression::Unary::Op::Cardinality;
    BuildAST(ctx2->valueExpression(), *value.expr);
  } else {
    value.op = ast::ValueExpression::Unary::Op::Size;
    BuildAST(ctx->listValueExpression()->valueExpression(), *value.expr);
  }
}

void BuildAST(GQLParser::SubCharacterOrByteStringContext* ctx,
              ast::SubCharacterOrByteString& value) {
  AssignInputPosition(ctx, value);
  value.direction = ctx->LEFT() != nullptr
                        ? ast::SubCharacterOrByteString::Direction::Left
                        : ast::SubCharacterOrByteString::Direction::Right;
  BuildAST(ctx->valueExpression(), *value.expr);
  BuildAST(ctx->stringLength()->numericValueExpression(), *value.length);
}

void BuildAST(GQLParser::TrimSingleCharacterOrByteStringContext* ctx,
              ast::TrimSingleCharacterOrByteString& value) {
  AssignInputPosition(ctx, value);
  auto trimCtx = ctx->trimOperands();
  if (auto ctx2 = trimCtx->trimSpecification()) {
    if (ctx2->LEADING() != nullptr) {
      value.specification = ast::TrimSpecification::LEADING;
    } else if (ctx2->TRAILING() != nullptr) {
      value.specification = ast::TrimSpecification::TRAILING;
    } else {
      value.specification = ast::TrimSpecification::BOTH;
    }
  }
  if (auto ctx3 = trimCtx->trimCharacterOrByteString()) {
    BuildAST(ctx3->valueExpression(), *value.trimString.emplace());
  }
  BuildAST(trimCtx->trimCharacterOrByteStringSource()->valueExpression(),
           *value.source);
}

void BuildAST(GQLParser::FoldCharacterStringContext* ctx,
              ast::FoldCharacterString& value) {
  AssignInputPosition(ctx, value);
  value.case_ = ctx->UPPER() != nullptr ? ast::FoldCharacterString::Case::Upper
                                        : ast::FoldCharacterString::Case::Lower;
  BuildAST(ctx->valueExpression(), *value.expr);
}

void BuildAST(GQLParser::TrimMultiCharacterCharacterStringContext* ctx,
              ast::TrimMultiCharacterCharacterString& value) {
  AssignInputPosition(ctx, value);
  if (ctx->BTRIM() != nullptr) {
    value.type = ast::TrimMultiCharacterCharacterString::TrimType::BTrim;
  } else if (ctx->LTRIM() != nullptr) {
    value.type = ast::TrimMultiCharacterCharacterString::TrimType::LTrim;
  } else {
    value.type = ast::TrimMultiCharacterCharacterString::TrimType::RTrim;
  }
  BuildAST(ctx->valueExpression(0), *value.source);
  if (auto ctx2 = ctx->valueExpression(1)) {
    BuildAST(ctx2, *value.trimString.emplace());
  }
}

void BuildAST(GQLParser::NormalizeCharacterStringContext* ctx,
              ast::NormalizeCharacterString& value) {
  AssignInputPosition(ctx, value);
  BuildAST(ctx->valueExpression(), *value.expr);
  if (auto ctx2 = ctx->normalForm()) {
    BuildAST(ctx2, value.form);
  }
}

void BuildAST(GQLParser::CharacterOrByteStringFunctionContext* ctx,
              ast::ValueFunction& value) {
  if (auto ctx2 = ctx->subCharacterOrByteString()) {
    BuildAST(ctx2, value.emplace<ast::SubCharacterOrByteString>());
  } else if (auto ctx2 = ctx->trimSingleCharacterOrByteString()) {
    BuildAST(ctx2, value.emplace<ast::TrimSingleCharacterOrByteString>());
  } else if (auto ctx2 = ctx->foldCharacterString()) {
    BuildAST(ctx2, value.emplace<ast::FoldCharacterString>());
  } else if (auto ctx2 = ctx->trimMultiCharacterCharacterString()) {
    BuildAST(ctx2, value.emplace<ast::TrimMultiCharacterCharacterString>());
  } else if (auto ctx2 = ctx->normalizeCharacterString()) {
    BuildAST(ctx2, value.emplace<ast::NormalizeCharacterString>());
  }
}

void BuildAST(GQLParser::TrigonometricFunctionContext* ctx,
              ast::ValueExpression::Unary& value) {
  auto name = ctx->trigonometricFunctionName();
  if (name->SIN() != nullptr) {
    value.op = ast::ValueExpression::Unary::Op::Sin;
  } else if (name->COS() != nullptr) {
    value.op = ast::ValueExpression::Unary::Op::Cos;
  } else if (name->TAN() != nullptr) {
    value.op = ast::ValueExpression::Unary::Op::Tan;
  } else if (name->COT() != nullptr) {
    value.op = ast::ValueExpression::Unary::Op::Cot;
  } else if (name->SINH() != nullptr) {
    value.op = ast::ValueExpression::Unary::Op::Sinh;
  } else if (name->COSH() != nullptr) {
    value.op = ast::ValueExpression::Unary::Op::Cosh;
  } else if (name->TANH() != nullptr) {
    value.op = ast::ValueExpression::Unary::Op::Tanh;
  } else if (name->ASIN() != nullptr) {
    value.op = ast::ValueExpression::Unary::Op::Asin;
  } else if (name->ACOS() != nullptr) {
    value.op = ast::ValueExpression::Unary::Op::Acos;
  } else if (name->ATAN() != nullptr) {
    value.op = ast::ValueExpression::Unary::Op::Atan;
  } else if (name->DEGREES() != nullptr) {
    value.op = ast::ValueExpression::Unary::Op::Degrees;
  } else if (name->RADIANS() != nullptr) {
    value.op = ast::ValueExpression::Unary::Op::Radians;
  }
  BuildAST(ctx->numericValueExpression(), *value.expr);
}

void BuildAST(GQLParser::NumericValueFunctionContext* ctx,
              ast::ValueExpression& value) {
  AssignInputPosition(ctx, value);
  if (auto ctx2 = ctx->lengthExpression()) {
    auto& value2 = value.option.emplace<ast::ValueExpression::Unary>();
    if (auto ctx3 = ctx2->charLengthExpression()) {
      value2.op = ast::ValueExpression::Unary::Op::CharLength;
      BuildAST(ctx3->characterStringValueExpression()->valueExpression(),
               *value2.expr);
    } else if (auto ctx3 = ctx2->byteLengthExpression()) {
      value2.op = ast::ValueExpression::Unary::Op::ByteLength;
      BuildAST(ctx3->byteStringValueExpression()->valueExpression(),
               *value2.expr);
    } else {
      value2.op = ast::ValueExpression::Unary::Op::PathLength;
      BuildAST(ctx2->pathLengthExpression()
                   ->pathValueExpression()
                   ->valueExpression(),
               *value2.expr);
    }
  } else if (auto ctx2 = ctx->cardinalityExpression()) {
    BuildAST(ctx2, value.option.emplace<ast::ValueExpression::Unary>());
  } else if (auto ctx2 = ctx->absoluteValueExpression()) {
    auto& value2 = value.option.emplace<ast::ValueExpression::Unary>();
    value2.op = ast::ValueExpression::Unary::Op::AbsoluteValue;
    BuildAST(ctx2->valueExpression(), *value2.expr);
  } else if (auto ctx2 = ctx->modulusExpression()) {
    auto& value2 = value.option.emplace<ast::ValueExpression::Binary>();
    value2.op = ast::ValueExpression::Binary::Op::Modulus;
    BuildAST(ctx2->numericValueExpressionDividend()->numericValueExpression(),
             *value2.left);
    BuildAST(ctx2->numericValueExpressionDivisor()->numericValueExpression(),
             *value2.right);
  } else if (auto ctx2 = ctx->trigonometricFunction()) {
    BuildAST(ctx2, value.option.emplace<ast::ValueExpression::Unary>());
  } else if (auto ctx2 = ctx->generalLogarithmFunction()) {
    auto& value2 = value.option.emplace<ast::ValueExpression::Binary>();
    value2.op = ast::ValueExpression::Binary::Op::GeneralLogarithm;
    BuildAST(ctx2->generalLogarithmBase()->numericValueExpression(),
             *value2.left);
    BuildAST(ctx2->generalLogarithmArgument()->numericValueExpression(),
             *value2.right);
  } else if (auto ctx2 = ctx->commonLogarithm()) {
    auto& value2 = value.option.emplace<ast::ValueExpression::Unary>();
    value2.op = ast::ValueExpression::Unary::Op::CommonLogarithm;
    BuildAST(ctx2->numericValueExpression(), *value2.expr);
  } else if (auto ctx2 = ctx->naturalLogarithm()) {
    auto& value2 = value.option.emplace<ast::ValueExpression::Unary>();
    value2.op = ast::ValueExpression::Unary::Op::NaturalLogarithm;
    BuildAST(ctx2->numericValueExpression(), *value2.expr);
  } else if (auto ctx2 = ctx->exponentialFunction()) {
    auto& value2 = value.option.emplace<ast::ValueExpression::Unary>();
    value2.op = ast::ValueExpression::Unary::Op::Exponential;
    BuildAST(ctx2->numericValueExpression(), *value2.expr);
  } else if (auto ctx2 = ctx->powerFunction()) {
    auto& value2 = value.option.emplace<ast::ValueExpression::Binary>();
    value2.op = ast::ValueExpression::Binary::Op::Power;
    BuildAST(ctx2->numericValueExpressionBase()->numericValueExpression(),
             *value2.left);
    BuildAST(ctx2->numericValueExpressionExponent()->numericValueExpression(),
             *value2.right);
  } else if (auto ctx2 = ctx->squareRoot()) {
    auto& value2 = value.option.emplace<ast::ValueExpression::Unary>();
    value2.op = ast::ValueExpression::Unary::Op::SquareRoot;
    BuildAST(ctx2->numericValueExpression(), *value2.expr);
  } else if (auto ctx2 = ctx->floorFunction()) {
    auto& value2 = value.option.emplace<ast::ValueExpression::Unary>();
    value2.op = ast::ValueExpression::Unary::Op::Floor;
    BuildAST(ctx2->numericValueExpression(), *value2.expr);
  } else if (auto ctx2 = ctx->ceilingFunction()) {
    auto& value2 = value.option.emplace<ast::ValueExpression::Unary>();
    value2.op = ast::ValueExpression::Unary::Op::Ceiling;
    BuildAST(ctx2->numericValueExpression(), *value2.expr);
  } else {
    GQL_ASSERT(false);
  }
}

void BuildAST(GQLParser::DurationValueFunctionContext* ctx,
              ast::ValueExpression& value) {
  AssignInputPosition(ctx, value);
  if (auto ctx2 = ctx->durationFunction()) {
    auto& value2 = value.option.emplace<ast::ValueFunction>()
                       .emplace<ast::DateTimeFunction>();
    value2.function = ast::DateTimeFunction::Function::DURATION;
    AssignInputPosition(ctx2, value2);
    if (auto ctx3 = ctx2->durationFunctionParameters()->durationString()) {
      BuildAST(
          ctx3->characterStringLiteral(),
          value2.parameters.emplace().emplace<ast::CharacterStringLiteral>());
    } else {
      BuildAST(ctx2->durationFunctionParameters()->recordConstructor(),
               value2.parameters.emplace().emplace<ast::RecordConstructor>());
    }
  } else {
    auto& value2 = value.option.emplace<ast::ValueExpression::Unary>();
    value2.op = ast::ValueExpression::Unary::Op::AbsoluteValue;
    BuildAST(ctx->absoluteValueExpression()->valueExpression(), *value2.expr);
  }
}
void BuildAST(GQLParser::ValueFunctionContext* ctx,
              ast::ValueExpression& value) {
  AssignInputPosition(ctx, value);
  if (auto ctx2 = ctx->numericValueFunction()) {
    BuildAST(ctx2, value);
  } else if (auto ctx2 = ctx->datetimeSubtraction()) {
    BuildAST(ctx2, value.option.emplace<ast::ValueFunction>()
                       .emplace<ast::DatetimeSubtraction>());
  } else if (auto ctx2 = ctx->datetimeValueFunction()) {
    BuildAST(ctx2, value.option.emplace<ast::ValueFunction>()
                       .emplace<ast::DateTimeFunction>());
  } else if (auto ctx2 = ctx->durationValueFunction()) {
    BuildAST(ctx2, value);
  } else if (auto ctx2 = ctx->characterOrByteStringFunction()) {
    BuildAST(ctx2, value.option.emplace<ast::ValueFunction>());
  } else if (auto ctx2 = ctx->listValueFunction()) {
    if (auto ctx3 = ctx2->trimListFunction()) {
      BuildAST(ctx3, value.option.emplace<ast::ValueExpression::Binary>());
    } else {
      BuildAST(ctx2->elementsFunction(),
               value.option.emplace<ast::ValueExpression::Unary>());
    }
  } else {
    GQL_ASSERT(false);
  }
}

void BuildAST(GQLParser::GraphExpressionContext* ctx,
              ast::GraphExpression& value) {
  AssignInputPosition(ctx, value);
  if (auto ctx2 = ctx->graphReference()) {
    BuildAST(ctx2, value.option.emplace<ast::GraphReference>());
  } else if (auto ctx2 = ctx->objectExpressionPrimary()) {
    BuildAST(ctx2, *value.option.emplace<ast::ValueExpressionPtr>());
  } else if (auto ctx2 = ctx->objectNameOrBindingVariable()) {
    BuildAST(ctx2->regularIdentifier(),
             value.option.emplace<ast::ObjectNameOrBindingVariable>());
  } else if (ctx->currentGraph()) {
    value.option.emplace<ast::CurrentGraph>();
  } else {
    GQL_ASSERT(false);
  }
}

void BuildAST(GQLParser::ExistsPredicateContext* ctx,
              ast::ExistsPredicate& value) {
  AssignInputPosition(ctx, value);
  if (auto ctx2 = ctx->graphPattern()) {
    BuildAST(ctx2, *value.option.emplace<ast::GraphPatternPtr>());
  } else if (auto ctx2 = ctx->matchStatementBlock()) {
    BuildAST(ctx2, *value.option.emplace<ast::MatchStatementBlockPtr>());
  } else if (auto ctx2 = ctx->nestedQuerySpecification()) {
    BuildAST(ctx2->procedureBody(),
             *value.option.emplace<ast::ProcedureBodyPtr>());
  } else {
    GQL_ASSERT(false);
  }
}

void BuildAST(GQLParser::NullPredicateContext* ctx, ast::NullPredicate& value) {
  AssignInputPosition(ctx, value);
  BuildAST(ctx->valueExpressionPrimary(), *value.expr);
  value.isNot = ctx->nullPredicatePart2()->NOT() != nullptr;
}

void BuildAST(GQLParser::ValueTypePredicateContext* ctx,
              ast::ValueTypePredicate& value) {
  AssignInputPosition(ctx, value);
  BuildAST(ctx->valueExpressionPrimary(), *value.expr);
  BuildAST(ctx->valueTypePredicatePart2()->valueType(), value.type);
  value.isNot = ctx->valueTypePredicatePart2()->NOT() != nullptr;
}

void BuildAST(GQLParser::DirectedPredicateContext* ctx,
              ast::DirectedPredicate& value) {
  AssignInputPosition(ctx, value);
  BuildAST(ctx->elementVariableReference(), value.element);
  value.isNot = ctx->directedPredicatePart2()->NOT() != nullptr;
}

void BuildAST(GQLParser::LabeledPredicateContext* ctx,
              ast::LabeledPredicate& value) {
  AssignInputPosition(ctx, value);
  BuildAST(ctx->elementVariableReference(), value.element);
  BuildAST(ctx->labeledPredicatePart2()->labelExpression(), value.label);
  value.isNot =
      ctx->labeledPredicatePart2()->isLabeledOrColon()->NOT() != nullptr;
}

void BuildAST(GQLParser::SourceDestinationPredicateContext* ctx,
              ast::SourceDestinationPredicate& value) {
  AssignInputPosition(ctx, value);
  BuildAST(ctx->nodeReference()->elementVariableReference(), value.node);
  if (auto ctx2 = ctx->sourcePredicatePart2()) {
    value.kind = ast::SourceDestinationPredicate::Kind::NodeIsSourceOfEdge;
    value.isNot = ctx2->NOT() != nullptr;
    BuildAST(ctx2->edgeReference()->elementVariableReference(), value.edge);
  } else {
    auto ctx3 = ctx->destinationPredicatePart2();
    value.kind = ast::SourceDestinationPredicate::Kind::NodeIsDestinationOfEdge;
    value.isNot = ctx3->NOT() != nullptr;
    BuildAST(ctx3->edgeReference()->elementVariableReference(), value.edge);
  }
}

void BuildAST(GQLParser::All_differentPredicateContext* ctx,
              ast::AllElementsPredicate& value) {
  AssignInputPosition(ctx, value);
  value.kind = ast::AllElementsPredicate::Kind::AllDifferent;
  for (auto element : ctx->elementVariableReference()) {
    BuildAST(element->bindingVariableReference()->bindingVariable(),
             value.elements.emplace_back());
  }
}

void BuildAST(GQLParser::SamePredicateContext* ctx,
              ast::AllElementsPredicate& value) {
  AssignInputPosition(ctx, value);
  value.kind = ast::AllElementsPredicate::Kind::Same;
  for (auto element : ctx->elementVariableReference()) {
    BuildAST(element->bindingVariableReference()->bindingVariable(),
             value.elements.emplace_back());
  }
}

void BuildAST(GQLParser::Property_existsPredicateContext* ctx,
              ast::PropertyExistsPredicate& value) {
  AssignInputPosition(ctx, value);
  BuildAST(ctx->elementVariableReference(), value.element);
  BuildAST(ctx->propertyName()->identifier(), value.property);
}

void BuildAST(GQLParser::PredicateContext* ctx, ast::Predicate& value) {
  if (auto ctx2 = ctx->existsPredicate()) {
    BuildAST(ctx2, value.emplace<ast::ExistsPredicate>());
  } else if (auto ctx2 = ctx->nullPredicate()) {
    BuildAST(ctx2, value.emplace<ast::NullPredicate>());
  } else if (auto ctx2 = ctx->valueTypePredicate()) {
    BuildAST(ctx2, value.emplace<ast::ValueTypePredicate>());
  } else if (auto ctx2 = ctx->directedPredicate()) {
    BuildAST(ctx2, value.emplace<ast::DirectedPredicate>());
  } else if (auto ctx2 = ctx->labeledPredicate()) {
    BuildAST(ctx2, value.emplace<ast::LabeledPredicate>());
  } else if (auto ctx2 = ctx->sourceDestinationPredicate()) {
    BuildAST(ctx2, value.emplace<ast::SourceDestinationPredicate>());
  } else if (auto ctx2 = ctx->all_differentPredicate()) {
    BuildAST(ctx2, value.emplace<ast::AllElementsPredicate>());
  } else if (auto ctx2 = ctx->samePredicate()) {
    BuildAST(ctx2, value.emplace<ast::AllElementsPredicate>());
  } else if (auto ctx2 = ctx->property_existsPredicate()) {
    BuildAST(ctx2, value.emplace<ast::PropertyExistsPredicate>());
  }
}

void BuildAST(GQLParser::ValueExpressionContext* ctx,
              ast::ValueExpression& value) {
  AssignInputPosition(ctx, value);
  if (auto ctx2 = dynamic_cast<GQLParser::SignedExprAltContext*>(ctx)) {
    auto& value2 = value.option.emplace<ast::ValueExpression::Unary>();
    BuildAST(ctx2->valueExpression(), *value2.expr);
    value2.op = (ctx2->PLUS_SIGN() != nullptr)
                    ? ast::ValueExpression::Unary::Op::Positive
                    : ast::ValueExpression::Unary::Op::Negative;
  } else if (auto ctx2 = dynamic_cast<GQLParser::MultDivExprAltContext*>(ctx)) {
    auto& value2 = value.option.emplace<ast::ValueExpression::Binary>();
    BuildAST(ctx2->valueExpression(0), *value2.left);
    BuildAST(ctx2->valueExpression(1), *value2.right);
    value2.op = (ctx2->ASTERISK() != nullptr)
                    ? ast::ValueExpression::Binary::Op::Multiply
                    : ast::ValueExpression::Binary::Op::Divide;
  } else if (auto ctx2 =
                 dynamic_cast<GQLParser::AddSubtractExprAltContext*>(ctx)) {
    auto& value2 = value.option.emplace<ast::ValueExpression::Binary>();
    BuildAST(ctx2->valueExpression(0), *value2.left);
    BuildAST(ctx2->valueExpression(1), *value2.right);
    value2.op = (ctx2->PLUS_SIGN() != nullptr)
                    ? ast::ValueExpression::Binary::Op::Add
                    : ast::ValueExpression::Binary::Op::Subtract;
  } else if (auto ctx2 =
                 dynamic_cast<GQLParser::ConcatenationExprAltContext*>(ctx)) {
    auto& value2 = value.option.emplace<ast::ValueExpression::Binary>();
    BuildAST(ctx2->valueExpression(0), *value2.left);
    BuildAST(ctx2->valueExpression(1), *value2.right);
    value2.op = ast::ValueExpression::Binary::Op::Concatenate;
  } else if (auto ctx2 =
                 dynamic_cast<GQLParser::ComparisonExprAltContext*>(ctx)) {
    auto& value2 = value.option.emplace<ast::ValueExpression::Comparison>();
    BuildAST(ctx2->valueExpression(0), *value2.left);
    BuildAST(ctx2->valueExpression(1), *value2.right);
    BuildAST(ctx2->compOp(), value2.op);
  } else if (auto ctx2 =
                 dynamic_cast<GQLParser::PredicateExprAltContext*>(ctx)) {
    BuildAST(ctx2->predicate(), value.option.emplace<ast::Predicate>());
  } else if (auto ctx2 =
                 dynamic_cast<GQLParser::NormalizedPredicateExprAltContext*>(
                     ctx)) {
    auto& value2 = value.option.emplace<ast::NormalizedPredicate>();
    BuildAST(ctx2->normalizedPredicatePart2(), value2);
    BuildAST(ctx2->valueExpression(), *value2.expr);
  } else if (auto ctx2 = dynamic_cast<GQLParser::IsNotExprAltContext*>(ctx)) {
    auto& value2 = value.option.emplace<ast::ValueExpression::Is>();
    BuildAST(ctx2->valueExpression(), *value2.expr);
    value2.isNot = ctx2->NOT() != nullptr;
    auto truthValue = ctx2->truthValue()->BOOLEAN_LITERAL()->getText();
    if (truthValue == "TRUE") {
      value2.value = ast::TruthValue::TRUE;
    } else if (truthValue == "FALSE") {
      value2.value = ast::TruthValue::FALSE;
    } else {
      value2.value = ast::TruthValue::UNKNOWN;
    }
  } else if (auto ctx2 = dynamic_cast<GQLParser::NotExprAltContext*>(ctx)) {
    auto& value2 = value.option.emplace<ast::ValueExpression::Unary>();
    BuildAST(ctx2->valueExpression(), *value2.expr);
    value2.op = ast::ValueExpression::Unary::Op::BoolNot;
  } else if (auto ctx2 =
                 dynamic_cast<GQLParser::ConjunctiveExprAltContext*>(ctx)) {
    auto& value2 = value.option.emplace<ast::ValueExpression::Binary>();
    BuildAST(ctx2->valueExpression(0), *value2.left);
    BuildAST(ctx2->valueExpression(1), *value2.right);
    value2.op = ast::ValueExpression::Binary::Op::BoolAnd;
  } else if (auto ctx2 =
                 dynamic_cast<GQLParser::DisjunctiveExprAltContext*>(ctx)) {
    auto& value2 = value.option.emplace<ast::ValueExpression::Binary>();
    BuildAST(ctx2->valueExpression(0), *value2.left);
    BuildAST(ctx2->valueExpression(1), *value2.right);
    value2.op = (ctx2->OR() != nullptr)
                    ? ast::ValueExpression::Binary::Op::BoolOr
                    : ast::ValueExpression::Binary::Op::BoolXor;
  } else if (auto ctx2 =
                 dynamic_cast<GQLParser::PropertyGraphExprAltContext*>(ctx)) {
    BuildAST(ctx2->graphExpression(),
             value.option.emplace<ast::GraphExpression>());
  } else if (auto ctx2 =
                 dynamic_cast<GQLParser::BindingTableExprAltContext*>(ctx)) {
    BuildAST(ctx2->bindingTableExpression(),
             value.option.emplace<ast::BindingTableExpression>());
  } else if (auto ctx2 =
                 dynamic_cast<GQLParser::ValueFunctionExprAltContext*>(ctx)) {
    BuildAST(ctx2->valueFunction(), value);
  } else if (auto ctx2 = dynamic_cast<GQLParser::PrimaryExprAltContext*>(ctx)) {
    BuildAST(ctx2->valueExpressionPrimary(), value);
  } else {
    GQL_ASSERT(false);
  }
}

void BuildAST(GQLParser::ParenthesizedPathPatternExpressionContext* ctx,
              ast::ParenthesizedPathPatternExpression& value);

void BuildAST(GQLParser::SimplifiedTertiaryContext* ctx,
              ast::SimplifiedTertiary& value);

void BuildAST(GQLParser::PropertyKeyValuePairListContext* ctx,
              ast::PropertyKeyValuePairList& value) {
  for (auto ctx2 : ctx->propertyKeyValuePair()) {
    auto& value2 = value.emplace_back();
    AssignInputPosition(ctx2, value2);
    BuildAST(ctx2->propertyName()->identifier(), value2.name);
    BuildAST(ctx2->valueExpression(), value2.value);
    value2.value.isValueExpressionRule = true;
  }
}

void BuildAST(GQLParser::ElementPatternFillerContext* ctx,
              ast::ElementPatternFiller& value) {
  AssignInputPosition(ctx, value);
  if (auto ctx2 = ctx->elementVariableDeclaration()) {
    auto& value2 = value.var.emplace();
    AssignInputPosition(ctx2, value2);
    BuildAST(ctx2->elementVariable()->bindingVariable(), value2);
  }
  if (auto ctx2 = ctx->isLabelExpression()) {
    BuildAST(ctx2->labelExpression(), value.labelExpr.emplace());
  }
  if (auto ctx2 = ctx->elementPatternPredicate()) {
    if (auto ctx3 = ctx2->elementPatternWhereClause()) {
      if (!value.var) {
        // 16.7.21 If an <element pattern> EP that contains an <element
        // pattern where clause> EPWC, then EP shall simply contain an
        // <element variable declaration> GPVD.
        auto token = ctx3->getStart();
        throw FormattedError({token->getLine(), token->getCharPositionInLine()},
                             ErrorCode::E0084,
                             "Element pattern containing WHERE clause must "
                             "contain an element variable declaration");
      }

      auto& value2 =
          value.predicate.emplace().emplace<ast::ElementPatternWhereClause>();
      AssignInputPosition(ctx3, value2);
      BuildAST(
          ctx3->searchCondition()->booleanValueExpression()->valueExpression(),
          *value2.condition);
    } else {
      auto& value2 = value.predicate.emplace()
                         .emplace<ast::ElementPropertySpecification>();
      AssignInputPosition(ctx2, value2);
      BuildAST(ctx2->elementPropertySpecification()->propertyKeyValuePairList(),
               value2.props);
    }
  }
}

void BuildAST(GQLParser::EdgePatternContext* ctx, ast::EdgePattern& value) {
  AssignInputPosition(ctx, value);
  if (auto ctx2 = ctx->fullEdgePattern()) {
    if (auto ctx3 = ctx2->fullEdgePointingLeft()) {
      value.direction = ast::EdgeDirectionPattern::Left;
      BuildAST(ctx3->elementPatternFiller(), value.filler);
    } else if (auto ctx3 = ctx2->fullEdgeUndirected()) {
      value.direction = ast::EdgeDirectionPattern::Undirected;
      BuildAST(ctx3->elementPatternFiller(), value.filler);
    } else if (auto ctx3 = ctx2->fullEdgePointingRight()) {
      value.direction = ast::EdgeDirectionPattern::Right;
      BuildAST(ctx3->elementPatternFiller(), value.filler);
    } else if (auto ctx3 = ctx2->fullEdgeLeftOrUndirected()) {
      value.direction = ast::EdgeDirectionPattern::LeftOrUndirected;
      BuildAST(ctx3->elementPatternFiller(), value.filler);
    } else if (auto ctx3 = ctx2->fullEdgeUndirectedOrRight()) {
      value.direction = ast::EdgeDirectionPattern::UndirectedOrRight;
      BuildAST(ctx3->elementPatternFiller(), value.filler);
    } else if (auto ctx3 = ctx2->fullEdgeLeftOrRight()) {
      value.direction = ast::EdgeDirectionPattern::LeftOrRight;
      BuildAST(ctx3->elementPatternFiller(), value.filler);
    } else if (auto ctx3 = ctx2->fullEdgeAnyDirection()) {
      value.direction = ast::EdgeDirectionPattern::AnyDirection;
      BuildAST(ctx3->elementPatternFiller(), value.filler);
    }
  } else if (auto ctx2 = ctx->abbreviatedEdgePattern()) {
    if (ctx2->LEFT_ARROW()) {
      value.direction = ast::EdgeDirectionPattern::Left;
    } else if (ctx2->TILDE()) {
      value.direction = ast::EdgeDirectionPattern::Undirected;
    } else if (ctx2->RIGHT_ARROW()) {
      value.direction = ast::EdgeDirectionPattern::Right;
    } else if (ctx2->LEFT_ARROW_TILDE()) {
      value.direction = ast::EdgeDirectionPattern::LeftOrUndirected;
    } else if (ctx2->TILDE_RIGHT_ARROW()) {
      value.direction = ast::EdgeDirectionPattern::UndirectedOrRight;
    } else if (ctx2->LEFT_MINUS_RIGHT()) {
      value.direction = ast::EdgeDirectionPattern::LeftOrRight;
    } else if (ctx2->MINUS_SIGN()) {
      value.direction = ast::EdgeDirectionPattern::AnyDirection;
    }
  }
}

void BuildAST(GQLParser::NodePatternContext* ctx, ast::NodePattern& value) {
  AssignInputPosition(ctx, value);
  BuildAST(ctx->elementPatternFiller(), value.filler);
}

void BuildAST(GQLParser::ElementPatternContext* ctx,
              ast::ElementPattern& value) {
  if (auto ctx2 = ctx->nodePattern()) {
    BuildAST(ctx2, value.emplace<ast::NodePattern>());
  } else {
    BuildAST(ctx->edgePattern(), value.emplace<ast::EdgePattern>());
  }
}

void BuildAST(GQLParser::PathModeContext* ctx, ast::PathMode& value) {
  if (ctx->WALK()) {
    value = ast::PathMode::WALK;
  } else if (ctx->TRAIL()) {
    value = ast::PathMode::TRAIL;
  } else if (ctx->SIMPLE()) {
    value = ast::PathMode::SIMPLE;
  } else if (ctx->ACYCLIC()) {
    value = ast::PathMode::ACYCLIC;
  }
}

void BuildAST(GQLParser::GraphPatternQuantifierContext* ctx,
              ast::GraphPatternQuantifier& value) {
  if (auto ctx2 = ctx->generalQuantifier()) {
    if (auto ctx3 = ctx2->lowerBound()) {
      BuildAST(ctx3->unsignedInteger(), value.lower);
    }
    if (auto ctx3 = ctx2->upperBound()) {
      BuildAST(ctx3->unsignedInteger(), value.upper.emplace());
    }
  } else if (auto ctx2 = ctx->fixedQuantifier()) {
    BuildAST(ctx2->unsignedInteger(), value.lower);
    value.upper = value.lower;
  } else if (ctx->ASTERISK()) {
  } else if (ctx->PLUS_SIGN()) {
    value.lower = 1;
  }
}

void BuildAST(GQLParser::SimplifiedFactorHighContext* ctx,
              ast::SimplifiedFactorHigh& value) {
  AssignInputPosition(ctx, value);
  if (auto ctx2 = ctx->simplifiedTertiary()) {
    BuildAST(ctx2, *value.tertiary);
  } else if (auto ctx2 = ctx->simplifiedQuantified()) {
    BuildAST(ctx2->simplifiedTertiary(), *value.tertiary);
    BuildAST(ctx2->graphPatternQuantifier(),
             value.quantifier.emplace<ast::GraphPatternQuantifier>());
  } else if (auto ctx2 = ctx->simplifiedQuestioned()) {
    BuildAST(ctx2->simplifiedTertiary(), *value.tertiary);
    value.quantifier = ast::SimplifiedFactorHigh::Optional{};
  }
}

void BuildAST(GQLParser::SimplifiedFactorLowContext* ctx,
              ast::SimplifiedFactorLow& value) {
  AssignInputPosition(ctx, value);
  if (auto ctx2 =
          dynamic_cast<GQLParser::SimplifiedFactorHighLabelContext*>(ctx)) {
    BuildAST(ctx2->simplifiedFactorHigh(), value.factors.emplace_back());
  } else {
    auto ctx3 =
        dynamic_cast<GQLParser::SimplifiedConjunctionLabelContext*>(ctx);
    BuildAST(ctx3->simplifiedFactorLow(), value);
    BuildAST(ctx3->simplifiedFactorHigh(), value.factors.emplace_back());
  }
}

void BuildAST(GQLParser::SimplifiedTermContext* ctx,
              ast::SimplifiedTerm& value) {
  AssignInputPosition(ctx, value);
  if (auto ctx2 =
          dynamic_cast<GQLParser::SimplifiedFactorLowLabelContext*>(ctx)) {
    BuildAST(ctx2->simplifiedFactorLow(), value.factors.emplace_back());
  } else {
    auto ctx3 =
        dynamic_cast<GQLParser::SimplifiedConcatenationLabelContext*>(ctx);
    BuildAST(ctx3->simplifiedTerm(), value);
    BuildAST(ctx3->simplifiedFactorLow(), value.factors.emplace_back());
  }
}

void BuildAST(GQLParser::SimplifiedContentsContext* ctx,
              ast::SimplifiedContents& value) {
  AssignInputPosition(ctx, value);
  if (auto ctx2 = ctx->simplifiedTerm()) {
    BuildAST(ctx2, value.terms.emplace_back());
  } else if (auto ctx2 = ctx->simplifiedPathUnion()) {
    value.op = ast::SimplifiedContents::Op::Union;
    for (auto ctx3 : ctx2->simplifiedTerm()) {
      BuildAST(ctx3, value.terms.emplace_back());
    }
  } else if (auto ctx2 = ctx->simplifiedMultisetAlternation()) {
    value.op = ast::SimplifiedContents::Op::MultisetAlternation;
    for (auto ctx3 : ctx2->simplifiedTerm()) {
      BuildAST(ctx3, value.terms.emplace_back());
    }
  }
}

void BuildAST(GQLParser::SimplifiedPrimaryContext* ctx,
              ast::SimplifiedPrimary& value) {
  if (auto ctx2 = ctx->labelName()) {
    BuildAST(ctx2->identifier(), value.emplace<ast::LabelName>());
  } else {
    BuildAST(ctx->simplifiedContents(),
             value.emplace<ast::SimplifiedContents>());
  }
}

void BuildAST(GQLParser::SimplifiedSecondaryContext* ctx,
              ast::SimplifiedTertiary& value) {
  AssignInputPosition(ctx, value);
  if (auto ctx2 = ctx->simplifiedPrimary()) {
    BuildAST(ctx2, value.primary);
  } else {
    value.isNegation = true;
    BuildAST(ctx->simplifiedNegation()->simplifiedPrimary(), value.primary);
  }
}

void BuildAST(GQLParser::SimplifiedTertiaryContext* ctx,
              ast::SimplifiedTertiary& value) {
  AssignInputPosition(ctx, value);
  if (auto ctx2 = ctx->simplifiedDirectionOverride()) {
    if (auto ctx3 = ctx2->simplifiedOverrideLeft()) {
      value.direction = ast::EdgeDirectionPattern::Left;
      BuildAST(ctx3->simplifiedSecondary(), value);
    } else if (auto ctx3 = ctx2->simplifiedOverrideUndirected()) {
      value.direction = ast::EdgeDirectionPattern::Undirected;
      BuildAST(ctx3->simplifiedSecondary(), value);
    } else if (auto ctx3 = ctx2->simplifiedOverrideRight()) {
      value.direction = ast::EdgeDirectionPattern::Right;
      BuildAST(ctx3->simplifiedSecondary(), value);
    } else if (auto ctx3 = ctx2->simplifiedOverrideLeftOrUndirected()) {
      value.direction = ast::EdgeDirectionPattern::LeftOrUndirected;
      BuildAST(ctx3->simplifiedSecondary(), value);
    } else if (auto ctx3 = ctx2->simplifiedOverrideUndirectedOrRight()) {
      value.direction = ast::EdgeDirectionPattern::UndirectedOrRight;
      BuildAST(ctx3->simplifiedSecondary(), value);
    } else if (auto ctx3 = ctx2->simplifiedOverrideLeftOrRight()) {
      value.direction = ast::EdgeDirectionPattern::LeftOrRight;
      BuildAST(ctx3->simplifiedSecondary(), value);
    } else if (auto ctx3 = ctx2->simplifiedOverrideAnyDirection()) {
      value.direction = ast::EdgeDirectionPattern::AnyDirection;
      BuildAST(ctx3->simplifiedSecondary(), value);
    }
  } else {
    BuildAST(ctx->simplifiedSecondary(), value);
  }
}

void BuildAST(GQLParser::SimplifiedPathPatternExpressionContext* ctx,
              ast::SimplifiedPathPatternExpression& value) {
  AssignInputPosition(ctx, value);
  if (auto ctx2 = ctx->simplifiedDefaultingLeft()) {
    value.direction = ast::EdgeDirectionPattern::Left;
    BuildAST(ctx2->simplifiedContents(), value.contents);
  } else if (auto ctx2 = ctx->simplifiedDefaultingUndirected()) {
    value.direction = ast::EdgeDirectionPattern::Undirected;
    BuildAST(ctx2->simplifiedContents(), value.contents);
  } else if (auto ctx2 = ctx->simplifiedDefaultingRight()) {
    value.direction = ast::EdgeDirectionPattern::Right;
    BuildAST(ctx2->simplifiedContents(), value.contents);
  } else if (auto ctx2 = ctx->simplifiedDefaultingLeftOrUndirected()) {
    value.direction = ast::EdgeDirectionPattern::LeftOrUndirected;
    BuildAST(ctx2->simplifiedContents(), value.contents);
  } else if (auto ctx2 = ctx->simplifiedDefaultingUndirectedOrRight()) {
    value.direction = ast::EdgeDirectionPattern::UndirectedOrRight;
    BuildAST(ctx2->simplifiedContents(), value.contents);
  } else if (auto ctx2 = ctx->simplifiedDefaultingLeftOrRight()) {
    value.direction = ast::EdgeDirectionPattern::LeftOrRight;
    BuildAST(ctx2->simplifiedContents(), value.contents);
  } else if (auto ctx2 = ctx->simplifiedDefaultingAnyDirection()) {
    value.direction = ast::EdgeDirectionPattern::AnyDirection;
    BuildAST(ctx2->simplifiedContents(), value.contents);
  }
}

void BuildAST(GQLParser::PathPrimaryContext* ctx, ast::PathPrimary& value) {
  if (auto ctx2 = dynamic_cast<GQLParser::PpElementPatternContext*>(ctx)) {
    BuildAST(ctx2->elementPattern(), value.emplace<ast::ElementPattern>());
  } else if (auto ctx2 = dynamic_cast<
                 GQLParser::PpParenthesizedPathPatternExpressionContext*>(
                 ctx)) {
    BuildAST(ctx2->parenthesizedPathPatternExpression(),
             *value.emplace<ast::ParenthesizedPathPatternExpressionPtr>());
  } else if (auto ctx2 = dynamic_cast<
                 GQLParser::PpSimplifiedPathPatternExpressionContext*>(ctx)) {
    BuildAST(ctx2->simplifiedPathPatternExpression(),
             value.emplace<ast::SimplifiedPathPatternExpression>());
  }
}

void BuildAST(GQLParser::PathFactorContext* ctx, ast::PathFactor& value) {
  AssignInputPosition(ctx, value);
  if (auto ctx2 = dynamic_cast<GQLParser::PfPathPrimaryContext*>(ctx)) {
    value.quantifier = ast::PathFactor::NoQuantifier{};
    BuildAST(ctx2->pathPrimary(), value.pattern);
  } else if (auto ctx2 =
                 dynamic_cast<GQLParser::PfQuantifiedPathPrimaryContext*>(
                     ctx)) {
    BuildAST(ctx2->graphPatternQuantifier(),
             value.quantifier.emplace<ast::GraphPatternQuantifier>());
    BuildAST(ctx2->pathPrimary(), value.pattern);
  } else if (auto ctx2 =
                 dynamic_cast<GQLParser::PfQuestionedPathPrimaryContext*>(
                     ctx)) {
    value.quantifier = ast::PathFactor::Optional{};
    BuildAST(ctx2->pathPrimary(), value.pattern);
  }
}

void BuildAST(GQLParser::PathTermContext* ctx, ast::PathTerm& value) {
  for (auto ctx2 : ctx->pathFactor()) {
    BuildAST(ctx2, value.emplace_back());
  }
}

void BuildAST(GQLParser::PathPatternExpressionContext* ctx,
              ast::PathPatternExpression& value) {
  AssignInputPosition(ctx, value);
  if (auto ctx2 = dynamic_cast<GQLParser::PpePathTermContext*>(ctx)) {
    BuildAST(ctx2->pathTerm(), value.terms.emplace_back());
  } else if (auto ctx2 =
                 dynamic_cast<GQLParser::PpeMultisetAlternationContext*>(ctx)) {
    value.op = ast::PathPatternExpression::Op::MultisetAlternation;
    for (auto ctx3 : ctx2->pathTerm()) {
      BuildAST(ctx3, value.terms.emplace_back());
    }
  } else if (auto ctx2 =
                 dynamic_cast<GQLParser::PpePatternUnionContext*>(ctx)) {
    value.op = ast::PathPatternExpression::Op::Union;
    for (auto ctx3 : ctx2->pathTerm()) {
      BuildAST(ctx3, value.terms.emplace_back());
    }
  }
}

void BuildAST(GQLParser::ParenthesizedPathPatternExpressionContext* ctx,
              ast::ParenthesizedPathPatternExpression& value) {
  AssignInputPosition(ctx, value);
  if (auto ctx2 = ctx->subpathVariableDeclaration()) {
    BuildAST(ctx2->subpathVariable()->regularIdentifier(), value.var.emplace());
  }
  if (auto ctx2 = ctx->pathModePrefix()) {
    BuildAST(ctx2->pathMode(), value.pathMode);
  }
  BuildAST(ctx->pathPatternExpression(), value.pattern);
  if (auto ctx2 = ctx->parenthesizedPathPatternWhereClause()) {
    BuildAST(
        ctx2->searchCondition()->booleanValueExpression()->valueExpression(),
        *value.where.emplace().condition);
  }
}

void BuildAST(GQLParser::StatementContext* ctx, ast::Statement& value);

void BuildAST(GQLParser::MatchModeContext* ctx, ast::MatchMode& value) {
  if (ctx->repeatableElementsMatchMode() != nullptr) {
    value = ast::MatchMode::RepeatableElements;
  } else {
    value = ast::MatchMode::DifferentEdges;
  }
}

void BuildAST(GQLParser::PathPatternPrefixContext* ctx,
              ast::PathPatternPrefix& value) {
  AssignInputPosition(ctx, value);
  if (auto ctx2 = ctx->pathModePrefix()) {
    BuildAST(ctx2->pathMode(), value.mode);
  } else if (auto ctx2 = ctx->pathSearchPrefix()) {
    auto& pathSearch = value.pathSearchPrefix.emplace();
    if (auto ctx3 = ctx2->allPathSearch()) {
      pathSearch.search = ast::PathSearchPrefix::Search::All;
      if (auto ctx4 = ctx3->pathMode()) {
        BuildAST(ctx4, value.mode);
      }
    } else if (auto ctx3 = ctx2->anyPathSearch()) {
      pathSearch.search = ast::PathSearchPrefix::Search::Any;
      if (auto ctx4 = ctx3->numberOfPaths()) {
        BuildAST(ctx4->nonNegativeIntegerSpecification(), pathSearch.number);
      }
      if (auto ctx4 = ctx3->pathMode()) {
        BuildAST(ctx4, value.mode);
      }
    } else if (auto ctx3 = ctx2->shortestPathSearch()) {
      if (auto ctx4 = ctx3->allShortestPathSearch()) {
        pathSearch.search = ast::PathSearchPrefix::Search::CountedShortestGroup;
        pathSearch.number = 1u;
        if (auto ctx5 = ctx4->pathMode()) {
          BuildAST(ctx5, value.mode);
        }
      } else if (auto ctx4 = ctx3->anyShortestPathSearch()) {
        pathSearch.search = ast::PathSearchPrefix::Search::CountedShortestPath;
        pathSearch.number = 1u;
        if (auto ctx5 = ctx4->pathMode()) {
          BuildAST(ctx5, value.mode);
        }
      } else if (auto ctx4 = ctx3->countedShortestPathSearch()) {
        pathSearch.search = ast::PathSearchPrefix::Search::CountedShortestPath;
        BuildAST(ctx4->numberOfPaths()->nonNegativeIntegerSpecification(),
                 pathSearch.number);
        if (auto ctx5 = ctx4->pathMode()) {
          BuildAST(ctx5, value.mode);
        }
      } else if (auto ctx4 = ctx3->countedShortestGroupSearch()) {
        pathSearch.search = ast::PathSearchPrefix::Search::CountedShortestGroup;
        if (auto ctx5 = ctx4->numberOfGroups()) {
          BuildAST(ctx5->nonNegativeIntegerSpecification(), pathSearch.number);
        }
        if (auto ctx5 = ctx4->pathMode()) {
          BuildAST(ctx5, value.mode);
        }
      }
    }
  }
}

void BuildAST(GQLParser::PathPatternContext* ctx, ast::PathPattern& value) {
  AssignInputPosition(ctx, value);
  if (auto ctx2 = ctx->pathVariableDeclaration()) {
    BuildAST(ctx2->pathVariable()->bindingVariable(), value.var.emplace());
  }
  if (auto ctx2 = ctx->pathPatternPrefix()) {
    BuildAST(ctx2, value.prefix.emplace());
  }
  BuildAST(ctx->pathPatternExpression(), value.expr);
}

void BuildAST(GQLParser::PathPatternListContext* ctx,
              ast::PathPatternList& value) {
  for (auto ctx2 : ctx->pathPattern()) {
    BuildAST(ctx2, value.emplace_back());
  }
}

void BuildAST(GQLParser::GraphPatternWhereClauseContext* ctx,
              ast::GraphPatternWhereClause& value) {
  AssignInputPosition(ctx, value);
  BuildAST(ctx->searchCondition()->booleanValueExpression()->valueExpression(),
           *value.condition);
}

void BuildAST(GQLParser::GraphPatternContext* ctx, ast::GraphPattern& value) {
  AssignInputPosition(ctx, value);
  if (auto ctx2 = ctx->matchMode()) {
    BuildAST(ctx2, value.matchMode.emplace());
  }
  BuildAST(ctx->pathPatternList(), value.paths);
  if (auto ctx2 = ctx->keepClause()) {
    BuildAST(ctx2->pathPatternPrefix(), value.keep.emplace());
  }
  if (auto ctx2 = ctx->graphPatternWhereClause()) {
    BuildAST(ctx2, value.where.emplace());
  }
}

void BuildAST(GQLParser::SimpleMatchStatementContext* ctx,
              ast::SimpleMatchStatement& value) {
  AssignInputPosition(ctx, value);
  auto ctx2 = ctx->graphPatternBindingTable();
  BuildAST(ctx2->graphPattern(), value.pattern);
  if (auto ctx3 = ctx2->graphPatternYieldClause()) {
    auto& yield = value.yield.emplace();
    auto ctx4 = ctx3->graphPatternYieldItemList();
    for (auto ctx5 : ctx4->graphPatternYieldItem()) {
      BuildAST(ctx5->bindingVariableReference()->bindingVariable(),
               yield.emplace_back());
    }
  }
}

void BuildAST(GQLParser::OptionalMatchStatementContext* ctx,
              ast::OptionalMatchStatement& value) {
  AssignInputPosition(ctx, value);
  auto ctx2 = ctx->optionalOperand();
  if (auto ctx3 = ctx2->simpleMatchStatement()) {
    BuildAST(ctx3, value.statements->statements.emplace_back()
                       .emplace<ast::SimpleMatchStatement>());
  } else if (auto ctx3 = ctx2->matchStatementBlock()) {
    BuildAST(ctx3, *value.statements);
  }
}

void BuildAST(GQLParser::MatchStatementContext* ctx,
              ast::MatchStatement& value) {
  if (auto ctx2 = ctx->simpleMatchStatement()) {
    BuildAST(ctx2, value.emplace<ast::SimpleMatchStatement>());
  } else {
    BuildAST(ctx->optionalMatchStatement(),
             value.emplace<ast::OptionalMatchStatement>());
  }
}

void BuildAST(GQLParser::MatchStatementBlockContext* ctx,
              ast::MatchStatementBlock& value) {
  AssignInputPosition(ctx, value);
  for (auto ctx2 : ctx->matchStatement()) {
    BuildAST(ctx2, value.statements.emplace_back());
  }
}

void BuildAST(GQLParser::CreateSchemaStatementContext* ctx,
              ast::CreateSchemaStatement& value) {
  AssignInputPosition(ctx, value);
  BuildAST(ctx->catalogSchemaParentAndName(), value.schema);
  if (ctx->IF() != nullptr) {
    value.ifNotExists = true;
  }
}

void BuildAST(GQLParser::DropSchemaStatementContext* ctx,
              ast::DropSchemaStatement& value) {
  AssignInputPosition(ctx, value);
  BuildAST(ctx->catalogSchemaParentAndName(), value.schema);
  if (ctx->IF() != nullptr) {
    value.ifExists = true;
  }
}

void BuildAST(GQLParser::OfGraphTypeContext* ctx, ast::OfGraphType& value) {
  if (auto ctx2 = ctx->graphTypeLikeGraph()) {
    BuildAST(ctx2->graphExpression(), value.emplace<ast::GraphExpression>());
  } else if (auto ctx2 = ctx->graphTypeReference()) {
    BuildAST(ctx2, value.emplace<ast::GraphTypeReference>());
  } else if (auto ctx2 = ctx->nestedGraphTypeSpecification()) {
    BuildAST(ctx2->graphTypeSpecificationBody(),
             value.emplace<ast::GraphTypeSpecificationBody>());
  }
}

void BuildAST(GQLParser::CreateGraphStatementContext* ctx,
              ast::CreateGraphStatement& value) {
  AssignInputPosition(ctx, value);
  BuildAST(ctx->catalogGraphParentAndName(), value.graph);
  if (auto ctx2 = ctx->ofGraphType()) {
    BuildAST(ctx2, value.graphType.emplace());
  }
  if (auto ctx2 = ctx->graphSource()) {
    BuildAST(ctx2->graphExpression(), value.source.emplace());
  }

  if (ctx->IF() != nullptr) {
    value.createType = ast::CreateGraphStatement::CreateType::IfNotExists;
  } else if (ctx->REPLACE() != nullptr) {
    value.createType = ast::CreateGraphStatement::CreateType::OrReplace;
  }
}

void BuildAST(GQLParser::DropGraphStatementContext* ctx,
              ast::DropGraphStatement& value) {
  AssignInputPosition(ctx, value);
  BuildAST(ctx->catalogGraphParentAndName(), value.graph);
  value.ifExists = !!ctx->IF();
}

void BuildAST(GQLParser::GraphTypeSourceContext* ctx,
              ast::GraphTypeSource& value) {
  if (auto ctx2 = ctx->copyOfGraphType()) {
    BuildAST(ctx2->graphTypeReference(), value.emplace<ast::CopyOfGraphType>());
  } else if (auto ctx2 = ctx->graphTypeLikeGraph()) {
    BuildAST(ctx2->graphExpression(), value.emplace<ast::GraphExpression>());
  } else if (auto ctx2 = ctx->nestedGraphTypeSpecification()) {
    BuildAST(ctx2->graphTypeSpecificationBody(),
             value.emplace<ast::GraphTypeSpecificationBody>());
  }
}

void BuildAST(GQLParser::CreateGraphTypeStatementContext* ctx,
              ast::CreateGraphTypeStatement& value) {
  AssignInputPosition(ctx, value);
  BuildAST(ctx->catalogGraphTypeParentAndName(), value.graphType);
  BuildAST(ctx->graphTypeSource(), value.source);

  if (ctx->IF() != nullptr) {
    value.createType = ast::CreateGraphTypeStatement::CreateType::IfNotExists;
  } else if (ctx->REPLACE() != nullptr) {
    value.createType = ast::CreateGraphTypeStatement::CreateType::OrReplace;
  }
}

void BuildAST(GQLParser::DropGraphTypeStatementContext* ctx,
              ast::DropGraphTypeStatement& value) {
  AssignInputPosition(ctx, value);
  BuildAST(ctx->catalogGraphTypeParentAndName(), value.graphType);
  if (ctx->IF() != nullptr) {
    value.ifExists = true;
  }
}

void BuildAST(GQLParser::YieldClauseContext* ctx, ast::YieldClause& value) {
  auto ctx2 = ctx->yieldItemList();
  for (auto ctx3 : ctx2->yieldItem()) {
    auto& value2 = value.emplace_back();
    AssignInputPosition(ctx3, value2);
    BuildAST(ctx3->yieldItemName()->fieldName()->identifier(), value2.name);
    if (auto ctx4 = ctx3->yieldItemAlias()) {
      BuildAST(ctx4->bindingVariable(), value2.alias.emplace());
    }
  }
}

void BuildAST(GQLParser::CallProcedureStatementContext* ctx,
              ast::CallProcedureStatement& value) {
  AssignInputPosition(ctx, value);
  value.isOptional = !!ctx->OPTIONAL();

  auto ctx2 = ctx->procedureCall();
  if (auto ctx3 = ctx2->inlineProcedureCall()) {
    auto& value2 = value.call.emplace<ast::InlineProcedureCall>();
    AssignInputPosition(ctx3, value2);
    if (auto ctx4 = ctx3->variableScopeClause()) {
      auto& value3 = value2.vars.emplace();
      if (auto ctx5 = ctx4->bindingVariableReferenceList()) {
        for (auto ctx6 : ctx5->bindingVariableReference()) {
          BuildAST(ctx6->bindingVariable(), value3.emplace_back());
        }
      }
    }
    BuildAST(ctx3->nestedProcedureSpecification()
                 ->procedureSpecification()
                 ->procedureBody(),
             *value2.spec);
  } else if (auto ctx3 = ctx2->namedProcedureCall()) {
    auto& value2 = value.call.emplace<ast::NamedProcedureCall>();
    AssignInputPosition(ctx3, value2);
    BuildAST(ctx3->procedureReference(), value2.proc);
    if (auto ctx4 = ctx3->procedureArgumentList()) {
      for (auto ctx5 : ctx4->procedureArgument()) {
        BuildAST(ctx5->valueExpression(), value2.args.emplace_back());
        value2.args.back().isValueExpressionRule = true;
      }
    }
    if (auto ctx4 = ctx3->yieldClause()) {
      BuildAST(ctx4, value2.yield);
    }
  }
}

void BuildAST(GQLParser::SimpleCatalogModifyingStatementContext* ctx,
              ast::SimpleCatalogModifyingStatement& value) {
  if (auto ctx2 = ctx->primitiveCatalogModifyingStatement()) {
    if (auto ctx3 = ctx2->createSchemaStatement()) {
      BuildAST(ctx3, value.emplace<ast::CreateSchemaStatement>());
    } else if (auto ctx3 = ctx2->dropSchemaStatement()) {
      BuildAST(ctx3, value.emplace<ast::DropSchemaStatement>());
    } else if (auto ctx3 = ctx2->createGraphStatement()) {
      BuildAST(ctx3, value.emplace<ast::CreateGraphStatement>());
    } else if (auto ctx3 = ctx2->dropGraphStatement()) {
      BuildAST(ctx3, value.emplace<ast::DropGraphStatement>());
    } else if (auto ctx3 = ctx2->createGraphTypeStatement()) {
      BuildAST(ctx3, value.emplace<ast::CreateGraphTypeStatement>());
    } else if (auto ctx3 = ctx2->dropGraphTypeStatement()) {
      BuildAST(ctx3, value.emplace<ast::DropGraphTypeStatement>());
    }
  } else {
    BuildAST(
        ctx->callCatalogModifyingProcedureStatement()->callProcedureStatement(),
        value.emplace<ast::CallProcedureStatement>());
  }
}

void BuildAST(GQLParser::LinearCatalogModifyingStatementContext* ctx,
              ast::LinearCatalogModifyingStatement& value) {
  AssignInputPosition(ctx, value);
  for (auto ctx2 : ctx->simpleCatalogModifyingStatement()) {
    BuildAST(ctx2, value.statements.emplace_back());
  }
}

void BuildAST(GQLParser::OptTypedGraphInitializerContext* ctx,
              ast::OptTypedGraphInitializer& value) {
  AssignInputPosition(ctx, value);
  if (auto ctx2 = ctx->graphReferenceValueType()) {
    BuildAST(ctx2, value.type.emplace());
  }
  BuildAST(ctx->graphInitializer()->graphExpression(), value.initializer);
}

void BuildAST(GQLParser::OptTypedBindingTableInitializerContext* ctx,
              ast::OptTypedBindingTableInitializer& value) {
  AssignInputPosition(ctx, value);
  if (auto ctx2 = ctx->bindingTableReferenceValueType()) {
    BuildAST(ctx2, value.type.emplace());
  }
  BuildAST(ctx->bindingTableInitializer()->bindingTableExpression(),
           value.initializer);
}

void BuildAST(GQLParser::OptTypedValueInitializerContext* ctx,
              ast::OptTypedValueInitializer& value) {
  AssignInputPosition(ctx, value);
  if (auto ctx2 = ctx->valueType()) {
    BuildAST(ctx2, value.type.emplace());
  }
  BuildAST(ctx->valueInitializer()->valueExpression(), *value.initializer);
  value.initializer->isValueExpressionRule = true;
}

void BuildAST(GQLParser::ValueVariableDefinitionContext* ctx,
              ast::ValueVariableDefinition& value) {
  AssignInputPosition(ctx, value);
  BuildAST(ctx->bindingVariable(), value.var);
  BuildAST(ctx->optTypedValueInitializer(), value.initializer);
}

void BuildAST(GQLParser::BindingVariableDefinitionContext* ctx,
              ast::BindingVariableDefinition& value) {
  if (auto ctx2 = ctx->graphVariableDefinition()) {
    auto& var = value.emplace<ast::GraphVariableDefinition>();
    AssignInputPosition(ctx2, var);
    BuildAST(ctx2->bindingVariable(), var.var);
    BuildAST(ctx2->optTypedGraphInitializer(), var.initializer);
  } else if (auto ctx2 = ctx->bindingTableVariableDefinition()) {
    auto& var = value.emplace<ast::BindingTableVariableDefinition>();
    AssignInputPosition(ctx2, var);
    BuildAST(ctx2->bindingVariable(), var.var);
    BuildAST(ctx2->optTypedBindingTableInitializer(), var.initializer);
  } else if (auto ctx2 = ctx->valueVariableDefinition()) {
    auto& var = value.emplace<ast::ValueVariableDefinition>();
    AssignInputPosition(ctx2, var);
    BuildAST(ctx2->bindingVariable(), var.var);
    BuildAST(ctx2->optTypedValueInitializer(), var.initializer);
  }
}

void BuildAST(GQLParser::ProcedureBodyContext* ctx, ast::ProcedureBody& value) {
  AssignInputPosition(ctx, value);
  if (auto ctx2 = ctx->atSchemaClause()) {
    BuildAST(ctx2->schemaReference(), value.schema.emplace());
  }
  if (auto ctx2 = ctx->bindingVariableDefinitionBlock()) {
    for (auto ctx3 : ctx2->bindingVariableDefinition()) {
      BuildAST(ctx3, value.vars.emplace_back());
    }
  }
  auto ctx2 = ctx->statementBlock();
  AssignInputPosition(ctx2, value.statements);
  BuildAST(ctx2->statement(), *value.statements.firstStatement);
  for (auto ctx3 : ctx2->nextStatement()) {
    auto& nextStatement = value.statements.nextStatements.emplace_back();
    BuildAST(ctx3->statement(), *nextStatement.statement);
    if (auto ctx4 = ctx3->yieldClause()) {
      BuildAST(ctx4, nextStatement.yield);
    }
  }
}

void BuildAST(GQLParser::ForStatementContext* ctx, ast::ForStatement& value) {
  AssignInputPosition(ctx, value);
  auto ctx2 = ctx->forItem();
  BuildAST(ctx2->forItemAlias()->bindingVariable(), value.alias);
  BuildAST(ctx2->forItemSource()->valueExpression(), value.source);
  if (auto ctx3 = ctx->forOrdinalityOrOffset()) {
    auto& value2 = value.ordinalityOrOffset.emplace();
    AssignInputPosition(ctx3, value2);
    BuildAST(ctx3->bindingVariable(), value2.var);
    value2.isOrdinality = !!ctx3->ORDINALITY();
  }
}

void BuildAST(GQLParser::OrderByClauseContext* ctx,
              ast::SortSpecificationList& value) {
  for (auto ctx2 : ctx->sortSpecificationList()->sortSpecification()) {
    auto& value2 = value.emplace_back();
    AssignInputPosition(ctx2, value2);
    BuildAST(ctx2->sortKey()->aggregatingValueExpression()->valueExpression(),
             value2.sortKey);
    value2.sortKey.isValueExpressionRule = true;
    if (auto ctx3 = ctx2->orderingSpecification()) {
      value2.ordering = (!!ctx3->ASCENDING() || !!ctx3->ASC())
                            ? ast::OrderingSpecification::ASCENDING
                            : ast::OrderingSpecification::DESCENDING;
    }
    if (auto ctx3 = ctx2->nullOrdering()) {
      value2.nullOrder = !!ctx3->FIRST() ? ast::NullOrdering::NULLS_FIRST
                                         : ast::NullOrdering::NULLS_LAST;
    }
  }
}

void BuildAST(GQLParser::OrderByAndPageStatementContext* ctx,
              ast::OrderByAndPageStatement& value) {
  AssignInputPosition(ctx, value);
  if (auto ctx2 = ctx->orderByClause()) {
    BuildAST(ctx2, value.orderBy);
  }
  if (auto ctx2 = ctx->offsetClause()) {
    BuildAST(ctx2->nonNegativeIntegerSpecification(), value.offset.emplace());
  }
  if (auto ctx2 = ctx->limitClause()) {
    BuildAST(ctx2->nonNegativeIntegerSpecification(), value.limit.emplace());
  }
}

void BuildAST(GQLParser::ReturnItemContext* ctx, ast::ReturnItem& value) {
  AssignInputPosition(ctx, value);
  BuildAST(ctx->aggregatingValueExpression()->valueExpression(),
           value.aggregate);
  value.aggregate.isValueExpressionRule = true;
  if (auto ctx2 = ctx->returnItemAlias()) {
    BuildAST(ctx2->identifier(), value.alias.emplace());
  } else {
    if (!std::holds_alternative<ast::BindingVariableReference>(
            value.aggregate.option)) {
      auto token = ctx->getStart();
      throw FormattedError({token->getLine(), token->getCharPositionInLine()},
                           ErrorCode::E0085, "Return item must have an alias");
    }
  }
}

void BuildAST(GQLParser::GroupByClauseContext* ctx,
              ast::GroupingElementList& value) {
  for (auto ctx2 : ctx->groupingElementList()->groupingElement()) {
    BuildAST(ctx2->bindingVariableReference()->bindingVariable(),
             value.emplace_back());
  }
}

void BuildAST(GQLParser::PrimitiveResultStatementContext* ctx,
              ast::PrimitiveResultStatement& value) {
  AssignInputPosition(ctx, value);
  if (auto ctx2 = ctx->returnStatement()) {
    auto& value2 =
        value.option.emplace<ast::PrimitiveResultStatement::Return>();
    AssignInputPosition(ctx2, value2.stmt);
    auto ctx3 = ctx2->returnStatementBody();
    if (ctx3->setQuantifier()) {
      BuildAST(ctx3->setQuantifier(), value2.stmt.quantifier);
    }
    if (auto ctx4 = ctx3->returnItemList()) {
      auto& value3 = value2.stmt.items.emplace();
      for (auto ctx5 : ctx4->returnItem()) {
        BuildAST(ctx5, value3.emplace_back());
      }
    }
    if (auto ctx4 = ctx3->groupByClause()) {
      BuildAST(ctx4, value2.stmt.groupBy);
    }
    if (auto ctx3 = ctx->orderByAndPageStatement()) {
      BuildAST(ctx3, value2.orderByAndPage.emplace());
    }

    if (ctx3->BINDINGS() != nullptr) {
      value2.stmt.items.emplace();
    }
  } else {
    value.option.emplace<ast::FinishValue>();
  }
}

void BuildAST(GQLParser::SelectItemListContext* ctx,
              ast::SelectItemList& value) {
  for (auto ctx2 : ctx->selectItem()) {
    auto& value2 = value.emplace_back();
    AssignInputPosition(ctx2, value2);
    BuildAST(ctx2->aggregatingValueExpression()->valueExpression(),
             value2.expr);
    value2.expr.isValueExpressionRule = true;
    if (auto ctx3 = ctx2->selectItemAlias()) {
      BuildAST(ctx3->identifier(), value2.alias.emplace());
    } else {
      if (!std::holds_alternative<ast::BindingVariableReference>(
              value2.expr.option)) {
        auto token = ctx2->getStart();
        throw FormattedError({token->getLine(), token->getCharPositionInLine()},
                             ErrorCode::E0086,
                             "Select item must have an alias");
      }
    }
  }
}

void BuildAST(GQLParser::SelectGraphMatchListContext* ctx,
              ast::SelectGraphMatchList& value) {
  AssignInputPosition(ctx, value);
  for (auto ctx2 : ctx->selectGraphMatch()) {
    auto& value2 = value.matches.emplace_back();
    AssignInputPosition(ctx2, value2);
    BuildAST(ctx2->graphExpression(), value2.graph);
    BuildAST(ctx2->matchStatement(), value2.match);
  }
}

void BuildAST(GQLParser::SelectQuerySpecificationContext* ctx,
              ast::SelectQuerySpecification& value) {
  AssignInputPosition(ctx, value);
  if (auto ctx2 = ctx->graphExpression()) {
    BuildAST(ctx2, value.graph.emplace());
  }
  BuildAST(ctx->nestedQuerySpecification()->procedureBody(), value.query);
}

void BuildAST(GQLParser::SelectStatementBodyContext* ctx,
              ast::SelectStatementBody& value) {
  if (auto ctx2 = ctx->selectGraphMatchList()) {
    BuildAST(ctx2, value.emplace<ast::SelectGraphMatchList>());
  } else {
    BuildAST(ctx->selectQuerySpecification(),
             value.emplace<ast::SelectQuerySpecification>());
  }
}

void BuildAST(GQLParser::SelectStatementContext* ctx,
              ast::SelectStatement& value) {
  AssignInputPosition(ctx, value);
  if (auto ctx2 = ctx->setQuantifier()) {
    BuildAST(ctx2, value.quantifier);
  }
  if (auto ctx2 = ctx->selectItemList()) {
    BuildAST(ctx2, value.items.emplace<ast::SelectItemList>());
  } else {
    value.items.emplace<ast::AsteriskValue>();
  }
  if (auto ctx2 = ctx->selectStatementBody()) {
    BuildAST(ctx2, value.body.emplace());
  }
  if (auto ctx2 = ctx->whereClause()) {
    BuildAST(
        ctx2->searchCondition()->booleanValueExpression()->valueExpression(),
        *value.where.emplace());
  }
  if (auto ctx2 = ctx->groupByClause()) {
    BuildAST(ctx2, value.groupBy);
  }
  if (auto ctx2 = ctx->havingClause()) {
    BuildAST(
        ctx2->searchCondition()->booleanValueExpression()->valueExpression(),
        *value.having.emplace());
  }
  if (auto ctx2 = ctx->orderByClause()) {
    BuildAST(ctx2, value.orderBy);
  }
  if (auto ctx2 = ctx->offsetClause()) {
    BuildAST(ctx2->nonNegativeIntegerSpecification(), value.offset.emplace());
  }
  if (auto ctx2 = ctx->limitClause()) {
    BuildAST(ctx2->nonNegativeIntegerSpecification(), value.limit.emplace());
  }
}

void BuildAST(GQLParser::FilterStatementContext* ctx,
              ast::FilterStatement& value) {
  AssignInputPosition(ctx, value);
  if (auto ctx2 = ctx->whereClause()) {
    BuildAST(
        ctx2->searchCondition()->booleanValueExpression()->valueExpression(),
        *value.condition);
  } else {
    BuildAST(
        ctx->searchCondition()->booleanValueExpression()->valueExpression(),
        *value.condition);
  }
}

void BuildAST(GQLParser::LetStatementContext* ctx, ast::LetStatement& value) {
  AssignInputPosition(ctx, value);
  for (auto ctx2 : ctx->letVariableDefinitionList()->letVariableDefinition()) {
    BuildAST(ctx2, value.definitions.emplace_back());
  }
}

void BuildAST(GQLParser::SimpleQueryStatementContext* ctx,
              ast::SimpleQueryStatement& value) {
  if (auto ctx2 = ctx->callQueryStatement()) {
    BuildAST(ctx2->callProcedureStatement(),
             value.emplace<ast::CallProcedureStatement>());
  } else {
    auto ctx3 = ctx->primitiveQueryStatement();
    if (auto ctx4 = ctx3->matchStatement()) {
      BuildAST(ctx4, value.emplace<ast::MatchStatement>());
    } else if (auto ctx4 = ctx3->letStatement()) {
      BuildAST(ctx4, value.emplace<ast::LetStatement>());
    } else if (auto ctx4 = ctx3->forStatement()) {
      BuildAST(ctx4, value.emplace<ast::ForStatement>());
    } else if (auto ctx4 = ctx3->filterStatement()) {
      BuildAST(ctx4, value.emplace<ast::FilterStatement>());
    } else if (auto ctx4 = ctx3->orderByAndPageStatement()) {
      BuildAST(ctx4, value.emplace<ast::OrderByAndPageStatement>());
    }
  }
}

void BuildAST(GQLParser::SimpleLinearQueryStatementContext* ctx,
              ast::SimpleLinearQueryStatement& value) {
  for (auto ctx2 : ctx->simpleQueryStatement()) {
    BuildAST(ctx2, value.emplace_back());
  }
}

void BuildAST(GQLParser::FocusedLinearQueryStatementContext* ctx,
              ast::LinearQueryStatement& value) {
  if (auto ctx2 = ctx->focusedLinearQueryAndPrimitiveResultStatementPart()) {
    auto& value2 = value.emplace<ast::LinearQueryStatementOption>();
    AssignInputPosition(ctx, value2);
    auto& value3 =
        value2.statements
            .emplace<std::vector<ast::FocusedLinearQueryStatementPart>>();
    for (auto ctx3 : ctx->focusedLinearQueryStatementPart()) {
      auto& value4 = value3.emplace_back();
      AssignInputPosition(ctx3, value4);
      BuildAST(ctx3->useGraphClause()->graphExpression(), value4.useGraph);
      BuildAST(ctx3->simpleLinearQueryStatement(), value4.statements);
    }
    auto& value5 = value3.emplace_back();
    BuildAST(ctx2->useGraphClause()->graphExpression(), value5.useGraph);
    BuildAST(ctx2->simpleLinearQueryStatement(), value5.statements);
    BuildAST(ctx2->primitiveResultStatement(), value2.result);
  } else if (auto ctx2 = ctx->focusedPrimitiveResultStatement()) {
    auto& value2 = value.emplace<ast::FocusedPrimitiveResultStatement>();
    AssignInputPosition(ctx2, value2);
    BuildAST(ctx2->useGraphClause()->graphExpression(), value2.useGraph);
    BuildAST(ctx2->primitiveResultStatement(), value2.result);
  } else if (auto ctx2 = ctx->focusedNestedQuerySpecification()) {
    auto& value2 = value.emplace<ast::NestedQuerySpecification>();
    AssignInputPosition(ctx2, value2);
    BuildAST(ctx2->useGraphClause()->graphExpression(),
             value2.useGraph.emplace());
    BuildAST(ctx2->nestedQuerySpecification()->procedureBody(),
             value2.procedure);
  } else if (auto ctx2 = ctx->selectStatement()) {
    BuildAST(ctx2, value.emplace<ast::SelectStatement>());
  }
}

void BuildAST(GQLParser::AmbientLinearQueryStatementContext* ctx3,
              ast::LinearQueryStatement& value) {
  if (auto ctx4 = ctx3->primitiveResultStatement()) {
    auto& value2 = value.emplace<ast::LinearQueryStatementOption>();
    AssignInputPosition(ctx3, value2);
    auto& value3 = value2.statements.emplace<ast::SimpleLinearQueryStatement>();
    BuildAST(ctx4, value2.result);
    if (auto ctx5 = ctx3->simpleLinearQueryStatement()) {
      BuildAST(ctx5, value3);
    }
  } else if (auto ctx4 = ctx3->nestedQuerySpecification()) {
    auto& value2 = value.emplace<ast::NestedQuerySpecification>();
    AssignInputPosition(ctx4, value2);
    BuildAST(ctx4->procedureBody(), value2.procedure);
  } else {
    GQL_ASSERT(false);
  }
}

void BuildAST(GQLParser::QueryConjunctionContext* ctx,
              ast::QueryConjunction& value) {
  if (auto ctx2 = ctx->setOperator()) {
    auto& value2 = value.emplace<ast::SetOperator>();
    AssignInputPosition(ctx2, value2);
    if (ctx2->UNION() != nullptr) {
      value2.kind = ast::SetOperator::Kind::UNION;
    } else if (ctx2->EXCEPT() != nullptr) {
      value2.kind = ast::SetOperator::Kind::EXCEPT;
    } else if (ctx2->INTERSECT() != nullptr) {
      value2.kind = ast::SetOperator::Kind::INTERSECT;
    }
    if (auto ctx3 = ctx2->setQuantifier()) {
      BuildAST(ctx3, value2.quantifier);
    }
  } else {
    value.emplace<ast::OtherwiseConjunctionValue>();
  }
}

namespace {
bool IsQueryConjunctionsEqual(const ast::QueryConjunction& a,
                              const ast::QueryConjunction& b) {
  if (a.index() != b.index()) {
    return false;
  }
  if (auto a2 = std::get_if<ast::SetOperator>(&a)) {
    auto b2 = std::get_if<ast::SetOperator>(&b);
    if (a2->kind != b2->kind) {
      return false;
    }
    if (a2->quantifier != b2->quantifier) {
      return false;
    }
  }
  return true;
}
}  // namespace

void BuildAST(GQLParser::CompositeQueryExpressionFocusedContext* ctx,
              ast::CompositeQueryExpression& value) {
  if (auto ctx2 = ctx->compositeQueryExpressionFocused()) {
    BuildAST(ctx2, value);
    ast::QueryConjunction conjunction;
    BuildAST(ctx->queryConjunction(), conjunction);
    if (value.queries.empty()) {
      value.conjunction = std::move(conjunction);
    } else {
      if (!IsQueryConjunctionsEqual(conjunction, value.conjunction)) {
        auto token = ctx->queryConjunction()->getStart();
        throw FormattedError({token->getLine(), token->getCharPositionInLine()},
                             ErrorCode::E0087,
                             "Every query conjunction must be the same");
      }
    }
  }
  BuildAST(ctx->focusedLinearQueryStatement(), value.queries.emplace_back());
}

void BuildAST(GQLParser::CompositeQueryExpressionAmbientContext* ctx,
              ast::CompositeQueryExpression& value) {
  if (auto ctx2 = ctx->compositeQueryExpressionAmbient()) {
    BuildAST(ctx2, value);
    ast::QueryConjunction conjunction;
    BuildAST(ctx->queryConjunction(), conjunction);
    if (value.queries.empty()) {
      value.conjunction = std::move(conjunction);
    } else {
      if (!IsQueryConjunctionsEqual(conjunction, value.conjunction)) {
        auto token = ctx->queryConjunction()->getStart();
        throw FormattedError({token->getLine(), token->getCharPositionInLine()},
                             ErrorCode::E0088,
                             "Every query conjunction must be the same");
      }
    }
  }
  BuildAST(ctx->ambientLinearQueryStatement(), value.queries.emplace_back());
}

void BuildAST(GQLParser::CompositeQueryExpressionContext* ctx,
              ast::CompositeQueryExpression& value) {
  AssignInputPosition(ctx, value);
  if (auto ctx2 = ctx->compositeQueryExpressionFocused()) {
    BuildAST(ctx2, value);
  } else {
    BuildAST(ctx->compositeQueryExpressionAmbient(), value);
  }
}

void BuildAST(GQLParser::InsertElementPatternFillerContext* ctx,
              ast::InsertElementPatternFiller& value) {
  AssignInputPosition(ctx, value);
  if (auto ctx2 = ctx->elementVariableDeclaration()) {
    auto& var = value.var.emplace();
    AssignInputPosition(ctx2, var);
    BuildAST(ctx2->elementVariable()->bindingVariable(), var);
  }
  if (auto ctx2 = ctx->labelAndPropertySetSpecification()) {
    if (auto ctx3 = ctx2->labelSetSpecification()) {
      BuildAST(ctx3, value.labels);
    }
    if (auto ctx3 = ctx2->elementPropertySpecification()) {
      BuildAST(ctx3->propertyKeyValuePairList(), value.props);
    }
  }
}

void BuildAST(GQLParser::InsertEdgePatternContext* ctx,
              ast::InsertEdgePattern& value) {
  AssignInputPosition(ctx, value);
  if (auto ctx2 = ctx->insertEdgePointingLeft()) {
    value.direction = ast::InsertEdgePattern::Direction::Left;
    if (auto ctx3 = ctx2->insertElementPatternFiller()) {
      BuildAST(ctx3, value.filler);
    }
  } else if (auto ctx2 = ctx->insertEdgePointingRight()) {
    value.direction = ast::InsertEdgePattern::Direction::Right;
    if (auto ctx3 = ctx2->insertElementPatternFiller()) {
      BuildAST(ctx3, value.filler);
    }
  } else {
    value.direction = ast::InsertEdgePattern::Direction::Undirected;
    if (auto ctx3 = ctx->insertEdgeUndirected()->insertElementPatternFiller()) {
      BuildAST(ctx3, value.filler);
    }
  }
}

void BuildAST(GQLParser::InsertPathPatternContext* ctx,
              ast::InsertPathPattern& value) {
  AssignInputPosition(ctx, value);
  for (auto ctx2 : ctx->insertNodePattern()) {
    auto& value2 = value.nodes.emplace_back();
    AssignInputPosition(ctx2, value2);
    if (auto ctx3 = ctx2->insertElementPatternFiller()) {
      BuildAST(ctx3, value2.filler);
    }
  }
  for (auto ctx2 : ctx->insertEdgePattern()) {
    BuildAST(ctx2, value.edges.emplace_back());
  }
}

void BuildAST(GQLParser::SetItemContext* ctx, ast::SetItem& value) {
  if (auto ctx2 = ctx->setPropertyItem()) {
    auto& value2 = value.emplace<ast::SetPropertyItem>();
    AssignInputPosition(ctx2, value2);
    BuildAST(ctx2->bindingVariableReference()->bindingVariable(), value2.var);
    BuildAST(ctx2->propertyName()->identifier(), value2.property);
    BuildAST(ctx2->valueExpression(), value2.value);
    value2.value.isValueExpressionRule = true;
  } else if (auto ctx2 = ctx->setAllPropertiesItem()) {
    auto& value2 = value.emplace<ast::SetAllPropertiesItem>();
    AssignInputPosition(ctx2, value2);
    BuildAST(ctx2->bindingVariableReference()->bindingVariable(), value2.var);
    if (auto ctx3 = ctx2->propertyKeyValuePairList()) {
      BuildAST(ctx3, value2.properties);
    }
  } else {
    auto ctx3 = ctx->setLabelItem();
    auto& value2 = value.emplace<ast::SetLabelItem>();
    AssignInputPosition(ctx3, value2);
    BuildAST(ctx3->bindingVariableReference()->bindingVariable(), value2.var);
    BuildAST(ctx3->labelName()->identifier(), value2.label);
  }
}

void BuildAST(GQLParser::RemoveItemContext* ctx, ast::RemoveItem& value) {
  if (auto ctx2 = ctx->removePropertyItem()) {
    auto& value2 = value.emplace<ast::RemovePropertyItem>();
    AssignInputPosition(ctx2, value2);
    BuildAST(ctx2->bindingVariableReference()->bindingVariable(), value2.var);
    BuildAST(ctx2->propertyName()->identifier(), value2.property);
  } else {
    auto ctx3 = ctx->removeLabelItem();
    auto& value2 = value.emplace<ast::RemoveLabelItem>();
    AssignInputPosition(ctx3, value2);
    BuildAST(ctx3->bindingVariableReference()->bindingVariable(), value2.var);
    BuildAST(ctx3->labelName()->identifier(), value2.label);
  }
}

void BuildAST(GQLParser::PrimitiveDataModifyingStatementContext* ctx,
              ast::PrimitiveDataModifyingStatement& value) {
  if (auto ctx2 = ctx->insertStatement()) {
    auto& value2 = value.emplace<ast::InsertStatement>();
    AssignInputPosition(ctx2, value2);
    for (auto ctx3 : ctx2->insertGraphPattern()
                         ->insertPathPatternList()
                         ->insertPathPattern()) {
      BuildAST(ctx3, value2.paths.emplace_back());
    }
  } else if (auto ctx2 = ctx->setStatement()) {
    auto& value2 = value.emplace<ast::SetStatement>();
    AssignInputPosition(ctx2, value2);
    for (auto ctx3 : ctx2->setItemList()->setItem()) {
      BuildAST(ctx3, value2.items.emplace_back());
    }
  } else if (auto ctx2 = ctx->removeStatement()) {
    auto& value2 = value.emplace<ast::RemoveStatement>();
    AssignInputPosition(ctx2, value2);
    for (auto ctx3 : ctx2->removeItemList()->removeItem()) {
      BuildAST(ctx3, value2.items.emplace_back());
    }
  } else if (auto ctx2 = ctx->deleteStatement()) {
    auto& value2 = value.emplace<ast::DeleteStatement>();
    AssignInputPosition(ctx2, value2);
    value2.detach = !!ctx2->DETACH();
    for (auto ctx3 : ctx2->deleteItemList()->deleteItem()) {
      BuildAST(ctx3->valueExpression(), value2.items.emplace_back());
      value2.items.back().isValueExpressionRule = true;
    }
  }
}

void BuildAST(GQLParser::SimpleDataAccessingStatementContext* ctx,
              ast::SimpleDataAccessingStatement& value) {
  if (auto ctx2 = ctx->simpleQueryStatement()) {
    BuildAST(ctx2, value.emplace<ast::SimpleQueryStatement>());
  } else {
    auto ctx3 = ctx->simpleDataModifyingStatement();
    if (auto ctx4 = ctx3->primitiveDataModifyingStatement()) {
      BuildAST(ctx4, value.emplace<ast::PrimitiveDataModifyingStatement>());
    } else {
      BuildAST(
          ctx3->callDataModifyingProcedureStatement()->callProcedureStatement(),
          value.emplace<ast::CallProcedureStatement>());
    }
  }
}

void BuildAST(GQLParser::SimpleLinearDataAccessingStatementContext* ctx,
              std::vector<ast::SimpleDataAccessingStatement>& value) {
  for (auto ctx2 : ctx->simpleDataAccessingStatement()) {
    BuildAST(ctx2, value.emplace_back());
  }
}

void BuildAST(GQLParser::LinearDataModifyingStatementContext* ctx,
              ast::LinearDataModifyingStatement& value) {
  AssignInputPosition(ctx, value);
  if (auto ctx2 = ctx->focusedLinearDataModifyingStatement()) {
    auto& value2 =
        value.option.emplace<ast::LinearDataModifyingStatementBody>();
    AssignInputPosition(ctx2, value2);
    if (auto ctx3 = ctx2->focusedLinearDataModifyingStatementBody()) {
      BuildAST(ctx3->useGraphClause()->graphExpression(),
               value.useGraph.emplace());
      BuildAST(ctx3->simpleLinearDataAccessingStatement(), value2.statements);
      if (auto ctx4 = ctx3->primitiveResultStatement()) {
        BuildAST(ctx4, value2.result.emplace());
      }
    } else {
      auto ctx4 = ctx2->focusedNestedDataModifyingProcedureSpecification();
      auto& value3 = value.option.emplace<ast::ProcedureBody>();
      AssignInputPosition(ctx4, value3);
      BuildAST(ctx4->useGraphClause()->graphExpression(),
               value.useGraph.emplace());
      BuildAST(
          ctx4->nestedDataModifyingProcedureSpecification()->procedureBody(),
          value3);
    }
  } else if (auto ctx2 = ctx->ambientLinearDataModifyingStatement()) {
    auto& value2 =
        value.option.emplace<ast::LinearDataModifyingStatementBody>();
    AssignInputPosition(ctx2, value2);
    if (auto ctx3 = ctx2->ambientLinearDataModifyingStatementBody()) {
      BuildAST(ctx3->simpleLinearDataAccessingStatement(), value2.statements);
      if (auto ctx4 = ctx3->primitiveResultStatement()) {
        BuildAST(ctx4, value2.result.emplace());
      }
    } else {
      BuildAST(
          ctx2->nestedDataModifyingProcedureSpecification()->procedureBody(),
          value.option.emplace<ast::ProcedureBody>());
    }
  }
}

void BuildAST(GQLParser::StatementContext* ctx, ast::Statement& value) {
  AssignInputPosition(ctx, value);
  if (auto ctx2 = ctx->linearCatalogModifyingStatement()) {
    auto& value2 = value.option.emplace<ast::LinearCatalogModifyingStatement>();
    AssignInputPosition(ctx2, value2);
    for (auto ctx3 : ctx2->simpleCatalogModifyingStatement()) {
      BuildAST(ctx3, value2.statements.emplace_back());
    }
  } else if (auto ctx2 = ctx->linearDataModifyingStatement()) {
    BuildAST(ctx2, value.option.emplace<ast::LinearDataModifyingStatement>());
  } else if (auto ctx2 = ctx->compositeQueryStatement()) {
    BuildAST(ctx2->compositeQueryExpression(),
             value.option.emplace<ast::CompositeQueryStatement>());
  }
}

void BuildAST(GQLParser::SessionSetParameterNameContext* ctx,
              ast::SessionSetParameterName& value) {
  AssignInputPosition(ctx, value);
  ProcessGeneralParameterReference(ctx->sessionParameterSpecification(),
                                   value.param);
  if (ctx->EXISTS() != nullptr) {
    value.ifNotExists = true;
  }
}

void BuildAST(GQLParser::SessionSetGraphParameterClauseContext* ctx,
              ast::SessionSetGraphParameterClause& value) {
  AssignInputPosition(ctx, value);
  BuildAST(ctx->sessionSetParameterName(), value.paramName);
  BuildAST(ctx->optTypedGraphInitializer(), value.initializer);
}

void BuildAST(GQLParser::SessionSetBindingTableParameterClauseContext* ctx,
              ast::SessionSetBindingTableParameterClause& value) {
  AssignInputPosition(ctx, value);
  BuildAST(ctx->sessionSetParameterName(), value.paramName);
  BuildAST(ctx->optTypedBindingTableInitializer(), value.initializer);
}

void BuildAST(GQLParser::SessionSetValueParameterClauseContext* ctx,
              ast::SessionSetValueParameterClause& value) {
  AssignInputPosition(ctx, value);
  BuildAST(ctx->sessionSetParameterName(), value.paramName);
  BuildAST(ctx->optTypedValueInitializer(), value.initializer);
}

void BuildAST(GQLParser::SessionSetCommandContext* ctx,
              ast::SessionSetCommand& value) {
  if (auto ctx2 = ctx->sessionSetSchemaClause()) {
    auto& value2 = value.emplace<ast::SessionSetSchemaClause>();
    AssignInputPosition(ctx2, value2);
    BuildAST(ctx2->schemaReference(), value2.schema);
  } else if (auto ctx2 = ctx->sessionSetGraphClause()) {
    auto& value2 = value.emplace<ast::SessionSetGraphClause>();
    AssignInputPosition(ctx2, value2);
    BuildAST(ctx2->graphExpression(), value2.graph);
  } else if (auto ctx2 = ctx->sessionSetTimeZoneClause()) {
    auto& value2 = value.emplace<ast::SessionSetTimeZoneClause>();
    AssignInputPosition(ctx2, value2);
    BuildAST(
        ctx2->setTimeZoneValue()->timeZoneString()->characterStringLiteral(),
        value2.timeZoneString);
  } else if (auto ctx2 = ctx->sessionSetParameterClause()) {
    auto& value2 = value.emplace<ast::SessionSetParameterClause>();
    if (auto ctx3 = ctx2->sessionSetGraphParameterClause()) {
      BuildAST(ctx3, value2.emplace<ast::SessionSetGraphParameterClause>());
    } else if (auto ctx3 = ctx2->sessionSetBindingTableParameterClause()) {
      BuildAST(ctx3,
               value2.emplace<ast::SessionSetBindingTableParameterClause>());
    } else if (auto ctx3 = ctx2->sessionSetValueParameterClause()) {
      BuildAST(ctx3, value2.emplace<ast::SessionSetValueParameterClause>());
    }
  }
}

void BuildAST(GQLParser::SessionResetCommandContext* ctx,
              ast::SessionResetCommand& value) {
  AssignInputPosition(ctx, value);
  value.arguments = ast::SessionResetArguments::Parameters;

  if (auto ctx2 = ctx->sessionResetArguments()) {
    if (ctx2->PARAMETERS() != nullptr) {
      value.arguments = ast::SessionResetArguments::Parameters;
    } else if (ctx2->CHARACTERISTICS() != nullptr) {
      value.arguments = ast::SessionResetArguments::Characteristics;
    } else if (ctx2->SCHEMA() != nullptr) {
      value.arguments = ast::SessionResetArguments::Schema;
    } else if (ctx2->GRAPH() != nullptr) {
      value.arguments = ast::SessionResetArguments::Graph;
    } else if (ctx2->ZONE() != nullptr) {
      value.arguments = ast::SessionResetArguments::TimeZone;
    }

    if (auto ctx3 = ctx2->sessionParameterSpecification()) {
      ProcessGeneralParameterReference(
          ctx3, value.arguments.emplace<ast::GeneralParameterReference>());
    }
  } else {
    value.arguments = ast::SessionResetArguments::Characteristics;
  }
}

void BuildAST(GQLParser::SessionActivityContext* ctx,
              ast::SessionActivity& value) {
  AssignInputPosition(ctx, value);
  for (auto* ctx2 : ctx->sessionResetCommand()) {
    BuildAST(ctx2, value.resetCommands.emplace_back());
  }
  for (auto* ctx2 : ctx->sessionSetCommand()) {
    BuildAST(ctx2, value.setCommands.emplace_back());
  }
}

void BuildAST(GQLParser::TransactionAccessModeContext* ctx,
              ast::TransactionAccessMode& value) {
  value = (ctx->ONLY() != nullptr) ? ast::TransactionAccessMode::ReadOnly
                                   : ast::TransactionAccessMode::ReadWrite;
}

void BuildAST(GQLParser::StartTransactionCommandContext* ctx,
              ast::StartTransactionCommand& value) {
  AssignInputPosition(ctx, value);
  if (auto* ctx2 = ctx->transactionCharacteristics()) {
    BuildAST(ctx2->transactionMode(0)->transactionAccessMode(),
             value.accessMode.emplace());
    if (auto* ctx3 = ctx2->transactionMode(1)) {
      // 8.2 Syntax Rule 2: "TC shall contain exactly one <transaction access
      // mode>"
      auto token = ctx3->getStart();
      throw FormattedError(
          {token->getLine(), token->getCharPositionInLine()}, ErrorCode::E0089,
          "Cannot specify more than one transaction access mode");
    }
  }
}

void BuildAST(GQLParser::EndTransactionCommandContext* ctx,
              ast::EndTransactionCommand& value) {
  if (ctx->rollbackCommand() != nullptr) {
    value = ast::EndTransactionCommand::ROLLBACK;
  } else {
    value = ast::EndTransactionCommand::COMMIT;
  }
}

void BuildAST(GQLParser::TransactionActivityContext* ctx,
              ast::TransactionActivity& value) {
  AssignInputPosition(ctx, value);
  if (auto* ctx2 = ctx->startTransactionCommand()) {
    BuildAST(ctx2, value.startCmd.emplace());
  }
  if (auto* ctx2 = ctx->endTransactionCommand()) {
    BuildAST(ctx2, value.endCmd.emplace());
  }
  if (auto* ctx2 = ctx->procedureSpecification()) {
    BuildAST(ctx2->procedureBody(), value.procedure.emplace());
  }
}

void BuildAST(GQLParser::GqlProgramContext* ctx, ast::GQLProgram& value) {
  AssignInputPosition(ctx, value);
  if (auto* ctx2 = ctx->programActivity()) {
    if (auto* ctx3 = ctx2->sessionActivity()) {
      BuildAST(ctx3, value.programActivity.emplace<ast::SessionActivity>());
    }
    if (auto* ctx3 = ctx2->transactionActivity()) {
      BuildAST(ctx3, value.programActivity.emplace<ast::TransactionActivity>());
    }
  }

  if (ctx->sessionCloseCommand() != nullptr) {
    value.sessionCloseCommand = true;
  }
}

}  // namespace gql::parser::ast_builder