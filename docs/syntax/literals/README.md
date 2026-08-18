# Literals

Literals are fixed values written directly in source code. Uranite supports scalar literals for numbers, text, characters, booleans, and the null value, as well as collection literals for arrays, maps, sets, and tuples.

---

## Table of Contents

- [Literals](#literals)
  - [Table of Contents](#table-of-contents)
  - [Scalar Literals](#scalar-literals)
  - [Collection Literals](#collection-literals)
  - [At a Glance](#at-a-glance)
  - [Subpage Index](#subpage-index)

---

## Scalar Literals

Scalar literals represent individual values of primitive or built-in types.

**Integer literals** support four bases: decimal (`255`), hexadecimal (`0xFF`), binary (`0b11111111`), and octal (`0o377`). Underscores can separate digit groups for readability (`1_000_000`).

**Float literals** use decimal notation (`3.14`) or scientific notation (`1.5e10`, `2.0E-3`).

**String literals** are double-quoted (`"hello"`), support escape sequences (`\n`, `\t`, `\xHH`), and encode as UTF-8.

**Char literals** are single-quoted (`'A'`, `'\n'`), represent a single 32-bit Unicode code point, and support the same escape sequences as strings.

**Boolean literals** are `True` and `False`. Both are case-sensitive keywords — lowercase `true` and `false` are not valid boolean values.

**None** represents the absence of a value. It is the only value assignable to optional types (`?T`).

**Regex literals** use slash delimiters (`/[a-z]+/`) and compile to pattern objects for matching operations.

---

## Collection Literals

Collection literals create instances of standard library collection types directly from inline syntax.

**Array literals** (`[1, 2, 3]`) create raw heap-allocated arrays.

**Map literals** (`{"name": "Alice", "age": "30"}`) create `HashMap` instances from key-value pairs enclosed in braces with colon-separated entries.

**Set literals** (`{1, 2, 3}`) create `HashSet` instances from values enclosed in braces without colons.

**Tuple literals** (`(10, "hello", 3.14)`) create fixed-size `Tuple` instances from parenthesized, comma-separated values.

---

## At a Glance

```uranite
I64 decimal = 255
I64 hexValue = 0xFF
I64 binaryValue = 0b11111111
I64 octalValue = 0o377

F64 ratio = 3.14

String greeting = "hello"
Char letter = 'A'

Boolean active = True
Boolean inactive = False

?String missing = None
```

---

## Subpage Index

| Document | Description |
|---|---|
| [Integer Literals](integer-literals.md) | Decimal, hexadecimal, octal, and binary integer formats with underscore digit separators. |
| [Float Literals](float-literals.md) | Floating-point literal syntax, scientific notation, and IEEE 754 representation. |
| [String Literals](string-literals.md) | Double-quoted string syntax, escape sequences, and UTF-8 encoding. |
| [Char Literals](char-literals.md) | Single-quoted character literals, escape sequences, and the 32-bit `Char` type. |
| [Boolean Literals](boolean-literals.md) | `True` and `False` keyword literals and their case sensitivity. |
| [None Literals](none-literals.md) | The `None` literal, optional types, and null-value semantics. |
| [Regex Literals](regex-literals.md) | Regular expression literal syntax, context-sensitive disambiguation, and pattern compilation. |
| [Array Literals](array-literals.md) | `[1, 2, 3]` syntax for creating raw heap-allocated arrays. |
| [Map Literals](map-literals.md) | `{"key": value}` syntax for creating `HashMap` instances. |
| [Set Literals](set-literals.md) | `{value1, value2}` syntax for creating `HashSet` instances. |
| [Tuple Literals](tuple-literals.md) | `(value1, value2)` syntax for creating `Tuple` instances. |
