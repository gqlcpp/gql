# GQL C++ Parser Library

This library is a C++ parser and AST (Abstract Syntax Tree) implementation for the GQL database language (ISO/IEC 39075:2024). 
It requires C++17 or later and is distributed under the Apache License 2.0.

### Contents
- [GQL Parser and AST](#gql-parser-and-ast) 
- [GQL Syntax Analyzer](#gql-syntax-analyzer)
- [Build](#build)
- [License](#license)

## GQL Parser and AST

The library is designed to map GQL syntax to C++ types, enabling full static type checking and type safety. The AST does not use virtual functions.

Below are examples demonstrating how GQL expressions are represented in C++ types.

#### GQL Specification:
```
<graph pattern> ::=
  [ <match mode> ] <path pattern list>
    [ <keep clause> ]
    [ <graph pattern where clause> ]

<path pattern list> ::=
  <path pattern> [ { <comma> <path pattern> }... ]    
```

#### gql C++ Library:
```cpp
using PathPatternList = std::vector<PathPattern>;

struct GraphPattern {
  std::optional<MatchMode> matchMode;
  PathPatternList patterns;
  std::optional<KeepClause> keep;
  std::optional<GraphPatternWhereClause> where;
};
```

#### GQL Specification:
```
<primitive data-modifying statement> ::=
    <insert statement>
  | <set statement>
  | <remove statement>
  | <delete statement>
```

#### gql C++ Library:
```cpp
using PrimitiveDataModifyingStatement =
    std::variant<InsertStatement, SetStatement, RemoveStatement, DeleteStatement>;
```

### Parser Usage

To parse a GQL query from a string:
```cpp
#include <gql/parser/parser.h>

void load_query(const char* query) {
  try {
    gql::ast::GQLProgram program = gql::parser::ParseProgram(query);
  } catch (const gql::parser::ParserError& e) {
    std::cerr << "Error: " << e.what() << std::endl;
  }
}
```

The AST provided by the library follows these principles:
- Simple `struct`s
- `std::optional` for optional child nodes
- `std::variant` for alternative child nodes
- `std::vector` for sequences of child nodes

The root node of the AST is `gql::ast::GQLProgram`. All AST nodes are defined in `include/gql/ast/nodes/`.

Thanks to static typing, traversing the AST should work seamlessly with full IDE support. Handling `variant`s can be somewhat cumbersome, so here’s a recommended approach:
```cpp
void process(const gql::ast::LinearDataModifyingStatementBody& body) {
  for (const auto& stmt : body.statements) {
    gql::ast::variant_switch(
        stmt,
        [&](const gql::ast::SimpleQueryStatement& value) {
          // Execute simple data-accessing statement
          ...
        },
        [&](const gql::ast::PrimitiveDataModifyingStatement& value) {
          // Execute primitive data-modifying statement
          ...
        },
        [&](const gql::ast::CallProcedureStatement& value) {
          // Execute call procedure statement
          ...
        });
  }
}
```

In certain cases, the library employs the `value_ptr` smart pointer class (with type aliases ending in `Ptr`) to manage recursive types. The parser ensures these pointers are always initialized, allowing safe access without additional checks.

### AST Extensibility

The AST data structures are intentionally simple, but you may need to store additional information in the AST beyond the parsing stage.

To facilitate this, you can provide a custom base class for AST nodes. If you define `GQL_AST_CUSTOM_NODE_BASE_INCLUDE` as a preprocessor macro with a header filename, 
the specified file will be included before the AST node class definitions. This file can define a custom `gql::ast::NodeBase<T>` template, allowing you to override 
the default base class for specific or all AST node types.

Refer to `include/gql/ast/nodes/base.h` for further details.

## GQL Syntax Analyzer

The Syntax Analyzer component performs the following functions:
- Applies the Syntax Rules of the ISO/IEC 39075:2024 standard to the GQL program code.
- Reports errors in the GQL program.
- Checks the fulfillment of Conformance Rules, reporting an error if the GQL program
uses an optional GQL feature not supported by this GQL implementation.
The list of unsupported features is specified in the client code.
- Performs transformations of the GQL program as provided by the Syntax Rules.
- Complements some AST nodes with information (called `auxData` in the code) that simplifies the execution of the GQL program.

Although the Syntax Analyzer makes relatively few changes to the AST, its use
greatly simplifies the code that directly executes the GQL program, removing
the need for many checks.

### Syntax Analyzer Usage

```cpp
#include <gql/syntax_analyzer/syntax_analyzer.h>

void analyze_query(const char* query) {
  try {
    gql::ast::GQLProgram program = gql::parser::ParseProgram(query);
    gql::SyntaxAnalyzerConfig config;
    gql::AnalyzeSyntax(program, config);
  } catch (const gql::parser::ParserError& e) {
    std::cerr << "Error: " << e.what() << std::endl;
  }
}
```

## Build

The library uses CMake as its build system. Client code should link against the `gql_parser` static library, defined in `src/parser/CMakeLists.txt`.

To build the library:
```shell
mkdir build
cd build
cmake ..
make
```

If your project does not use CMake, you can manually include the library sources in your build system with minimal effort.

## License

This library is distributed under the Apache License 2.0.

