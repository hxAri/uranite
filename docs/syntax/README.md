# Language Syntax

This section is the complete reference for every syntactic construct in the Uranite programming language. It covers everything from lexical tokens and primitive types through ownership semantics, concurrency primitives, and inline assembly. Each subsection maps directly to the compiler's parser, semantic analyzer, and IR lowering stages — when a construct is described, the documentation explains not just how to write it, but how the compiler processes it.

---

## Table of Contents

- [Syntax Philosophy](#syntax-philosophy)
- [How Syntax Flows Through the Compiler](#how-syntax-flows-through-the-compiler)
- [Section Map](#section-map)
  - [Lexical Conventions](#lexical-conventions)
  - [Types](#types)
  - [Variables](#variables)
  - [Operators](#operators)
  - [Expressions](#expressions)
  - [Control Flow](#control-flow)
  - [Functions](#functions)
  - [Classes](#classes)
  - [Structs](#structs)
  - [Interfaces](#interfaces)
  - [Traits](#traits)
  - [Abstract Classes](#abstract-classes)
  - [Generics](#generics)
  - [Enums](#enums)
  - [Collections](#collections)
  - [Memory and Ownership](#memory-and-ownership)
  - [Error Handling](#error-handling)
  - [Async and Concurrency](#async-and-concurrency)
  - [Modules and Packages](#modules-and-packages)
  - [Inline Assembly](#inline-assembly)
  - [Interop](#interop)

---

## Syntax Philosophy

Uranite's syntax is built on four foundational principles that differentiate it from C-family languages:

**Indentation defines structure.** Blocks are opened by a colon and delimited by indentation level, not braces. This is not cosmetic — the lexer produces `Indent` and `Dedent` tokens that the parser consumes as structural delimiters. The compiler enforces consistent indentation (4 spaces per level) as a hard requirement, not a style preference. There are no optional braces, no "one-liner" forms that bypass indentation, and no ambiguity about where a block begins or ends.

```uranite
public function classify( I64 temperature ) -> String:
    if temperature > 100:
        return "extreme"
    elif temperature > 50:
        return "high"
    else:
        return "normal"
```

**Keywords replace symbols for logical operations.** Boolean logic uses `and`, `or`, and `not` exclusively. The symbols `&&`, `||`, and `!` are not valid tokens. Identity checks use `is` and `is not` rather than reference-equality operators. This makes conditional expressions read as natural-language predicates:

```uranite
if connection is not None and retryCount < maxRetries:
    if isAuthenticated or hasGuestAccess:
        if not rateLimited:
            processRequest( connection )
```

**Static typing is explicit and mandatory.** Every variable, parameter, and return type carries an explicit type annotation. There is no type inference for declarations — `I64 count = 0`, never `count = 0`. Generic type parameters are specified at instantiation (`ArrayList<String>`), with diamond inference (`new ArrayList<>()`) permitted only in constructor calls where the target type is unambiguous from the left-hand side of an assignment.

**Move semantics are the default.** Non-primitive types transfer ownership on assignment. After `ArrayList<I64> moved = original`, the variable "original" is invalidated and any subsequent use is a compile-time error caught by the borrow checker. This eliminates an entire class of use-after-free and double-free bugs without requiring a garbage collector.

These four principles — structural indentation, keyword-based logic, explicit typing, and ownership-by-default — form the syntactic identity of Uranite. Every construct documented in this section operates within these constraints.

---

## How Syntax Flows Through the Compiler

Understanding Uranite's syntax requires understanding how each construct is processed by the compiler's 12-stage pipeline. The syntax section documents the first three stages most directly, but every construct ultimately flows through all twelve.

### Stage 1: Lexer

The lexer (`src/uranite/lexer/lexer.cpp`, ~740 lines) transforms raw source text into a stream of tokens. It handles:

- **Indentation processing** — The `handleIndentation()` algorithm counts leading whitespace at each line start, maintains an indentation stack, and emits `Indent`/`Dedent` tokens that the parser treats as block delimiters.
- **Keyword recognition** — 62 reserved keywords (from `and` to `yield`) are distinguished from identifiers during tokenization.
- **Literal scanning** — Integer literals (decimal, hex `0x`, octal `0o`, binary `0b`), floating-point literals, string literals (with escape sequences), character literals, and boolean literals (`True`/`False`) are each scanned by dedicated routines.
- **Operator and punctuation tokens** — Multi-character operators (`==`, `!=`, `<=`, `>=`, `..`, `...`, `::`, `->`, `=>`) are recognized by lookahead.

The lexer produces a flat `std::vector<token::Token>` with no tree structure. Every syntactic construct documented in this section begins as a sequence of these tokens.

### Stage 2: Parser

The parser (`src/uranite/parser/parser.cpp`, ~2,960 lines) is a recursive-descent parser that consumes the token stream and produces an Abstract Syntax Tree (AST). The AST consists of 72 node kinds organized into three categories:

- **Expressions** — Values that produce a result: `BinaryExpression`, `CallExpression`, `ConstructExpression`, `MemberAccessExpression`, `LambdaExpression`, `CastExpression`, `IndexExpression`, `RangeExpression`, `ArrayExpression`, `TupleExpression`, `ComprehensionExpression`, literals, and identifiers.
- **Statements** — Actions that execute: `VariableStatement`, `AssignStatement`, `IfStatement`, `ForStatement`, `WhileStatement`, `MatchStatement`, `ReturnStatement`, `BreakStatement`, `ContinueStatement`, `ThrowStatement`, `TryCatchStatement`, `DeferStatement`, `DeleteStatement`, `UnsafeBlockStatement`, `PassStatement`, and `ExpressionStatement`.
- **Declarations** — Named entities: `FunctionDeclaration`, `ClassDeclaration`, `InterfaceDeclaration`, `StructDeclaration`, `EnumDeclaration`, `ImportDeclaration`, `ExternDeclaration`, `ModuleDeclaration`, `TypeAliasDeclaration`, `ImplementDeclaration`, and `ExportDeclaration`.

The parser handles operator precedence, indentation-delimited blocks, generic type parameter parsing (distinguishing `<` as a comparison from `<` as a generic bracket), and the disambiguation of ambiguous constructs like `{` (which can open a map literal, a set literal, an export block, or a kwargs marker).

### Stage 3: Semantic Analyzer

The semantic analyzer (`src/uranite/semantic/analyzer.cpp`, ~5,450 lines) performs two-pass analysis over the AST:

- **Pass 1 (Registration)** — `analyzeModuleRegistration()` eagerly registers all top-level declarations (classes, interfaces, enums, functions, constants, type aliases) into the type registry. This allows forward references — a class can reference another class declared later in the same file.
- **Pass 2 (Analysis)** — Full type checking, overload resolution, operator-to-interface mapping (e.g., `+` maps to `Addable.add()`), generic instantiation, borrow checking, and symbol resolution. Every expression is assigned a resolved type. Every function call is bound to a specific declaration. Every operator is mapped to its corresponding interface method.

The semantic analyzer produces an annotated AST where every node carries its resolved type information. This annotated AST is the input to HIR lowering.

### Stages 4–12: IR and Backend

After semantic analysis, the annotated AST flows through:

- **HIR Lowering** — Converts the AST into High-level IR, preserving semantic constructs (loops, match statements, classes) but attaching resolved type information.
- **HIR Validation** — Verifies HIR structural invariants.
- **MIR Lowering** — Flattens HIR into Mid-level IR: a linear sequence of instructions organized into basic blocks with explicit terminators. Loops become header/body/update/exit block patterns. Method calls become explicit virtual dispatch through itables.
- **MIR Liveness, Borrow Check, Optimization** — Register liveness analysis, ownership verification at the IR level, and optimization passes.
- **MIR Codegen** — Maps MIR instructions to LLVM IR. This is the production codegen path. Handles constant folding, type coercion, itable generation for virtual dispatch, shadow stack emission for exception handling, and wrapping arithmetic for integer operations.
- **LLVM Backend** — LLVM optimization passes and native code generation.
- **Linker** — Links against C runtime libraries and produces the final executable.

Each syntax construct documented in the subsections below identifies where in this pipeline it is processed and how it is transformed at each stage.

---

## Section Map

### [Lexical Conventions](lexical-conventions/README.md)

The foundational building blocks of Uranite source code: how text becomes tokens.

| Document | Description |
|---|---|
| [Source Files and Encoding](lexical-conventions/source-files-and-encoding.md) | File encoding requirements, the `.urn` extension, and source file structure rules. |
| [Comments and Doccomments](lexical-conventions/comments-and-doccomments.md) | Single-line `#` comments, triple-quoted `"""..."""` doccomments placed after declarations, and the "Parameters:", "Returns:", "Complexity:" doccomment format. |
| [Indentation and Blocks](lexical-conventions/indentation-and-blocks.md) | The lexer's indentation algorithm, the indentation stack, `Indent`/`Dedent` token emission, tab normalization, and the 4-space rule. |
| [Reserved Keywords](lexical-conventions/reserved-keywords.md) | Complete enumeration of all 62 reserved keywords, organized by category (control flow, declarations, types, modifiers, operators, memory). |
| [Identifiers and Naming](lexical-conventions/identifiers-and-naming.md) | Identifier rules, naming conventions (descriptive names required, no single-letter identifiers), and the linter's enforcement of minimum name lengths. |
| [Integer Literals](lexical-conventions/integer-literals.md) | Decimal, hexadecimal (`0x`), octal (`0o`), and binary (`0b`) integer literal formats. |
| [Float Literals](lexical-conventions/float-literals.md) | Floating-point literal syntax, scientific notation, and IEEE 754 representation. |
| [String Literals](lexical-conventions/string-literals.md) | String literal syntax, escape sequences, null-terminated UTF-8 representation, and string interning. |
| [Char Literals](lexical-conventions/char-literals.md) | Character literal syntax, Unicode representation, and the 32-bit `Char` type. |
| [Boolean and None Literals](lexical-conventions/boolean-and-none-literals.md) | `True`, `False`, and `None` literal tokens and their semantic meaning. |
| [Regex Literals](lexical-conventions/regex-literals.md) | Regular expression literal syntax and compilation. |

### [Types](types/README.md)

The complete type system: every type kind the compiler recognizes, from machine integers to metatypes.

| Document | Description |
|---|---|
| [Integer Types](types/integer-types.md) | `I8`, `I16`, `I32`, `I64`, `Int`, `Long` — signed integer types with sizes, ranges, and LLVM IR mapping. |
| [Unsigned Integer Types](types/unsigned-integer-types.md) | `U8`, `U16`, `U32`, `U64`, `Byte` — unsigned integer types. |
| [Floating-Point Types](types/floating-point-types.md) | `F32`, `F64`, `Float`, `Double` — IEEE 754 floating-point types. |
| [Boolean Type](types/boolean-type.md) | The `Boolean` type, `True`/`False` values, and truthiness rules. |
| [Char Type](types/char-type.md) | The 32-bit `Char` type for Unicode characters. |
| [String Type](types/string-type.md) | The `String` type — null-terminated UTF-8, OOP wrapper methods, and memory representation. |
| [Void and None](types/void-and-none.md) | `Void` as a return type, `None` as the null value, and their distinct roles. |
| [Nullable Types](types/nullable-types.md) | The `?T` nullable type prefix, `None` assignment, and `is`/`is not` checks. |
| [Callable Types](types/callable-types.md) | `Callable<ReturnType, <ParamTypes>>` for function references and lambda typing. |
| [Tuple Types](types/tuple-types.md) | `Tuple<E>` as a fixed-size heterogeneous container and its type representation. |
| [Array Types](types/array-types.md) | Raw `Memory<T>` arrays, array literals, and their relationship to heap allocation. |
| [Pointer Types](types/pointer-types.md) | Raw pointer types for low-level memory access within `unsafe` blocks. |
| [Reference Types](types/reference-types.md) | Reference semantics and their interaction with the borrow checker. |
| [Union Types](types/union-types.md) | Union type declarations for type-safe tagged unions. |
| [Result Types](types/result-types.md) | Result types for error-handling patterns without exceptions. |
| [Meta Types](types/meta-types.md) | Metatype representations used in reflection and type introspection. |
| [Future Type](types/future-type.md) | `Future<T>` as the return type of async functions. |
| [Generator Type](types/generator-type.md) | `Generator<T>` for lazy value production with `yield`. |
| [Type Aliases](types/type-aliases.md) | `type NewName = ExistingType` declarations and their semantic equivalence. |
| [Type Identity and Qualified Names](types/type-identity-and-qualified-names.md) | The `->qualified` system for type comparison, the `qualnames.hpp` registry, and why short names must never be used for type identity. |
| [OOP Wrappers](types/oop-wrappers.md) | Primitive wrapper classes (`uranite.language.Integer`, `uranite.language.Float`, etc.) and their builtin descriptor registration. |

### [Variables](variables/README.md)

Variable declarations, constants, assignment, and mutability.

| Document | Description |
|---|---|
| [Variable Declarations](variables/variable-declarations.md) | Typed variable declarations (`Type name = value`), scope rules, and shadowing behavior. |
| [Constants](variables/constants.md) | Top-level `const` declarations compiled as LLVM `GlobalVariable` constants. |
| [Assignment](variables/assignment.md) | Simple assignment statements and ownership transfer semantics. |
| [Compound Assignment](variables/compound-assignment.md) | `+=`, `-=`, `*=`, `/=` operators and their desugaring. |
| [Mutable and Volatile](variables/mutable-and-volatile.md) | The `mutable` and `volatile` modifiers and their effects on optimization and access rules. |

### [Operators](operators/README.md)

Every operator in the language, its precedence, and how operators map to interface methods.

| Document | Description |
|---|---|
| [Arithmetic Operators](operators/arithmetic-operators.md) | `+`, `-`, `*`, `/`, `%` — wrapping arithmetic for integers, IEEE 754 for floats, auto-inserted zero-checks for division and modulo. |
| [Comparison Operators](operators/comparison-operators.md) | `==`, `!=`, `<`, `>`, `<=`, `>=` — mapped to `Equatable` and `Comparable` interface methods. |
| [Logical Operators](operators/logical-operators.md) | `and`, `or`, `not` — keyword-based boolean logic with short-circuit evaluation. |
| [Bitwise Operators](operators/bitwise-operators.md) | Bitwise AND, OR, XOR, NOT, left shift, and right shift operations. |
| [Assignment Operators](operators/assignment-operators.md) | Simple and compound assignment operators. |
| [Identity Operators](operators/identity-operators.md) | `is` and `is not` for reference identity and `None` checking. |
| [Power Operator](operators/power-operator.md) | The exponentiation operator and its precedence. |
| [Unary Operators](operators/unary-operators.md) | Prefix negation (`-x`) mapped to `Negatable.negate()`, and other unary operations. |
| [Increment and Decrement](operators/increment-and-decrement.md) | `++` and `--` as prefix and postfix operators. |
| [Operator Precedence](operators/operator-precedence.md) | Complete precedence table from highest to lowest binding strength. |
| [Operator Interfaces](operators/operator-interfaces.md) | The `OperatorInterfaceMapping` system: how `+` maps to `Addable.add()`, `==` maps to `Equatable.equals()`, and the full mapping table. |

### [Expressions](expressions/README.md)

All expression forms that produce values.

| Document | Description |
|---|---|
| [Binary Expressions](expressions/binary-expressions.md) | Arithmetic, comparison, and logical binary operations. |
| [Unary Expressions](expressions/unary-expressions.md) | Prefix and postfix unary operations. |
| [String Concatenation](expressions/string-concatenation.md) | String `+` operator behavior and performance characteristics. |
| [String Formatting](expressions/string-formatting.md) | Formatted string construction patterns. |
| [Range Expressions](expressions/range-expressions.md) | Exclusive ranges (`0..10`) and inclusive ranges (`1...15`). |
| [Index Expressions](expressions/index-expressions.md) | Collection indexing (`array[index]`) mapped to `Indexable.get()`. |
| [Member Access Expressions](expressions/member-access-expressions.md) | Dot-notation field access (`object.field`). |
| [Method Call Expressions](expressions/method-call-expressions.md) | Method invocation (`object.method( args )`), virtual dispatch, and itable lookup. |
| [Call Expressions](expressions/call-expressions.md) | Free function calls, static method calls, and scope-resolution (`Class::method`). |
| [Constructor Expressions](expressions/constructor-expressions.md) | `new ClassName( args )` heap allocation and constructor invocation. |
| [Cast Expressions](expressions/cast-expressions.md) | `value as TargetType` explicit type casting. |
| [Instanceof and Subclassof](expressions/instanceof-and-subclassof.md) | Runtime type checking expressions. |
| [Self and Super](expressions/self-and-super.md) | `self` for instance reference, `super` for parent class access. |
| [Lambda Expressions](expressions/lambda-expressions.md) | Anonymous functions: `lambda ParamType param: body`. |
| [Await Expressions](expressions/await-expressions.md) | `await futureValue` for asynchronous result extraction. |
| [Yield Expressions](expressions/yield-expressions.md) | `yield value` for generator-based lazy evaluation. |

### [Control Flow](control-flow/README.md)

Branching, looping, and pattern matching.

| Document | Description |
|---|---|
| [If / Elif / Else](control-flow/if-elif-else.md) | Conditional branching with indentation-delimited bodies. |
| [While Loops](control-flow/while-loops.md) | Condition-based iteration. |
| [For-In Loops](control-flow/for-in-loops.md) | Iterator-based loops using the `Iterator<T>` protocol (`has`/`next`). |
| [Range Iteration](control-flow/range-iteration.md) | `for x in start..end` and `for x in start...end` range-based loops. |
| [Collection Iteration](control-flow/collection-iteration.md) | Iterating over `ArrayList`, `HashSet`, `Generator`, and other `Iterable<T>` types. |
| [Map Destructuring Iteration](control-flow/map-destructuring-iteration.md) | `for Key key, Value value in hashMap` two-variable destructuring. |
| [Break and Continue](control-flow/break-and-continue.md) | Loop control statements. |
| [Match Statements](control-flow/match-statements.md) | Pattern matching with `case` branches and `default` fallback. |
| [Match Expressions](control-flow/match-expressions.md) | Match as an expression producing a value. |
| [Switch/Case Statements](control-flow/switch-case-statements.md) | Traditional switch-case branching for value-based dispatch. |
| [Pass Statement](control-flow/pass-statement.md) | The `pass` no-op placeholder for empty blocks. |

### [Functions](functions/README.md)

Function declarations, parameter forms, closures, and external linkage.

| Document | Description |
|---|---|
| [Function Declarations](functions/function-declarations.md) | `function name( params ) -> ReturnType:` syntax, access modifiers, and the "self" convention. |
| [Parameters and Return Types](functions/parameters-and-return-types.md) | Typed parameters, mandatory return type annotations, and `Void` return. |
| [Default Parameters](functions/default-parameters.md) | `param=defaultValue` syntax and evaluation semantics. |
| [Variadic Parameters](functions/variadic-parameters.md) | `Type args{}` variadic parameter syntax, the `Args<T>` struct, and variadic forwarding. |
| [Keyword Parameters](functions/keyword-parameters.md) | `Type kwargs{}` keyword parameter syntax and the `Kwargs<String, T>` struct. |
| [Nested Functions](functions/nested-functions.md) | Functions defined inside other functions with lexical scope access. |
| [Closures and Capture](functions/closures-and-capture.md) | Variable capture semantics for lambdas and nested functions. |
| [Recursion](functions/recursion.md) | Recursive function patterns and tail-call behavior. |
| [Extern Declarations](functions/extern-declarations.md) | `extern function` for C ABI linkage to native libraries. |

### [Classes](classes/README.md)

Object-oriented programming constructs.

| Document | Description |
|---|---|
| [Class Declarations](classes/class-declarations.md) | `class ClassName:` syntax and class body structure. |
| [Fields and Visibility](classes/fields-and-visibility.md) | Instance fields with `public`, `private`, `protect` modifiers. |
| [Constructors](classes/constructors.md) | Constructor methods named after the class, taking `self` as first parameter. |
| [Constructor Overloading](classes/constructor-overloading.md) | Multiple constructors with different parameter signatures. |
| [Methods](classes/methods.md) | Instance methods, the implicit `self` parameter, and method resolution. |
| [Static Methods](classes/static-methods.md) | Class-level methods without `self`, invoked via `ClassName.method()`. |
| [Properties](classes/properties.md) | `property name( self ) -> Type:` computed field accessors. |
| [Inheritance and Extends](classes/inheritance-and-extends.md) | Single inheritance with `extends` and `parent()` constructor chaining. |
| [Parent Constructor Calls](classes/parent-constructor-calls.md) | `parent( args )` syntax for invoking superclass constructors. |
| [Method Overriding](classes/method-overriding.md) | `override function` for replacing inherited method implementations. |
| [Readonly Classes](classes/readonly-classes.md) | `Readonly class` where fields are immutable after construction. |
| [Final Classes](classes/final-classes.md) | Classes that cannot be subclassed. |
| [Native Classes](classes/native-classes.md) | Classes with native (C runtime) backing implementations. |
| [Virtual Dispatch](classes/virtual-dispatch.md) | Itable-based virtual method dispatch in MIR codegen. |

### [Structs](structs/README.md)

Value types with no inheritance.

| Document | Description |
|---|---|
| [Struct Declarations](structs/struct-declarations.md) | `struct StructName:` syntax, value semantics, and differences from classes. |

### [Interfaces](interfaces/README.md)

Contract-based polymorphism.

| Document | Description |
|---|---|
| [Interface Declarations](interfaces/interface-declarations.md) | `interface InterfaceName:` with bodyless method signatures (trailing semicolons). |
| [Implementing Interfaces](interfaces/implementing-interfaces.md) | `class X implements Y:` and the `implement InterfaceName for ClassName:` block form. |
| [Multiple Interface Implementation](interfaces/multiple-interface-implementation.md) | `implements InterfaceA, InterfaceB` syntax for implementing multiple contracts. |
| [Builtin Operator Interfaces](interfaces/builtin-operator-interfaces.md) | `Addable`, `Subtractable`, `Multipliable`, `Dividable`, `Modulable`, `Equatable`, `Comparable`, `Negatable`, `Stringable`, `Hashable`, `Indexable`, `Iterable`, `Iterator`, `Droper`. |

### [Traits](traits/README.md)

Reusable method implementations mixed into classes.

| Document | Description |
|---|---|
| [Trait Declarations](traits/trait-declarations.md) | `trait TraitName:` with default method implementations. |
| [Trait Mixing with Use](traits/trait-mixing-with-use.md) | `use TraitName` to mix trait methods into a class body. |
| [Trait Method Resolution](traits/trait-method-resolution.md) | Resolution order when multiple traits provide the same method name. |

### [Abstract Classes](abstract-classes/README.md)

Partially implemented base classes.

| Document | Description |
|---|---|
| [Abstract Declarations](abstract-classes/abstract-declarations.md) | `abstract class` with `abstract function` bodyless signatures and concrete methods. |
| [Polymorphic Dispatch](abstract-classes/polymorphic-dispatch.md) | How abstract method calls resolve at runtime through vtable dispatch. |

### [Generics](generics/README.md)

Parametric polymorphism.

| Document | Description |
|---|---|
| [Type Parameters](generics/type-parameters.md) | `<T>`, `<K, V>` type parameter syntax and constraints. |
| [Generic Classes](generics/generic-classes.md) | `class Container<T>:` with type-parameterized fields and methods. |
| [Generic Functions](generics/generic-functions.md) | `function identity<T>( T value ) -> T:` standalone generic functions. |
| [Generic Static Methods](generics/generic-static-methods.md) | `static function create<T>( T value ) -> T:` on generic classes. |
| [Diamond Inference](generics/diamond-inference.md) | `new ArrayList<>()` where the type parameter is inferred from context. |
| [Where Clauses](generics/where-clauses.md) | Constraint clauses restricting type parameters to specific interfaces. |
| [Type Erasure and Runtime Representation](generics/type-erasure-and-runtime-representation.md) | How generic types are represented in MIR and LLVM IR. |

### [Enums](enums/README.md)

Enumerated types with optional backing values.

| Document | Description |
|---|---|
| [Simple Enums](enums/simple-enums.md) | `enum Direction:` with `unit` variants. |
| [Backed Enums](enums/backed-enums.md) | `enum HttpStatus backed I64:` with `unit Ok 200` value association. |
| [Enum Name and Value Properties](enums/enum-name-and-value-properties.md) | `.name` and `.value` accessors on enum variants. |
| [Enum Methods](enums/enum-methods.md) | Methods and properties defined on enum types. |
| [Per-Variant Overrides](enums/per-variant-overrides.md) | Variant-level method overrides with colon-indented bodies after the backing value. |

### [Collections](collections/README.md)

Collection types, literals, and comprehensions.

| Document | Description |
|---|---|
| [ArrayList](collections/array-list.md) | `ArrayList<E>` — resizable array-backed list with `add`, `get`, `remove`, `size`. |
| [HashMap](collections/hash-map.md) | `HashMap<K, V>` — hash-based key-value associative container. |
| [HashSet](collections/hash-set.md) | `HashSet<E>` — hash-based unordered unique element collection. |
| [Tuple](collections/tuple.md) | `Tuple<E>` — fixed-size ordered container. |
| [Pair](collections/pair.md) | `Pair<K, V>` — two-element key-value pair. |
| [Generator](collections/generator.md) | `Generator<T>` — lazy value sequence produced by `yield`. |
| [Array Literals](collections/array-literals.md) | `[1, 2, 3]` syntax and `Memory<T>` heap allocation desugaring. |
| [Map Literals](collections/map-literals.md) | `{"key": value}` syntax and `HashMap` constructor desugaring. |
| [Set Literals](collections/set-literals.md) | `{value1, value2}` syntax and `HashSet` constructor desugaring. |
| [Tuple Literals](collections/tuple-literals.md) | `(value1, value2)` syntax and `Tuple` constructor desugaring. |
| [List Comprehensions](collections/list-comprehensions.md) | `[expr for Type x in iterable if condition]` array comprehension syntax. |
| [Map Comprehensions](collections/map-comprehensions.md) | `{key: value for Type x in iterable}` map comprehension syntax. |
| [Set Comprehensions](collections/set-comprehensions.md) | `{expr for Type x in iterable}` set comprehension syntax. |
| [Sequence Interfaces](collections/sequence-interfaces.md) | `Collection<E>`, `Sequence<E>`, `ImmutableSequence<E>`, `Map<K, V>` interface hierarchy. |
| [Iterator Protocol](collections/iterator-protocol.md) | `Iterable<T>` and `Iterator<T>` — the `iterator()`/`has`/`next` protocol for `for-in` loops. |

### [Memory and Ownership](memory-and-ownership/README.md)

Ownership semantics, move rules, manual memory management, and safety boundaries.

| Document | Description |
|---|---|
| [Ownership Model](memory-and-ownership/ownership-model.md) | The single-owner rule: every heap-allocated value has exactly one owner at any point. |
| [Move Semantics](memory-and-ownership/move-semantics.md) | Assignment transfers ownership. The source variable is invalidated. |
| [Move Keyword](memory-and-ownership/move-keyword.md) | Explicit `move` for transferring ownership in ambiguous contexts. |
| [Own Keyword](memory-and-ownership/own-keyword.md) | The `own` annotation for ownership transfer in function parameters. |
| [Borrow Checking](memory-and-ownership/borrow-checking.md) | The semantic and MIR borrow checkers: how use-after-move is detected. |
| [Memory Intrinsic](memory-and-ownership/memory-intrinsic.md) | `Memory<T>` — compiler intrinsic for raw heap allocation, `get`/`set`/`resize`. |
| [Arena Allocator](memory-and-ownership/arena-allocator.md) | `Arena<T>` — bulk allocation with single-deallocation semantics. |
| [Slab Allocator](memory-and-ownership/slab-allocator.md) | Slab allocation for fixed-size object pools. |
| [Delete Statement](memory-and-ownership/delete-statement.md) | `delete variable` explicit deallocation. |
| [Addressof Operator](memory-and-ownership/addressof-operator.md) | Taking raw memory addresses for low-level operations. |
| [Droper Interface](memory-and-ownership/droper-interface.md) | `Droper` interface with `drop( self )` — custom cleanup on deallocation. |
| [Unsafe Blocks](memory-and-ownership/unsafe-blocks.md) | `unsafe:` blocks for raw pointer operations, bypassing borrow checker restrictions. |

### [Error Handling](error-handling/README.md)

Exception-based error handling with stack unwinding.

| Document | Description |
|---|---|
| [Exception Hierarchy](error-handling/exception-hierarchy.md) | `Throwable` > `Exception` > `Error` > specific error types (`ArithmeticError`, `ZeroDivisionError`, `OverflowError`). |
| [Try / Except / Finally](error-handling/try-except-finally.md) | Structured exception handling with typed catch clauses and cleanup blocks. |
| [Raise Statement](error-handling/raise-statement.md) | `raise new Exception( message, code, cause )` — throwing exceptions. |
| [Raises Specifier](error-handling/raises-specifier.md) | Function-level annotation declaring which exceptions a function may throw. |
| [Defer Statement](error-handling/defer-statement.md) | `defer action()` — scope-exit cleanup executing regardless of how the scope exits. |
| [Custom Exceptions](error-handling/custom-exceptions.md) | Defining application-specific exception classes extending `Exception`. |

### [Async and Concurrency](async-and-concurrency/README.md)

Asynchronous programming, concurrency primitives, and parallelism.

| Document | Description |
|---|---|
| [Async Functions](async-and-concurrency/async-functions.md) | `async function` declarations that return `Future<T>`. |
| [Future Type](async-and-concurrency/future-type.md) | `Future<T>` as the handle for pending asynchronous results. |
| [Await Expressions](async-and-concurrency/await-expressions.md) | `await future` — suspending execution until a future resolves. |
| [Generators and Yield](async-and-concurrency/generators-and-yield.md) | `yield value` for lazy sequence generation via `Generator<T>`. |
| [Coroutines](async-and-concurrency/coroutines.md) | Lightweight cooperative multitasking primitives. |
| [Fibers](async-and-concurrency/fibers.md) | User-space fiber scheduling and context switching. |
| [Threading](async-and-concurrency/threading.md) | OS-level threading via `Thread`, `ScopedThread`, and thread pools. |
| [Synchronization Primitives](async-and-concurrency/synchronization-primitives.md) | `Mutex`, `RwLock`, `Semaphore`, `Barrier`, and `ConditionVariable`. |
| [Channels](async-and-concurrency/channels.md) | Typed message-passing channels for inter-thread communication. |

### [Modules and Packages](modules-and-packages/README.md)

The module system, import mechanics, and visibility controls.

| Document | Description |
|---|---|
| [Package Declarations](modules-and-packages/package-declarations.md) | `package dotted.name` at the top of every source file. |
| [Import Syntax](modules-and-packages/import-syntax.md) | `from package.path import Entity`, comma-separated imports, and brace-wrapped imports. |
| [Access Modifiers](modules-and-packages/access-modifiers.md) | `public`, `private`, `protect`, `Default` visibility levels and their scoping rules. |
| [Export Blocks](modules-and-packages/export-blocks.md) | `export { ... }` for grouping declarations to expose from a module. |
| [Module Resolution](modules-and-packages/module-resolution.md) | The compiler's algorithm for translating import paths to filesystem paths (3 candidate paths, 6-level stdlib search, include path prefix resolution). |

### [Inline Assembly](inline-assembly/README.md)

Embedding raw machine instructions in Uranite source.

| Document | Description |
|---|---|
| [Asm Volatile](inline-assembly/asm-volatile.md) | `asm volatile` blocks with GCC-style constraint syntax for x86_64 `syscall` and AArch64 `svc #0` instructions. |

### [Interop](interop/README.md)

Foreign function interface and native library integration.

| Document | Description |
|---|---|
| [FFI](interop/ffi.md) | Foreign function interface mechanics for calling C code from Uranite. |
| [Linking Native Libraries](interop/linking-native-libraries.md) | The `-l` flag, `link` manifest field, and dynamic/static library linking. |
| [Cross-Compilation](interop/cross-compilation.md) | `--target` triples, sysroot configuration, and multi-architecture builds. |
