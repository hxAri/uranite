# Bitwise Operators

---

## Table of Contents

- [Bitwise Operators](#bitwise-operators)
  - [Table of Contents](#table-of-contents)
  - [Overview](#overview)
  - [Operator Reference](#operator-reference)
  - [Bitwise AND](#bitwise-and)
  - [Bitwise OR](#bitwise-or)
  - [Bitwise XOR](#bitwise-xor)
  - [Bitwise NOT](#bitwise-not)
  - [Left Shift](#left-shift)
  - [Right Shift](#right-shift)
  - [Precedence](#precedence)
  - [Bitwise Operations in Functions](#bitwise-operations-in-functions)
  - [Method Reference](#method-reference)
    - [Operators](#operators)
    - [Precedence Table](#precedence-table)
  - [Examples](#examples)
    - [Bit Flag Operations](#bit-flag-operations)
    - [Shift Operations](#shift-operations)
    - [Bit Manipulation Functions](#bit-manipulation-functions)
    - [Bitwise Logic](#bitwise-logic)

---

## Overview

Uranite provides six bitwise operators for manipulating individual bits within integer values: AND (`&`), OR (`|`), XOR (`^`), NOT (`~`), left shift (`<<`), and right shift (`>>`). All bitwise operators require integer operands and produce integer results.

---

## Operator Reference

| Operator | Operation | Description |
|---|---|---|
| `&` | AND | Sets each bit to 1 if both bits are 1 |
| `\|` | OR | Sets each bit to 1 if either bit is 1 |
| `^` | XOR | Sets each bit to 1 if exactly one bit is 1 |
| `~` | NOT | Inverts all bits |
| `<<` | Left shift | Shifts bits left, filling with zeros |
| `>>` | Right shift | Shifts bits right (arithmetic shift) |

---

## Bitwise AND

The `&` operator produces a value where each bit is 1 only if both corresponding bits are 1.

```uranite
from uranite.io.console import puts

public function main() -> I32:
    I64 result = 12 & 10
    puts( result.toString() )
    return 0
```

Output:

```
8
```

Binary: `1100 & 1010 = 1000` which is `8`.

---

## Bitwise OR

The `|` operator produces a value where each bit is 1 if either corresponding bit is 1.

```uranite
from uranite.io.console import puts

public function main() -> I32:
    I64 result = 12 | 10
    puts( result.toString() )
    return 0
```

Output:

```
14
```

Binary: `1100 | 1010 = 1110` which is `14`.

---

## Bitwise XOR

The `^` operator produces a value where each bit is 1 if exactly one of the corresponding bits is 1.

```uranite
from uranite.io.console import puts

public function main() -> I32:
    I64 result = 12 ^ 10
    puts( result.toString() )
    return 0
```

Output:

```
6
```

Binary: `1100 ^ 1010 = 0110` which is `6`.

---

## Bitwise NOT

The `~` operator inverts all bits.

```uranite
from uranite.io.console import puts

public function main() -> I32:
    I64 notZero = ~0
    puts( notZero.toString() )
    I64 notFive = ~5
    puts( notFive.toString() )
    return 0
```

Output:

```
-1
-6
```

`~0` inverts all bits, producing `-1` in two's complement. `~5` inverts the bits of `5` (`...0101`), producing `-6` (`...1010`).

---

## Left Shift

The `<<` operator shifts bits to the left, filling vacated positions with zeros. Each left shift by 1 doubles the value.

```uranite
from uranite.io.console import puts

public function main() -> I32:
    I64 shifted = 1 << 4
    puts( shifted.toString() )
    return 0
```

Output:

```
16
```

`1 << 4` shifts `1` left by 4 positions: `0001` becomes `10000` which is `16`.

---

## Right Shift

The `>>` operator shifts bits to the right. Each right shift by 1 halves the value (with truncation).

```uranite
from uranite.io.console import puts

public function main() -> I32:
    I64 result = 32 >> 2
    puts( result.toString() )
    return 0
```

Output:

```
8
```

`32 >> 2` shifts `32` right by 2 positions: `100000` becomes `1000` which is `8`.

---

## Precedence

Bitwise operators have specific precedence levels relative to other operators.

| Precedence | Operators | Description |
|---|---|---|
| Higher | `<<`, `>>` | Shift operators |
| Middle | `&` | Bitwise AND |
| Middle | `^` | Bitwise XOR |
| Lower | `\|` | Bitwise OR |

Shift operators bind tighter than AND, which binds tighter than XOR, which binds tighter than OR. Use parentheses to override precedence when combining bitwise operators.

---

## Bitwise Operations in Functions

Bitwise operators work in function parameters and return values.

```uranite
from uranite.io.console import puts

public function hasBit( I64 value, I64 position ) -> Boolean:
    I64 mask = 1 << position
    I64 result = value & mask
    if result > 0:
        return True
    return False

public function setBit( I64 value, I64 position ) -> I64:
    I64 mask = 1 << position
    return value | mask

public function main() -> I32:
    I64 flags = 0
    flags = setBit( flags, 0 )
    flags = setBit( flags, 2 )

    Boolean bit0 = hasBit( flags, 0 )
    Boolean bit1 = hasBit( flags, 1 )
    Boolean bit2 = hasBit( flags, 2 )
    puts( bit0.toString() )
    puts( bit1.toString() )
    puts( bit2.toString() )
    return 0
```

Output:

```
True
False
True
```

---

## Method Reference

### Operators

| Operator | Operation | Operands |
|---|---|---|
| `&` | Bitwise AND | Integer, Integer |
| `\|` | Bitwise OR | Integer, Integer |
| `^` | Bitwise XOR | Integer, Integer |
| `~` | Bitwise NOT | Integer (unary) |
| `<<` | Left shift | Integer, Integer |
| `>>` | Right shift | Integer, Integer |

### Precedence Table

| Level | Operators |
|---|---|
| Highest | `~` (unary prefix) |
| High | `<<`, `>>` |
| Medium | `&` |
| Medium | `^` |
| Low | `\|` |

---

## Examples

### Bit Flag Operations

```uranite
from uranite.io.console import puts

public function main() -> I32:
    I64 permissions = 7
    I64 readMask = 4
    I64 writeMask = 2
    I64 execMask = 1

    I64 hasRead = permissions & readMask
    I64 hasWrite = permissions & writeMask
    I64 hasExec = permissions & execMask

    puts( hasRead.toString() )
    puts( hasWrite.toString() )
    puts( hasExec.toString() )

    I64 noWrite = permissions & ~writeMask
    puts( noWrite.toString() )

    I64 addRead = 0 | readMask
    puts( addRead.toString() )
    return 0
```

Output:

```
4
2
1
5
4
```

### Shift Operations

```uranite
from uranite.io.console import puts

public function main() -> I32:
    I64 shifted = 1 << 0
    puts( shifted.toString() )
    I64 shifted1 = 1 << 1
    puts( shifted1.toString() )
    I64 shifted2 = 1 << 2
    puts( shifted2.toString() )
    I64 shifted3 = 1 << 3
    puts( shifted3.toString() )
    I64 shifted4 = 1 << 4
    puts( shifted4.toString() )

    I64 large = 256
    I64 halved = large >> 1
    puts( halved.toString() )
    I64 quartered = large >> 2
    puts( quartered.toString() )
    return 0
```

Output:

```
1
2
4
8
16
128
64
```

### Bit Manipulation Functions

```uranite
from uranite.io.console import puts

public function hasBit( I64 value, I64 position ) -> Boolean:
    I64 mask = 1 << position
    I64 result = value & mask
    if result > 0:
        return True
    return False

public function setBit( I64 value, I64 position ) -> I64:
    I64 mask = 1 << position
    return value | mask

public function clearBit( I64 value, I64 position ) -> I64:
    I64 mask = ~(1 << position)
    return value & mask

public function toggleBit( I64 value, I64 position ) -> I64:
    I64 mask = 1 << position
    return value ^ mask

public function main() -> I32:
    I64 flags = 0

    flags = setBit( flags, 0 )
    flags = setBit( flags, 2 )
    puts( flags.toString() )

    Boolean bit0 = hasBit( flags, 0 )
    Boolean bit1 = hasBit( flags, 1 )
    Boolean bit2 = hasBit( flags, 2 )
    puts( bit0.toString() )
    puts( bit1.toString() )
    puts( bit2.toString() )

    flags = clearBit( flags, 0 )
    puts( flags.toString() )

    flags = toggleBit( flags, 1 )
    puts( flags.toString() )
    return 0
```

Output:

```
5
True
False
True
4
6
```

### Bitwise Logic

```uranite
from uranite.io.console import puts

public function main() -> I32:
    I64 alpha = 170
    I64 beta = 85
    I64 andResult = alpha & beta
    I64 orResult = alpha | beta
    I64 xorResult = alpha ^ beta
    puts( andResult.toString() )
    puts( orResult.toString() )
    puts( xorResult.toString() )

    I64 notZero = ~0
    puts( notZero.toString() )

    I64 notOne = ~1
    puts( notOne.toString() )
    return 0
```

Output:

```
0
255
255
-1
-2
```
