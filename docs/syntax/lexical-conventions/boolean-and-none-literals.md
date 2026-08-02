# Boolean and None Literals

Uranite provides three keyword literals for truth values and absence of value: `True`, `False`, and `None`. Boolean literals represent logical truth values used in conditions, comparisons, and logical expressions. `None` represents the explicit absence of a value, used with optional types to indicate that no value is present. All three are reserved keywords — they cannot be used as variable names, function names, or type names.

---

## Table of Contents

- [Boolean and None Literals](#boolean-and-none-literals)
  - [Table of Contents](#table-of-contents)
  - [Overview](#overview)
  - [Boolean Literals](#boolean-literals)
    - [Syntax and Casing](#syntax-and-casing)
    - [Type of Boolean Literals](#type-of-boolean-literals)
    - [Boolean in Conditions](#boolean-in-conditions)
    - [Boolean as Return Values](#boolean-as-return-values)
  - [Logical Operators](#logical-operators)
    - [The "and" Operator](#the-and-operator)
    - [The "or" Operator](#the-or-operator)
    - [The "not" Operator](#the-not-operator)
    - [Operator Precedence](#operator-precedence)
    - [Evaluation Behavior](#evaluation-behavior)
    - [Complete Truth Table](#complete-truth-table)
  - [The Boolean Class](#the-boolean-class)
    - [Class Overview](#class-overview)
    - [Methods](#methods)
    - [Method Examples](#method-examples)
  - [None Literal](#none-literal)
    - [What None Represents](#what-none-represents)
    - [None Syntax](#none-syntax)
    - [None is a Reserved Keyword](#none-is-a-reserved-keyword)
  - [Optional Types](#optional-types)
    - [Declaring Optional Types](#declaring-optional-types)
    - [Assigning Values to Optionals](#assigning-values-to-optionals)
    - [Returning Optional Values](#returning-optional-values)
    - [Optional Type Naming](#optional-type-naming)
  - [None Comparison](#none-comparison)
    - [Equality Comparison](#equality-comparison)
    - [Identity Check with "is"](#identity-check-with-is)
    - [Equality vs Identity](#equality-vs-identity)
    - [None in Conditional Chains](#none-in-conditional-chains)
  - [The Boolean Type Name](#the-boolean-type-name)
  - [None Has No Wrapper Class](#none-has-no-wrapper-class)
  - [Examples](#examples)
    - [Boolean Literal Examples](#boolean-literal-examples)
    - [Logical Operation Examples](#logical-operation-examples)
    - [None Handling Examples](#none-handling-examples)
    - [Practical Usage](#practical-usage)

---

## Overview

| Literal | Type | Description |
|---|---|---|
| `True` | `Boolean` | Boolean truth value representing logical true |
| `False` | `Boolean` | Boolean truth value representing logical false |
| `None` | `None` | Sentinel value representing the absence of a value |

All three literals are PascalCase. This casing is mandatory and consistent across all Uranite keyword literals. Lowercase variants (`true`, `false`, `none`) are not boolean or none literals — they are treated as ordinary identifiers, which will fail during compilation if no variable with that name exists in scope.

---

## Boolean Literals

### Syntax and Casing

Boolean literals use PascalCase: `True` and `False`. No other casing is accepted:

```uranite
Boolean isActive = True
Boolean isDeleted = False
```

The following are **not** boolean literals:

| Attempted | What Happens |
|---|---|
| `true` | Treated as an identifier. Compilation error if no variable named `true` exists. |
| `false` | Treated as an identifier. Compilation error if no variable named `false` exists. |
| `TRUE` | Treated as an identifier. Compilation error if no variable named `TRUE` exists. |
| `FALSE` | Treated as an identifier. Compilation error if no variable named `FALSE` exists. |

Uranite follows Python's convention of PascalCase boolean literals, not the lowercase convention used by C, Java, Rust, or Go. This is consistent with Uranite's PascalCase convention for `None` and type names.

### Type of Boolean Literals

Every boolean literal has the type `Boolean`. The `Boolean` type can hold exactly two values: `True` and `False`. No other values are possible — there is no implicit conversion from integers, strings, or pointers to `Boolean`.

```uranite
Boolean flag = True
Boolean empty = False
```

Boolean values occupy 1 bit of logical storage. A `Bool` variable stores nothing more than a single truth value.

### Boolean in Conditions

Boolean values are the native type for `if`, `elif`, and `while` conditions. These control flow statements expect a `Boolean` expression:

```uranite
Boolean shouldRun = True

if shouldRun:
    puts( "running" )

while shouldRun:
    puts( "looping" )
    shouldRun = False
```

Boolean expressions formed by comparison operators (`==`, `!=`, `<`, `>`, `<=`, `>=`) also produce `Boolean` values:

```uranite
I64 count = 10

if count > 0:
    puts( "count is positive" )

Boolean isZero = count == 0
```

### Boolean as Return Values

Functions can return `Boolean` to indicate success, presence, validity, or any binary state:

```uranite
public function isEven( I64 number ) -> Boolean:
    return number % 2 == 0

public function isPositive( I64 number ) -> Boolean:
    return number > 0
```

The return value is a `Boolean` literal or a boolean expression. Both forms are valid:

```uranite
public function alwaysTrue() -> Boolean:
    return True

public function isAdult( I64 age ) -> Boolean:
    return age >= 18
```

---

## Logical Operators

Uranite uses English-word logical operators: `and`, `or`, and `not`. The symbolic equivalents (`&&`, `||`, `!`) are **not valid syntax** and will produce compilation errors.

### The "and" Operator

The `and` operator performs logical conjunction. The result is `True` only when both operands are `True`:

```uranite
Boolean result = True and True
Boolean mixed = True and False
Boolean neither = False and False
```

The variable `result` is `True`. The variable `mixed` is `False`. The variable `neither` is `False`.

Both operands must be `Boolean`. Passing non-boolean operands (integers, strings, objects) produces a compilation error: "logical operators require boolean operands."

```uranite
Boolean valid = isActive and isVerified
Boolean both = count > 0 and name != ""
```

Comparison expressions produce `Boolean`, so they can serve as operands to `and` directly.

### The "or" Operator

The `or` operator performs logical disjunction. The result is `True` when at least one operand is `True`:

```uranite
Boolean result = False or True
Boolean either = True or False
Boolean neither = False or False
```

The variable `result` is `True`. The variable `either` is `True`. The variable `neither` is `False`.

Like `and`, both operands must be `Boolean`:

```uranite
Boolean allowed = isAdmin or isModerator
Boolean hasContent = name != "" or fallback != ""
```

### The "not" Operator

The `not` operator performs logical negation. It is a unary prefix operator that inverts a boolean value:

```uranite
Boolean result = not True
Boolean positive = not False
```

The variable `result` is `False`. The variable `positive` is `True`.

The `not` operator is especially useful for negating function return values and boolean variables:

```uranite
Boolean isInvalid = not isValid
Boolean notFound = not contains

if not isReady:
    puts( "waiting" )
```

The `not` operator also works on non-boolean values through truthiness rules:

| Operand Type | Truthiness Rule |
|---|---|
| `Boolean` | Direct inversion: `True` becomes `False`, `False` becomes `True` |
| Integer | Zero is truthy under `not` (produces `True`), non-zero is falsy under `not` (produces `False`) |
| Pointer / Optional | `None` is truthy under `not` (produces `True`), non-`None` is falsy under `not` (produces `False`) |

```uranite
I64 count = 0
Boolean isEmpty = not count
```

The variable `isEmpty` is `True` because `not` applied to zero produces `True`.

Note: while `not` accepts non-boolean values, `and` and `or` require strictly `Boolean` operands. This is an intentional asymmetry — `not` serves as both a logical negator and a truthiness converter.

### Operator Precedence

When combining logical operators, `not` binds tightest, then `and`, then `or`:

| Precedence | Operator | Associativity |
|---|---|---|
| Highest | `not` | Unary (prefix) |
| Middle | `and` | Left-to-right |
| Lowest | `or` | Left-to-right |

This means `True or False and not False` is parsed as `True or (False and (not False))`:

```uranite
Boolean result = True or False and not False
```

The `not False` evaluates to `True`. Then `False and True` evaluates to `False`. Then `True or False` evaluates to `True`. The variable `result` is `True`.

Use parentheses to override precedence when the default grouping is not what you intend:

```uranite
Boolean explicit = ( True or False ) and not False
```

The parenthesized `True or False` evaluates to `True`. Then `not False` evaluates to `True`. Then `True and True` evaluates to `True`. The variable `explicit` is `True`.

### Evaluation Behavior

Both `and` and `or` evaluate **both operands** before producing a result. Uranite does **not** implement short-circuit evaluation for logical operators. Both sides of `and` and `or` are always evaluated, even when the result could be determined from the left operand alone:

```uranite
Boolean result = False and expensiveCheck()
```

In this example, `expensiveCheck()` is called even though the left operand is `False` and the final result of `and` must be `False` regardless. If `expensiveCheck()` has side effects (printing, modifying state, performing I/O), those side effects will occur.

To achieve short-circuit behavior, use nested `if` statements:

```uranite
if isReady:
    if expensiveCheck():
        puts( "both conditions met" )
```

This ensures `expensiveCheck()` is only called when `isReady` is `True`.

### Complete Truth Table

| Expression | Result |
|---|---|
| `True and True` | `True` |
| `True and False` | `False` |
| `False and True` | `False` |
| `False and False` | `False` |
| `True or True` | `True` |
| `True or False` | `True` |
| `False or True` | `True` |
| `False or False` | `False` |
| `not True` | `False` |
| `not False` | `True` |

Compound expressions follow from these rules:

| Expression | Steps | Result |
|---|---|---|
| `not True and False` | `(not True) and False` = `False and False` | `False` |
| `not ( True and False )` | `not False` | `True` |
| `True or False and True` | `True or (False and True)` = `True or False` | `True` |
| `( True or False ) and not True` | `True and False` | `False` |
| `not False or not True` | `True or False` | `True` |
| `not ( False or True )` | `not True` | `False` |

---

## The Boolean Class

### Class Overview

`Boolean` is a `final` class — it cannot be subclassed. It wraps a primitive boolean value and provides methods for logical operations, negation, and string conversion. The `Boolean` class is the OOP wrapper for the `Bool` primitive type, located in the `uranite.language.boolean` package.

Every boolean literal (`True`, `False`) is typed as `Boolean`. Method calls are available directly on boolean values and boolean variables.

### Methods

| Method | Return Type | Description |
|---|---|---|
| `getValue()` | `Boolean` | Return the underlying truth value |
| `toString()` | `String` | Return "True" if the value is `True`, "False" if the value is `False` |
| `negate()` | `Boolean` | Return a new `Boolean` with the opposite truth value |
| `logicalAnd( Boolean other )` | `Boolean` | Return a new `Boolean` representing `self and other` |
| `logicalOr( Boolean other )` | `Boolean` | Return a new `Boolean` representing `self or other` |

All methods return new `Boolean` instances. The original value is never modified — `Boolean` is immutable.

### Method Examples

```uranite
Boolean flag = True

Boolean value = flag.getValue()
String text = flag.toString()
Boolean opposite = flag.negate()
```

The variable `value` is `True`. The variable `text` is "True". The variable `opposite` is `False`.

The `logicalAnd()` and `logicalOr()` methods provide method-call alternatives to the `and`/`or` operators:

```uranite
Boolean left = True
Boolean right = False

Boolean conjunction = left.logicalAnd( right )
Boolean disjunction = left.logicalOr( right )
```

The variable `conjunction` is `False`. The variable `disjunction` is `True`.

The `toString()` method returns PascalCase strings matching the literal syntax — "True" for a true value, "False" for a false value. This is useful for including boolean values in output:

```uranite
from uranite.io.console import puts

Boolean isReady = True
puts( "Ready: " + isReady.toString() )
```

This prints "Ready: True".

---

## None Literal

### What None Represents

`None` represents the explicit absence of a value. It is not zero, not an empty string, not false — it is a distinct concept meaning "no value exists here." `None` is used with optional types to indicate that a variable, parameter, or return value holds nothing.

In languages like Python, `None` serves a similar role. In languages like Rust, `None` corresponds to the `None` variant of `Option<T>`. In languages like Java, it corresponds to `null` — but Uranite's `None` is type-safe and cannot be assigned to non-optional variables.

### None Syntax

`None` is a PascalCase keyword literal. It appears wherever an expression is expected:

```uranite
String? name = None
```

The `?` suffix on the type annotation marks the variable as optional — it can hold either a `String` value or `None`. Without the `?`, assigning `None` would be a type error.

### None is a Reserved Keyword

`None` cannot be used as a variable name, function name, parameter name, class name, or any other identifier:

```
I64 None = 42
```

This produces a compilation error. The identifier `None` is permanently reserved by the language.

Like `True` and `False`, the casing is strict. Lowercase `none` is not the `None` literal — it would be treated as an ordinary identifier.

---

## Optional Types

### Declaring Optional Types

Optional types are declared with the `?` prefix on the type name:

```uranite
String? maybeName = None
I64? maybeCount = None
Boolean? maybeFlag = None
Char? maybeChar = None
```

The `?` syntax wraps the inner type in an optional container. A `String?` variable can hold either a `String` value or `None`. A `I64?` variable can hold either an `I64` value or `None`. Any type can be made optional — primitives, classes, structs, interfaces, and even other optional types.

An optional variable without an initial value defaults to holding a value of its inner type, not `None`. To explicitly start with no value, assign `None`:

```uranite
String? name = None
```

### Assigning Values to Optionals

An optional variable accepts both values of its inner type and `None`:

```uranite
String? greeting = "hello"
greeting = None
greeting = "world"
```

The variable `greeting` starts as a `String` with value "hello", then becomes `None`, then becomes a `String` again with value "world". All three assignments are valid because `String?` accepts both `String` and `None`.

A non-optional variable does **not** accept `None`:

```uranite
String name = None
```

This produces a compilation error. The type `String` (without `?`) requires a `String` value. To allow `None`, the type must be declared as `String?`.

### Returning Optional Values

Functions can declare optional return types to indicate they may not produce a value:

```uranite
public function findUser( String username ) -> String?:
    if username == "admin":
        return "Administrator"
    return None
```

The return type `String?` allows the function to return either a `String` value or `None`. Callers must handle the possibility that the return value is `None`:

```uranite
String? user = findUser( "admin" )
if user is None:
    puts( "user not found" )
```

### Optional Type Naming

When Uranite reports optional types in error messages or diagnostics, the type name appears as `Optional<T>` where `T` is the inner type:

| Declaration | Full Type Name |
|---|---|
| `String?` | `Optional<String>` |
| `I64?` | `Optional<I64>` |
| `Boolean?` | `Optional<Boolean>` |
| `ArrayList<I64>?` | `Optional<ArrayList<I64>>` |

The `?` suffix is the standard way to declare optional types in source code.

---

## None Comparison

### Equality Comparison

The equality operators `==` and `!=` work with `None` on either side. When comparing against `None`, the comparison checks whether the optional value is absent:

```uranite
String? name = None

if name == None:
    puts( "name is absent" )

if name != None:
    puts( "name is present" )
```

The `==` operator returns `True` when the optional holds `None`, `False` when it holds a value. The `!=` operator returns the opposite.

None comparison is special-cased in the type system. Normally, `==` and `!=` require compatible types on both sides. But when either operand is `None`, the comparison is always allowed regardless of the other operand's type, and the result is always `Boolean`.

### Identity Check with "is"

The `is` keyword performs an identity check. It is commonly used to check whether an optional value is `None`:

```uranite
String? name = None

if name is None:
    puts( "name is absent" )
```

The `is` keyword always produces a `Boolean` result. It checks reference identity — whether the value literally is `None`, not whether it is equal to `None` through some equality method.

### Equality vs Identity

For `None` checks, `==` and `is` behave identically in practice:

| Expression | Behavior |
|---|---|
| `value == None` | Equality comparison. Returns `True` if value is `None`. |
| `value is None` | Identity check. Returns `True` if value is `None`. |
| `value != None` | Inequality comparison. Returns `True` if value is not `None`. |

Both forms are idiomatic. Use `is None` when checking for the absence of a value (reads naturally in English). Use `== None` when the comparison is part of a larger equality expression.

The `is` keyword is not limited to `None` checks — it can compare any two values for reference identity. But its most common use is with `None`.

### None in Conditional Chains

None checks commonly appear in conditional chains where different actions are taken depending on whether a value is present:

```uranite
public function displayUser( String? name, I64? age ) -> Void:
    if name is None and age is None:
        puts( "no information available" )
        return
    if name is None:
        puts( "name unknown, age: " + age.toString() )
        return
    if age is None:
        puts( "name: " + name + ", age unknown" )
        return
    puts( "name: " + name + ", age: " + age.toString() )
```

Each branch handles a specific combination of present and absent values. After confirming a value is not `None`, it can be used as its inner type (`String` or `I64`) within that branch.

---

## The Boolean Type Name

The type name for boolean values in Uranite is `Boolean` — always spelled out in full. There is no shorthand like `Bool` or `bool`. Use `Boolean` for all variable declarations, function parameters, return types, and generic type arguments:

```uranite
Boolean flag = True
Boolean alsoFlag = False
```

Because `Boolean` is a `final` class in the `uranite.language.boolean` package, boolean values have full method access. Every `True` and `False` literal is an instance of `Boolean`:

```uranite
String text = flag.toString()
Boolean negated = alsoFlag.negate().getValue()
```

---

## None Has No Wrapper Class

Unlike `Boolean`, `Char`, `String`, and the numeric types that all have OOP wrapper classes, `None` has no wrapper class. `None` is a type-system concept representing the absence of a value — it carries no data and needs no methods.

You cannot call methods on `None`:

```uranite
String? empty = None
```

The variable `empty` holds `None`. There are no methods to call on it. To check whether an optional holds `None`, use comparison operators (`==`, `!=`) or the `is` keyword on the optional variable, not on `None` itself.

---

## Examples

### Boolean Literal Examples

```uranite
Boolean isActive = True
Boolean isDeleted = False
Boolean isEnabled = True
Boolean isEmpty = False

Boolean copy = isActive
Boolean inverted = not isDeleted
```

### Logical Operation Examples

```uranite
Boolean both = True and True
Boolean either = False or True
Boolean negated = not True
Boolean compound = ( True or False ) and not False
```

The variable `both` is `True`. The variable `either` is `True`. The variable `negated` is `False`. The variable `compound` is `True` (parenthesized `True or False` produces `True`, `not False` produces `True`, `True and True` produces `True`).

```uranite
Boolean canAccess = isActive and isVerified
Boolean hasPermission = isAdmin or isModerator
Boolean isBlocked = not isActive or isDeleted
```

The variable `isBlocked` evaluates as `(not isActive) or isDeleted` due to `not` binding tighter than `or`.

### None Handling Examples

```uranite
String? name = None
I64? count = None
Boolean? flag = None

String? greeting = "hello"
I64? total = 42
```

The first three variables hold `None`. The variable `greeting` holds a `String` value "hello". The variable `total` holds an `I64` value 42.

```uranite
if name == None:
    puts( "name is absent" )

if name != None:
    puts( "name is present" )

if name is None:
    puts( "name identity check" )
```

All three patterns are valid ways to check for `None`.

### Practical Usage

A complete example demonstrating boolean logic, optional types, `None` handling, and `Boolean` class methods:

```uranite
from uranite.io.console import puts

public function findUser( String username ) -> String?:
    if username == "admin":
        return "Administrator"
    if username == "guest":
        return "Guest User"
    return None

public function greet( String? name ) -> String:
    if name is None:
        return "Hello, stranger!"
    return "Hello, " + name + "!"

public function checkPermissions( Boolean isAdmin, Boolean isActive ) -> Boolean:
    if not isActive:
        return False
    if isAdmin and isActive:
        return True
    return False

public function describeAccess( Boolean isAdmin, Boolean isModerator, Boolean isBanned ) -> String:
    if isBanned:
        return "access denied: banned"
    if isAdmin or isModerator:
        return "access granted: elevated"
    return "access granted: standard"

public function main() -> I32:
    String? user = findUser( "admin" )
    String message = greet( user )
    puts( message )

    String? unknown = findUser( "nobody" )
    String fallback = greet( unknown )
    puts( fallback )

    Boolean allowed = checkPermissions( True, True )
    Boolean denied = checkPermissions( True, False )

    if allowed:
        puts( "access granted" )
    if not denied:
        puts( "access denied for inactive user" )

    String accessLevel = describeAccess( False, True, False )
    puts( accessLevel )

    Boolean flag = True
    Boolean opposite = flag.negate().getValue()
    puts( "flag: " + flag.toString() )
    puts( "opposite: " + opposite.toString() )

    Boolean conjunction = flag.logicalAnd( opposite )
    Boolean disjunction = flag.logicalOr( opposite )
    puts( "True AND False: " + conjunction.toString() )
    puts( "True OR False: " + disjunction.toString() )

    String? firstName = "Alice"
    String? middleName = None
    String? lastName = "Smith"

    if firstName is None or lastName is None:
        puts( "incomplete name" )
    else:
        String fullName = firstName + " " + lastName
        if middleName is None:
            puts( "full name: " + fullName )
        else:
            puts( "full name: " + firstName + " " + middleName + " " + lastName )

    return 0
```

This example demonstrates optional return types with `None` for absent values, `None` identity checking via `is` and equality checking via `==`, logical operator chains with `and`, `or`, and `not`, boolean method calls on the `Boolean` wrapper (`negate()`, `getValue()`, `toString()`, `logicalAnd()`, `logicalOr()`), conditional branching based on boolean values and `None` checks, and multi-variable `None` handling in conditional chains.
