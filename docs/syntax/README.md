# Language Syntax

This section is the complete reference for every syntactic construct in the Uranite programming language. It covers lexical conventions, literals, declarations, statements, expressions, the type system, ownership semantics, and advanced language features. Each subsection describes what the construct does, how to write it, and how it interacts with other language features.

---

## Table of Contents

- [Language Syntax](#language-syntax)
  - [Table of Contents](#table-of-contents)
  - [Syntax Philosophy](#syntax-philosophy)
  - [Section Map](#section-map)
    - [Lexical Conventions](#lexical-conventions)
    - [Literals](#literals)
    - [Declarations](#declarations)
    - [Statements](#statements)
    - [Expressions](#expressions)
    - [Types](#types)
    - [Ownership](#ownership)
    - [Advanced Concepts](#advanced-concepts)
  - [Reserved Keywords](#reserved-keywords)
    - [Control Flow Keywords](#control-flow-keywords)
    - [Declaration Keywords](#declaration-keywords)
    - [Access Modifier Keywords](#access-modifier-keywords)
    - [Object-Oriented Keywords](#object-oriented-keywords)
    - [Memory and Safety Keywords](#memory-and-safety-keywords)
    - [Logical Operator Keywords](#logical-operator-keywords)
    - [Literal Value Keywords](#literal-value-keywords)
    - [Error Handling Keywords](#error-handling-keywords)
    - [General Purpose Keywords](#general-purpose-keywords)
  - [Complete Keyword Reference](#complete-keyword-reference)
  - [Case Sensitivity](#case-sensitivity)
  - [Builtin Type Names](#builtin-type-names)

---

## Syntax Philosophy

Uranite's syntax is built on four foundational principles that differentiate it from C-family languages.

**Indentation defines structure.** Blocks are opened by a colon and delimited by indentation level, not braces. There are no optional braces, no single-line forms that bypass indentation, and no ambiguity about where a block begins or ends.

```uranite
public function classify( I64 temperature ) -> String:
    if temperature > 100:
        return "extreme"
    elif temperature > 50:
        return "high"
    else:
        return "normal"
```

**Keywords replace symbols for logical operations.** Boolean logic uses `and`, `or`, and `not` exclusively. The symbols `&&`, `||`, and `!` are not valid tokens. Identity checks use `is` and `is not` rather than reference-equality operators. Both operands of `and` and `or` are always evaluated.

**Static typing is explicit and mandatory.** Every variable, parameter, and return type carries an explicit type annotation. There is no type inference for declarations. Generic type parameters are specified at instantiation (`ArrayList<String>`), with diamond inference (`new ArrayList<>()`) permitted only in constructor calls where the target type is unambiguous from the left-hand side of an assignment.

**Move semantics are the default.** Non-primitive types transfer ownership on assignment. After assignment to a new binding, the original variable is invalidated. Any subsequent access to the original produces a compile-time error caught by the borrow checker. There is no garbage collector, no reference counting, and no runtime pause for memory reclamation.

```uranite
ArrayList<String> original = new ArrayList<>()
original.add( "alpha" )
original.add( "beta" )

ArrayList<String> transferred = original
```

After the last line, `original` is invalidated and cannot be used.

---

## Section Map

### [Lexical Conventions](lexical-conventions/README.md)

The foundational building blocks of Uranite source code: how text becomes tokens.

| Document | Description |
|---|---|
| [Source Files and Encoding](lexical-conventions/source-files-and-encoding.md) | File encoding requirements, the `.urn` extension, and source file structure rules. |
| [Comments and Doccomments](lexical-conventions/comments-and-doccomments.md) | Single-line `#` comments, triple-quoted `"""..."""` doccomments, and the required doccomment format. |
| [Indentation and Blocks](lexical-conventions/indentation-and-blocks.md) | How indentation defines block boundaries, the 4-space convention, and common indentation errors. |
| [Identifiers and Naming](lexical-conventions/identifiers-and-naming.md) | Identifier rules, naming conventions, and linter enforcement of minimum name lengths. |

### [Literals](literals/README.md)

All literal value forms for scalars and collections.

| Document | Description |
|---|---|
| [Integer Literals](literals/integer-literals.md) | Decimal, hexadecimal (`0x`), octal (`0o`), and binary (`0b`) integer literal formats. |
| [Float Literals](literals/float-literals.md) | Floating-point literal syntax, scientific notation, and IEEE 754 representation. |
| [String Literals](literals/string-literals.md) | String literal syntax, escape sequences, and UTF-8 encoding. |
| [Char Literals](literals/char-literals.md) | Character literal syntax, Unicode representation, and the 32-bit `Char` type. |
| [Boolean Literals](literals/boolean-literals.md) | `True` and `False` literal tokens. |
| [None Literals](literals/none-literals.md) | The `None` literal and its role as the null-equivalent value. |
| [Regex Literals](literals/regex-literals.md) | Regular expression literal syntax and compilation. |
| [Array Literals](literals/array-literals.md) | `[1, 2, 3]` syntax for creating raw arrays. |
| [Map Literals](literals/map-literals.md) | `{"key": value}` syntax for creating `HashMap` instances. |
| [Set Literals](literals/set-literals.md) | `{value1, value2}` syntax for creating `HashSet` instances. |
| [Tuple Literals](literals/tuple-literals.md) | `(value1, value2)` syntax for creating `Tuple` instances. |

### [Declarations](declarations/README.md)

Constructs that define named entities: types, functions, and bindings.

| Document | Description |
|---|---|
| [Variable](declarations/variable.md) | Typed variable declarations, scope rules, mutability with `mut`, and `volatile` modifier. |
| [Constants](declarations/constants.md) | Top-level `const` declarations for compile-time constant values. |
| [Functions](declarations/functions.md) | Function declarations, parameters, return types, default values, variadic and keyword parameters, `extern` declarations, and nested functions. |
| [Classes](declarations/classes.md) | Class declarations with fields, methods, constructors, visibility modifiers, static members, properties, `readonly`, `final`, `native`, and `virtual` dispatch. |
| [Class Inheritance](declarations/class-inheritance.md) | Single inheritance with `extends`, `parent()` constructor delegation, method overriding with `override`, and polymorphic dispatch. |
| [Abstract Classes](declarations/abstract-classes.md) | `abstract class` with `abstract function` signatures and concrete methods. |
| [Structs](declarations/structs.md) | Value-type struct declarations with fields and value semantics. |
| [Enums](declarations/enums.md) | Enum declarations with `unit` variants, `backed` enums with explicit discriminant values, and enum methods. |
| [Interfaces](declarations/interfaces.md) | Interface declarations defining method contracts, `implements` on classes, and multiple interface implementation. |
| [Traits](declarations/traits.md) | Trait declarations with default method implementations and `use` for mixing into classes. |
| [Generics](declarations/generics.md) | Generic type parameters, `where` clause constraints, diamond inference, and generic functions. |

### [Statements](statements/README.md)

Constructs that execute actions and control flow.

| Document | Description |
|---|---|
| [Assignment](statements/assignment.md) | Simple and compound assignment operators (`=`, `+=`, `-=`, `*=`, `/=`). |
| [If / Elif / Else](statements/if-elif-else.md) | Conditional branching with indentation-delimited bodies. |
| [While Loops](statements/while-loops.md) | Condition-based iteration. |
| [For-In Loops](statements/for-in-loops.md) | Iterator-based loops over ranges, collections, and any `Iterable<T>` type. |
| [Switch / Case](statements/switch-case.md) | Block-based value dispatch with `case` arms. |
| [Break and Continue](statements/break-and-continue.md) | Loop control statements for early exit and iteration skipping. |
| [Return](statements/return.md) | Return values from functions and transfer control to the caller. |
| [Pass](statements/pass.md) | No-op placeholder for intentionally empty blocks. |
| [Yield](statements/yield.md) | Produce values from generator functions without terminating them. |
| [Raise](statements/raise.md) | Throw exceptions with `raise` and declare throwable functions with `raises`. |
| [Try / Except / Finally](statements/try-except-finally.md) | Structured exception handling with typed `except` clauses and cleanup blocks. |
| [Defer](statements/defer.md) | Schedule cleanup statements that execute on scope exit regardless of control flow. |
| [Delete](statements/delete.md) | Explicitly destroy objects and release resources. |

### [Expressions](expressions/README.md)

All expression forms that produce values.

| Document | Description |
|---|---|
| [Call Expressions](expressions/call-expressions.md) | Free function calls, static method calls, and argument passing. |
| [Method Call Expressions](expressions/method-call-expressions.md) | Instance method invocation and dynamic dispatch. |
| [Constructor Expressions](expressions/constructor-expressions.md) | `new ClassName( args )` heap allocation and constructor invocation. |
| [Member Access Expressions](expressions/member-access-expressions.md) | Dot-notation field access (`object.field`). |
| [Index Expressions](expressions/index-expressions.md) | Collection indexing (`collection[index]`) mapped to `Indexable.get()`. |
| [Range Expressions](expressions/range-expressions.md) | Exclusive ranges (`0..10`) for iteration and slicing. |
| [Instanceof Expressions](expressions/instanceof-expressions.md) | Runtime type checking with `instanceof`. |
| [Subclassof Expressions](expressions/subclassof-expressions.md) | Compile-time subclass relationship checks with `subclassof`. |
| [Self Expressions](expressions/self-expressions.md) | `self` for referencing the current instance within methods. |
| [Parent Expressions](expressions/parent-expressions.md) | `parent` for accessing parent class constructors and overridden methods. |
| [Lambda Expressions](expressions/lambda-expressions.md) | Anonymous functions with `lambda` and captured scope. |
| [Match Expressions](expressions/match-expressions.md) | Inline pattern matching with `match ... in` and `=>` arms. |
| [Await Expressions](expressions/await-expressions.md) | `await` for suspending until async operations complete. |
| [Operator Precedence](expressions/operator-precedence.md) | Complete precedence table from highest to lowest binding strength. |

**[Operators](expressions/operators/README.md)**

| Document | Description |
|---|---|
| [Arithmetic Operators](expressions/operators/arithmetic-operators.md) | `+`, `-`, `*`, `/`, `%` with wrapping arithmetic and auto-inserted zero-checks. |
| [Comparison Operators](expressions/operators/comparison-operators.md) | `==`, `!=`, `<`, `>`, `<=`, `>=`, `is`, `is not`. |
| [Logical Operators](expressions/operators/logical-operators.md) | `and`, `or`, `not` keyword-based boolean logic. |
| [Bitwise Operators](expressions/operators/bitwise-operators.md) | Bitwise AND, OR, XOR, NOT, left shift, and right shift. |
| [Unary Operators](expressions/operators/unary-operators.md) | Prefix negation and other unary operations. |

**[Comprehensions](expressions/comprehensions/README.md)**

| Document | Description |
|---|---|
| [List Comprehensions](expressions/comprehensions/list-comprehensions.md) | `[expr for Type x in iterable]` array comprehension syntax. |
| [Map Comprehensions](expressions/comprehensions/map-comprehensions.md) | `{key: value for Type x in iterable}` map comprehension syntax. |
| [Set Comprehensions](expressions/comprehensions/set-comprehensions.md) | `{expr for Type x in iterable}` set comprehension syntax. |

### [Types](types/README.md)

The complete type system: every type kind the language supports.

| Document | Description |
|---|---|
| [Primitive Types](types/primitive-types.md) | Overview of all built-in scalar types, their sizes, ranges, and relationships. |
| [Void and None Types](types/void-and-none-types.md) | `Void` as a return type and `None` as the null value. |
| [Optional Types](types/optional-types.md) | The `?T` optional type prefix and `is`/`is not` checks. |
| [Union Types](types/union-types.md) | Union type declarations for type-safe tagged unions. |
| [Pointer and Reference Types](types/pointer-and-reference-types.md) | Raw pointer types, borrowed references, and borrow checker interaction. |
| [Type Aliases](types/type-aliases.md) | `type NewName = ExistingType` declarations. |
| [Type Casting](types/type-casting.md) | The `as` keyword for explicit type conversions. |
| [Type Compatibility](types/type-compatibility.md) | Rules governing which types can be assigned to which. |
| [Type Identity](types/type-identity.md) | How Uranite determines whether two types are the same. |
| [Type Inference](types/type-inference.md) | Diamond syntax and the boundaries of what the compiler can infer. |

### [Ownership](ownership/README.md)

Ownership semantics, move rules, manual memory management, and safety boundaries.

| Document | Description |
|---|---|
| [Ownership Model](ownership/ownership-model.md) | The single-owner rule, `move`, `own`, `addressof`, and `reference` keywords. |
| [Borrow Checking](ownership/borrow-checking.md) | How use-after-move is detected and prevented at compile time. |
| [Memory Management](ownership/memory-management.md) | Manual memory management strategies and allocator patterns. |
| [Droper Interface](ownership/droper-interface.md) | `Droper` interface with `drop( self )` for custom cleanup on deallocation. |
| [Unsafe Blocks](ownership/unsafe-blocks.md) | `unsafe:` blocks for raw pointer operations and bypassing borrow checker restrictions. |

### [Advanced Concepts](advanced-concepts/README.md)

Higher-level language features that combine multiple constructs.

| Document | Description |
|---|---|
| [Error Handling](advanced-concepts/error-handling.md) | Exception hierarchy, `try`/`except`/`finally` patterns, `raise`/`raises`, `defer`, and custom exceptions. |
| [Async](advanced-concepts/async.md) | `async`/`await`, `Future<T>`, and the pure-Uranite async runtime. |
| [Modules and Packages](advanced-concepts/modules-and-packages.md) | `package`, `import`, `from`, `export`, access modifiers, and module resolution. |
| [Inline Assembly](advanced-concepts/inline-assembly.md) | `asm` blocks for embedding platform-specific instructions. |
| [Interop](advanced-concepts/interop.md) | FFI, `extern` declarations, native library linking, and cross-compilation. |

---

## Reserved Keywords

Uranite reserves 78 keywords. These identifiers are unconditionally reserved and cannot be used as variable names, function names, type names, parameter names, or any other user-defined identifier.

### Control Flow Keywords

| Keyword | Purpose | Documentation |
|---|---|---|
| `break` | Exit the innermost loop immediately. | [Break and Continue](statements/break-and-continue.md) |
| `case` | Individual branch within a `switch` statement. | [Switch / Case](statements/switch-case.md) |
| `continue` | Skip the rest of the current iteration. | [Break and Continue](statements/break-and-continue.md) |
| `elif` | Additional conditional branch in an if/elif/else chain. | [If / Elif / Else](statements/if-elif-else.md) |
| `else` | Fallback branch when no `if` or `elif` condition matched. | [If / Elif / Else](statements/if-elif-else.md) |
| `for` | Iterate over sequences, ranges, or any `Iterable<T>` type. | [For-In Loops](statements/for-in-loops.md) |
| `if` | Conditional branch based on a `Boolean` condition. | [If / Elif / Else](statements/if-elif-else.md) |
| `match` | Inline pattern matching with `=>` arms. | [Match Expressions](expressions/match-expressions.md) |
| `return` | Return a value from a function. | [Return](statements/return.md) |
| `switch` | Block-based branching with `case` arms. | [Switch / Case](statements/switch-case.md) |
| `while` | Loop while a `Boolean` condition remains `True`. | [While Loops](statements/while-loops.md) |
| `yield` | Produce a value from a generator function. | [Yield](statements/yield.md) |

### Declaration Keywords

| Keyword | Purpose | Documentation |
|---|---|---|
| `class` | Declare a class type with fields, methods, and constructors. | [Classes](declarations/classes.md) |
| `const` | Declare a compile-time constant value. | [Constants](declarations/constants.md) |
| `enum` | Declare an enumeration type with named variants. | [Enums](declarations/enums.md) |
| `extern` | Declare an external function implemented outside Uranite. | [Functions](declarations/functions.md) |
| `from` | Specify the source module in an import statement. | [Modules and Packages](advanced-concepts/modules-and-packages.md) |
| `function` | Declare a named function. | [Functions](declarations/functions.md) |
| `implements` | Declare that a class implements an interface contract. | [Interfaces](declarations/interfaces.md) |
| `import` | Import entities from another module. | [Modules and Packages](advanced-concepts/modules-and-packages.md) |
| `interface` | Declare an abstract method contract. | [Interfaces](declarations/interfaces.md) |
| `package` | Declare the package identity of a source file. | [Modules and Packages](advanced-concepts/modules-and-packages.md) |
| `static` | Declare a class-level member that belongs to the class itself. | [Classes](declarations/classes.md) |
| `struct` | Declare a value-type struct. | [Structs](declarations/structs.md) |
| `trait` | Declare a trait with reusable method implementations. | [Traits](declarations/traits.md) |
| `type` | Declare a type alias. | [Type Aliases](types/type-aliases.md) |

### Access Modifier Keywords

| Keyword | Purpose | Documentation |
|---|---|---|
| `private` | Visible only within the declaring class. | [Modules and Packages](advanced-concepts/modules-and-packages.md) |
| `protect` | Visible within the declaring class and its subclasses. | [Modules and Packages](advanced-concepts/modules-and-packages.md) |
| `public` | Visible to all modules that import the declaration. | [Modules and Packages](advanced-concepts/modules-and-packages.md) |

When no access modifier is specified, declarations default to package-private visibility.

### Object-Oriented Keywords

| Keyword | Purpose | Documentation |
|---|---|---|
| `abstract` | Mark a class as non-instantiable or a method as requiring override. | [Abstract Classes](declarations/abstract-classes.md) |
| `delete` | Explicitly destroy an object and release its resources. | [Delete](statements/delete.md) |
| `extends` | Declare that a class inherits from a parent class. | [Class Inheritance](declarations/class-inheritance.md) |
| `final` | Prevent a class from being subclassed or a method from being overridden. | [Classes](declarations/classes.md) |
| `native` | Mark a method as implemented in native code. | [Classes](declarations/classes.md) |
| `new` | Construct a new object instance. | [Constructor Expressions](expressions/constructor-expressions.md) |
| `override` | Mark a method as intentionally overriding a parent class method. | [Class Inheritance](declarations/class-inheritance.md) |
| `parent` | Reference the parent class for constructors or overridden methods. | [Parent Expressions](expressions/parent-expressions.md) |
| `property` | Declare a computed property with getter semantics. | [Classes](declarations/classes.md) |
| `readonly` | Mark a field as immutable after construction. | [Classes](declarations/classes.md) |
| `Readonly` | Alternate casing of `readonly`. Identical in meaning. | [Classes](declarations/classes.md) |
| `self` | Reference the current object instance within methods. | [Self Expressions](expressions/self-expressions.md) |
| `virtual` | Mark a method for dynamic dispatch. | [Classes](declarations/classes.md) |

### Memory and Safety Keywords

| Keyword | Purpose | Documentation |
|---|---|---|
| `addressof` | Obtain the raw memory address of a variable. | [Ownership Model](ownership/ownership-model.md) |
| `move` | Transfer ownership of a value to a new binding. | [Ownership Model](ownership/ownership-model.md) |
| `mut` | Mark a variable or parameter as mutable. | [Variable](declarations/variable.md) |
| `own` | Declare explicit ownership semantics for a parameter. | [Ownership Model](ownership/ownership-model.md) |
| `reference` | Pass a value by reference without transferring ownership. | [Ownership Model](ownership/ownership-model.md) |
| `unsafe` | Enter a block where borrow checker rules are relaxed. | [Unsafe Blocks](ownership/unsafe-blocks.md) |

### Logical Operator Keywords

Uranite uses word-form logical operators exclusively. The symbols `&&` and `||` are not valid tokens. Both operands of `and` and `or` are always evaluated.

| Keyword | Purpose | Documentation |
|---|---|---|
| `and` | Logical AND. Evaluates both operands. | [Logical Operators](expressions/operators/logical-operators.md) |
| `not` | Logical NOT. Negates a `Boolean` value. | [Logical Operators](expressions/operators/logical-operators.md) |
| `or` | Logical OR. Evaluates both operands. | [Logical Operators](expressions/operators/logical-operators.md) |

### Literal Value Keywords

These three keywords are the only identifiers that begin with an uppercase letter and are reserved. All other capitalized identifiers (`I64`, `String`, `ArrayList`) are type names, not keywords.

| Keyword | Purpose | Documentation |
|---|---|---|
| `False` | Boolean false literal. | [Boolean Literals](literals/boolean-literals.md) |
| `None` | Null-equivalent representing the absence of a value. | [None Literals](literals/none-literals.md) |
| `True` | Boolean true literal. | [Boolean Literals](literals/boolean-literals.md) |

### Error Handling Keywords

| Keyword | Purpose | Documentation |
|---|---|---|
| `except` | Catch an exception by type within a `try` block. | [Try / Except / Finally](statements/try-except-finally.md) |
| `finally` | Execute cleanup code regardless of whether an exception occurred. | [Try / Except / Finally](statements/try-except-finally.md) |
| `raise` | Throw an exception. | [Raise](statements/raise.md) |
| `raises` | Declare in a function signature that it may raise exceptions. | [Raise](statements/raise.md) |
| `try` | Begin a protected block for exception handling. | [Try / Except / Finally](statements/try-except-finally.md) |

### General Purpose Keywords

| Keyword | Purpose | Documentation |
|---|---|---|
| `as` | Type casting or import aliasing. | [Type Casting](types/type-casting.md) |
| `asm` | Introduce an inline assembly block. | [Inline Assembly](advanced-concepts/inline-assembly.md) |
| `async` | Mark a function as asynchronous, returning `Future<T>`. | [Async](advanced-concepts/async.md) |
| `await` | Suspend execution until an async operation completes. | [Await Expressions](expressions/await-expressions.md) |
| `backed` | Specify a backing type for an enum. | [Enums](declarations/enums.md) |
| `defer` | Schedule a statement to execute on scope exit. | [Defer](statements/defer.md) |
| `export` | Make declarations available for import by other modules. | [Modules and Packages](advanced-concepts/modules-and-packages.md) |
| `in` | Iteration target in `for` loops or membership test. | [For-In Loops](statements/for-in-loops.md) |
| `instanceof` | Runtime type check against a class or interface. | [Instanceof Expressions](expressions/instanceof-expressions.md) |
| `is` | Identity comparison or `None` checking. | [Comparison Operators](expressions/operators/comparison-operators.md) |
| `lambda` | Declare an anonymous function expression. | [Lambda Expressions](expressions/lambda-expressions.md) |
| `pass` | No-op placeholder for intentionally empty blocks. | [Pass](statements/pass.md) |
| `subclassof` | Compile-time subclass relationship check. | [Subclassof Expressions](expressions/subclassof-expressions.md) |
| `unit` | Declare individual variants within an enum type. | [Enums](declarations/enums.md) |
| `use` | Mix a trait into a class body. | [Traits](declarations/traits.md) |
| `volatile` | Prevent compiler optimization of reads and writes. | [Variable](declarations/variable.md) |
| `where` | Type constraint clause on generic declarations. | [Generics](declarations/generics.md) |

---

## Complete Keyword Reference

All 78 keywords sorted alphabetically. Entry #56 (`readonly`) and #57 (`Readonly`) are two accepted casings of the same keyword.

| # | Keyword | Category | Casing |
|---|---|---|---|
| 1 | `abstract` | OOP | all lowercase |
| 2 | `addressof` | Memory | all lowercase |
| 3 | `and` | Logic | all lowercase |
| 4 | `as` | General | all lowercase |
| 5 | `asm` | General | all lowercase |
| 6 | `async` | General | all lowercase |
| 7 | `await` | General | all lowercase |
| 8 | `backed` | General | all lowercase |
| 9 | `break` | Control Flow | all lowercase |
| 10 | `case` | Control Flow | all lowercase |
| 11 | `class` | Declarations | all lowercase |
| 12 | `const` | Declarations | all lowercase |
| 13 | `continue` | Control Flow | all lowercase |
| 14 | `defer` | General | all lowercase |
| 15 | `delete` | OOP | all lowercase |
| 16 | `elif` | Control Flow | all lowercase |
| 17 | `else` | Control Flow | all lowercase |
| 18 | `enum` | Declarations | all lowercase |
| 19 | `except` | Error Handling | all lowercase |
| 20 | `export` | General | all lowercase |
| 21 | `extends` | OOP | all lowercase |
| 22 | `extern` | Declarations | all lowercase |
| 23 | `False` | Literal Values | capital F |
| 24 | `final` | OOP | all lowercase |
| 25 | `finally` | Error Handling | all lowercase |
| 26 | `for` | Control Flow | all lowercase |
| 27 | `from` | Declarations | all lowercase |
| 28 | `function` | Declarations | all lowercase |
| 29 | `if` | Control Flow | all lowercase |
| 30 | `implements` | Declarations | all lowercase |
| 31 | `import` | Declarations | all lowercase |
| 32 | `in` | General | all lowercase |
| 33 | `instanceof` | General | all lowercase |
| 34 | `interface` | Declarations | all lowercase |
| 35 | `is` | General | all lowercase |
| 36 | `lambda` | General | all lowercase |
| 37 | `match` | Control Flow | all lowercase |
| 38 | `move` | Memory | all lowercase |
| 39 | `mut` | Memory | all lowercase |
| 40 | `native` | OOP | all lowercase |
| 41 | `new` | OOP | all lowercase |
| 42 | `None` | Literal Values | capital N |
| 43 | `not` | Logic | all lowercase |
| 44 | `or` | Logic | all lowercase |
| 45 | `override` | OOP | all lowercase |
| 46 | `own` | Memory | all lowercase |
| 47 | `package` | Declarations | all lowercase |
| 48 | `parent` | OOP | all lowercase |
| 49 | `pass` | General | all lowercase |
| 50 | `private` | Access | all lowercase |
| 51 | `property` | OOP | all lowercase |
| 52 | `protect` | Access | all lowercase |
| 53 | `public` | Access | all lowercase |
| 54 | `raise` | Error Handling | all lowercase |
| 55 | `raises` | Error Handling | all lowercase |
| 56 | `readonly` | OOP | all lowercase |
| 57 | `Readonly` | OOP | capital R |
| 58 | `reference` | Memory | all lowercase |
| 59 | `return` | Control Flow | all lowercase |
| 60 | `self` | OOP | all lowercase |
| 61 | `static` | Declarations | all lowercase |
| 62 | `struct` | Declarations | all lowercase |
| 63 | `subclassof` | General | all lowercase |
| 64 | `switch` | Control Flow | all lowercase |
| 65 | `trait` | Declarations | all lowercase |
| 66 | `True` | Literal Values | capital T |
| 67 | `try` | Error Handling | all lowercase |
| 68 | `type` | Declarations | all lowercase |
| 69 | `unit` | General | all lowercase |
| 70 | `unsafe` | Memory | all lowercase |
| 71 | `use` | General | all lowercase |
| 72 | `virtual` | OOP | all lowercase |
| 73 | `volatile` | General | all lowercase |
| 74 | `where` | General | all lowercase |
| 75 | `while` | Control Flow | all lowercase |
| 76 | `yield` | Control Flow | all lowercase |

---

## Case Sensitivity

Keyword matching is case-sensitive. Most keywords are entirely lowercase. Five entries have non-lowercase forms:

| Keyword | Casing | Notes |
|---|---|---|
| `False` | Capital F | `false` (all lowercase) is a valid identifier, not a keyword. |
| `True` | Capital T | `true` (all lowercase) is a valid identifier, not a keyword. |
| `None` | Capital N | `none` (all lowercase) is a valid identifier, not a keyword. |
| `Readonly` | Capital R | Alternate casing of `readonly`. Both forms are accepted. |
| `readonly` | All lowercase | Primary casing. Identical in meaning to `Readonly`. |

All other capitalization variants (`IF`, `Class`, `RETURN`, `TRUE`) are not keywords and can be used as identifiers, though doing so is strongly discouraged.

---

## Builtin Type Names

Beyond the 78 reserved keywords, Uranite recognizes a set of builtin type names that the compiler pre-registers during initialization. These are identifiers at the lexical level, not reserved words. While shadowing them with local variables is technically valid, the linter flags such usage as a warning.

**Integer types:** `I8`, `I16`, `I32`, `I64`, `U8`, `U16`, `U32`, `U64`, `Int`, `UInt`, `Byte`

**Floating-point types:** `Float`, `Double`

**Core types:** `Boolean`, `Char`, `String`, `Void`, `Object`

**Memory types:** `Memory<T>`, `Args<T>`, `Kwargs<K, V>`

**Async types:** `Future<T>`, `Generator<T>`

**Error hierarchy:** `Throwable`, `Error`, `Exception`, `Warning`, `Traceback`, `ArithmeticError`, `ZeroDivisionError`, `OverflowError`, `UnderflowError`

For detailed coverage of each type, see the [Standard Library Language section](../stdlib/language/README.md) and [Types](types/README.md).
