# Void and None Types

---

## Table of Contents

- [Void and None Types](#void-and-none-types)
  - [Table of Contents](#table-of-contents)
  - [Overview](#overview)
  - [Void](#void)
    - [Void as a Return Type](#void-as-a-return-type)
    - [Functions That Return Nothing](#functions-that-return-nothing)
    - [Early Return in Void Functions](#early-return-in-void-functions)
    - [Void Cannot Be Stored](#void-cannot-be-stored)
  - [None](#none)
    - [None as an Absent Value](#none-as-an-absent-value)
    - [None Is Not an Empty Value](#none-is-not-an-empty-value)
  - [Optional Types](#optional-types)
    - [Declaring Optional Variables](#declaring-optional-variables)
    - [Returning Optional Values](#returning-optional-values)
    - [Optional Parameters](#optional-parameters)
    - [Supported Optional Inner Types](#supported-optional-inner-types)
  - [Checking for None](#checking-for-none)
    - [is None](#is-none)
    - [is not None](#is-not-none)
    - [Equality Operators with None](#equality-operators-with-none)
    - [No Implicit Truthiness for Optionals](#no-implicit-truthiness-for-optionals)
  - [Void vs None](#void-vs-none)
  - [Common Patterns](#common-patterns)
    - [Guard Clauses with None](#guard-clauses-with-none)
    - [Default Values](#default-values)
    - [First Non-None Selection](#first-non-none-selection)
    - [Counting Present Values](#counting-present-values)
  - [Practical Examples](#practical-examples)
    - [User Lookup](#user-lookup)
    - [Safe Arithmetic](#safe-arithmetic)
    - [Optional Formatting](#optional-formatting)
    - [Configuration Reader](#configuration-reader)

---

## Overview

Uranite has two distinct concepts for representing "absence":

- **`Void`** represents the absence of a return value. A function that returns `Void` performs an action but produces no result.
- **`None`** represents the absence of a value in an optional context. An optional variable holding `None` means "no value is present."

These are fundamentally different. `Void` is a type-level concept — it tells the compiler that a function produces nothing. `None` is a value — it can be stored in variables, passed as arguments, returned from functions, and compared with other values.

| Property | `Void` | `None` |
|---|---|---|
| Purpose | Mark functions that return nothing | Represent absent values |
| Is a value? | No | Yes |
| Can be stored in a variable? | No | Yes (in optional types) |
| Can be compared? | No | Yes (`is`, `==`, `!=`) |
| Can be passed as an argument? | No | Yes |
| Can be returned from a function? | Yes (as return type) | Yes (as a value) |

---

## Void

### Void as a Return Type

`Void` is used exclusively as a function return type. It declares that the function performs an action without producing a result:

```uranite
from uranite.io.console import puts

public function greet( String name ) -> Void:
    puts( "Hello, " + name + "!" )

public function main() -> I32:
    greet( "Alice" )
    return 0
```

This outputs `Hello, Alice!`. The `greet` function prints a message but returns no value. Its return type `-> Void` makes this explicit.

### Functions That Return Nothing

A `Void` function may omit the `return` statement entirely. The function returns automatically when execution reaches the end of its body:

```uranite
from uranite.io.console import puts

public function logMessage( String message ) -> Void:
    puts( "[LOG] " + message )

public function doNothing() -> Void:
    pass

public function main() -> I32:
    logMessage( "starting" )
    doNothing()
    puts( "done" )
    return 0
```

This outputs:

```
[LOG] starting
done
```

The `doNothing` function uses `pass` as a placeholder body — it does nothing and returns immediately. The `logMessage` function prints its message and returns when the body ends. Neither function needs a `return` statement.

### Early Return in Void Functions

A `Void` function can use `return` without a value to exit early:

```uranite
from uranite.io.console import puts

public function earlyReturn( I64 value ) -> Void:
    if value < 0:
        puts( "negative, returning early" )
        return
    puts( "positive: " + value.toString() )

public function main() -> I32:
    earlyReturn( 5 )
    earlyReturn( -1 )
    earlyReturn( 0 )
    return 0
```

This outputs:

```
positive: 5
negative, returning early
positive: 0
```

When `value` is negative, the function prints the message and returns immediately — the `puts` call after the `if` block is never reached. The bare `return` statement (with no value) is the only valid return form in a `Void` function. Writing `return someValue` in a `Void` function is a compilation error.

### Void Cannot Be Stored

`Void` is not a value type. You cannot declare a variable of type `Void`, pass `Void` as a function argument, or use a `Void` function call as part of an expression:

```uranite
greet( "Alice" )
```

This is a standalone statement. You cannot write `String result = greet( "Alice" )` because `greet` returns `Void` — there is no value to assign.

---

## None

### None as an Absent Value

`None` is a keyword literal that represents the absence of a value. It is used with optional types to indicate that no value is present:

```uranite
?String name = None
?I64 count = None
```

`None` is a specific value that can be stored, compared, and returned. It is not the same as zero, an empty string, or `False` — it means "no value exists here at all."

### None Is Not an Empty Value

`None` is distinct from empty or zero values of any type:

| Value | Type | Meaning |
|---|---|---|
| `None` | Optional | No value exists |
| `""` | `String` | An empty string (a real value) |
| `0` | `I64` | The integer zero (a real value) |
| `False` | `Boolean` | The boolean false (a real value) |

An optional string holding `None` means "there is no string." An optional string holding `""` means "there is a string, and it happens to be empty." These are different states that should be handled differently.

---

## Optional Types

### Declaring Optional Variables

The `?` prefix creates an optional type — a type that can hold either a value of the inner type or `None`:

```uranite
?String maybeName = "Alice"
?I64 maybeCount = None
?Boolean maybeFlag = True
?Char maybeLetter = None
```

The `?T` syntax means "either a value of type `T` or `None`." A `?String` variable can hold any `String` value or `None`. A `?I64` variable can hold any `I64` value or `None`.

Optional variables can be reassigned between having a value and being `None`:

```uranite
?String name = "Alice"
name = None
name = "Bob"
```

### Returning Optional Values

Functions that might not have a result to return use optional return types:

```uranite
from uranite.io.console import puts

public function findUser( String name ) -> ?String:
    if name == "admin":
        return "Administrator"
    return None

public function main() -> I32:
    ?String admin = findUser( "admin" )
    ?String guest = findUser( "guest" )
    if admin is not None:
        puts( "found: " + admin )
    if guest is None:
        puts( "guest not found" )
    return 0
```

This outputs:

```
found: Administrator
guest not found
```

The `findUser` function returns `?String` — it either returns a `String` value or `None`. Callers must check for `None` before using the result.

### Optional Parameters

Functions can accept optional parameters, allowing callers to pass either a value or `None`:

```uranite
from uranite.io.console import puts

public function formatGreeting( String name, ?String title ) -> String:
    if title is not None:
        return "Hello, " + title + " " + name + "!"
    return "Hello, " + name + "!"

public function main() -> I32:
    puts( formatGreeting( "Alice", "Dr." ) )
    puts( formatGreeting( "Bob", None ) )
    return 0
```

This outputs:

```
Hello, Dr. Alice!
Hello, Bob!
```

### Supported Optional Inner Types

Optional types work with `String`, `I64` (and other integer types), `Boolean`, `Char`, and class types. Any type can be wrapped in `?` to make it optional:

```uranite
?String optString = "hello"
?I64 optInt = 42
?Boolean optBool = True
?Char optChar = 'A'
```

---

## Checking for None

### is None

The `is` keyword checks whether an optional value is `None`. This is the primary way to test for absent values:

```uranite
from uranite.io.console import puts

public function main() -> I32:
    ?String name = None
    if name is None:
        puts( "name is absent" )
    ?I64 count = 42
    if count is None:
        puts( "count is absent" )
    else:
        puts( "count has value" )
    return 0
```

This outputs:

```
name is absent
count has value
```

The `is` keyword performs an identity check — it tests whether the value is specifically `None`, not whether it equals some other value.

### is not None

The `is not` construct checks whether an optional value is present (not `None`):

```uranite
from uranite.io.console import puts

public function main() -> I32:
    ?I64 present = 42
    ?I64 absent = None
    if present is not None:
        puts( "present has value" )
    if absent is not None:
        puts( "absent has value" )
    else:
        puts( "absent confirmed None" )
    return 0
```

This outputs:

```
present has value
absent confirmed None
```

`is not None` is the idiomatic way to check whether an optional value is present before using it. It reads naturally as "if this value is not absent, proceed."

### Equality Operators with None

The `==` and `!=` operators also work with `None`:

```uranite
from uranite.io.console import puts

public function main() -> I32:
    ?String name = None
    Boolean eqNone = name == None
    puts( "== None: " + eqNone.toString() )
    Boolean neqNone = name != None
    puts( "!= None: " + neqNone.toString() )
    ?String present = "hello"
    Boolean presentEq = present == None
    puts( "present == None: " + presentEq.toString() )
    return 0
```

This outputs:

```
== None: True
!= None: False
present == None: False
```

Both `is None` and `== None` produce the same result. The `is` keyword is preferred for None checks because it reads more clearly and communicates intent — you are checking identity (whether the value is specifically `None`), not equality.

### No Implicit Truthiness for Optionals

Uranite does **not** allow optional values to be used directly as conditions. You cannot write `if optionalValue:` — the compiler requires explicit `Boolean` conditions:

```uranite
?String name = "Alice"
if name is not None:
    puts( name )
```

Writing `if name:` would produce a compilation error: "condition must be of type 'bool'". Always use explicit `is None` or `is not None` checks. This applies to all optional types — `?String`, `?I64`, `?Boolean`, `?Char`, and any other optional.

---

## Void vs None

Understanding when to use `Void` versus `None`:

**Use `Void`** as a return type when a function performs a side effect (printing, logging, modifying state) and produces no result:

```uranite
public function logError( String message ) -> Void:
    puts( "[ERROR] " + message )
```

**Use `None`** as a return value when a function might or might not find a result:

```uranite
public function findItem( String key ) -> ?String:
    if key == "name":
        return "Alice"
    return None
```

**Use `Void`** when the caller should not expect a value. Use `?T` (returning `None` when absent) when the caller needs to know whether a value was found.

A function returning `Void` and a function returning `?String` that returns `None` are semantically different:
- `Void` says "this function never produces a value — do not try to use its result."
- `?String` returning `None` says "this function tried to produce a value but could not find one."

---

## Common Patterns

### Guard Clauses with None

Check for `None` at the beginning of a function and return early:

```uranite
from uranite.io.console import puts

public function processName( ?String name ) -> String:
    if name is None:
        return "(anonymous)"
    String upper = name.toUpper()
    return upper

public function main() -> I32:
    puts( processName( "alice" ) )
    puts( processName( None ) )
    return 0
```

This outputs:

```
ALICE
(anonymous)
```

The guard clause handles the `None` case immediately, allowing the rest of the function to work with the value without further None checks.

### Default Values

Provide a fallback value when an optional is `None`:

```uranite
from uranite.io.console import puts

public function withDefault( ?String value, String fallback ) -> String:
    if value is None:
        return fallback
    return value

public function main() -> I32:
    puts( withDefault( "hello", "default" ) )
    puts( withDefault( None, "default" ) )
    return 0
```

This outputs:

```
hello
default
```

### First Non-None Selection

Select the first non-None value from a series of optional values:

```uranite
from uranite.io.console import puts

public function firstNonNone( ?String first, ?String second, ?String fallback ) -> String:
    if first is not None:
        return first
    if second is not None:
        return second
    if fallback is not None:
        return fallback
    return "default"

public function main() -> I32:
    String result = firstNonNone( None, "backup", "fallback" )
    puts( result )
    String result2 = firstNonNone( "primary", "backup", "fallback" )
    puts( result2 )
    String result3 = firstNonNone( None, None, None )
    puts( result3 )
    return 0
```

This outputs:

```
backup
primary
default
```

Each `is not None` check tests whether the value is present. The function returns the first value that exists, or the hardcoded "default" string if all three are `None`.

### Counting Present Values

Count how many optional values are present (not `None`):

```uranite
from uranite.io.console import puts

public function countPresent( ?I64 alpha, ?I64 beta, ?I64 gamma ) -> I64:
    I64 count = 0
    if alpha is not None:
        count = count + 1
    if beta is not None:
        count = count + 1
    if gamma is not None:
        count = count + 1
    return count

public function main() -> I32:
    I64 all = countPresent( 1, 2, 3 )
    puts( "all present: " + all.toString() )
    I64 some = countPresent( 1, None, 3 )
    puts( "some present: " + some.toString() )
    I64 none = countPresent( None, None, None )
    puts( "none present: " + none.toString() )
    return 0
```

This outputs:

```
all present: 3
some present: 2
none present: 0
```

---

## Practical Examples

### User Lookup

A function that searches for a user and returns their information if found:

```uranite
from uranite.io.console import puts

public function findUser( String username ) -> ?String:
    if username == "admin":
        return "Administrator (full access)"
    if username == "alice":
        return "Alice Smith (standard)"
    if username == "bob":
        return "Bob Jones (readonly)"
    return None

public function displayUser( String username ) -> Void:
    ?String info = findUser( username )
    if info is None:
        puts( "User '" + username + "' not found" )
    else:
        puts( "Found: " + info )

public function main() -> I32:
    displayUser( "admin" )
    displayUser( "alice" )
    displayUser( "unknown" )
    return 0
```

This outputs:

```
Found: Administrator (full access)
Found: Alice Smith (standard)
User 'unknown' not found
```

The `findUser` function returns `?String` — either user information or `None`. The `displayUser` function checks for `None` and handles both cases.

### Safe Arithmetic

Perform arithmetic operations that return `None` on invalid inputs:

```uranite
from uranite.io.console import puts

public function safeDivide( I64 numerator, I64 denominator ) -> ?I64:
    if denominator == 0:
        return None
    return numerator / denominator

public function safeAdd( ?I64 left, ?I64 right ) -> ?I64:
    if left is None:
        return None
    if right is None:
        return None
    return left + right

public function main() -> I32:
    ?I64 result1 = safeDivide( 10, 3 )
    if result1 is not None:
        puts( "10 / 3 = " + result1.toString() )
    ?I64 result2 = safeDivide( 10, 0 )
    if result2 is None:
        puts( "10 / 0 = undefined" )
    ?I64 sum = safeAdd( result1, safeDivide( 6, 2 ) )
    if sum is not None:
        puts( "sum = " + sum.toString() )
    ?I64 noSum = safeAdd( result1, result2 )
    if noSum is None:
        puts( "cannot add with None" )
    return 0
```

This outputs:

```
10 / 3 = 3
10 / 0 = undefined
sum = 6
cannot add with None
```

`safeDivide` returns `None` instead of crashing on division by zero. `safeAdd` propagates `None` — if either operand is `None`, the result is `None`. This pattern prevents errors from cascading through a computation chain.

### Optional Formatting

Format values with placeholders for absent data:

```uranite
from uranite.io.console import puts

public function formatField( String label, ?String value ) -> String:
    if value is None:
        return label + ": <not provided>"
    return label + ": " + value

public function formatRecord( ?String name, ?String email, ?String phone ) -> Void:
    puts( formatField( "Name", name ) )
    puts( formatField( "Email", email ) )
    puts( formatField( "Phone", phone ) )
    puts( "---" )

public function main() -> I32:
    formatRecord( "Alice", "alice@example.com", "555-1234" )
    formatRecord( "Bob", None, "555-5678" )
    formatRecord( None, None, None )
    return 0
```

This outputs:

```
Name: Alice
Email: alice@example.com
Phone: 555-1234
---
Name: Bob
Email: <not provided>
Phone: 555-5678
---
Name: <not provided>
Email: <not provided>
Phone: <not provided>
---
```

Each field is formatted independently. Missing values display a placeholder instead of crashing or producing empty output.

### Configuration Reader

A configuration system that returns `None` for missing keys:

```uranite
from uranite.io.console import puts

public function getConfig( String key ) -> ?String:
    if key == "host":
        return "localhost"
    if key == "port":
        return "8080"
    if key == "name":
        return "myapp"
    return None

public function getConfigOrDefault( String key, String fallback ) -> String:
    ?String value = getConfig( key )
    if value is None:
        return fallback
    return value

public function main() -> I32:
    String host = getConfigOrDefault( "host", "0.0.0.0" )
    puts( "host: " + host )
    String port = getConfigOrDefault( "port", "3000" )
    puts( "port: " + port )
    String database = getConfigOrDefault( "database", "sqlite" )
    puts( "database: " + database )
    ?String missing = getConfig( "secret" )
    if missing is None:
        puts( "secret: not configured" )
    return 0
```

This outputs:

```
host: localhost
port: 8080
database: sqlite
secret: not configured
```

The `getConfig` function returns `?String` — present for known keys, `None` for unknown keys. The `getConfigOrDefault` wrapper provides a fallback value when the key is missing, while `getConfig` alone lets the caller distinguish between "key found" and "key not found."
