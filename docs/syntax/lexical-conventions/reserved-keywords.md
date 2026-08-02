# Reserved Keywords

Uranite reserves 78 keywords. These identifiers are unconditionally reserved — they cannot be used as variable names, function names, type names, parameter names, or any other user-defined identifier. Attempting to use a reserved keyword as an identifier produces a compilation error. This document catalogs every keyword, explains case sensitivity rules, and provides usage examples for each category.

---

## Table of Contents

- [Reserved Keywords](#reserved-keywords)
  - [Table of Contents](#table-of-contents)
  - [Unconditional Reservation](#unconditional-reservation)
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
  - [Complete Keyword Reference](#complete-keyword-reference)
  - [Builtin Type Names](#builtin-type-names)
    - [How Builtin Types Differ from Keywords](#how-builtin-types-differ-from-keywords)
    - [Complete Builtin Type List](#complete-builtin-type-list)
  - [Usage Examples](#usage-examples)
    - [Control Flow Examples](#control-flow-examples)
    - [Declaration and Module Examples](#declaration-and-module-examples)
    - [Object-Oriented Examples](#object-oriented-examples)
    - [Memory and Ownership Examples](#memory-and-ownership-examples)
    - [Error Handling Examples](#error-handling-examples)
    - [Async and Concurrency Examples](#async-and-concurrency-examples)
    - [Pattern Matching and Enum Examples](#pattern-matching-and-enum-examples)
  - [Common Errors with Keywords](#common-errors-with-keywords)

---

## Unconditional Reservation

Keywords in Uranite are **unconditionally reserved**. There is no context in which a keyword can be used as a user-defined identifier. The compiler always recognizes a keyword as a keyword, regardless of where it appears.

The following code is invalid:

```uranite
I64 class = 5
```

The compiler interprets `class` as the keyword for declaring a class type, not as a variable name. This produces a syntax error because the parser encounters a class declaration keyword where it expects an identifier.

This applies to all 78 keywords without exception. Even keywords that are reserved for future use (like `switch` and `trait`) cannot be used as identifiers.

---

## Case Sensitivity

Keyword matching is case-sensitive. Most keywords are entirely lowercase. Five exceptions have non-lowercase forms:

| Keyword | Case | Notes |
|---|---|---|
| `False` | Capital F | Boolean literal. Lowercase `false` is a valid identifier, not a keyword. |
| `True` | Capital T | Boolean literal. Lowercase `true` is a valid identifier, not a keyword. |
| `None` | Capital N | Null-equivalent literal. Lowercase `none` is a valid identifier, not a keyword. |
| `Readonly` | Capital R | Alternate casing of `readonly`. Both forms are accepted. |
| `readonly` | All lowercase | Primary casing. Identical in meaning to `Readonly`. |

The `readonly` / `Readonly` pair is the only keyword with two accepted casings. All other keywords have exactly one valid form.

Because matching is exact, `IF`, `Class`, `RETURN`, `TRUE`, and similar capitalization variants are not keywords. They are valid user-defined identifiers. Only the exact casing listed in the reference table is reserved:

```uranite
I64 TRUE = 1
I64 FALSE = 0
```

This compiles without error because `TRUE` and `FALSE` (all uppercase) are not keywords. Only `True` and `False` (capital first letter, lowercase rest) are reserved. While this is valid, using identifier names that resemble keywords is strongly discouraged because it creates confusing code.

---

## Keyword Categories

Keywords are organized into nine functional categories.

### Control Flow

Keywords that alter execution flow within a function body.

| Keyword | Purpose |
|---|---|
| `break` | Exit the innermost loop immediately. |
| `case` | Individual branch within a `switch` statement. Not used with `match`. |
| `continue` | Skip the rest of the current iteration and proceed to the next. |
| `else` | Fallback branch in conditional chains when no `if` or `elif` condition matched. |
| `elif` | Additional conditional branch in an if/elif/else chain (else-if). |
| `for` | Iterate over sequences, ranges, or any type implementing the iterator protocol. |
| `if` | Conditional branch — execute a block only when a boolean condition is `True`. |
| `match` | Match expression. Inline pattern matching using `match <expr> in <arms>` syntax with `=>` arrows. |
| `return` | Return a value from a function and transfer control to the caller. |
| `switch` | Switch statement. Block-based branching using `case` arms with colon-delimited bodies. |
| `while` | Loop that repeats as long as a boolean condition remains `True`. |
| `yield` | Produce a value from a generator function without terminating it. |

### Declarations

Keywords that introduce named entities — types, functions, modules, and bindings.

| Keyword | Purpose |
|---|---|
| `class` | Declare a class type with fields, methods, and constructors. |
| `const` | Declare a compile-time constant value. |
| `enum` | Declare an enumeration type with named variants. |
| `extern` | Declare an external (foreign) function or symbol implemented outside Uranite. |
| `from` | Specify the source module in an import statement (`from module import entity`). |
| `function` | Declare a named function. |
| `implements` | Declare that a class implements an interface contract. |
| `import` | Import entities from another module into the current scope. |
| `interface` | Declare an interface — an abstract contract that classes can implement. |
| `package` | Declare the package identity of a source file. Must be the first statement. |
| `static` | Declare a class-level member that belongs to the class itself, not instances. |
| `struct` | Declare a value-type struct with public fields. |
| `trait` | Reserved for future use (trait-based composition). |
| `type` | Declare a type alias that gives an existing type a new name. |

### Access Modifiers

Keywords that control visibility of declarations across module and class boundaries.

| Keyword | Purpose |
|---|---|
| `private` | Visible only within the declaring class. Cannot be accessed from outside. |
| `protect` | Visible within the declaring class and its subclasses. Hidden from external code. |
| `public` | Visible to all modules that import the declaration. Required for cross-module access. |

When no access modifier is specified, declarations default to package-private visibility — accessible within the same package but not importable by external modules.

### Object-Oriented Programming

Keywords for class hierarchies, polymorphism, construction, and instance semantics.

| Keyword | Purpose |
|---|---|
| `abstract` | Mark a class as non-instantiable or a method as requiring override in subclasses. |
| `delete` | Explicitly destroy an object and release its resources. |
| `extends` | Declare that a class inherits from a parent class. |
| `final` | Prevent a class from being subclassed or a method from being overridden. |
| `native` | Mark a method as implemented in native code (C or assembly) rather than Uranite. |
| `new` | Construct a new object instance by invoking a class constructor. |
| `override` | Mark a method as intentionally overriding a parent class method. |
| `parent` | Reference the parent class to call parent constructors or overridden methods. |
| `property` | Declare a computed property with getter and/or setter semantics. |
| `readonly` | Mark a field as immutable after construction. Also accepted as `Readonly`. |
| `Readonly` | Alternate casing of `readonly`. Identical in meaning and behavior. |
| `self` | Reference the current object instance within methods and constructors. |
| `virtual` | Mark a method for dynamic dispatch, enabling polymorphic calls through parent references. |

### Memory and Safety

Keywords governing ownership, borrowing, raw memory access, and unsafe operations.

| Keyword | Purpose |
|---|---|
| `addressof` | Obtain the raw memory address of a variable as a `U64` value. |
| `move` | Transfer ownership of a value to a new binding. The original binding becomes invalid. |
| `mut` | Mark a variable or parameter as mutable, allowing reassignment after initialization. |
| `own` | Declare explicit ownership semantics for a parameter or field. |
| `reference` | Pass a value by reference (borrow) without transferring ownership. |
| `unsafe` | Enter an unsafe block where borrow checker rules are relaxed and raw memory operations are permitted. |

### Logical Operators

Keywords that serve as boolean operators. Uranite uses word-form logical operators exclusively — the symbols `&&` and `||` are not valid in Uranite.

| Keyword | Purpose |
|---|---|
| `and` | Logical AND with short-circuit evaluation. Evaluates the right operand only if the left is `True`. |
| `not` | Logical NOT. Negates a boolean value. |
| `or` | Logical OR with short-circuit evaluation. Evaluates the right operand only if the left is `False`. |

The `!` character is used only as part of the `!=` (not-equal) operator. It is never used as a standalone negation operator — use `not` instead.

### Literal Values

Keywords representing built-in constant values. These are capitalized to distinguish them visually from control-flow and declaration keywords.

| Keyword | Purpose |
|---|---|
| `False` | Boolean false literal. The only falsy boolean value. |
| `None` | Null-equivalent — represents the absence of a value. Used with nullable types. |
| `True` | Boolean true literal. The only truthy boolean value. |

These three keywords are the only identifiers in Uranite that begin with an uppercase letter and are reserved. All other capitalized identifiers (like `I64`, `String`, `ArrayList`) are user-facing type names, not keywords.

### Error Handling

Keywords for structured exception handling with try/except/finally blocks.

| Keyword | Purpose |
|---|---|
| `except` | Catch an exception by type within a `try` block. |
| `finally` | Execute cleanup code regardless of whether an exception occurred. Always runs. |
| `raise` | Throw an exception. Takes a `Throwable` object as its operand. |
| `raises` | Declare in a function signature that the function may raise a specific exception type. |
| `try` | Begin a protected block for exception handling. Must be followed by `except` and/or `finally`. |

Note the distinction between `raise` (imperative — raise an exception now) and `raises` (declarative — this function's signature declares that it may raise).

### General Purpose

Keywords spanning type casting, concurrency, metaprogramming, inline assembly, and miscellaneous language features.

| Keyword | Purpose |
|---|---|
| `as` | Type casting (`value as TargetType`) or import aliasing (`import module as alias`). |
| `asm` | Introduce an inline assembly block for direct hardware-level instructions. |
| `async` | Mark a function as asynchronous. The function returns a `Future<T>` value. |
| `await` | Suspend execution until an asynchronous operation completes and unwrap the `Future<T>`. |
| `backed` | Specify a backing type for an enum (`enum Direction backed I32:`). |
| `defer` | Schedule a statement to execute when the current scope exits, regardless of how it exits. |
| `export` | Make declarations available for import by other modules via an export block. |
| `in` | Membership test (`element in collection`) or iteration target in `for` loops (`for x in items`). |
| `instanceof` | Runtime type check. Returns `True` if an object is an instance of a given class or interface. |
| `is` | Identity comparison. Tests reference equality or checks whether a value `is None`. |
| `lambda` | Declare an anonymous function expression with captured scope. |
| `pass` | No-op placeholder for intentionally empty blocks. Required when a block has no statements. |
| `subclassof` | Compile-time check of subclass relationships between types. |
| `unit` | Declare individual variants within an enum type. |
| `use` | Bring a module or type into scope (alternative import form). |
| `volatile` | Mark a variable as volatile, preventing the compiler from optimizing away reads and writes. |
| `where` | Type constraint clause on generic declarations (`function sort<T>( ... ) -> ... where T implements Comparable:`). |

---

## Complete Keyword Reference

All 78 keywords sorted alphabetically. The "Casing" column shows the exact form that is reserved — only that exact casing is a keyword.

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

Entry #56 (`readonly`) and #57 (`Readonly`) are two accepted casings of the same keyword. Both compile identically. The total of 78 includes both forms plus the 76 other unique keywords.

---

## Builtin Type Names

Beyond the 78 reserved keywords, Uranite has a set of **builtin type names** that the compiler recognizes as part of the standard type system. These are pre-registered during compiler initialization and resolve to their corresponding standard library implementations automatically.

### How Builtin Types Differ from Keywords

Keywords and builtin type names behave differently:

**Keywords** are unconditionally reserved. The compiler always treats them as language constructs, never as user-defined identifiers. You cannot declare a variable named `class`, `if`, or `return` under any circumstances.

**Builtin type names** are pre-registered identifiers, not reserved words. The compiler recognizes names like `I64`, `String`, and `Boolean` as type names during semantic analysis, but they are technically identifiers at the lexical level. This means:

- A local variable named `I64` would shadow the type name within that scope
- The code would compile, but the type name becomes inaccessible in that scope
- The linter flags such shadowing as a warning

While shadowing builtin type names is technically valid, it is strongly discouraged because it creates confusing code where a type name refers to a variable instead of its expected type.

### Complete Builtin Type List

**Integer types:**

| Type | Description |
|---|---|
| `I8` | Signed 8-bit integer (-128 to 127) |
| `I16` | Signed 16-bit integer (-32,768 to 32,767) |
| `I32` | Signed 32-bit integer (-2,147,483,648 to 2,147,483,647) |
| `I64` | Signed 64-bit integer (-9,223,372,036,854,775,808 to 9,223,372,036,854,775,807) |
| `U8` | Unsigned 8-bit integer (0 to 255) |
| `U16` | Unsigned 16-bit integer (0 to 65,535) |
| `U32` | Unsigned 32-bit integer (0 to 4,294,967,295) |
| `U64` | Unsigned 64-bit integer (0 to 18,446,744,073,709,551,615) |
| `Int` | Platform-width signed integer (64-bit on supported systems). |
| `UInt` | Platform-width unsigned integer (64-bit on supported systems). |
| `Byte` | Unsigned 8-bit integer for raw byte data. |

**Floating-point types:**

| Type | Description |
|---|---|
| `Float` | 32-bit IEEE 754 single-precision floating-point number |
| `Double` | 64-bit IEEE 754 double-precision floating-point number |

**Core types:**

| Type | Description |
|---|---|
| `Boolean` | Boolean type. Holds `True` or `False`. |
| `Char` | 32-bit Unicode character. Holds a single code point. |
| `String` | Immutable UTF-8 string. |
| `Void` | Unit type representing no value. Used as return type for functions with no return value. |
| `Object` | Root of the class hierarchy. Every class implicitly extends `Object`. |

**Memory types:**

| Type | Description |
|---|---|
| `Memory<T>` | Raw heap-allocated memory block for type `T`. Manual allocation and deallocation. |
| `Args<T>` | Container for variadic positional arguments in functions using `T args{}` syntax. |
| `Kwargs<K, V>` | Container for keyword arguments in functions using `V kwargs{}` syntax. |

**Async types:**

| Type | Description |
|---|---|
| `Future<T>` | Represents an asynchronous computation that will eventually produce a value of type `T`. |
| `Generator<T>` | Represents a lazy sequence of values produced by `yield` statements. |

**Error hierarchy:**

| Type | Description |
|---|---|
| `Throwable` | Root of all throwable types. Parent of both `Error` and `Exception`. |
| `Error` | Represents serious errors that typically should not be caught (e.g., out of memory). |
| `Exception` | Represents recoverable errors intended to be caught with `try`/`except`. |
| `Warning` | Represents non-fatal diagnostic conditions. |
| `Traceback` | Represents a stack trace captured when an exception is raised. |
| `ArithmeticError` | Raised for arithmetic failures. Parent of division and overflow errors. |
| `ZeroDivisionError` | Raised when dividing or taking modulo by zero. |
| `OverflowError` | Raised when an arithmetic operation exceeds the maximum value of its type. |
| `UnderflowError` | Raised when an arithmetic operation falls below the minimum value of its type. |

---

## Usage Examples

### Control Flow Examples

Conditional branching with `if`, `elif`, and `else`:

```uranite
public function classify( I64 value ) -> String:
    if value > 0:
        return "positive"
    elif value < 0:
        return "negative"
    else:
        return "zero"
```

Iteration with `for`, `continue`, and `break`:

```uranite
public function sumPositive( ArrayList<I64> numbers ) -> I64:
    I64 total = 0
    for I64 number in numbers:
        if number <= 0:
            continue
        total = total + number
    return total
```

Loop with `while` and early exit with `break`:

```uranite
public function findFirst( ArrayList<I64> values, I64 target ) -> I64:
    I64 index = 0
    while index < values.size():
        if values.get( index ) == target:
            break
        index = index + 1
    return index
```

Generator function with `yield`:

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

Match expression (inline pattern matching with `in` and `=>`):

```uranite
public function describe( Shape shape ) -> String:
    return match shape in \
        Shape.Circle => "round", \
        Shape.Rectangle => "four sides", \
        Shape.Triangle => "three sides", \
        * => "unknown"
```

Switch statement (block-based branching with `case`):

```uranite
public enum Shape:
    unit Circle
    unit Rectangle
    unit Triangle

public function describeSwitch( Shape shape ) -> String:
    switch shape:
        case Shape.Circle:
            return "round"
        case Shape.Rectangle:
            return "four sides"
        case Shape.Triangle:
            return "three sides"
        case *:
            return "unknown"
    return "unreachable"
```

### Declaration and Module Examples

Package declaration, imports, constants, type aliases, enums, interfaces, structs, and classes:

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
    public I64 xCoordinate
    public I64 yCoordinate

public class Canvas implements Renderable:

    private ArrayList<Point> points

    public static function create() -> Canvas:
        Canvas canvas = new Canvas()
        return canvas

    public function render( self ) -> String:
        return "Canvas"
```

### Object-Oriented Examples

Inheritance with `extends`, constructor delegation with `parent`, method overriding with `override` and `virtual`, and access modifiers:

```uranite
public class Animal:

    protect String species

    public function Animal( self, String species ) -> Void:
        self.species = species

    public virtual function speak( self ) -> String:
        return "..."

public class Dog extends Animal:

    private String name

    public function Dog( self, String name ) -> Void:
        parent( "Canis familiaris" )
        self.name = name

    public override function speak( self ) -> String:
        return "Woof!"

public final class GuideDog extends Dog:

    readonly String handler

    public function GuideDog( self, String name, String handler ) -> Void:
        parent( name )
        self.handler = handler
```

The `final` keyword on `GuideDog` prevents further subclassing. The `readonly` keyword on `handler` ensures the field cannot be reassigned after construction.

Abstract classes with `abstract`:

```uranite
public abstract class Shape:

    public abstract function area( self ) -> Double;

    public function describe( self ) -> String:
        return "Shape with area " + self.area().toString()
```

Abstract classes cannot be instantiated directly. Subclasses must override all abstract methods.

Type checking with `instanceof`, `is`, and `as`:

```uranite
public function process( Object value ) -> String:
    if value is None:
        return "nothing"
    if value instanceof String:
        String text = value as String
        return text
    return value.toString()
```

The `is` keyword tests identity (reference equality or `None` comparison). The `instanceof` keyword tests type membership at runtime. The `as` keyword performs type casting.

### Memory and Ownership Examples

Ownership transfer with `move`:

```uranite
from uranite.memory.memory import Memory

public function transferOwnership() -> Void:
    Memory<I64> buffer = new Memory<I64>( 256 )
    Memory<I64> newOwner = move buffer
```

After `move buffer`, the `buffer` binding becomes invalid. Any attempt to use `buffer` after the move produces a compilation error from the borrow checker.

Borrowing with `reference`:

```uranite
public function doubleValue( reference I64 value ) -> I64:
    return value * 2
```

The `reference` keyword passes `value` by reference without transferring ownership. The caller retains ownership, and the function borrows the value for the duration of the call.

Raw address access with `addressof`:

```uranite
public function getAddress() -> Void:
    I64 counter = 42
    U64 address = addressof counter
```

The `addressof` keyword extracts the raw memory address of a variable. This is a low-level operation typically used only in unsafe code or for interop with native libraries.

Unsafe blocks with `unsafe`:

```uranite
public function rawMemoryOperation() -> Void:
    unsafe:
        Memory<U8> raw = new Memory<U8>( 4096 )
        raw.set( 0, 0xFF )
        raw.free()
```

Inside an `unsafe` block, borrow checker rules are relaxed. Manual memory management, raw pointer arithmetic, and unchecked operations are permitted. Code inside `unsafe` blocks is the programmer's responsibility to keep correct.

Mutable parameters with `mut`:

```uranite
public function increment( mut I64 counter ) -> I64:
    counter = counter + 1
    return counter
```

The `mut` keyword marks the parameter as mutable, allowing reassignment within the function body. Without `mut`, parameters are immutable by default.

Deferred cleanup with `defer`:

```uranite
public function readAndProcess( String path ) -> Void:
    Memory<U8> buffer = new Memory<U8>( 4096 )
    defer buffer.free()
    processData( buffer )
```

The `defer` statement schedules `buffer.free()` to execute when the enclosing scope exits, regardless of whether the function returns normally or an exception is raised. This guarantees resource cleanup without wrapping the entire function in try/finally.

### Error Handling Examples

Declaring throwable functions with `raises`, throwing with `raise`, and catching with `except`:

```uranite
public function safeDivide( I64 numerator, I64 denominator ) -> I64 raises ZeroDivisionError:
    if denominator == 0:
        raise new ZeroDivisionError( "division by zero" )
    return numerator / denominator
```

The `raises` keyword in the function signature declares that this function may raise a `ZeroDivisionError`. Callers must handle this with `try`/`except` or propagate the exception by declaring `raises` in their own signature.

Catching exceptions with `try`, `except`, and `finally`:

```uranite
from uranite.io.console import puts

public function compute() -> Void:
    try:
        I64 result = safeDivide( 100, 0 )
        puts( result.toString() )
    except ZeroDivisionError error:
        puts( error.toString() )
    finally:
        puts( "computation complete" )
```

The `try` block protects the code inside it. If a `ZeroDivisionError` is raised, control transfers to the `except` block. The `finally` block runs unconditionally — whether the `try` block completed normally, an exception was caught, or an exception propagated upward.

Multiple `except` clauses can handle different exception types:

```uranite
public function riskyComputation( I64 value ) -> I64:
    try:
        I64 result = value * value * value
        return result / ( value - 10 )
    except ZeroDivisionError error:
        return 0
    except OverflowError error:
        return -1
```

### Async and Concurrency Examples

Asynchronous functions with `async` and `await`:

```uranite
from uranite.io.file import readFile

public async function loadConfig( String path ) -> String:
    String content = await readFile( path )
    return content
```

The `async` keyword marks the function as asynchronous. It returns a `Future<String>` rather than a plain `String`. The `await` keyword suspends execution until the asynchronous operation completes, then unwraps the `Future<T>` to get the inner value.

The `await` keyword can only appear inside `async` functions. Using `await` in a non-async function produces a compilation error.

Calling multiple async operations:

```uranite
public async function loadAll() -> Void:
    String config = await loadConfig( "config.yaml" )
    String data = await loadConfig( "data.yaml" )
    puts( config )
    puts( data )
```

### Pattern Matching and Enum Examples

Enum declarations with `unit` and optional backing types with `backed`:

```uranite
public enum Color:
    unit Red
    unit Green
    unit Blue

public enum HttpStatus backed I32:
    unit Ok 200
    unit NotFound 404
    unit InternalError 500
```

The `unit` keyword declares individual variants within an enum. Without `backed`, variants have no associated integer value. With `backed`, each variant maps to a specific value of the backing type.

Match expression with `match ... in` and `=>`:

```uranite
public function statusMessage( HttpStatus status ) -> String:
    return match status in \
        HttpStatus.Ok => "Success", \
        HttpStatus.NotFound => "Not Found", \
        HttpStatus.InternalError => "Internal Server Error", \
        * => "Unknown Status"
```

Switch statement with `case` arms:

```uranite
public function statusDescription( HttpStatus status ) -> String:
    switch status:
        case HttpStatus.Ok:
            return "Request succeeded"
        case HttpStatus.NotFound:
            return "Resource not found"
        case HttpStatus.InternalError:
            return "Server encountered an error"
        case *:
            return "Unknown status code"
    return "unreachable"
```

Type-based matching with `instanceof` and `subclassof`:

```uranite
public function handleInput( Object input ) -> Void:
    if input instanceof String:
        processText( input as String )
    elif input instanceof I64:
        processNumber( input as I64 )
    else:
        processGeneric( input )
```

The `instanceof` keyword performs a runtime type check. The `subclassof` keyword performs a compile-time check of subclass relationships, useful in generic constraints.

Empty blocks with `pass`:

```uranite
public interface Serializable:
    public function serialize( self ) -> String;

public class Placeholder implements Serializable:

    public function serialize( self ) -> String:
        pass
```

The `pass` keyword is required when a block must exist syntactically but has no statements. Without `pass`, an empty block produces a syntax error because the compiler expects at least one indented line after a colon.

Inline assembly with `asm`:

```uranite
public function atomicIncrement( reference I64 value ) -> Void:
    unsafe:
        asm:
            "lock incq ($0)" : "+m"( value )
```

The `asm` keyword introduces an inline assembly block for direct hardware-level instructions. This is an advanced feature used for atomic operations, SIMD instructions, and system calls that cannot be expressed in Uranite.

Volatile variables with `volatile`:

```uranite
public function spinWait( volatile reference Boolean flag ) -> Void:
    while flag == False:
        pass
```

The `volatile` keyword prevents the compiler from optimizing away reads to `flag`. Without `volatile`, the compiler might cache the value of `flag` in a register and never re-read it from memory, causing the loop to spin indefinitely even after another thread sets `flag` to `True`.

Export blocks with `export`:

```uranite
export:
    public class Logger
    public function createLogger
```

The `export` keyword makes declarations available for import by other modules. Declarations inside an `export` block are treated as part of the module's public API.

Generic constraints with `where`:

```uranite
public function maximum<T>( T first, T second ) -> T where T implements Comparable:
    if first.greaterThan( second ):
        return first
    return second
```

The `where` clause constrains the generic type parameter `T` to types that implement the `Comparable` interface. This allows the function to call `greaterThan` on values of type `T`, which is guaranteed to exist because `Comparable` requires it.

Lambda expressions with `lambda`:

```uranite
public function applyTransform( ArrayList<I64> values ) -> ArrayList<I64>:
    return values.map( lambda ( I64 element ) -> I64: element * 2 )
```

The `lambda` keyword introduces an anonymous function expression. The lambda captures variables from its enclosing scope and can be passed as a value to higher-order functions.

---

## Common Errors with Keywords

**Using a keyword as a variable name:**

```
error: unexpected keyword 'class'
  --> source.urn:1:5
  | expected identifier
```

No keyword can be used as a variable name, function name, parameter name, type name, or any other user-defined identifier. Choose a different name.

**Using lowercase `true`, `false`, or `none`:**

```uranite
Boolean active = true
```

Lowercase `true` is not a keyword — it is parsed as an identifier. The compiler looks for a variable named `true` and reports "undeclared identifier" if none exists. Use `True`, `False`, and `None` with the required capitalization.

