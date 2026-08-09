# String Formatting

---

## Table of Contents

- [String Formatting](#string-formatting)
  - [Table of Contents](#table-of-contents)
  - [Overview](#overview)
  - [Basic Placeholders](#basic-placeholders)
    - [Auto-Indexed Placeholders](#auto-indexed-placeholders)
    - [Multiple Placeholders](#multiple-placeholders)
  - [Explicit Index Placeholders](#explicit-index-placeholders)
  - [Named Placeholders](#named-placeholders)
  - [Type Specifiers](#type-specifiers)
    - [Float Precision](#float-precision)
    - [Integer Bases](#integer-bases)
    - [Scientific Notation](#scientific-notation)
  - [Width and Alignment](#width-and-alignment)
    - [Right Alignment](#right-alignment)
    - [Left Alignment](#left-alignment)
    - [Center Alignment](#center-alignment)
    - [Integer Width](#integer-width)
  - [Boolean Formatting](#boolean-formatting)
  - [Escaping Braces](#escaping-braces)
  - [Formatting in Functions](#formatting-in-functions)
  - [Format vs Concatenation](#format-vs-concatenation)
  - [Method Reference](#method-reference)
    - [Placeholder Syntax](#placeholder-syntax)
    - [Type Specifiers Reference](#type-specifiers-reference)
    - [Alignment Specifiers](#alignment-specifiers)
  - [Examples](#examples)
    - [Basic Format Patterns](#basic-format-patterns)
    - [Number Formatting](#number-formatting)
    - [Functions with Formatting](#functions-with-formatting)
    - [Width Alignment and Escaping](#width-alignment-and-escaping)

---

## Overview

Uranite formats strings using the `format` method on `String` values. The method uses curly-brace placeholders that are replaced with argument values at runtime. Uranite has no string interpolation syntax — all formatting goes through `format`.

```uranite
from uranite.io.console import puts

public function main() -> I32:
    String message = "Hello, {}!".format( "Alice" )
    puts( message )
    return 0
```

Output:

```
Hello, Alice!
```

---

## Basic Placeholders

### Auto-Indexed Placeholders

Empty braces `{}` are filled in order. The first `{}` receives the first argument, the second `{}` receives the second argument, and so on.

```uranite
from uranite.io.console import puts

public function main() -> I32:
    String greeting = "Hello, {}!".format( "Alice" )
    puts( greeting )
    return 0
```

Output:

```
Hello, Alice!
```

### Multiple Placeholders

Multiple `{}` placeholders map to arguments in order.

```uranite
from uranite.io.console import puts

public function main() -> I32:
    String message = "Name: {}, Age: {}".format( "Bob", 25 )
    puts( message )
    return 0
```

Output:

```
Name: Bob, Age: 25
```

---

## Explicit Index Placeholders

Use `{0}`, `{1}`, etc. to reference arguments by position. This allows reordering or reusing arguments.

```uranite
from uranite.io.console import puts

public function main() -> I32:
    String reordered = "{1} before {0}".format( "second", "first" )
    puts( reordered )
    return 0
```

Output:

```
first before second
```

---

## Named Placeholders

Use `{name}` to reference keyword arguments by name.

```uranite
from uranite.io.console import puts

public function main() -> I32:
    String result = "Hello, {name}! You are {age}.".format( name="Charlie", age=30 )
    puts( result )
    return 0
```

Output:

```
Hello, Charlie! You are 30.
```

---

## Type Specifiers

Format specifiers follow a colon inside the placeholder: `{:spec}` or `{index:spec}`.

### Float Precision

Use `:.Nf` to control decimal places for floating-point values.

```uranite
from uranite.io.console import puts

public function main() -> I32:
    F64 pi = 3.14159265
    String twoDecimal = "{:.2f}".format( pi )
    puts( twoDecimal )
    String fourDecimal = "{:.4f}".format( pi )
    puts( fourDecimal )
    return 0
```

Output:

```
3.14
3.1416
```

### Integer Bases

Format integers in different number bases.

```uranite
from uranite.io.console import puts

public function main() -> I32:
    I64 number = 255
    String decimal = "{:d}".format( number )
    puts( decimal )
    String hexLower = "{:x}".format( number )
    puts( hexLower )
    String hexUpper = "{:X}".format( number )
    puts( hexUpper )
    String octal = "{:o}".format( number )
    puts( octal )
    String binary = "{:b}".format( number )
    puts( binary )
    return 0
```

Output:

```
255
ff
FF
377
11111111
```

| Specifier | Base | Description |
|---|---|---|
| `d` | 10 | Decimal |
| `x` | 16 | Lowercase hexadecimal |
| `X` | 16 | Uppercase hexadecimal |
| `o` | 8 | Octal |
| `b` | 2 | Binary |

### Scientific Notation

Use `:e` for scientific notation on floating-point values.

```uranite
from uranite.io.console import puts

public function main() -> I32:
    F64 large = 123456.789
    String sci = "{:e}".format( large )
    puts( sci )
    return 0
```

Output:

```
1.234568e+05
```

---

## Width and Alignment

Specify a minimum field width with `:N` or `:>N` (right-align), `:<N` (left-align), `:^N` (center-align).

### Right Alignment

Right alignment is the default. Use `:>N` or just `:N`.

```uranite
from uranite.io.console import puts

public function main() -> I32:
    String result = "{:>10}".format( "right" )
    puts( result )
    return 0
```

Output:

```
     right
```

The value is padded with spaces on the left to reach width 10.

### Left Alignment

Use `:<N` to left-align.

```uranite
from uranite.io.console import puts

public function main() -> I32:
    String result = "{:<10}".format( "left" )
    puts( result )
    return 0
```

Output:

```
left      
```

### Center Alignment

Use `:^N` to center-align.

```uranite
from uranite.io.console import puts

public function main() -> I32:
    String result = "{:^10}".format( "mid" )
    puts( result )
    return 0
```

Output:

```
   mid    
```

### Integer Width

Width specifiers work with integers too.

```uranite
from uranite.io.console import puts

public function main() -> I32:
    I64 number = 42
    String padded = "{:>8d}".format( number )
    puts( padded )
    return 0
```

Output:

```
      42
```

---

## Boolean Formatting

Boolean values format as `True` or `False`.

```uranite
from uranite.io.console import puts

public function main() -> I32:
    Boolean active = True
    String result = "Active: {}".format( active )
    puts( result )

    Boolean done = False
    String result2 = "Done: {}".format( done )
    puts( result2 )
    return 0
```

Output:

```
Active: True
Done: False
```

---

## Escaping Braces

Use `{{` and `}}` to produce literal `{` and `}` characters in the output.

```uranite
from uranite.io.console import puts

public function main() -> I32:
    String result = "Use {{}} for placeholders: {}".format( "value" )
    puts( result )
    return 0
```

Output:

```
Use {} for placeholders: value
```

---

## Formatting in Functions

The `format` method works in function bodies and return expressions.

```uranite
from uranite.io.console import puts

public function formatRecord( String name, I64 score ) -> String:
    return "Player {} scored {} points".format( name, score )

public function main() -> I32:
    String record1 = formatRecord( "Alice", 95 )
    puts( record1 )
    String record2 = formatRecord( "Bob", 78 )
    puts( record2 )
    return 0
```

Output:

```
Player Alice scored 95 points
Player Bob scored 78 points
```

---

## Format vs Concatenation

For multi-value string construction, `format` is more concise than concatenation. Concatenation requires `.toString()` calls and intermediate variables for non-string values. `format` handles type conversion automatically.

**Concatenation approach**:

```uranite
I64 score = 95
String scoreStr = score.toString()
String message = "Score: " + scoreStr
```

**Format approach**:

```uranite
I64 score = 95
String message = "Score: {}".format( score )
```

Both produce the same result. Use `format` when building strings with multiple values or when readability matters.

---

## Method Reference

### Placeholder Syntax

| Syntax | Description |
|---|---|
| `{}` | Auto-indexed, filled in order |
| `{0}`, `{1}` | Explicit index |
| `{name}` | Named, matched to keyword argument |
| `{{` | Literal `{` in output |
| `}}` | Literal `}` in output |

### Type Specifiers Reference

| Specifier | Description | Applies To |
|---|---|---|
| `d` | Decimal integer | Integer |
| `f` | Fixed-point | Float |
| `e` | Scientific notation | Float |
| `x` | Lowercase hex | Integer |
| `X` | Uppercase hex | Integer |
| `o` | Octal | Integer |
| `b` | Binary | Integer |
| `.Nf` | Float with N decimal places | Float |

### Alignment Specifiers

| Syntax | Description |
|---|---|
| `{:>N}` | Right-align in width N (default) |
| `{:<N}` | Left-align in width N |
| `{:^N}` | Center-align in width N |

---

## Examples

### Basic Format Patterns

```uranite
from uranite.io.console import puts

public function main() -> I32:
    String greeting = "Hello, {}!".format( "Alice" )
    puts( greeting )

    String multi = "Name: {}, Age: {}".format( "Bob", 25 )
    puts( multi )

    String reorder = "{1} before {0}".format( "second", "first" )
    puts( reorder )

    Boolean active = True
    String status = "Active: {}".format( active )
    puts( status )
    return 0
```

Output:

```
Hello, Alice!
Name: Bob, Age: 25
first before second
Active: True
```

### Number Formatting

```uranite
from uranite.io.console import puts

public function main() -> I32:
    F64 pi = 3.14159265
    String twoDecimal = "{:.2f}".format( pi )
    puts( twoDecimal )
    String fourDecimal = "{:.4f}".format( pi )
    puts( fourDecimal )

    I64 number = 255
    String decimal = "{:d}".format( number )
    puts( decimal )
    String hexLower = "{:x}".format( number )
    puts( hexLower )
    String hexUpper = "{:X}".format( number )
    puts( hexUpper )
    String octal = "{:o}".format( number )
    puts( octal )
    String binary = "{:b}".format( number )
    puts( binary )
    return 0
```

Output:

```
3.14
3.1416
255
ff
FF
377
11111111
```

### Functions with Formatting

```uranite
from uranite.io.console import puts

public function formatRecord( String name, I64 score ) -> String:
    return "Player {} scored {} points".format( name, score )

public function main() -> I32:
    String record1 = formatRecord( "Alice", 95 )
    puts( record1 )
    String record2 = formatRecord( "Bob", 78 )
    puts( record2 )

    String named = "Hello, {name}! You are {age}.".format( name="Charlie", age=30 )
    puts( named )
    return 0
```

Output:

```
Player Alice scored 95 points
Player Bob scored 78 points
Hello, Charlie! You are 30.
```

### Width Alignment and Escaping

```uranite
from uranite.io.console import puts

public function main() -> I32:
    String right = "{:>10}".format( "right" )
    puts( right )

    String left = "{:<10}".format( "left" )
    puts( left )

    String center = "{:^10}".format( "mid" )
    puts( center )

    I64 number = 42
    String padded = "{:>8d}".format( number )
    puts( padded )

    String escaped = "Use {{}} for placeholders: {}".format( "value" )
    puts( escaped )
    return 0
```

Output:

```
     right
left      
   mid    
      42
Use {} for placeholders: value
```
