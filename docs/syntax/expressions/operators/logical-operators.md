# Logical Operators

---

## Table of Contents

- [Logical Operators](#logical-operators)
  - [Table of Contents](#table-of-contents)
  - [Overview](#overview)
  - [Operator Reference](#operator-reference)
  - [The and Operator](#the-and-operator)
  - [The or Operator](#the-or-operator)
  - [The not Operator](#the-not-operator)
  - [Combining with Comparisons](#combining-with-comparisons)
  - [Logical Operators in Functions](#logical-operators-in-functions)
  - [Grouping with Parentheses](#grouping-with-parentheses)
  - [Evaluation Behavior](#evaluation-behavior)
  - [Precedence](#precedence)
  - [Method Reference](#method-reference)
    - [Operators](#operators)
    - [Precedence Table](#precedence-table)
  - [Examples](#examples)
    - [Eligibility Logic](#eligibility-logic)
    - [Weather Conditions](#weather-conditions)
    - [Truth Tables](#truth-tables)
    - [Multi-Condition Classification](#multi-condition-classification)

---

## Overview

Uranite provides three logical operators: `and`, `or`, and `not`. These are keyword-based operators — Uranite does not use `&&`, `||`, or `!`. All logical operators work on `Boolean` operands and produce `Boolean` results.

---

## Operator Reference

| Operator | Operation | Description |
|---|---|---|
| `and` | Logical AND | `True` if both operands are `True` |
| `or` | Logical OR | `True` if either operand is `True` |
| `not` | Logical NOT | Inverts a `Boolean` value |

---

## The and Operator

The `and` operator returns `True` only when both operands are `True`.

```uranite
from uranite.io.console import puts

public function main() -> I32:
    Boolean both = True and True
    Boolean one = True and False
    Boolean neither = False and False
    puts( both.toString() )
    puts( one.toString() )
    puts( neither.toString() )
    return 0
```

Output:

```
True
False
False
```

---

## The or Operator

The `or` operator returns `True` when at least one operand is `True`.

```uranite
from uranite.io.console import puts

public function main() -> I32:
    Boolean either = True or False
    Boolean both = True or True
    Boolean neither = False or False
    puts( either.toString() )
    puts( both.toString() )
    puts( neither.toString() )
    return 0
```

Output:

```
True
True
False
```

---

## The not Operator

The `not` operator inverts a `Boolean` value.

```uranite
from uranite.io.console import puts

public function main() -> I32:
    Boolean negTrue = not True
    Boolean negFalse = not False
    puts( negTrue.toString() )
    puts( negFalse.toString() )
    return 0
```

Output:

```
False
True
```

---

## Combining with Comparisons

Logical operators combine comparison results to express compound conditions.

```uranite
from uranite.io.console import puts

public function main() -> I32:
    I64 age = 25
    I64 score = 80
    if age >= 18 and score >= 70:
        puts( "qualified" )
    if age < 18 or score < 50:
        puts( "should not print" )
    else:
        puts( "not disqualified" )
    if not (age < 18):
        puts( "adult" )
    return 0
```

Output:

```
qualified
not disqualified
adult
```

The `and` operator requires both conditions to hold. The `or` operator requires at least one. The `not` operator inverts the result of a parenthesized expression.

---

## Logical Operators in Functions

Logical expressions can be used in `return` statements and as function arguments.

```uranite
from uranite.io.console import puts

public function isEligible( I64 age, Boolean hasLicense ) -> Boolean:
    return age >= 18 and hasLicense

public function main() -> I32:
    Boolean result1 = isEligible( 25, True )
    Boolean result2 = isEligible( 15, True )
    Boolean result3 = isEligible( 25, False )
    puts( result1.toString() )
    puts( result2.toString() )
    puts( result3.toString() )
    return 0
```

Output:

```
True
False
False
```

---

## Grouping with Parentheses

Parentheses control the evaluation order of logical expressions.

```uranite
from uranite.io.console import puts

public function main() -> I32:
    Boolean tooHot = False
    Boolean tooCold = False
    if not tooHot and not tooCold:
        puts( "comfortable" )
    return 0
```

Output:

```
comfortable
```

Without parentheses, `not` applies to the immediately following operand. Use parentheses to group more complex expressions: `not (a or b)`.

---

## Evaluation Behavior

Both sides of `and` and `or` are always evaluated. Uranite does not short-circuit logical operators — both operands are computed regardless of the first operand's value.

---

## Precedence

| Precedence | Operator | Description |
|---|---|---|
| Highest | `not` | Unary negation |
| Middle | `and` | Logical conjunction |
| Lowest | `or` | Logical disjunction |

`and` binds tighter than `or`. The expression `a or b and c` is evaluated as `a or (b and c)`.

All logical operators bind lower than comparison operators. `x > 5 and y < 10` is evaluated as `(x > 5) and (y < 10)`.

---

## Method Reference

### Operators

| Operator | Operands | Result |
|---|---|---|
| `and` | `Boolean`, `Boolean` | `Boolean` |
| `or` | `Boolean`, `Boolean` | `Boolean` |
| `not` | `Boolean` (unary) | `Boolean` |

### Precedence Table

| Level | Operators |
|---|---|
| Highest | Arithmetic, Comparisons |
| High | `not` |
| Medium | `and` |
| Low | `or` |

---

## Examples

### Eligibility Logic

```uranite
from uranite.io.console import puts

public function isEligible( I64 age, Boolean hasId ) -> Boolean:
    return age >= 18 and hasId

public function canEnter( Boolean isVip, Boolean hasTicket ) -> Boolean:
    return isVip or hasTicket

public function main() -> I32:
    puts( isEligible( 25, True ).toString() )
    puts( isEligible( 15, True ).toString() )
    puts( isEligible( 25, False ).toString() )
    puts( canEnter( False, True ).toString() )
    puts( canEnter( False, False ).toString() )
    return 0
```

Output:

```
True
False
False
True
False
```

### Weather Conditions

```uranite
from uranite.io.console import puts

public function main() -> I32:
    I64 temperature = 22
    I64 humidity = 45

    if temperature >= 18 and temperature <= 28:
        puts( "comfortable temperature" )
    if humidity >= 30 and humidity <= 60:
        puts( "comfortable humidity" )
    if temperature < 0 or temperature > 40:
        puts( "extreme temperature" )
    else:
        puts( "normal temperature" )

    Boolean tooHot = temperature > 30
    Boolean tooCold = temperature < 10
    if not tooHot and not tooCold:
        puts( "just right" )
    return 0
```

Output:

```
comfortable temperature
comfortable humidity
normal temperature
just right
```

### Truth Tables

```uranite
from uranite.io.console import puts

public function main() -> I32:
    Boolean andTT = True and True
    Boolean andTF = True and False
    Boolean andFT = False and True
    Boolean andFF = False and False
    puts( andTT.toString() )
    puts( andTF.toString() )
    puts( andFT.toString() )
    puts( andFF.toString() )

    Boolean orTT = True or True
    Boolean orTF = True or False
    Boolean orFT = False or True
    Boolean orFF = False or False
    puts( orTT.toString() )
    puts( orTF.toString() )
    puts( orFT.toString() )
    puts( orFF.toString() )

    Boolean notT = not True
    Boolean notF = not False
    puts( notT.toString() )
    puts( notF.toString() )
    return 0
```

Output:

```
True
False
False
False
True
True
True
False
False
True
```

### Multi-Condition Classification

```uranite
from uranite.io.console import puts

public function classify( I64 age, Boolean isStudent, Boolean isVeteran ) -> String:
    if age < 12:
        return "child"
    if age >= 12 and age < 18:
        return "youth"
    if isStudent or isVeteran:
        return "discounted"
    return "standard"

public function main() -> I32:
    puts( classify( 8, False, False ) )
    puts( classify( 15, False, False ) )
    puts( classify( 22, True, False ) )
    puts( classify( 40, False, True ) )
    puts( classify( 30, False, False ) )
    return 0
```

Output:

```
child
youth
discounted
discounted
standard
```
