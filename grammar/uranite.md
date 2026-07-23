
<!--
@author hxAri (hxari)
@create 2025-02-24 15:15
@update 2026-07-19 20:05
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

Uranite (Accelerated Execution Through Quantum Precision) is an indentation-based, compiled language with a focus on ownership-based memory safety and high performance.

## 1. Lexical Structure

### 1.1 Identifiers
```
IDENTIFIER = [a-zA-Z_][a-zA-Z0-9_]*
```

### 1.2 Literals
- **Integer**: `123`, `0xABC`, `0b101`, `0o123`
- **Float**: `123.456`
- **String**: `"Hello, World!"`, `"""Multi-line string"""`
- **Char**: `'A'`, `'\n'`, `'\x41'`
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
- **Other**: `as`, `in`, `is`, `defer`, `async`, `await`, `use`, `asm`, `volatile`

### 1.4 Operators & Punctuation
- **Arithmetic**: `+`, `-`, `*`, `/`, `%`, `**`, `++`, `--`
- **Bitwise**: `&`, `|`, `^`, `~`, `<<`, `>>`
- **Comparison**: `==`, `!=`, `<`, `>`, `<=`, `>=`, `is`, `is not`, `in`, `not in`, `instanceof`, `subclassof`
- **Assignment**: `=`, `+=`, `-=`, `*=`, `/=`, `%=`, `&=`, `|=`, `^=`, `<<=`, `>>=`
- **Range**: `..` (exclusive), `...` (inclusive)
- **Punctuation**: `->`, `=>`, `.`, `:`, `,`, `;`, `@`, `#`, `?`
- **Delimiters**: `(`, `)`, `[`, `]`, `{`, `}`

### 1.5 Line Continuation
- **Explicit**: A backslash `\` immediately before a newline joins two physical lines into one logical line.
- **Implicit**: Newlines inside matched `()`, `[]`, `{}` pairs are suppressed by the lexer.

### 1.6 Comments & Doccomments
- **Line comments**: `# comment text`
- **Doccomments**: Triple-quoted strings (`"""..."""` or `'''...'''`) placed after declarations serve as documentation.

---

## 2. Syntax (Semi-Formal EBNF)

### 2.1 Top-Level
```ebnf
Program         ::= PackageDecl? Statement*
PackageDecl     ::= "package" PackageName NEWLINE
PackageName     ::= PackageSegment ("." PackageSegment)*
PackageSegment  ::= (IDENTIFIER | KEYWORD) ("-" (IDENTIFIER | KEYWORD | INTEGER))*
Statement       ::= CompoundStmt | SimpleStmt
```

Package segments accept keywords and hyphenated names (e.g., `uranite.async.context`, `uranite.os.vfs.file-descriptor`, `x86-64`).

### 2.2 Declarations
```ebnf
ImportDecl      ::= "import" ImportPath ("as" IDENTIFIER)? NEWLINE
                  | "from" ImportPath "import" ("*" | "{" ImportItem ("," ImportItem)* "}" | ImportItem ("," ImportItem)*) NEWLINE
ImportPath      ::= PackageSegment ("." PackageSegment)*
ImportItem      ::= IDENTIFIER ("as" IDENTIFIER)?

ExternDecl      ::= "extern" "function" IDENTIFIER "(" Params? ")" ("->" Type)? ";" NEWLINE

FunctionDecl    ::= Modifiers "function" IDENTIFIER GenericParams? "(" Params? ")" ("->" Type)? ("raises" Type ("|" Type)*)? (":" Block | ";")

ClassDecl       ::= Modifiers "class" IDENTIFIER GenericParams? 
                    ("extends" Type)? ("implements" Type ("," Type)*)? (":" Block | ";")

StructDecl      ::= Modifiers "struct" IDENTIFIER GenericParams? (":" Block | ";")

EnumDecl        ::= Modifiers "enum" IDENTIFIER ("backed" Type)? ":" INDENT EnumBody DEDENT
EnumBody        ::= (EnumVariant | EnumMethod | EnumField)+
EnumVariant     ::= "unit" IDENTIFIER Expression?
EnumMethod      ::= Modifiers ("function" | "property") IDENTIFIER GenericParams? "(" Params? ")" ("->" Type)? (":" Block | ";")
EnumField       ::= Modifiers Type IDENTIFIER ("=" Expression)? NEWLINE

InterfaceDecl   ::= Modifiers "interface" IDENTIFIER GenericParams? ("extends" Type ("," Type)*)? (":" Block | ";")

TraitDecl       ::= Modifiers "trait" IDENTIFIER GenericParams? ":" Block

ImplementDecl   ::= "implements" GenericParams? Type "for" Type ":" Block

TypeAliasDecl   ::= Modifiers "type" IDENTIFIER GenericParams? "=" Type NEWLINE

ConstDecl       ::= "const" Type IDENTIFIER "=" Expression NEWLINE

ExportBlock     ::= "export" "{" IDENTIFIER ("," IDENTIFIER)* "}"
```

### 2.3 Statements & Blocks
```ebnf
Block           ::= NEWLINE INDENT Statement* DEDENT
                  | SimpleStmt

SimpleStmt      ::= ReturnStmt | VarDecl | Assignment | ExpressionStmt
                  | BreakStmt | ContinueStmt | DeleteStmt | RaiseStmt
                  | DeferStmt | YieldStmt | PassStmt | AsmStmt

ReturnStmt      ::= "return" Expression? NEWLINE

IfStmt          ::= "if" Expression ":" Block ("elif" Expression ":" Block)* ("else" ":" Block)?

WhileStmt       ::= "while" Expression ":" Block

ForStmt         ::= "for" Type? IDENTIFIER "=" Expression ";" Expression ";" ForUpdate ":" Block
                  | "for" Type? IDENTIFIER "in" Expression ":" Block
ForUpdate       ::= Expression (AugAssign Expression | "++" | "--")?

MatchStmt       ::= "match" Expression ":" INDENT MatchCase* DEDENT
MatchCase       ::= Pattern "=>" (Block | SimpleStmt)

SwitchStmt      ::= "switch" Expression ":" INDENT SwitchCase+ DEDENT
SwitchCase      ::= "case" (Pattern | "*") ":" Block

Pattern         ::= IDENTIFIER ("." IDENTIFIER)*
                  | Literal
                  | "*"

TryStmt         ::= "try" ":" Block ExceptClause+ FinallyClause?
ExceptClause    ::= "except" (Type ("as" IDENTIFIER)?)? ":" Block
FinallyClause   ::= "finally" ":" Block

UnsafeBlock     ::= "unsafe" ":" Block

VarDecl         ::= Modifiers? Type IDENTIFIER ("=" Expression)? NEWLINE

Assignment      ::= Primary AugAssign Expression
                  | Primary "=" Expression
AugAssign       ::= "+=" | "-=" | "*=" | "/=" | "%=" | "&=" | "|=" | "^=" | "<<=" | ">>="

DeleteStmt      ::= "delete" Expression NEWLINE
RaiseStmt       ::= "raise" Expression NEWLINE
DeferStmt       ::= "defer" (Block | Statement)
YieldStmt       ::= "yield" ("from")? Expression NEWLINE
PassStmt        ::= "pass" NEWLINE
BreakStmt       ::= "break" NEWLINE
ContinueStmt    ::= "continue" NEWLINE
```

### 2.4 Expressions
```ebnf
Expression      ::= Disjunction | MatchExpr | LambdaExpr

Disjunction     ::= Conjunction ("or" Conjunction)*
Conjunction     ::= Inversion ("and" Inversion)*
Inversion       ::= "not" Inversion | Comparison

Comparison      ::= RangeExpr (CompareOp RangeExpr)*
CompareOp       ::= "==" | "!=" | "<=" | ">=" | "<" | ">" 
                  | "is" "not" | "is" | "not" "in" | "in" 
                  | "instanceof" | "subclassof"

RangeExpr       ::= BitwiseOr ((".." | "...") BitwiseOr)?

BitwiseOr       ::= BitwiseXor ("|" BitwiseXor)*
BitwiseXor      ::= BitwiseAnd ("^" BitwiseAnd)*
BitwiseAnd      ::= Shift ("&" Shift)*
Shift           ::= Sum (("<<" | ">>") Sum)*
Sum             ::= Term (("+" | "-") Term)*
Term            ::= Factor (("*" | "/" | "%") Factor)*

Factor          ::= ("addressof" | "move") Primary
                  | ("+" | "-" | "~") Factor
                  | Power
Power           ::= AwaitExpr | Primary ("**" Factor)?

AwaitExpr       ::= "await" Expression

Primary         ::= Primary "." IDENTIFIER
                  | Primary "(" Args? ")"
                  | Primary "[" Expression "]"
                  | Primary "++"
                  | Primary "--"
                  | Atom

Atom            ::= IDENTIFIER
                  | Literal
                  | "(" Expression ")"
                  | "(" ")"
                  | "(" Expression "," Expression* ")"
                  | "[" Expression ("for" Type? IDENTIFIER "in" Expression ("if" Expression)?)? "]"
                  | "[" (Expression ("," Expression)*)? "]"
                  | "self"
                  | "parent"
                  | "new" Type "(" Args? ")"
                  | "True"
                  | "False"
                  | "None"

MatchExpr       ::= "match" Expression "in" MatchArm ("," MatchArm)* ("," "*" "=>" Expression)?
MatchArm        ::= Expression "=>" Expression

LambdaExpr      ::= "lambda" LambdaParam ("," LambdaParam)* ":" Expression
                  | "async"? "function" "(" Params? ")" ("->" Type)? ":" (Block | Expression)
                  | "async"? "lambda" LambdaParam ("," LambdaParam)* ":" Expression
LambdaParam     ::= "final"? Type IDENTIFIER
```

### 2.5 Inline Assembly
```ebnf
AsmStmt         ::= "asm" "volatile"? STRING ":" AsmOutput ":" AsmInput ":" AsmClobber
AsmOutput       ::= "output" "(" AsmOperand ("," AsmOperand)* ")" | ε
AsmInput        ::= "input" "(" AsmOperand ("," AsmOperand)* ")" | ε
AsmClobber      ::= "clobber" "(" STRING ("," STRING)* ")" | ε
AsmOperand      ::= STRING IDENTIFIER
```

### 2.6 Types
```ebnf
Type            ::= UnionType
UnionType       ::= IntersectionType ("|" IntersectionType)*
IntersectionType::= BaseType ("&" BaseType)*
BaseType        ::= IDENTIFIER GenericArgs? 
                  | "?" BaseType
                  | BaseType "[]"
                  | "(" Type ")"
                  | "Callable" "<" Type "," "<" Type ("," Type)* ">" ">"
                  | "Meta" "<" Type ">"
```

### 2.7 Parameters & Arguments
```ebnf
Params          ::= Param ("," Param)*
Param           ::= "self"
                  | "&" "self"
                  | PropertyMod? "mut"? Type IDENTIFIER ("[]" | "{}")? ("=" Expression)?
PropertyMod     ::= ("public" | "private" | "protect") "Readonly"?

Args            ::= Expression ("," Expression)*

GenericParams   ::= "<" GenericParam ("," GenericParam)* ">"
GenericParam    ::= IDENTIFIER (":" Type ("," Type)*)?

GenericArgs     ::= "<" Type? ("," Type)* ">"
```

### 2.8 Modifiers
```ebnf
Modifiers       ::= AccessModifier? OopModifier*
AccessModifier  ::= "public" | "private" | "protect"
OopModifier     ::= "virtual" | "override" | "abstract" | "static" | "final" | "Readonly" | "const" | "async" | "property" | "native"
```

---

## 3. Key Features

- **Indentation Scoping**: Uses `:` and INDENT/DEDENT (like Python) instead of curly braces for blocks.
- **Ownership/Borrowing**: Keywords `ref`, `own`, `move`, `mut` indicate memory management semantics.
- **Nullable Types**: `?Type` prefix for optional values, with `None` as the null literal.
- **Expression-based**: `match expr in pattern => value, ...` is an expression.
- **Line Continuation**: Backslash before newline joins lines; implicit inside delimiters.
- **Variadic Parameters**: `Type name[]` declares a variadic parameter. Only one per function. Parameters with defaults after the variadic are keyword-only.
- **Keyword Parameters**: `Type name{}` declares a keyword parameter (must be last).
- **Constructor Property Declaration**: Parameters prefixed with access modifiers (`public Type name`) auto-declare class fields.
- **Self Parameter**: Methods take `self` or `&self` as first parameter.
- **Inline Assembly**: `asm volatile` with output/input/clobber operand syntax for direct hardware access.
- **Export Blocks**: `export { Name1, Name2 }` to make declarations importable by other modules.
- **Async/Await**: `async function` declarations return `Future<T>`. `await` suspends execution.
- **Range Expressions**: `a..b` (exclusive end), `a...b` (inclusive end).
- **Tuple Expressions**: `(a, b, c)` creates a tuple; `()` is an empty tuple.
- **Comprehensions**: `[expr for var in iterable if condition]` for list comprehension.
- **Enum Variants**: Require `unit` keyword. Backed enums: `enum Name backed I64:` with `unit Variant 0`.
- **Addressof**: `addressof expr` returns the memory address as `I64`.
- **Move Semantics**: `move expr` transfers ownership.
- **Identity Check**: `is` for identity/None comparison. `is not` as compound operator.
- **Switch Statement**: `switch expr:` with `case pattern:` branches and `case *:` for default.
- **Property Methods**: `property` modifier on methods for getter/setter access syntax.
- **Doccomments**: `"""..."""` after declarations for documentation.
- **Hyphenated Imports**: Module paths support hyphens (`uranite.os.vfs.file-descriptor`).
