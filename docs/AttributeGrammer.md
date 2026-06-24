# Semantic Analysis Report

This document describes the semantic checks the **semantic analyzer**
([`src/semantic_analyzer/analyzer.cpp`](../src/semantic_analyzer/analyzer.cpp))
performs on the abstract syntax tree produced by the parser, the type-system
assumptions it makes, and how it reports problems.

## Pipeline position

The analyzer is the third stage of the compiler (tokenize → parse → **analyze**
→ generate). It walks the AST, builds a `SymbolTable` of constants, types,
variables, and functions, and validates the program against the rules below.
Each violation is collected (with its source line/column) rather than aborting
on the first error, so a single run reports as many problems as it can find. If
any error is collected, `analyze()` returns a failure `Result` and code
generation does not run.

## Error reporting

Diagnostics are printed to `stderr` in a compiler-style format
(see [`include/utils/diagnostics.h`](../include/utils/diagnostics.h)):

```
error: <message>
  --> line <line>, column <column>

Semantic analysis failed with <N> error(s).
```

Every collected error is tagged with the source position of the offending AST
node. Colors are emitted only when `stderr` is an interactive terminal.

## Checks performed

### Program & function structure

| # | Rule | Example message |
|---|------|-----------------|
| 1 | The program must have a body. | `Program body is missing.` |
| 2 | The program's start name must match its end name. | `Program name 't' end name 'x' does not match.` |
| 3 | A function's start name must match its end name. | `Function 'F' end name 'G' does not match.` |
| 4 | A function must declare parameters. | `No parameters declared.` |
| 5 | A body (program or function) must contain at least one statement. | `No statements in body.` |

### Declarations

| # | Rule | Example message |
|---|------|-----------------|
| 6 | A constant name must be unique in its scope. | `Constant 'a' is already declared in the current scope.` |
| 7 | A constant defined via a named value must reference a declared type. | `Type 'X' for constant 'a' is not declared.` |
| 8 | A type name must be unique in its scope. | `Type 'A' is already declared in the current scope.` |
| 9 | Each member (literal) of a user-defined type must be unique. | `Member 'm' of type 'A' is already declared in the current scope.` |
| 10 | A variable of a user-defined type must reference a declared type. | `Type 'Undeclared' for variable 'x' is not declared.` |
| 11 | A variable name must be unique in its scope. | `Variable 'x' is already declared in the current scope.` |
| 12 | A function name must be unique in its scope. | `Function 'F' is already declared in the current scope.` |

### Scoping

Implemented by `SymbolTable` (`enterScope` / `exitScope`,
[`src/semantic_analyzer/symbol_table.cpp`](../src/semantic_analyzer/symbol_table.cpp)):

- Names declared in an outer scope are visible to inner scopes.
- Names declared in an inner scope are **not** visible to outer scopes.
- The same name may be declared in different scopes (shadowing is allowed).
- A function is declared in its enclosing scope **before** its body is
  analyzed, so recursive calls can resolve the function name.

### Statements

| # | Rule | Example message |
|---|------|-----------------|
| 13 | An `if` condition must be boolean. | `Condition in 'if' statement must be of boolean type.` |
| 14 | A `while` condition must be boolean. | `Condition in 'while' statement must be of boolean type.` |
| 15 | A `repeat … until` condition must be boolean. | `Condition in 'repeat' statement must be of boolean type.` |
| 16 | A `for` loop's expression must be boolean. | `Condition in 'for' statement must be of boolean type.` |
| 17 | A `case` selector must be integer, char, boolean, or a user-defined type. | `Case expression has an unknown type.` |
| 18 | Each non-literal `case` label must be a declared constant/enum literal. | `Case constant 'Bogus' is not declared.` |
| 19 | An `output` argument must be an integer or character expression (strings are handled separately). | `Output statement expects an integer or character expression.` |
| 20 | A `return` statement must carry an expression, and inside a function the returned value must match the function's declared return type (so all returns in a function agree). | `Return statement has no expression.` / `Return type mismatch in function 'F'. …` |
| 21 | An assignment's right-hand side type must match the declared type of the target. | `Type mismatch in assignment to 'x'. …` |
| 22 | Both operands of a swap (`:=:`) must be declared and share the same type. | `Type mismatch in swap statement. …` / `One of the identifiers in the swap statement is not declared.` |

### Expressions, identifiers & built-ins

| # | Rule | Example message |
|---|------|-----------------|
| 23 | Relational operators require matching operand types and yield boolean. | `Type mismatch in relational expression. …` |
| 24 | Additive/term operators (`+ - or`) require compatible operands. | `Type mismatch in term expression. …` |
| 25 | Multiplicative/factor operators (`* / mod and`) require compatible operands. | `Type mismatch in factor expression. …` |
| 26 | Unary minus requires an integer operand. | `Unary minus operator requires an integer operand, …` |
| 27 | Logical `not` requires a boolean operand. | `Logical NOT operator requires a boolean operand, …` |
| 28 | An identifier used in an expression must be declared. | `Identifier 'y' is not declared.` |
| 29 | A call must reference a declared name that is a function. | `Function 'G' is not declared.` / `'x' is not a function.` |
| 30 | `succ` / `pred` require an integer operand. | `Successor or predecessor function requires an integer operand, …` |
| 31 | `ord` requires a character operand; `chr` requires an integer operand. | `Ordinal conversion function requires a character operand, …` |

## Type-system assumptions

These are deliberate simplifications made by the analyzer:

1. A user-defined type is treated as an **ordered list of literals** (each
   literal is declared as a constant with an increasing ordinal).
2. `+` / `-` are allowed on a user-defined ordered-literal type only when its
   members are integer-valued; they are **not** allowed for boolean types.
   Other arithmetic operators are not assumed for type members.
3. The predeclared literals `true` and `false` are treated as boolean
   constants.
4. A function's return type is taken from its declaration and mapped to a
   `SymbolType` by `Symbol::getSymbolType` (`integer`, `char`, `string`,
   `boolean`, otherwise `UserDefined`). There is **no** `void` symbol type:
   `SymbolType` only has `Integer`, `Char`, `String`, `Boolean`, and
   `UserDefined`. The return type is recorded on the function's symbol and is
   used by the inference described in point 5 — it is not discarded.
5. When a variable that has not been declared is assigned the result of a
   function call, it is declared implicitly with the function's return type.
   User-defined types cannot be inferred this way and must be declared
   explicitly.


## Tests

These rules are exercised by
[`tests/unit/test_analyzer.cpp`](../tests/unit/test_analyzer.cpp), which feeds
small WinZigC programs through tokenize → parse → analyze and asserts on the
collected diagnostics. The underlying scope/symbol store has its own tests in
[`tests/unit/test_symbol_table.cpp`](../tests/unit/test_symbol_table.cpp). Run
them with:

```bash
./build/tests --gtest_filter='SemanticAnalyzerTest.*:SymbolTableTest.*'
```
