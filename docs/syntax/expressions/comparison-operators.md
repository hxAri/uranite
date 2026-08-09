# Comparison Operators

---

## Table of Contents

- [Comparison Operators](#comparison-operators)
  - [Table of Contents](#table-of-contents)
  - [Overview](#overview)
  - [Operator Reference](#operator-reference)
  - [Integer Comparisons](#integer-comparisons)
    - [Equality and Inequality](#equality-and-inequality)
    - [Ordering](#ordering)
  - [Float Comparisons](#float-comparisons)
  - [String Comparisons](#string-comparisons)
  - [Boolean Comparisons](#boolean-comparisons)
  - [None Comparisons](#none-comparisons)
  - [Comparisons in Conditions](#comparisons-in-conditions)
  - [Comparisons in Functions](#comparisons-in-functions)
  - [Precedence](#precedence)
  - [Method Reference](#method-reference)
    - [Operators](#operators)
    - [Precedence Table](#precedence-table)
  - [Examples](#examples)
    - [Score Classification](#score-classification)
    - [Min Max and Clamp](#min-max-and-clamp)
    - [String and None Comparisons](#string-and-none-comparisons)
    - [Range and Equality Checks](#range-and-equality-checks)

---

## Overview

Uranite provides six comparison operators that produce `Boolean` results: equality (`==`), inequality (`!=`), less-than (`<`), greater-than (`>`), less-or-equal (`<=`), and greater-or-equal (`>=`). These operators work on numeric types, strings, and optionals with `None`.

---

## Operator Reference

| Operator | Operation | Result |
|---|---|---|
| `==` | Equal to | `True` if values are equal |
| `!=` | Not equal to | `True` if values differ |
| `<` | Less than | `True` if left is smaller |
| `>` | Greater than | `True` if left is larger |
| `<=` | Less than or equal | `True` if left is smaller or equal |
| `>=` | Greater than or equal | `True` if left is larger or equal |

---

## Integer Comparisons

### Equality and Inequality

```uranite
from uranite.io.console import puts

public function main() -> I32:
    Boolean equal = 10 == 10
    Boolean notEqual = 10 != 20
    puts( equal.toString() )
    puts( notEqual.toString() )
    return 0
```

Output:

```
True
True
```

### Ordering

```uranite
from uranite.io.console import puts

public function main() -> I32:
    Boolean less = 5 < 10
    Boolean greater = 10 > 5
    Boolean lessEq = 10 <= 10
    Boolean greaterEq = 10 >= 5
    puts( less.toString() )
    puts( greater.toString() )
    puts( lessEq.toString() )
    puts( greaterEq.toString() )
    return 0
```

Output:

```
True
True
True
True
```

`<=` returns `True` when the left value is less than or equal to the right. `>=` returns `True` when the left value is greater than or equal to the right.

---

## Float Comparisons

All six comparison operators work on floating-point values.

```uranite
from uranite.io.console import puts

public function main() -> I32:
    F64 alpha = 3.14
    F64 beta = 3.14
    Boolean equal = alpha == beta
    puts( equal.toString() )
    Boolean less = 1.5 < 2.5
    puts( less.toString() )
    Boolean greater = 9.9 > 1.1
    puts( greater.toString() )
    return 0
```

Output:

```
True
True
True
```

---

## String Comparisons

Strings are compared by content using `==` and `!=`. Comparison is case-sensitive.

```uranite
from uranite.io.console import puts

public function main() -> I32:
    Boolean same = "hello" == "hello"
    Boolean different = "hello" != "world"
    Boolean caseSensitive = "Hello" == "hello"
    puts( same.toString() )
    puts( different.toString() )
    puts( caseSensitive.toString() )
    return 0
```

Output:

```
True
True
False
```

`"Hello"` and `"hello"` are not equal because comparison is case-sensitive.

---

## Boolean Comparisons

Booleans support equality and inequality.

```uranite
from uranite.io.console import puts

public function main() -> I32:
    Boolean same = True == True
    Boolean different = True != False
    puts( same.toString() )
    puts( different.toString() )
    return 0
```

Output:

```
True
True
```

---

## None Comparisons

Optional values can be compared with `None` using `==` and `!=`.

```uranite
from uranite.io.console import puts

public function main() -> I32:
    ?String name = None
    Boolean isNone = name == None
    puts( isNone.toString() )

    ?String present = "Alice"
    Boolean isPresent = present != None
    puts( isPresent.toString() )
    return 0
```

Output:

```
True
True
```

For `None` checks, the `is` and `is not` keywords are the preferred form (see [Type Identity](../types/type-identity.md)), but `==` and `!=` with `None` also work.

---

## Comparisons in Conditions

Comparison results are `Boolean` and can be used directly in `if` conditions.

```uranite
from uranite.io.console import puts

public function main() -> I32:
    I64 score = 85
    if score >= 90:
        puts( "excellent" )
    elif score >= 70:
        puts( "good" )
    else:
        puts( "needs work" )

    if score != 100:
        puts( "not perfect" )
    return 0
```

Output:

```
good
not perfect
```

---

## Comparisons in Functions

Comparison operators work in function bodies and return expressions.

```uranite
from uranite.io.console import puts

public function max( I64 first, I64 second ) -> I64:
    if first > second:
        return first
    return second

public function min( I64 first, I64 second ) -> I64:
    if first < second:
        return first
    return second

public function main() -> I32:
    I64 bigger = max( 42, 17 )
    I64 smaller = min( 42, 17 )
    puts( bigger.toString() )
    puts( smaller.toString() )
    return 0
```

Output:

```
42
17
```

---

## Precedence

Comparison operators have two precedence levels: relational operators bind tighter than equality operators.

| Precedence | Operators | Description |
|---|---|---|
| Higher | `<`, `>`, `<=`, `>=` | Relational |
| Lower | `==`, `!=` | Equality |

Both levels bind lower than arithmetic operators. `a + b > c` evaluates `a + b` first, then compares the result to `c`.

---

## Method Reference

### Operators

| Operator | Operation | Result Type |
|---|---|---|
| `==` | Equal | `Boolean` |
| `!=` | Not equal | `Boolean` |
| `<` | Less than | `Boolean` |
| `>` | Greater than | `Boolean` |
| `<=` | Less or equal | `Boolean` |
| `>=` | Greater or equal | `Boolean` |

### Precedence Table

| Level | Operators |
|---|---|
| Highest | Arithmetic (`+`, `-`, `*`, `/`, `%`) |
| Middle | Relational (`<`, `>`, `<=`, `>=`) |
| Lower | Equality (`==`, `!=`) |

---

## Examples

### Score Classification

```uranite
from uranite.io.console import puts

public function classify( I64 score ) -> String:
    if score >= 90:
        return "excellent"
    if score >= 80:
        return "good"
    if score >= 70:
        return "average"
    return "below average"

public function main() -> I32:
    puts( classify( 95 ) )
    puts( classify( 85 ) )
    puts( classify( 72 ) )
    puts( classify( 50 ) )
    return 0
```

Output:

```
excellent
good
average
below average
```

### Min Max and Clamp

```uranite
from uranite.io.console import puts

public function maxValue( I64 first, I64 second ) -> I64:
    if first > second:
        return first
    return second

public function minValue( I64 first, I64 second ) -> I64:
    if first < second:
        return first
    return second

public function clamp( I64 value, I64 lower, I64 upper ) -> I64:
    if value < lower:
        return lower
    if value > upper:
        return upper
    return value

public function main() -> I32:
    puts( maxValue( 42, 17 ).toString() )
    puts( minValue( 42, 17 ).toString() )
    puts( clamp( 150, 0, 100 ).toString() )
    puts( clamp( -5, 0, 100 ).toString() )
    puts( clamp( 50, 0, 100 ).toString() )
    return 0
```

Output:

```
42
17
100
0
50
```

### String and None Comparisons

```uranite
from uranite.io.console import puts

public function main() -> I32:
    String first = "apple"
    String second = "apple"
    String third = "banana"

    Boolean same = first == second
    Boolean different = first != third
    puts( same.toString() )
    puts( different.toString() )

    ?String present = "hello"
    ?String absent = None
    Boolean presentNotNone = present != None
    Boolean absentIsNone = absent == None
    puts( presentNotNone.toString() )
    puts( absentIsNone.toString() )
    return 0
```

Output:

```
True
True
True
True
```

### Range and Equality Checks

```uranite
from uranite.io.console import puts

public function isInRange( I64 value, I64 lower, I64 upper ) -> Boolean:
    if value >= lower:
        if value <= upper:
            return True
    return False

public function main() -> I32:
    Boolean inRange = isInRange( 50, 0, 100 )
    Boolean below = isInRange( -10, 0, 100 )
    Boolean above = isInRange( 150, 0, 100 )
    Boolean atEdge = isInRange( 0, 0, 100 )
    puts( inRange.toString() )
    puts( below.toString() )
    puts( above.toString() )
    puts( atEdge.toString() )

    I64 alpha = 10
    I64 beta = 20
    I64 gamma = 10
    Boolean eqAlphaBeta = alpha == beta
    Boolean eqAlphaGamma = alpha == gamma
    Boolean neqAlphaBeta = alpha != beta
    puts( eqAlphaBeta.toString() )
    puts( eqAlphaGamma.toString() )
    puts( neqAlphaBeta.toString() )
    return 0
```

Output:

```
True
False
False
True
False
True
True
```
