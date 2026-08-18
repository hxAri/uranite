# Operator Precedence

- [Table of Contents](#table-of-contents)

## Table of Contents

- [Operator Precedence](#operator-precedence)
  - [Table of Contents](#table-of-contents)
  - [Overview](#overview)
  - [Precedence Table](#precedence-table)
  - [Arithmetic Precedence](#arithmetic-precedence)
  - [Parentheses Override](#parentheses-override)
  - [Comparison and Arithmetic](#comparison-and-arithmetic)
  - [Unary Negation](#unary-negation)
  - [Logical Operators](#logical-operators)

## Overview

Operator precedence determines the order in which operators are evaluated in an expression without parentheses. Operators with higher precedence bind more tightly than operators with lower precedence.

When two operators have the same precedence, they associate left-to-right. Parentheses override precedence and force a specific evaluation order.

## Precedence Table

Operators are listed from lowest precedence (evaluated last) to highest precedence (evaluated first).

| Precedence | Operators | Description | Associativity |
|---|---|---|---|
| 1 (lowest) | `or` | Logical OR | Left |
| 2 | `and` | Logical AND | Left |
| 3 | `\|` | Bitwise OR | Left |
| 4 | `^` | Bitwise XOR | Left |
| 5 | `&` | Bitwise AND | Left |
| 6 | `==` `!=` | Equality | Left |
| 7 | `<` `<=` `>` `>=` `is` `in` `instanceof` `subclassof` | Comparison and type checks | Left |
| 8 | `<<` `>>` | Bitwise shift | Left |
| 9 | `+` `-` | Addition, subtraction | Left |
| 10 | `*` `/` `%` | Multiplication, division, modulo | Left |
| 11 | `**` | Exponentiation | Right |
| 12 (highest) | `as` | Type cast | Left |
| (unary) | `-` `not` `~` `*` `&` `addressof` `move` `++` `--` | Unary operators | Right |

Unary operators bind tighter than all binary operators. Among binary operators, `as` binds tightest and `or` binds loosest.

## Arithmetic Precedence

Multiplication, division, and modulo bind tighter than addition and subtraction. In `2 + 3 * 4`, the multiplication evaluates first.

```uranite
package testing

from uranite.io.console import puts

public function main() -> I32:
    I64 result = 2 + 3 * 4
    puts( result )
    return 0
```

The expression evaluates as `2 + (3 * 4)`. Multiplication produces `12`, then addition produces `14`. The program prints `14`.

## Parentheses Override

Parentheses force a specific evaluation order regardless of precedence. Any subexpression enclosed in parentheses evaluates before operators outside the parentheses.

```uranite
package testing

from uranite.io.console import puts

public function main() -> I32:
    I64 result = ( 2 + 3 ) * 4
    puts( result )
    return 0
```

The parentheses force addition before multiplication. The expression evaluates as `5 * 4`. The program prints `20`.

## Comparison and Arithmetic

Comparison operators have lower precedence than arithmetic operators. In `2 + 3 > 4`, the addition evaluates before the comparison.

```uranite
package testing

from uranite.io.console import puts

public function main() -> I32:
    Boolean result = 2 + 3 > 4
    if result:
        puts( "true" )
    else:
        puts( "false" )
    return 0
```

The expression evaluates as `(2 + 3) > 4`, producing `5 > 4`, which is `True`. The program prints `true`.

## Unary Negation

Unary operators bind tighter than all binary operators. In `-3 + 5`, the negation applies to `3` before the addition.

```uranite
package testing

from uranite.io.console import puts

public function main() -> I32:
    I64 result = -3 + 5
    puts( result )
    return 0
```

The expression evaluates as `(-3) + 5`. Unary negation produces `-3`, then addition produces `2`. The program prints `2`.

## Logical Operators

The `and` operator has higher precedence than `or`. In a mixed expression, `and` groups first.

The expression `a or b and c` evaluates as `a or (b and c)`. Use parentheses to make intent explicit when combining logical operators.
