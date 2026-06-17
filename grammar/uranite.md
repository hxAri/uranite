
<!--
@author hxAri (hxari)
@create 2025-02-24 15:15
@update 2026-06-17 20:03
@github https://github.com/uranite-lang/uranite

Uranite - Uranite Copyright (c) 2025 - hxAri <hxari@proton.me>
Uranite Licence under GNU General Public Licence v3

Formal grammar specification for the Uranite programming language.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
any later version.

You should have received a copy of the GNU General Public License
along with this program. If not, see <https://www.gnu.org/licenses/>.
-->

# Uranite Language Grammar

Uranite (Advanced Execution Through Endless Recursion) is an indentation-based, compiled language with a focus on ownership-based memory safety and high performance.

## 1. Lexical Structure

### 1.1 Identifiers
```
IDENTIFIER = [a-zA-Z_][a-zA-Z0-9_]*
```

### 1.2 Literals
- **Integer**: `123`, `0xABC`, `0b101`, `0o123`
- **Float**: `123.456`
- **String**: `"Hello, World!"`, `"""Multi-line string"""`
- **Char**: `'A'`
- **Boolean**: `True`, `False`
- **None**: `None`

### 1.3 Keywords
- **Control Flow**: `if`, `elif`, `else`, `match`, `switch`, `case`, `for`, `while`, `break`, `continue`, `return`, `yield`, `pass`
- **Declarations**: `function`, `lambda`, `mut`, `const`, `static`, `type`, `class`, `struct`, `enum`, `interface`, `trait`, `implements`, `package`, `import`, `from`, `extern`, `unit`, `backed`, `export`
- **Access Modifiers**: `public`, `private`, `protect`
- **OOP**: `virtual`, `override`, `abstract`, `final`, `Readonly`, `native`, `extends`, `self`, `parent`, `new`, `delete`, `property`
- **Memory & Safety**: `unsafe`, `ref`, `own`, `move`, `addressof`
- **Logic**: `and`, `or`, `not`
- **Error Handling**: `try`, `except`, `raise`, `raises`, `finally`
- **Primitive Types**: `I8`, `I16`, `I32`, `I64`, `U8`, `U16`, `U32`, `U64`, `F32`, `F64`, `Bool`, `Boolean`, `Byte`, `Bytes`, `Char`, `Integer`, `Long`, `String`, `Void`, `NoneType`, `Object`, `Callable`, `Meta`
- **Other**: `as`, `in`, `is`, `where`, `defer`, `async`, `await`, `use`, `asm`, `volatile`

### 1.4 Operators & Punctuation
- **Arithmetic**: `+`, `-`, `*`, `/`, `%`, `**`, `++`, `--`
- **Bitwise**: `&`, `|`, `^`, `~`, `<<`, `>>`
- **Comparison**: `==`, `!=`, `<`, `>`, `<=`, `>=`, `is`, `is not`, `in`, `not in`, `instanceof`, `subclassof`
- **Assignment**: `=`, `+=`, `-=`, `*=`, `/=`, `%=`, `&=`, `|=`, `^=`, `<<=`, `>>=`
- **Range**: `..`, `...`
- **Punctuation**: `->`, `=>`, `.`, `..`, `...`, `:`, `,`, `;`, `@`, `#`, `?`, `!`
- **Delimiters**: `(`, `)`, `[`, `]`, `{`, `}`

---

## 2. Syntax (Semi-Formal EBNF)

### 2.1 Top-Level
```ebnf
Program         ::= PackageDecl? Statement*
PackageDecl     ::= "package" IDENTIFIER ("." IDENTIFIER)* NEWLINE
Statement       ::= CompoundStmt | SimpleStmt
```

### 2.2 Declarations
```ebnf
ImportDecl      ::= "import" IDENTIFIER ("." IDENTIFIER)* ("as" IDENTIFIER)? NEWLINE
                  | "from" IDENTIFIER ("." IDENTIFIER)* "import" ("*" | "{" ImportItem ("," ImportItem)* "}" | ImportItem) NEWLINE
ImportItem      ::= IDENTIFIER ("as" IDENTIFIER)?

ExternDecl      ::= "extern" "function" IDENTIFIER "(" Params? ")" ("->" Type)? ";" NEWLINE

FunctionDecl    ::= AccessModifier? ("virtual" | "override" | "abstract" | "static" | "final" | "async")* 
                    "function" IDENTIFIER GenericParams? "(" Params? ")" ("->" Type)? ("raises" Type ("|" Type)*)? (":" Block | NEWLINE)

ClassDecl       ::= AccessModifier? "Readonly"? "final"? "class" IDENTIFIER GenericParams? 
                    ("extends" Type)? ("implements" Type ("," Type)*)? (":" Block | ";")

StructDecl      ::= AccessModifier? "struct" IDENTIFIER GenericParams? (":" Block | ";")

EnumDecl        ::= AccessModifier? "enum" IDENTIFIER ("backed" Type)? ":" INDENT EnumVariant+ DEDENT
EnumVariant     ::= "unit" IDENTIFIER Expression?

InterfaceDecl   ::= AccessModifier? "interface" IDENTIFIER GenericParams? ("extends" Type ("," Type)*)? (":" Block | ";")

TraitDecl       ::= AccessModifier? "trait" IDENTIFIER GenericParams? ":" Block

ImplementDecl   ::= "implements" GenericParams? Type "for" Type ":" Block

TypeAliasDecl   ::= AccessModifier? "type" IDENTIFIER GenericParams? "=" Type NEWLINE

ExportBlock     ::= "export" "{" IDENTIFIER ("," IDENTIFIER)* "}"
```

### 2.3 Statements & Blocks
```ebnf
Block           ::= NEWLINE INDENT Statement* DEDENT
                  | SimpleStmt

Statement       ::= ReturnStmt 
                  | IfStmt 
                  | MatchStmt 
                  | SwitchStmt
                  | ForStmt 
                  | WhileStmt 
                  | VarDecl 
                  | Assignment
                  | ExpressionStmt 
                  | UnsafeBlock 
                  | DeferStmt 
                  | TryStmt
                  | SpawnStmt
                  | "break" NEWLINE
                  | "continue" NEWLINE
                  | "pass" NEWLINE
                  | "delete" Expression NEWLINE
                  | "raise" Expression NEWLINE
                  | AsmStmt NEWLINE

ReturnStmt      ::= "return" Expression? NEWLINE

IfStmt          ::= "if" Expression ":" Block ("elif" Expression ":" Block)* ("else" ":" Block)?

WhileStmt       ::= "while" Expression ":" Block

ForStmt         ::= "for" VarDecl ";" Expression ";" Expression ":" Block
                  | "for" Type? IDENTIFIER "in" Expression ":" Block

MatchStmt       ::= "match" Expression ":" INDENT MatchCase* DEDENT
MatchCase       ::= Pattern "=>" (Block | SimpleStmt)

VarDecl         ::= AccessModifier? Type IDENTIFIER ("=" Expression)? NEWLINE

DeferStmt       ::= "defer" (Block | Statement)

AsmStmt         ::= "asm" "volatile" LIT_STRING ":" AsmOutput ":" AsmInput ":" AsmClobber
AsmOutput       ::= "output" "(" AsmOperand ("," AsmOperand)* ")" | ε
AsmInput        ::= "input" "(" AsmOperand ("," AsmOperand)* ")" | ε
AsmClobber      ::= "clobber" "(" LIT_STRING ("," LIT_STRING)* ")" | ε
AsmOperand      ::= LIT_STRING IDENTIFIER
```

### 2.4 Expressions
```ebnf
Expression      ::= LogicalExpr | MatchExpr | LambdaExpr
LogicalExpr     ::= ComparisonExpr (("and" | "or") ComparisonExpr)*
ComparisonExpr  ::= RangeExpr (CompareOp RangeExpr)*
RangeExpr       ::= ArithmeticExpr (RangeOp ArithmeticExpr)?
ArithmeticExpr  ::= Term (("+" | "-") Term)*
Term            ::= Factor (("*" | "/" | "%") Factor)*
Factor          ::= ("addressof" | "move") Primary | ("+" | "-" | "~" | "not") Factor | Primary
Primary         ::= Atom ("." IDENTIFIER | "(" Args? ")" | "[" Expression "]" | "++" | "--")*
Atom            ::= IDENTIFIER | Literal | "(" Expression ")" | "self" | "parent" | "new" Type "(" Args? ")"
```

### 2.5 Types
```ebnf
Type            ::= UnionType
UnionType       ::= IntersectionType ("|" IntersectionType)*
IntersectionType::= BaseType ("&" BaseType)*
BaseType        ::= IDENTIFIER GenericArgs? 
                  | "?" BaseType
                  | BaseType "[]"
                  | "(" Type ")"
                  | "Callable" "<" Type "," "<" Type ("," Type)* ">" ">"
```

---

## 3. Key Features

- **Indentation Scoping**: Uses `:` and INDENT/DEDENT (like Python) instead of curly braces for blocks.
- **Ownership/Borrowing**: Keywords `ref`, `own`, `move`, `mut` indicate memory management semantics.
- **Uniform Function Call Syntax (UFCS)**: Supports both `func(obj, arg)` and `obj.func(arg)`.
- **Expression-based**: Many constructs are expressions.
- **Nullable types**: `?Type` prefix for optional values, with `None` as the null literal.
- **Inline Assembly**: `asm volatile` with output/input/clobber operand syntax for direct hardware access.
- **Export Blocks**: `export { Name1, Name2 }` to make declarations importable by other modules.
