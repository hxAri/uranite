# Boolean Type

---

## Table of Contents

- [Boolean Type](#boolean-type)
  - [Table of Contents](#table-of-contents)
  - [Overview](#overview)
  - [Boolean Values](#boolean-values)
  - [Boolean in Conditions](#boolean-in-conditions)
    - [If Statements](#if-statements)
    - [While Loops](#while-loops)
    - [No Implicit Truthiness](#no-implicit-truthiness)
  - [Logical Operators](#logical-operators)
    - [The and Operator](#the-and-operator)
    - [The or Operator](#the-or-operator)
    - [The not Operator](#the-not-operator)
    - [Combining Logical Operators](#combining-logical-operators)
    - [Parenthesized Logic](#parenthesized-logic)
    - [Eager Evaluation](#eager-evaluation)
  - [Comparison Operators](#comparison-operators)
    - [Equality and Inequality](#equality-and-inequality)
    - [Relational Comparisons](#relational-comparisons)
    - [String Comparisons](#string-comparisons)
    - [Float Comparisons](#float-comparisons)
  - [The is Keyword](#the-is-keyword)
    - [None Checks](#none-checks)
    - [is vs Equals](#is-vs-equals)
  - [Boolean Methods](#boolean-methods)
    - [Method Reference](#method-reference)
    - [Using toString](#using-tostring)
    - [Using negate](#using-negate)
    - [Using logicalAnd and logicalOr](#using-logicaland-and-logicalor)
  - [Common Patterns](#common-patterns)
    - [Flag Variables](#flag-variables)
    - [Guard Clauses](#guard-clauses)
    - [Multi-Condition Validation](#multi-condition-validation)
    - [Negated Conditions](#negated-conditions)
  - [Practical Examples](#practical-examples)
    - [Leap Year Detection](#leap-year-detection)
    - [Triangle Validity](#triangle-validity)
    - [Range Checking](#range-checking)
    - [Search with Early Return](#search-with-early-return)

---

## Overview

`Boolean` is the truth value type in Uranite. It has exactly two values: `True` and `False`. Boolean values are the native result of all comparison and logical operations, and they are the only type accepted as conditions in `if` statements and `while` loops.

| Property | Value |
|---|---|
| Type name | `Boolean` |
| Possible values | `True`, `False` |
| Storage size | 1 byte |
| Subclassable | No (`final` class) |

Unlike some languages that allow integers, strings, or pointers to be used directly as conditions, Uranite enforces strict Boolean typing. Every condition must evaluate to a `Boolean` value — there is no implicit "truthiness" conversion.

---

## Boolean Values

`True` and `False` are keyword literals. Note the capital first letter — lowercase `true` and `false` are not valid in Uranite.

```uranite
Boolean isActive = True
Boolean isDeleted = False
```

Boolean variables are commonly used as flags, status indicators, and return values from predicate functions:

```uranite
Boolean hasPermission = True
Boolean isConnected = False
Boolean isReady = True
```

---

## Boolean in Conditions

### If Statements

`Boolean` values drive all conditional logic. Every `if` and `elif` condition must be a `Boolean` expression:

```uranite
from uranite.io.console import puts

public function main() -> I32:
    Boolean hasPermission = True
    if hasPermission:
        puts( "access granted" )
    Boolean isAdmin = False
    if isAdmin:
        puts( "admin mode" )
    else:
        puts( "user mode" )
    return 0
```

This outputs:

```
access granted
user mode
```

### While Loops

`Boolean` values also control `while` loop continuation:

```uranite
from uranite.io.console import puts

public function main() -> I32:
    I64 count = 5
    Boolean keepGoing = True
    while keepGoing:
        puts( count.toString() )
        count = count - 1
        if count == 0:
            keepGoing = False
    puts( "done" )
    return 0
```

This outputs:

```
5
4
3
2
1
done
```

The loop runs as long as `keepGoing` is `True`. When `count` reaches zero, `keepGoing` is set to `False` and the loop exits on the next iteration.

### No Implicit Truthiness

Uranite does **not** perform implicit truthiness conversion. You cannot use an integer, string, float, or optional type directly as a condition. Only `Boolean` values are accepted:

```uranite
I64 count = 42
if count > 0:
    processItems( count )
```

Writing `if count:` would produce a compilation error: "condition must be of type 'bool'". You must write an explicit comparison that produces a `Boolean` result.

Similarly, to check whether a string is empty:

```uranite
String name = ""
Boolean nameEmpty = name.isEmpty()
if nameEmpty:
    handleMissing()
```

And to check whether an optional value is present:

```uranite
?String result = findUser( "alice" )
if result is None:
    handleNotFound()
```

This strictness eliminates an entire class of bugs common in languages with implicit truthiness — accidentally using an integer where a Boolean was intended, or relying on platform-specific truthiness rules.

---

## Logical Operators

Uranite uses keyword-based logical operators: `and`, `or`, and `not`. The symbolic operators `&&`, `||`, and `!` do not exist in Uranite.

### The and Operator

`and` returns `True` only when both operands are `True`:

| Left | Right | Result |
|---|---|---|
| `True` | `True` | `True` |
| `True` | `False` | `False` |
| `False` | `True` | `False` |
| `False` | `False` | `False` |

```uranite
Boolean canAccess = isAuthenticated and hasPermission
```

### The or Operator

`or` returns `True` when at least one operand is `True`:

| Left | Right | Result |
|---|---|---|
| `True` | `True` | `True` |
| `True` | `False` | `True` |
| `False` | `True` | `True` |
| `False` | `False` | `False` |

```uranite
Boolean shouldRetry = isTimeout or isTransient
```

### The not Operator

`not` is a unary prefix operator that inverts a `Boolean` value:

| Operand | Result |
|---|---|
| `True` | `False` |
| `False` | `True` |

```uranite
Boolean isInvalid = not isValid
```

### Combining Logical Operators

Multiple logical operators can be combined in a single expression:

```uranite
from uranite.io.console import puts

public function main() -> I32:
    Boolean valueA = True
    Boolean valueB = False
    Boolean valueC = True
    Boolean andResult = valueA and valueB
    puts( "T and F: " + andResult.toString() )
    Boolean orResult = valueA or valueB
    puts( "T or F: " + orResult.toString() )
    Boolean notResult = not valueA
    puts( "not T: " + notResult.toString() )
    Boolean complex = valueA and valueC
    puts( "T and T: " + complex.toString() )
    return 0
```

This outputs:

```
T and F: False
T or F: True
not T: False
T and T: True
```

### Parenthesized Logic

Use parentheses to control the order of evaluation in complex logical expressions:

```uranite
from uranite.io.console import puts

public function main() -> I32:
    Boolean valueA = True
    Boolean valueB = False
    Boolean valueC = True
    Boolean result = valueA and (valueB or valueC)
    puts( "T and (F or T): " + result.toString() )
    Boolean result2 = (valueA or valueB) and valueC
    puts( "(T or F) and T: " + result2.toString() )
    Boolean result3 = not (valueA and valueB)
    puts( "not (T and F): " + result3.toString() )
    return 0
```

This outputs:

```
T and (F or T): True
(T or F) and T: True
not (T and F): True
```

Without parentheses, `and` and `or` evaluate left to right. Parentheses override this order, ensuring the inner expression is evaluated first.

### Eager Evaluation

Uranite's `and` and `or` operators use **eager evaluation** — both operands are always evaluated, even if the result can be determined from the first operand alone.

```uranite
Boolean result = checkFirst() and checkSecond()
```

In this example, `checkSecond()` is always called, even if `checkFirst()` returns `False`. This differs from languages where `&&` short-circuits and skips the right side.

This matters when the right-hand operand has side effects. If you need short-circuit behavior, use nested `if` statements:

```uranite
Boolean result = False
if checkFirst():
    if checkSecond():
        result = True
```

This ensures `checkSecond()` is only called when `checkFirst()` returns `True`.

---

## Comparison Operators

All comparison operators produce `Boolean` results.

### Equality and Inequality

| Operator | Meaning | Example |
|---|---|---|
| `==` | Equal to | `x == 42` |
| `!=` | Not equal to | `x != 0` |

```uranite
I64 value = 42
Boolean isFortyTwo = value == 42
Boolean isNotZero = value != 0
```

### Relational Comparisons

| Operator | Meaning | Example |
|---|---|---|
| `<` | Less than | `age < 18` |
| `>` | Greater than | `score > 90` |
| `<=` | Less than or equal | `count <= 100` |
| `>=` | Greater than or equal | `price >= 0` |

```uranite
from uranite.io.console import puts

public function main() -> I32:
    I64 age = 25
    Boolean isAdult = age >= 18
    puts( "isAdult: " + isAdult.toString() )
    Boolean isSenior = age >= 65
    puts( "isSenior: " + isSenior.toString() )
    Boolean isYoungAdult = age >= 18 and age < 30
    puts( "isYoungAdult: " + isYoungAdult.toString() )
    return 0
```

This outputs:

```
isAdult: True
isSenior: False
isYoungAdult: True
```

Both operands of a comparison must be the same type. To compare an integer with a float, convert one operand explicitly with `as`.

### String Comparisons

Strings can be compared for equality and inequality:

```uranite
from uranite.io.console import puts

public function main() -> I32:
    String greeting = "hello"
    Boolean isHello = greeting == "hello"
    puts( "isHello: " + isHello.toString() )
    Boolean isEmpty = greeting == ""
    puts( "isEmpty: " + isEmpty.toString() )
    return 0
```

This outputs:

```
isHello: True
isEmpty: False
```

String equality compares the content of the strings, not their memory addresses.

### Float Comparisons

Float comparisons follow IEEE 754 rules. NaN comparisons always return `False` (except `!=`):

```uranite
F64 score = 85.5
Boolean passing = score >= 60.0
```

See the [Floating-Point Types](floating-point-types.md) document for details on NaN behavior in comparisons.

---

## The is Keyword

### None Checks

The `is` keyword is used for identity checks, most commonly to test whether an optional value is `None`:

```uranite
from uranite.io.console import puts

public function main() -> I32:
    ?I64 maybeValue = None
    if maybeValue is None:
        puts( "value is None" )
    ?String name = "Alice"
    if name is None:
        puts( "SHOULD NOT PRINT" )
    else:
        puts( "name has value" )
    return 0
```

This outputs:

```
value is None
name has value
```

### is vs Equals

The `is` keyword checks identity (whether two references point to the same object), while `==` checks value equality (whether two values have the same content). For `None` checks, always use `is`:

```uranite
?String result = findUser( "alice" )
if result is None:
    handleNotFound()
```

---

## Boolean Methods

`Boolean` is a `final` class — it cannot be subclassed. It provides a small set of methods for working with truth values programmatically.

**Important:** When using the result of a method call in another method call, always store intermediate results in variables. Do not chain method calls.

### Method Reference

| Method | Return Type | Description |
|---|---|---|
| `getValue()` | `Boolean` | Returns the underlying truth value |
| `toString()` | `String` | Returns `"True"` or `"False"` |
| `negate()` | `Boolean` | Returns the opposite truth value |
| `logicalAnd( Boolean other )` | `Boolean` | Returns `True` if both are `True` |
| `logicalOr( Boolean other )` | `Boolean` | Returns `True` if either is `True` |
| `hash()` | `I64` | Returns a hash code |

### Using toString

`toString()` converts a Boolean to its string representation — the literal text `"True"` or `"False"`:

```uranite
from uranite.io.console import puts

public function main() -> I32:
    Boolean active = True
    puts( active.toString() )
    Boolean disabled = False
    puts( disabled.toString() )
    return 0
```

This outputs:

```
True
False
```

This is essential when you need to display a Boolean value, since string concatenation with `+` requires both operands to be strings.

### Using negate

`negate()` returns the opposite truth value — `True` becomes `False` and `False` becomes `True`:

```uranite
from uranite.io.console import puts

public function main() -> I32:
    Boolean active = True
    Boolean inactive = active.negate()
    puts( inactive.toString() )
    return 0
```

This outputs `False`. The `negate()` method is equivalent to the `not` operator, but uses method syntax.

### Using logicalAnd and logicalOr

`logicalAnd()` and `logicalOr()` provide method-based alternatives to the `and` and `or` operators:

```uranite
from uranite.io.console import puts

public function main() -> I32:
    Boolean active = True
    Boolean disabled = False
    Boolean conjunction = active.logicalAnd( disabled )
    puts( "logicalAnd: " + conjunction.toString() )
    Boolean disjunction = active.logicalOr( disabled )
    puts( "logicalOr: " + disjunction.toString() )
    return 0
```

This outputs:

```
logicalAnd: False
logicalOr: True
```

These methods behave identically to their operator counterparts. They exist for cases where method syntax reads more clearly.

---

## Common Patterns

### Flag Variables

Boolean flags control program behavior. Set them based on conditions, then use them later:

```uranite
from uranite.io.console import puts

public function main() -> I32:
    I64 errorCount = 0
    Boolean hasErrors = False

    I64 result = processData()
    if result < 0:
        errorCount = errorCount + 1
        hasErrors = True

    if hasErrors:
        puts( "processing completed with errors" )
    else:
        puts( "processing completed successfully" )
    return 0
```

### Guard Clauses

Return early from functions when preconditions are not met:

```uranite
public function processAge( I64 age ) -> String:
    if age < 0:
        return "invalid: negative age"
    if age > 150:
        return "invalid: age too large"
    if age < 18:
        return "minor"
    return "adult"
```

Each `if` statement checks a Boolean condition (the result of a comparison) and returns early if the condition is true.

### Multi-Condition Validation

Combine multiple Boolean conditions to validate complex requirements:

```uranite
from uranite.io.console import puts

public function isValidPassword( String password ) -> Boolean:
    I64 length = password.length()
    if length < 8:
        return False
    if length > 128:
        return False
    Boolean hasContent = password.isEmpty() == False
    return hasContent

public function main() -> I32:
    Boolean valid = isValidPassword( "mypassword123" )
    puts( "valid: " + valid.toString() )
    Boolean tooShort = isValidPassword( "abc" )
    puts( "too short: " + tooShort.toString() )
    return 0
```

### Negated Conditions

Use `not` to invert a Boolean condition. Store the result of a method call before negating:

```uranite
from uranite.io.console import puts

public function main() -> I32:
    I64 value = 42
    Boolean isZero = value == 0
    Boolean isNotZero = not isZero
    puts( "isNotZero: " + isNotZero.toString() )
    if not isZero:
        puts( "value is not zero" )
    return 0
```

This outputs:

```
isNotZero: True
value is not zero
```

The `not` operator can be applied directly in `if` conditions: `if not isZero:` reads naturally and evaluates the negation before branching.

---

## Practical Examples

### Leap Year Detection

A year is a leap year if it is divisible by 4, except for years divisible by 100, unless also divisible by 400:

```uranite
from uranite.io.console import puts

public function isLeapYear( I64 year ) -> Boolean:
    if year % 400 == 0:
        return True
    if year % 100 == 0:
        return False
    return year % 4 == 0

public function main() -> I32:
    Boolean leap2024 = isLeapYear( 2024 )
    puts( "2024 leap: " + leap2024.toString() )
    Boolean leap1900 = isLeapYear( 1900 )
    puts( "1900 leap: " + leap1900.toString() )
    Boolean leap2000 = isLeapYear( 2000 )
    puts( "2000 leap: " + leap2000.toString() )
    return 0
```

This outputs:

```
2024 leap: True
1900 leap: False
2000 leap: True
```

2024 is divisible by 4 (leap). 1900 is divisible by 100 but not 400 (not leap). 2000 is divisible by 400 (leap).

### Triangle Validity

Three sides form a valid triangle if all sides are positive and the sum of any two sides exceeds the third:

```uranite
from uranite.io.console import puts

public function isValidTriangle( F64 sideA, F64 sideB, F64 sideC ) -> Boolean:
    if sideA <= 0.0 or sideB <= 0.0 or sideC <= 0.0:
        return False
    Boolean sumCheck1 = sideA + sideB > sideC
    Boolean sumCheck2 = sideA + sideC > sideB
    Boolean sumCheck3 = sideB + sideC > sideA
    return sumCheck1 and sumCheck2 and sumCheck3

public function main() -> I32:
    Boolean valid = isValidTriangle( 3.0, 4.0, 5.0 )
    puts( "3,4,5 triangle: " + valid.toString() )
    Boolean invalid = isValidTriangle( 1.0, 2.0, 10.0 )
    puts( "1,2,10 triangle: " + invalid.toString() )
    Boolean degenerate = isValidTriangle( 0.0, 5.0, 5.0 )
    puts( "0,5,5 triangle: " + degenerate.toString() )
    return 0
```

This outputs:

```
3,4,5 triangle: True
1,2,10 triangle: False
0,5,5 triangle: False
```

The function uses `or` to check for non-positive sides (returning `False` early) and `and` to verify all three triangle inequality conditions hold.

### Range Checking

Check whether a value falls within a specific range:

```uranite
from uranite.io.console import puts

public function isInRange( I64 value, I64 minimum, I64 maximum ) -> Boolean:
    return value >= minimum and value <= maximum

public function classifyTemperature( F64 celsius ) -> String:
    if celsius < 0.0:
        return "freezing"
    Boolean isCold = celsius < 15.0
    if isCold:
        return "cold"
    Boolean isMild = celsius < 25.0
    if isMild:
        return "mild"
    Boolean isWarm = celsius < 35.0
    if isWarm:
        return "warm"
    return "hot"

public function main() -> I32:
    Boolean inRange = isInRange( 42, 0, 100 )
    puts( "42 in 0-100: " + inRange.toString() )
    Boolean outOfRange = isInRange( 150, 0, 100 )
    puts( "150 in 0-100: " + outOfRange.toString() )
    String classification = classifyTemperature( 22.0 )
    puts( "22C: " + classification )
    String hot = classifyTemperature( 40.0 )
    puts( "40C: " + hot )
    return 0
```

This outputs:

```
42 in 0-100: True
150 in 0-100: False
22C: mild
40C: hot
```

### Search with Early Return

Use Boolean conditions to search through data and return as soon as a match is found:

```uranite
from uranite.io.console import puts

public function containsNegative( I64 valueA, I64 valueB, I64 valueC, I64 valueD ) -> Boolean:
    if valueA < 0:
        return True
    if valueB < 0:
        return True
    if valueC < 0:
        return True
    if valueD < 0:
        return True
    return False

public function allPositive( I64 valueA, I64 valueB, I64 valueC ) -> Boolean:
    Boolean posA = valueA > 0
    Boolean posB = valueB > 0
    Boolean posC = valueC > 0
    return posA and posB and posC

public function main() -> I32:
    Boolean hasNeg = containsNegative( 10, 20, -5, 30 )
    puts( "has negative: " + hasNeg.toString() )
    Boolean noNeg = containsNegative( 10, 20, 30, 40 )
    puts( "no negative: " + noNeg.toString() )
    Boolean allPos = allPositive( 1, 2, 3 )
    puts( "all positive: " + allPos.toString() )
    Boolean notAllPos = allPositive( 1, -2, 3 )
    puts( "not all positive: " + notAllPos.toString() )
    return 0
```

This outputs:

```
has negative: True
no negative: False
all positive: True
not all positive: False
```

The `containsNegative` function demonstrates early return — each `if` checks one value and returns `True` immediately when a negative is found, avoiding unnecessary checks. The `allPositive` function demonstrates combining multiple Boolean results with `and`.
