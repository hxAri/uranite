# Integer Literals

Uranite supports integer literals in four number bases: decimal, hexadecimal, octal, and binary. All integer literals are stored as signed 64-bit values (`int64_t`) and typed as `I64` during semantic analysis. This document specifies the lexer scanning rules for each base, underscore separator handling, the parser's conversion pipeline, type inference behavior, and overflow semantics.

---

## Table of Contents

- [Overview](#overview)
- [Decimal Literals](#decimal-literals)
- [Hexadecimal Literals](#hexadecimal-literals)
- [Octal Literals](#octal-literals)
- [Binary Literals](#binary-literals)
- [Underscore Separators](#underscore-separators)
  - [Lexer Stripping Behavior](#lexer-stripping-behavior)
  - [Placement Rules](#placement-rules)
  - [Recommended Usage](#recommended-usage)
- [Type Suffixes](#type-suffixes)
- [The Parsing Pipeline](#the-parsing-pipeline)
  - [Stage 1: Lexer Scanning](#stage-1-lexer-scanning)
  - [Stage 2: Parser Conversion](#stage-2-parser-conversion)
  - [Stage 3: Semantic Type Assignment](#stage-3-semantic-type-assignment)
  - [Stage 4: MIR Code Generation](#stage-4-mir-code-generation)
- [Default Type: I64](#default-type-i64)
  - [OOP Wrapper Resolution](#oop-wrapper-resolution)
  - [Contextual Typing](#contextual-typing)
- [Overflow and Parsing Limits](#overflow-and-parsing-limits)
  - [Lexer-Level Limits](#lexer-level-limits)
  - [Parser-Level Overflow](#parser-level-overflow)
  - [Semantic-Level Range Checks](#semantic-level-range-checks)
  - [I64 Range](#i64-range)
- [The IntegerType System](#the-integertype-system)
- [Examples](#examples)
  - [Valid Integer Literals](#valid-integer-literals)
  - [Invalid Integer Literals](#invalid-integer-literals)
  - [Practical Usage](#practical-usage)

---

## Overview

An integer literal is a sequence of digits optionally preceded by a base prefix (`0x`, `0o`, `0b`). The lexer scans the character sequence, strips underscore separators, and produces a `LiteralInteger` token containing the raw string. The parser then converts the raw string to a 64-bit signed integer (`int64_t`) using `std::stoll()` with the appropriate base. The semantic analyzer assigns the type `I64` to every integer literal expression.

| Base | Prefix | Digits | Example |
|---|---|---|---|
| Decimal | (none) | `0`-`9` | `42`, `1000`, `999_999` |
| Hexadecimal | `0x` or `0X` | `0`-`9`, `a`-`f`, `A`-`F` | `0xFF`, `0x2A`, `0X1F4` |
| Octal | `0o` or `0O` | `0`-`7` | `0o52`, `0O777`, `0o17` |
| Binary | `0b` or `0B` | `0`, `1` | `0b101010`, `0B1111_0000` |

All four bases produce the same token type (`LiteralInteger`) and the same AST node (`IntegerLiteralExpression`). The base distinction exists only in the source text and the raw string stored in the token — the parsed numeric value is always a single `int64_t`.

---

## Decimal Literals

Decimal literals consist of one or more digits `0`-`9`, optionally separated by underscores. No prefix is required:

```uranite
I64 zero = 0
I64 answer = 42
I64 million = 1_000_000
I64 maxPort = 65535
```

The lexer scans decimal literals by accumulating consecutive digit and underscore characters. It stops at the first character that is neither a digit nor an underscore. If the next character is `.` followed by a digit, the literal transitions to a float literal (covered in the float literals document). If the next character is `e` or `E`, it likewise transitions to scientific notation (float).

If the character following the digit sequence is alphabetic, the lexer reads it as a type suffix appended to the numeric string. This suffix is included in the raw token value but is not currently used by the parser for type inference — all integer literals are typed as `I64` regardless of suffix.

---

## Hexadecimal Literals

Hexadecimal literals begin with the prefix `0x` or `0X`, followed by one or more hexadecimal digits (`0`-`9`, `a`-`f`, `A`-`F`), optionally separated by underscores:

```uranite
I64 red = 0xFF0000
I64 permissions = 0x1A4
I64 mask = 0xFF_FF_FF_FF
I64 address = 0xDEAD_BEEF
```

The lexer detects the hexadecimal prefix by checking if the current character is `0` and the next character (via `peek()`) is `x` or `X`. It then advances past both prefix characters and accumulates hex digits and underscores using `std::isxdigit()`.

Hexadecimal digits are case-insensitive. `0xFF`, `0XFF`, `0xff`, and `0Xff` all produce the same numeric value. The raw string preserves the original casing from the source.

---

## Octal Literals

Octal literals begin with the prefix `0o` or `0O`, followed by one or more octal digits (`0`-`7`), optionally separated by underscores:

```uranite
I64 fileMode = 0o644
I64 fullPerms = 0o777
I64 readOnly = 0o444
```

The lexer detects the octal prefix by checking for `0` followed by `o` or `O`. It then accumulates characters in the range `'0'` through `'7'` and underscores.

Note that Uranite uses the explicit `0o` prefix for octal, not the C-style leading-zero convention. A literal `0644` is a decimal number `644`, not an octal number. This eliminates the common source of bugs in C where leading zeros silently change the numeric base.

---

## Binary Literals

Binary literals begin with the prefix `0b` or `0B`, followed by one or more binary digits (`0` or `1`), optionally separated by underscores:

```uranite
I64 flags = 0b1010
I64 byte = 0b1111_0000
I64 mask = 0b0000_0000_0000_0001
I64 pattern = 0B10101010
```

The lexer detects the binary prefix by checking for `0` followed by `b` or `B`. It then accumulates only `'0'`, `'1'`, and `'_'` characters.

Binary literals are particularly useful for bit flags, hardware register values, and bitmask operations where the individual bit positions carry meaning.

---

## Underscore Separators

### Lexer Stripping Behavior

Underscores within numeric literals serve as visual separators to improve readability. The lexer strips them during scanning — they are never included in the token value string that reaches the parser.

The stripping logic is identical across all four bases. In each scanning loop, the lexer checks:

```
if current character is '_':
    advance (skip the underscore)
else:
    append character to token value
    advance
```

The underscore is consumed but not appended. The resulting token value contains only digits (and the base prefix, if applicable).

For example, the source text `1_000_000` produces a token with value `"1000000"`. The source text `0xFF_FF` produces a token with value `"0xFFFF"`. The parser never sees underscore characters in integer token values.

### Placement Rules

The lexer does not enforce strict placement rules for underscores. It accepts underscores anywhere within the digit sequence:

- Between digits: `1_000` (standard grouping)
- Multiple consecutive underscores: `1__000` (accepted, unusual)
- Leading underscore after prefix: `0x_FF` (accepted, not recommended)
- Trailing underscore: `1000_` (the underscore is consumed; the next character determines what follows)

However, an underscore cannot appear **before** the first digit of a literal. A bare `_` is an identifier start character, not a numeric start character. The lexer dispatches `_` to `readIdentifierOrKeyword()`, not `readNumber()`.

### Recommended Usage

Group digits in sets of three for decimal, four for hexadecimal and binary, and three for octal:

```uranite
I64 population = 8_000_000_000
I64 colorValue = 0xFF_00_FF
I64 bitmask = 0b1111_0000_1010_0101
I64 permissions = 0o755
```

These groupings mirror conventional numeric formatting and make large values immediately readable.

---

## Type Suffixes

The lexer accepts an optional alphabetic suffix immediately following a decimal integer literal. After scanning the digit sequence, if the next character is alphabetic, the lexer reads consecutive alphanumeric characters and appends them to the token value:

```
Source:  42i32
Token:   LiteralInteger, value = "42i32"
```

The suffix is included in the raw token string, but the parser does **not** currently use it for type inference. The parser's integer conversion logic extracts only digit characters (and the `-` sign) from the raw string before passing to `std::stoll()`. Any trailing alphabetic suffix is silently ignored during numeric conversion.

All integer literals are typed as `I64` regardless of any suffix. Explicit type narrowing is performed through variable type annotations, not through literal suffixes:

```uranite
I32 small = 42
I16 shorter = 100
I8 tiny = 7
```

The literal `42` is parsed as `int64_t(42)` and typed as `I64`. When assigned to an `I32` variable, the semantic analyzer handles the implicit narrowing conversion.

---

## The Parsing Pipeline

### Stage 1: Lexer Scanning

The `readNumber()` method in the lexer performs the initial character scan. It:

1. Checks for base prefixes (`0b`, `0o`, `0x`) by examining the first two characters.
2. For prefixed literals: advances past the prefix and scans valid digits for that base, stripping underscores.
3. For decimal literals: scans digits and underscores, then checks for `.` (float transition) and `e`/`E` (scientific notation transition).
4. Appends any trailing alphabetic suffix.
5. Returns a `LiteralInteger` token with the accumulated raw string.

The raw string preserved in the token includes the base prefix but excludes underscores:

| Source Text | Token Value |
|---|---|
| `42` | `"42"` |
| `1_000_000` | `"1000000"` |
| `0xFF` | `"0xFF"` |
| `0b1010` | `"0b1010"` |
| `0o644` | `"0o644"` |
| `0xFF_FF` | `"0xFFFF"` |

### Stage 2: Parser Conversion

When the parser encounters a `LiteralInteger` token, it converts the raw string to a 64-bit integer value. The conversion logic uses base-aware prefix detection:

1. If the raw string has length > 2 and starts with `0x` or `0X`: call `std::stoll(raw.substr(2), nullptr, 16)` — hexadecimal conversion with prefix stripped.
2. If the raw string has length > 2 and starts with `0b` or `0B`: call `std::stoll(raw.substr(2), nullptr, 2)` — binary conversion with prefix stripped.
3. If the raw string has length > 2 and starts with `0o` or `0O`: call `std::stoll(raw.substr(2), nullptr, 8)` — octal conversion with prefix stripped.
4. Otherwise: extract only digit and `-` characters into a clean numeric string, then call `std::stoll(numericString)` — decimal conversion with any trailing suffix stripped.

The result is an `IntegerLiteralExpression` AST node containing both the original raw string and the parsed `int64_t` value.

### Stage 3: Semantic Type Assignment

The semantic analyzer processes `IntegerLiteral` expression nodes with a straightforward type assignment:

1. Look up the `I64` class type via `typeRegistry.lookupType("I64")`.
2. If found, use the class type (OOP wrapper `uranite.language.i64.I64`).
3. If not found (fallback), use the primitive integer type `typeRegistry.getInteger64()` (primitive `uranite.builtin.i64`).

Every integer literal receives the same type: `I64`. There is no literal-level distinction between `I8`, `I16`, `I32`, `I64`, or unsigned variants. Type narrowing happens at the assignment site through implicit conversion, not at the literal itself.

### Stage 4: MIR Code Generation

In MIR codegen, a `ConstantInteger` instruction maps directly to an LLVM `ConstantInt`:

```
llvm::ConstantInt::get(
    llvm::Type::getInt64Ty(context),
    instruction.integerConstantValue,
    true    ← signed
)
```

The LLVM type is always `i64` (signed 64-bit integer). The `true` parameter indicates signed representation. The resulting LLVM `Value*` is stored as the variable's runtime value.

---

## Default Type: I64

### OOP Wrapper Resolution

Every integer literal in Uranite is typed as `I64` — the 64-bit signed integer OOP wrapper class. The semantic analyzer resolves this by first looking up the class name "I64" in the type registry. This resolves to `uranite.language.i64.I64`, the OOP wrapper class defined in `stdlibs/language/i64.urn`.

The OOP wrapper provides methods like `toString()`, `hashCode()`, `equals()`, and arithmetic operator implementations through interface dispatch (`Addable`, `Subtractable`, `Multipliable`, etc.). This means integer literals have method access:

```uranite
I64 value = 42
String text = value.toString()
I64 hash = value.hashCode()
```

If the OOP wrapper class is not available (e.g., during bootstrapping or in minimal compilation modes), the semantic analyzer falls back to the primitive `uranite.builtin.i64` type, which is a raw 64-bit integer with no methods.

### Contextual Typing

When an integer literal is assigned to a variable with a narrower type annotation, the semantic analyzer performs implicit narrowing:

```uranite
I32 small = 42
I16 shorter = 100
I8 tiny = 7
U64 unsigned = 255
```

In each case, the literal `42`, `100`, `7`, or `255` is parsed as `int64_t` and initially typed as `I64`. The assignment to a narrower type triggers an implicit conversion in the type checker. The MIR codegen still emits the value as an `i64` LLVM constant — the LLVM backend handles truncation and sign extension as needed during instruction selection.

---

## Overflow and Parsing Limits

### Lexer-Level Limits

The lexer imposes no length limit on integer literals. It accumulates digit characters into a `std::string` without bounds checking. A literal with thousands of digits is accepted at the lexer level and produces a valid `LiteralInteger` token.

### Parser-Level Overflow

The parser converts the raw string to `int64_t` using `std::stoll()`. This function throws `std::out_of_range` if the value exceeds the range of `long long` (which is at least 64 bits on all supported platforms). If the conversion overflows, the exception propagates as an unhandled error — the compiler does not currently catch `std::out_of_range` and produce a user-friendly diagnostic.

In practice, this means literals outside the `int64_t` range cause a hard crash rather than a descriptive error message. This is a known limitation.

### Semantic-Level Range Checks

The semantic analyzer does not perform explicit range validation on integer literal values. It does not check whether the literal `300` fits in an `I8` variable (range: -128 to 127) or whether `100_000` fits in an `I16` variable (range: -32768 to 32767). Overflow from narrowing conversions is silent at the semantic level — the value is truncated at the LLVM instruction selection stage.

### I64 Range

The `I64` type represents a signed 64-bit integer with the following bounds:

| Bound | Value |
|---|---|
| Minimum | -9,223,372,036,854,775,808 (`-2^63`) |
| Maximum | 9,223,372,036,854,775,807 (`2^63 - 1`) |

Any literal within this range is correctly represented. Literals outside this range cause `std::stoll()` to throw `std::out_of_range`.

---

## The IntegerType System

The compiler's type system represents integer types through the `IntegerType` struct, which extends the base `Type` with two fields:

| Field | Type | Description |
|---|---|---|
| `bitWidth` | `int` | Number of bits: 8, 16, 32, or 64. |
| `isSigned` | `bool` | `true` for signed types (`I8`-`I64`), `false` for unsigned (`U8`-`U64`). |

The type registry pre-creates all eight standard integer types during initialization:

| Type Name | Primitive Qualified Name | Bit Width | Signed |
|---|---|---|---|
| `I8` | `uranite.builtin.i8` | 8 | Yes |
| `I16` | `uranite.builtin.i16` | 16 | Yes |
| `I32` | `uranite.builtin.i32` | 32 | Yes |
| `I64` | `uranite.builtin.i64` | 64 | Yes |
| `U8` | `uranite.builtin.u8` | 8 | No |
| `U16` | `uranite.builtin.u16` | 16 | No |
| `U32` | `uranite.builtin.u32` | 32 | No |
| `U64` | `uranite.builtin.u64` | 64 | No |

The `IntegerType` constructor auto-generates the type name from signedness and bit width: signed types get an "i" prefix (`i8`, `i16`, `i32`, `i64`), unsigned types get a "u" prefix (`u8`, `u16`, `u32`, `u64`).

Each primitive integer type also has a corresponding OOP wrapper class in `stdlibs/language/` (e.g., `I64` class at `uranite.language.i64.I64`) that provides method access, operator dispatch, and interface conformance.

---

## Examples

### Valid Integer Literals

```uranite
I64 decimal = 42
I64 negative = -17
I64 zero = 0
I64 large = 9_223_372_036_854_775_807

I64 hex = 0xFF
I64 hexUpper = 0XAB
I64 hexGrouped = 0xFF_FF_FF_FF

I64 octal = 0o755
I64 octalLower = 0o644

I64 binary = 0b1010
I64 binaryGrouped = 0b1111_0000_1010_0101
I64 binaryUpper = 0B1100_0011
```

### Invalid Integer Literals

The following are **not** valid integer literals:

```
0x          → prefix with no digits (lexer produces empty hex value)
0b2         → '2' is not a binary digit (lexer stops at '2', token is "0b")
0o8         → '8' is not an octal digit (lexer stops at '8', token is "0o")
0789        → decimal literal, not octal (no 0o prefix; parsed as 789)
_42         → underscore start dispatches to identifier, not number
```

Note that `0789` is a valid **decimal** literal with value 789. The leading `0` is part of the decimal digit sequence. Only the explicit `0o` prefix triggers octal parsing.

### Practical Usage

```uranite
from uranite.io.console import puts

public const I64 MAX_CONNECTIONS = 1024
public const I64 DEFAULT_BUFFER_SIZE = 4_096
public const I64 COLOR_MASK = 0x00FF_FFFF
public const I64 READ_FLAG = 0b0000_0100
public const I64 WRITE_FLAG = 0b0000_0010
public const I64 EXEC_FLAG = 0b0000_0001

public function hasPermission( I64 mode, I64 flag ) -> Boolean:
    return ( mode & flag ) != 0

public function main() -> I32:
    I64 permissions = 0o755
    if hasPermission( permissions, READ_FLAG ):
        puts( "read access granted" )
    return 0
```

This example demonstrates decimal constants with underscore grouping, hexadecimal color masks, binary bit flags, and an octal file permission value — each base used in its natural domain.
