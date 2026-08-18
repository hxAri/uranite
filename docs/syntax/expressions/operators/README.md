# Operators

Operators combine values into expressions. Uranite organizes operators into four categories: arithmetic, comparison, logical, bitwise, and unary. Each binary operator dispatches through an operator interface, so user-defined types can participate in operator expressions by implementing the corresponding interface.

---

## Table of Contents

- [Operators](#operators)
  - [Table of Contents](#table-of-contents)
  - [Overview](#overview)
  - [At a Glance](#at-a-glance)
  - [Subpage Index](#subpage-index)

---

## Overview

**Arithmetic operators** (`+`, `-`, `*`, `/`, `%`) perform numeric computation. Integer arithmetic uses wrapping behavior for addition, subtraction, and multiplication. Division and modulo include automatic zero-checks that raise `ZeroDivisionError` at runtime.

**Comparison operators** (`==`, `!=`, `<`, `>`, `<=`, `>=`) compare two values and return a `Boolean` result. The `is` keyword tests identity: whether two references point to the same object, or whether a value `is None`.

**Logical operators** (`and`, `or`, `not`) operate on `Boolean` values exclusively. Both `and` and `or` evaluate both operands — there is no short-circuit evaluation. The `not` keyword is a unary prefix operator that inverts a `Boolean`.

**Bitwise operators** (`&`, `|`, `^`, `~`) manipulate individual bits of integer values. The `&` operator performs bitwise AND, `|` performs bitwise OR, `^` performs bitwise XOR, and `~` performs bitwise complement.

**Unary operators** include negation (`-`) for numeric types, bitwise complement (`~`) for integers, and logical negation (`not`) for booleans.

---

## At a Glance

```uranite
I64 sum = 10 + 5
I64 remainder = 10 % 3

Boolean equal = sum == 15
Boolean greater = sum > 10

Boolean both = equal and greater
Boolean negated = not equal

I64 negative = -sum

I64 masked = 0xFF & 0x0F
```

---

## Subpage Index

| Document | Description |
|---|---|
| [Arithmetic Operators](arithmetic-operators.md) | Addition, subtraction, multiplication, division, and modulo with wrapping and zero-check semantics. |
| [Bitwise Operators](bitwise-operators.md) | Bitwise AND (`&`), OR (`\|`), XOR (`^`), complement (`~`), and shift operators on integer types. |
| [Comparison Operators](comparison-operators.md) | Equality (`==`, `!=`), relational (`<`, `>`, `<=`, `>=`), and identity (`is`) operators. |
| [Logical Operators](logical-operators.md) | Boolean `and`, `or`, and `not` operators with full (non-short-circuit) evaluation. |
| [Unary Operators](unary-operators.md) | Prefix negation (`-`), bitwise complement (`~`), and logical negation (`not`). |
