# Reserved Keywords

Uranite reserves 75 unique keywords. These identifiers are unconditionally reserved — they cannot be used as variable names, function names, type names, parameter names, or any other user-defined identifier. This document catalogs every keyword, explains the lexer mechanism that enforces reservation, and details the relationship between keywords and the compiler's builtin identifier registry.

---

## Table of Contents

- [Keyword Recognition Mechanism](#keyword-recognition-mechanism)
  - [The keymaps Registry](#the-keymaps-registry)
  - [The readIdentifierOrKeyword Method](#the-readidentifierorkeyword-method)
  - [Case Sensitivity](#case-sensitivity)
- [Keyword Categories](#keyword-categories)
  - [Control Flow](#control-flow)
  - [Declarations](#declarations)
  - [Access Modifiers](#access-modifiers)
  - [Object-Oriented Programming](#object-oriented-programming)
  - [Memory and Safety](#memory-and-safety)
  - [Logical Operators](#logical-operators)
  - [Literal Values](#literal-values)
  - [Error Handling](#error-handling)
  - [General Purpose](#general-purpose)
- [Complete Keyword Reference Table](#complete-keyword-reference-table)
- [Builtin Identifiers](#builtin-identifiers)
  - [The builtinIdentifiers Registry](#the-builtinidentifiers-registry)
  - [Distinction from Keywords](#distinction-from-keywords)
- [Keyword Usage Examples](#keyword-usage-examples)
  - [Control Flow](#control-flow-examples)
  - [Declarations and Modules](#declarations-and-modules-examples)
  - [Memory and Ownership](#memory-and-ownership-examples)
  - [Error Handling](#error-handling-examples)
  - [Async and Concurrency](#async-and-concurrency-examples)
  - [Pattern Matching and Enums](#pattern-matching-and-enums-examples)
- [What Cannot Be a Keyword](#what-cannot-be-a-keyword)

---

## Keyword Recognition Mechanism

### The keymaps Registry

Keywords are defined in a single static `std::unordered_map<std::string, Type>` returned by the `keymaps()` function in `src/uranite/token/token.cpp`. This map contains 76 entries mapping string literals to `token::Type` enum values. Two entries ("readonly" and "Readonly") map to the same `KeywordReadonly` token type, yielding 75 unique keywords.

The map is constructed once on first access (static local initialization) and persists for the lifetime of the process. It is never modified after construction.

### The readIdentifierOrKeyword Method

When the lexer encounters an alphabetic character or underscore, it calls `readIdentifierOrKeyword()`. This method:

1. Accumulates consecutive alphanumeric and underscore characters into an `identifierValue` string.
2. Looks up `identifierValue` in the `keymaps()` registry.
3. If found, returns a token with the matched keyword type (e.g., `KeywordIf`, `KeywordClass`).
4. If not found, returns a token with type `Identifier`.

This is a simple hash-table lookup — O(1) average case. There is no separate lexer mode for keywords, no priority rules, and no context-dependent keyword resolution. If the string matches a keymaps entry, it is a keyword. Always.

This means keywords are **unconditionally reserved**. The following code is invalid:

```uranite
I64 class = 5
```

The lexer produces `Identifier("I64")` `KeywordClass` `Assignment` `LiteralInteger("5")`, and the parser fails because it encounters `KeywordClass` where it expects an identifier.

### Case Sensitivity

Keyword matching is case-sensitive. Most keywords are entirely lowercase. Four exceptions have non-lowercase forms:

| Keyword | Case | Notes |
|---|---|---|
| `False` | Capital F | Boolean literal. Lowercase "false" is not a keyword. |
| `True` | Capital T | Boolean literal. Lowercase "true" is not a keyword. |
| `None` | Capital N | Null-equivalent literal. Lowercase "none" is not a keyword. |
| `readonly` / `Readonly` | Both forms | Dual-cased — both map to `KeywordReadonly`. |

The `readonly` / `Readonly` dual entry is the only keyword with two accepted casings. All other keywords have exactly one valid form.

Because the keymaps registry uses exact string matching, `IF`, `Class`, `RETURN`, and similar variants are not keywords — they are valid identifiers. Only the exact casing listed in the registry is reserved.

---

## Keyword Categories

The compiler organizes keywords into nine categories, each backed by a dedicated helper function that returns the category's token types. These categories are used internally for parser dispatch and diagnostic messages.

### Control Flow

Keywords that alter execution flow within a function body.

| Keyword | Token Type | Purpose |
|---|---|---|
| `break` | `KeywordBreak` | Exit the innermost loop. |
| `continue` | `KeywordContinue` | Skip to the next iteration of the innermost loop. |
| `else` | `KeywordElse` | Fallback branch in conditional chains. |
| `elif` | `KeywordElif` | Additional conditional branch (else-if). |
| `for` | `KeywordFor` | Iteration over sequences, ranges, or iterators. |
| `if` | `KeywordIf` | Conditional branch. |
| `match` | `KeywordMatch` | Pattern matching on values, types, or enum variants. |
| `return` | `KeywordReturn` | Return a value from a function. |
| `while` | `KeywordWhile` | Loop with a boolean condition. |
| `yield` | `KeywordYield` | Produce a value from a generator function. |

### Declarations

Keywords that introduce named entities — types, functions, modules, and bindings.

| Keyword | Token Type | Purpose |
|---|---|---|
| `class` | `KeywordClass` | Declare a class type. |
| `const` | `KeywordConstant` | Declare a compile-time constant. |
| `enum` | `KeywordEnum` | Declare an enumeration type. |
| `extern` | `KeywordExtern` | Declare an external (foreign) function or symbol. |
| `from` | `KeywordFrom` | Specify the module in an import statement. |
| `function` | `KeywordFunction` | Declare a function. |
| `implements` | `KeywordImplements` | Declare that a class implements an interface. |
| `import` | `KeywordImport` | Import entities from a module. |
| `interface` | `KeywordInterface` | Declare an interface (abstract contract). |
| `mut` | `KeywordMutable` | Mark a variable or parameter as mutable. |
| `package` | `KeywordPackage` | Declare the package name for a source file. |
| `static` | `KeywordStatic` | Declare a class-level (not instance-level) member. |
| `struct` | `KeywordStruct` | Declare a value-type struct. |
| `type` | `KeywordType` | Declare a type alias. |

### Access Modifiers

Keywords that control visibility of declarations across module boundaries.

| Keyword | Token Type | Purpose |
|---|---|---|
| `private` | `KeywordPrivate` | Visible only within the declaring class. |
| `protect` | `KeywordProtect` | Visible within the declaring class and subclasses. |
| `public` | `KeywordPublic` | Visible to all modules that import the declaration. |

### Object-Oriented Programming

Keywords for class hierarchies, polymorphism, and instance semantics.

| Keyword | Token Type | Purpose |
|---|---|---|
| `abstract` | `KeywordAbstract` | Mark a class or method as abstract (no implementation). |
| `delete` | `KeywordDelete` | Explicitly destroy an object. |
| `extends` | `KeywordExtends` | Declare class inheritance. |
| `final` | `KeywordFinal` | Prevent further subclassing or overriding. |
| `native` | `KeywordNative` | Mark a method as implemented in native code (C/assembly). |
| `new` | `KeywordNew` | Construct a new object instance. |
| `override` | `KeywordOverride` | Mark a method as overriding a parent method. |
| `parent` | `KeywordParent` | Reference the parent class (super). |
| `property` | `KeywordProperty` | Declare a computed property with getter/setter semantics. |
| `readonly` | `KeywordReadonly` | Mark a field as immutable after construction. Also accepted as "Readonly". |
| `self` | `KeywordSelf` | Reference the current object instance. |
| `virtual` | `KeywordVirtual` | Mark a method for dynamic dispatch via vtable. |

### Memory and Safety

Keywords governing ownership, borrowing, and unsafe operations.

| Keyword | Token Type | Purpose |
|---|---|---|
| `addressof` | `KeywordAddressof` | Obtain the raw memory address of a variable. |
| `move` | `KeywordMove` | Transfer ownership of a value to a new binding. |
| `own` | `KeywordOwn` | Declare ownership semantics for a parameter or field. |
| `reference` | `KeywordReference` | Pass by reference (borrow without ownership transfer). |
| `unsafe` | `KeywordUnsafe` | Enter an unsafe block where borrow checker rules are relaxed. |

### Logical Operators

Keywords that serve as boolean operators, replacing symbolic equivalents.

| Keyword | Token Type | Purpose |
|---|---|---|
| `and` | `KeywordAnd` | Logical AND. Replaces `&&`. |
| `not` | `KeywordNot` | Logical NOT. Replaces `!` in boolean context. |
| `or` | `KeywordOr` | Logical OR. Replaces `\|\|`. |

Uranite uses word-form logical operators exclusively. The symbols `&&` and `||` are not valid tokens. The `!` character exists as the `Bang` token but is used only in the `!=` (not-equal) operator, never as a standalone logical negation.

### Literal Values

Keywords representing built-in constant values.

| Keyword | Token Type | Purpose |
|---|---|---|
| `False` | `KeywordFalse` | Boolean false literal. |
| `None` | `KeywordNone` | Null-equivalent — absence of a value. |
| `True` | `KeywordTrue` | Boolean true literal. |

These three keywords are capitalized, following the convention that literal values are visually distinct from control-flow and declaration keywords.

### Error Handling

Keywords for structured exception handling.

| Keyword | Token Type | Purpose |
|---|---|---|
| `except` | `KeywordExcept` | Catch an exception by type. |
| `finally` | `KeywordFinally` | Execute cleanup code regardless of exception state. |
| `raise` | `KeywordRaise` | Throw an exception. |
| `raises` | `KeywordRaises` | Declare in a function signature that the function may throw. |
| `try` | `KeywordTry` | Begin a protected block for exception handling. |

Note the distinction between `raise` (imperative — throw now) and `raises` (declarative — this function may throw).

### General Purpose

Keywords that do not fit neatly into a single category, spanning type casting, concurrency, pattern matching, metaprogramming, and miscellaneous language features.

| Keyword | Token Type | Purpose |
|---|---|---|
| `as` | `KeywordAs` | Type casting or import aliasing. |
| `asm` | `KeywordAssembly` | Inline assembly block. |
| `async` | `KeywordAsync` | Mark a function as asynchronous (returns `Future<T>`). |
| `await` | `KeywordAwait` | Suspend until an async operation completes. |
| `backed` | `KeywordBacked` | Specify a backing type for an enum. |
| `case` | `KeywordCase` | Individual branch within a `match` expression. |
| `defer` | `KeywordDefer` | Schedule a statement to execute when the current scope exits. |
| `export` | `KeywordExport` | Make declarations available for import by other modules. |
| `in` | `KeywordIn` | Membership test or iteration target in `for` loops. |
| `instanceof` | `KeywordInstanceOf` | Runtime type check against a class or interface. |
| `is` | `KeywordIs` | Identity comparison (reference equality or `None` check). |
| `lambda` | `KeywordLambda` | Declare an anonymous function expression. |
| `pass` | `KeywordPass` | No-op placeholder for empty blocks. |
| `subclassof` | `KeywordSubclassOf` | Compile-time subclass relationship check. |
| `switch` | `KeywordSwitch` | Reserved for future use (currently parsed but may alias `match`). |
| `trait` | `KeywordTrait` | Reserved for future use (trait-based composition). |
| `unit` | `KeywordUnit` | Declare enum variants. |
| `use` | `KeywordUse` | Bring a module or type into scope (alternative import form). |
| `volatile` | `KeywordVolatile` | Mark a variable as volatile (prevents compiler optimizations on reads/writes). |
| `where` | `KeywordWhere` | Type constraint clause on generic declarations. |

---

## Complete Keyword Reference Table

All 75 unique keywords sorted alphabetically with their exact casing, token type, and category.

| # | Keyword | Token Type | Category |
|---|---|---|---|
| 1 | `abstract` | `KeywordAbstract` | OOP |
| 2 | `addressof` | `KeywordAddressof` | Memory & Safety |
| 3 | `and` | `KeywordAnd` | Logic |
| 4 | `as` | `KeywordAs` | General |
| 5 | `asm` | `KeywordAssembly` | General |
| 6 | `async` | `KeywordAsync` | General |
| 7 | `await` | `KeywordAwait` | General |
| 8 | `backed` | `KeywordBacked` | General |
| 9 | `break` | `KeywordBreak` | Control Flow |
| 10 | `case` | `KeywordCase` | General |
| 11 | `class` | `KeywordClass` | Declarations |
| 12 | `const` | `KeywordConstant` | Declarations |
| 13 | `continue` | `KeywordContinue` | Control Flow |
| 14 | `defer` | `KeywordDefer` | General |
| 15 | `delete` | `KeywordDelete` | OOP |
| 16 | `elif` | `KeywordElif` | Control Flow |
| 17 | `else` | `KeywordElse` | Control Flow |
| 18 | `enum` | `KeywordEnum` | Declarations |
| 19 | `except` | `KeywordExcept` | Error Handling |
| 20 | `export` | `KeywordExport` | General |
| 21 | `extends` | `KeywordExtends` | OOP |
| 22 | `extern` | `KeywordExtern` | Declarations |
| 23 | `False` | `KeywordFalse` | Literal Values |
| 24 | `final` | `KeywordFinal` | OOP |
| 25 | `finally` | `KeywordFinally` | Error Handling |
| 26 | `for` | `KeywordFor` | Control Flow |
| 27 | `from` | `KeywordFrom` | Declarations |
| 28 | `function` | `KeywordFunction` | Declarations |
| 29 | `if` | `KeywordIf` | Control Flow |
| 30 | `implements` | `KeywordImplements` | Declarations |
| 31 | `import` | `KeywordImport` | Declarations |
| 32 | `in` | `KeywordIn` | General |
| 33 | `instanceof` | `KeywordInstanceOf` | General |
| 34 | `interface` | `KeywordInterface` | Declarations |
| 35 | `is` | `KeywordIs` | General |
| 36 | `lambda` | `KeywordLambda` | General |
| 37 | `match` | `KeywordMatch` | Control Flow |
| 38 | `move` | `KeywordMove` | Memory & Safety |
| 39 | `mut` | `KeywordMutable` | Declarations |
| 40 | `native` | `KeywordNative` | OOP |
| 41 | `new` | `KeywordNew` | OOP |
| 42 | `None` | `KeywordNone` | Literal Values |
| 43 | `not` | `KeywordNot` | Logic |
| 44 | `or` | `KeywordOr` | Logic |
| 45 | `override` | `KeywordOverride` | OOP |
| 46 | `own` | `KeywordOwn` | Memory & Safety |
| 47 | `package` | `KeywordPackage` | Declarations |
| 48 | `parent` | `KeywordParent` | OOP |
| 49 | `pass` | `KeywordPass` | General |
| 50 | `private` | `KeywordPrivate` | Access Modifiers |
| 51 | `property` | `KeywordProperty` | OOP |
| 52 | `protect` | `KeywordProtect` | Access Modifiers |
| 53 | `public` | `KeywordPublic` | Access Modifiers |
| 54 | `raise` | `KeywordRaise` | Error Handling |
| 55 | `raises` | `KeywordRaises` | Error Handling |
| 56 | `readonly` | `KeywordReadonly` | OOP |
| 57 | `reference` | `KeywordReference` | Memory & Safety |
| 58 | `return` | `KeywordReturn` | Control Flow |
| 59 | `self` | `KeywordSelf` | OOP |
| 60 | `static` | `KeywordStatic` | Declarations |
| 61 | `struct` | `KeywordStruct` | Declarations |
| 62 | `subclassof` | `KeywordSubclassOf` | General |
| 63 | `switch` | `KeywordSwitch` | General |
| 64 | `trait` | `KeywordTrait` | General |
| 65 | `True` | `KeywordTrue` | Literal Values |
| 66 | `try` | `KeywordTry` | Error Handling |
| 67 | `type` | `KeywordType` | Declarations |
| 68 | `unit` | `KeywordUnit` | General |
| 69 | `unsafe` | `KeywordUnsafe` | Memory & Safety |
| 70 | `use` | `KeywordUse` | General |
| 71 | `virtual` | `KeywordVirtual` | OOP |
| 72 | `volatile` | `KeywordVolatile` | General |
| 73 | `where` | `KeywordWhere` | General |
| 74 | `while` | `KeywordWhile` | Control Flow |
| 75 | `yield` | `KeywordYield` | Control Flow |

Entry #56 also accepts the casing "Readonly" — both forms map to `KeywordReadonly`.

---

## Builtin Identifiers

### The builtinIdentifiers Registry

Beyond the 75 reserved keywords, the compiler maintains a set of **builtin identifiers** in `qualnames.hpp`. These are type names, class names, and interface names that the compiler recognizes as part of the standard type system. They are registered via the `builtinIdentifiers()` function, which returns an `std::unordered_set<std::string>`.

The builtin identifiers include:

**Numeric types:** `I8`, `I16`, `I32`, `I64`, `U8`, `U16`, `U32`, `U64`, `Int`, `UInt`, `Float`, `Double`, `Byte`

**Core types:** `String`, `Boolean`, `Bool`, `Char`, `Void`, `Object`

**Memory types:** `Memory`, `Args`, `Kwargs`

**Async types:** `Future`, `Generator`

**Error hierarchy:** `Error`, `Exception`, `Warning`, `Throwable`, `Traceback`, `ArithmeticError`, `ZeroDivisionError`, `OverflowError`, `UnderflowError`

### Distinction from Keywords

Builtin identifiers are **not** lexer keywords. The lexer produces `Identifier("I64")`, not `KeywordI64` — there is no such token type. The distinction matters:

1. **Keywords** are resolved at the lexer level. The `readIdentifierOrKeyword()` method checks the keymaps registry and emits a keyword token type. The parser receives `KeywordIf`, `KeywordClass`, etc., and dispatches on these types directly.

2. **Builtin identifiers** are resolved at the semantic analysis level. The lexer produces a generic `Identifier` token. The semantic analyzer then looks up the identifier in the type registry, where builtin types are pre-registered during compiler initialization. The identifier "I64" resolves to the `uranite.language.i64.I64` class through the type registry, not through the keyword system.

This means builtin identifiers can technically be shadowed by user-defined names — declaring a local variable named `I64` would shadow the type name within that scope. However, the linter flags such shadowing as a warning, and the practice is strongly discouraged because it creates confusing code where a type name refers to a variable.

Keywords cannot be shadowed at all. The lexer always produces a keyword token for any string that matches a keymaps entry, regardless of scope or context.

### The qualnames Registry

The `qualnames.hpp` file centralizes all entity name strings used across the compiler into a nested namespace hierarchy under `uranite::semantic::qualname`. This includes:

- **Qualified names** — Fully-qualified type paths like "uranite.language.i64.I64" used for type identity comparisons throughout the compiler.
- **Short names** — Unqualified type names like "I64" used for user-facing display and lookup.
- **Method names** — Standard method names like "toString", "hashCode", "equals" that the compiler recognizes for operator dispatch and protocol conformance.
- **Operator mappings** — The `OperatorMapping` struct and `ArithmeticMappings`/`FullOperatorMappings` tables that map operator tokens to interface method calls.

The qualnames registry does not affect lexing. It operates entirely at the semantic analysis and code generation levels, providing a single source of truth for all name comparisons that the compiler performs against user-defined and builtin entities.

---

## Keyword Usage Examples

### Control Flow Examples

```uranite
public function classify( I64 value ) -> String:
    if value > 0:
        return "positive"
    elif value < 0:
        return "negative"
    else:
        return "zero"
```

```uranite
public function sum( ArrayList<I64> numbers ) -> I64:
    I64 total = 0
    for I64 number in numbers:
        if number == 0:
            continue
        total = total + number
    return total
```

```uranite
public function findFirst( ArrayList<I64> values, I64 target ) -> I64:
    I64 index = 0
    while index < values.size():
        if values.get( index ) == target:
            break
        index = index + 1
    return index
```

```uranite
public function fibonacci() -> Generator<I64>:
    I64 previous = 0
    I64 current = 1
    while True:
        yield current
        I64 next = previous + current
        previous = current
        current = next
```

### Declarations and Modules Examples

```uranite
package myapp.models

from uranite.collection.array-list import ArrayList
from uranite.collection.hash-map import HashMap
import uranite.io.console as console

public const I64 MAX_CAPACITY = 1024

public type StringList = ArrayList<String>

public enum Direction backed I32:
    unit North 0
    unit South 1
    unit East 2
    unit West 3

public interface Renderable:
    public function render( self ) -> String;

public struct Point:
    public I64 x
    public I64 y

public class Canvas implements Renderable:
    private ArrayList<Point> points

    public static function create() -> Canvas:
        Canvas canvas = new Canvas()
        return canvas

    public function render( self ) -> String:
        return "Canvas"
```

### Memory and Ownership Examples

```uranite
from uranite.memory.memory import Memory

public function transferOwnership() -> Void:
    Memory<I64> buffer = new Memory<I64>( 256 )
    Memory<I64> newOwner = move buffer

public function borrowValue( reference I64 value ) -> I64:
    return value * 2

public function rawAddress() -> Void:
    I64 counter = 42
    U64 address = addressof counter

public function dangerousOperation() -> Void:
    unsafe:
        Memory<U8> raw = new Memory<U8>( 4096 )
        raw.set( 0, 0xFF )
        raw.free()
```

### Error Handling Examples

```uranite
from uranite.math.errors import ZeroDivisionError

public function safeDivide( I64 numerator, I64 denominator ) -> I64 raises ZeroDivisionError:
    if denominator == 0:
        raise new ZeroDivisionError( "division by zero" )
    return numerator / denominator

public function compute() -> Void:
    try:
        I64 result = safeDivide( 100, 0 )
    except ZeroDivisionError error:
        console.puts( error.toString() )
    finally:
        console.puts( "computation complete" )
```

### Async and Concurrency Examples

```uranite
from uranite.io.file import readFile

public async function loadConfig( String path ) -> String:
    String content = await readFile( path )
    return content

public async function loadAll() -> Void:
    String config = await loadConfig( "config.yaml" )
    String data = await loadConfig( "data.yaml" )
```

### Pattern Matching and Enums Examples

```uranite
public enum Shape:
    unit Circle
    unit Rectangle
    unit Triangle

public function area( Shape shape, F64 dimension ) -> F64:
    match shape:
        case Shape.Circle:
            return 3.14159 * dimension * dimension
        case Shape.Rectangle:
            return dimension * dimension
        case Shape.Triangle:
            return 0.5 * dimension * dimension
```

```uranite
public function describeValue( Object value ) -> String:
    if value is None:
        return "nothing"
    if value instanceof String:
        String text = value as String
        return text
    return value.toString()
```

---

## What Cannot Be a Keyword

Identifiers that start with a digit are rejected by the lexer before `readIdentifierOrKeyword()` runs — digit-leading tokens are dispatched to `readNumber()` instead. This means tokens like "3d" or "2x" are never tested against the keyword registry; they are parsed as numeric literals (possibly with a suffix).

Identifiers containing characters outside `[a-zA-Z0-9_]` cannot match any keyword, because `readIdentifierOrKeyword()` stops accumulating at the first non-alphanumeric, non-underscore character. A string like "class-name" produces two tokens: `KeywordClass` and then (after the `-` is processed as `Minus`) `Identifier("name")`.

The underscore character (`_`) is valid in identifiers but does not appear in any keyword. No keyword contains an underscore, a digit, or an uppercase letter (except `False`, `True`, `None`, and `Readonly`).
