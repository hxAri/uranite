# Char Type

Uranite's `Char` type represents a single Unicode character as a 32-bit integer. It maps to LLVM's `i32` — wide enough to hold any Unicode scalar value (U+0000 through U+10FFFF). Character literals use single-quote delimiters (`'A'`), support 7 escape sequences plus hex escapes, and compile to `ConstantInt::get(Int32Ty, value)` with zero overhead from the OOP wrapper.

This document covers the complete `Char` type specification, LLVM representation, lexer processing with escape sequences, the full compilation pipeline from source to LLVM IR, character classification and case conversion methods, casting between `Char` and integer types, and the OOP wrapper class.

---

## Table of Contents

- [Type Identity](#type-identity)
- [LLVM Representation](#llvm-representation)
  - [Type Mapping](#type-mapping)
  - [Constant Emission](#constant-emission)
  - [Memory Layout](#memory-layout)
- [Character Literals](#character-literals)
  - [Basic Syntax](#basic-syntax)
  - [Escape Sequences](#escape-sequences)
  - [Hex Escapes](#hex-escapes)
  - [Delimiter Rule](#delimiter-rule)
- [Compilation Pipeline](#compilation-pipeline)
  - [Lexer Stage](#lexer-stage)
  - [Parser Stage](#parser-stage)
  - [HIR Stage](#hir-stage)
  - [MIR Stage](#mir-stage)
  - [Codegen Stage](#codegen-stage)
- [Casting and Conversions](#casting-and-conversions)
  - [Char to Integer](#char-to-integer)
  - [Integer to Char](#integer-to-char)
  - [Char to String](#char-to-string)
- [Comparisons](#comparisons)
- [Assignability Rules](#assignability-rules)
- [The OOP Wrapper](#the-oop-wrapper)
  - [Char Class](#char-class)
  - [Classification Methods](#classification-methods)
  - [Case Conversion Methods](#case-conversion-methods)
  - [Builtin Method Codegen](#builtin-method-codegen)
- [Examples](#examples)
  - [Character Declarations](#character-declarations)
  - [Escape Sequences in Practice](#escape-sequences-in-practice)
  - [Classification and Conversion](#classification-and-conversion)
  - [Character Arithmetic](#character-arithmetic)
  - [Practical Usage](#practical-usage)

---

## Type Identity

| Property | Value |
|---|---|
| Type Name | `Char` |
| Primitive Kind | `Type::Kind::Char` |
| Qualified Name (primitive) | `uranite.builtin.char` |
| Qualified Name (OOP wrapper) | `uranite.language.char.Char` |
| LLVM Type | `i32` |
| Literal Delimiter | Single quotes (`'...'`) |

The `Char` type is distinct from both integers and strings. It is classified by `isPrimitive()` (via `Kind::Char`), is part of the `oopWrapperQualified` set, but is not in `integerOopQualified` or `floatOopQualified` — it is not considered a numeric type by the wrapper classification helpers.

---

## LLVM Representation

### Type Mapping

The `Char` type maps to `llvm::Type::getInt32Ty(context)` — a 32-bit integer. This mapping applies to both the primitive kind and the OOP wrapper:

| Source Type | Condition | LLVM Type |
|---|---|---|
| `Type::Kind::Char` | Always | `i32` |
| `Type::Kind::Class` | `className == "Char"` | `i32` |

The 32-bit width accommodates the full Unicode scalar range (U+0000 to U+10FFFF, maximum value 1,114,111, which fits in 21 bits). Using `i32` rather than `i21` ensures natural alignment and efficient register use.

### Constant Emission

Character constants are emitted by `generateConstantChar()`:

```
ConstantInt::get(Int32Ty, static_cast<uint32_t>(instruction.charConstantValue))
```

The `charConstantValue` field stores the character as a C++ `char`, which is cast to `uint32_t` for the LLVM constant. This produces an unsigned 32-bit integer constant.

### Memory Layout

| Property | Value |
|---|---|
| Storage Size | 4 bytes |
| Alignment | 4 bytes |
| Representation | Unicode code point as unsigned 32-bit integer |

Characters occupy 4 bytes regardless of their code point value. A `Char` holding `'A'` (U+0041) and a `Char` holding a CJK ideograph both use exactly 4 bytes.

---

## Character Literals

### Basic Syntax

Character literals are enclosed in single quotes. Each literal contains exactly one character (or one escape sequence):

```uranite
Char letter = 'A'
Char digit = '7'
Char space = ' '
Char at = '@'
```

Double quotes delimit strings, not characters. `"A"` is a `String`, `'A'` is a `Char`.

### Escape Sequences

The lexer's `readChar()` function recognizes 7 escape sequences:

| Escape | Character | Code Point | Description |
|---|---|---|---|
| `\'` | `'` | U+0027 | Single quote (literal) |
| `\\` | `\` | U+005C | Backslash |
| `\0` | NUL | U+0000 | Null character |
| `\n` | LF | U+000A | Line feed (newline) |
| `\r` | CR | U+000D | Carriage return |
| `\t` | TAB | U+0009 | Horizontal tab |
| `\xHH` | varies | U+00HH | Hex byte (2 hex digits) |

Any unrecognized escape character after `\` is taken literally — `\q` produces the character `q`.

### Hex Escapes

The `\x` escape reads up to 2 hexadecimal digits and interprets them as a byte value:

```uranite
Char null = '\x00'
Char bell = '\x07'
Char delete = '\x7F'
Char maxByte = '\xFF'
```

The hex value is parsed via `std::stoi(hexByteValue, nullptr, 16)` and cast to `char`. This limits `\x` escapes to the range 0x00–0xFF (0–255). Characters outside this range require direct Unicode input or integer-to-Char casting.

If no hex digits follow `\x`, the lexer emits a diagnostic error and produces the literal character `x`.

### Delimiter Rule

Single quotes delimit characters; double quotes delimit strings. The lexer dispatches on the opening delimiter:

- `'` triggers `readChar()` — expects exactly one character (or escape), then closing `'`.
- `"` triggers `readString()` — reads a sequence of characters until closing `"`.

An unterminated character literal (missing closing `'`) produces an `Error` token with a diagnostic message.

---

## Compilation Pipeline

### Lexer Stage

The `readChar()` method processes character literals:

1. Advance past the opening `'`.
2. Check for `\` — if present, enter escape sequence handling.
3. For non-escape characters, capture the single character directly.
4. Expect and consume the closing `'`.
5. Return a `LiteralChar` token with the character value as a string.

The token stores the actual character value (post-escape-processing), not the raw source text.

### Parser Stage

The parser encounters a `LiteralChar` token and extracts the character:

```
value = current().value[0]   — first byte of the token's string value
→ CharLiteralExpression(value, source)
```

The `CharLiteralExpression` AST node stores the character as a C++ `char`.

### HIR Stage

HIR lowering creates an `HIRCharLiteral` node:

```
HIRCharLiteral {
    charValue: char   — the character value
}
```

### MIR Stage

MIR lowering emits a `ConstantChar` instruction:

```
MIRInstruction(MIRInstructionKind::ConstantChar)
    .charConstantValue = charLiteral.charValue
    .destinationVariable = allocated variable
```

### Codegen Stage

The `generateConstantChar()` function emits the LLVM constant:

```
ConstantInt::get(Int32Ty, static_cast<uint32_t>(charConstantValue))
→ setVariableValue(destination, constValue)
```

**Complete pipeline example for `'A'`:**

```
Source:  'A'
Lexer:   LiteralChar token, value = "A"
Parser:  CharLiteralExpression(value = 'A' / 0x41)
HIR:     HIRCharLiteral(charValue = 0x41)
MIR:     ConstantChar(charConstantValue = 0x41)
LLVM IR: i32 65
```

---

## Casting and Conversions

### Char to Integer

Casting `Char` to an integer type extracts the Unicode code point as a numeric value. Since `Char` is `i32` at the LLVM level, this is a type reinterpretation:

```uranite
Char letter = 'A'
I32 codePoint = letter as I32
I64 wideCode = letter as I64
```

- `Char` → `I32`: identity (both are `i32`)
- `Char` → `I64`: sign extension via `CreateSExt`
- `Char` → `I8`/`I16`: truncation via `CreateTrunc`

### Integer to Char

Casting an integer to `Char` treats the integer value as a Unicode code point:

```uranite
I32 code = 66
Char letter = code as Char
```

- `I32` → `Char`: identity (both are `i32`)
- `I64` → `Char`: truncation via `CreateTrunc`
- `I8`/`I16` → `Char`: sign extension via `CreateSExt`

### Char to String

The `toString()` method converts a `Char` to a single-character `String`. This is handled by the OOP wrapper's `toString()` method, which uses `self.value as String` — the codegen emits a `snprintf`-style conversion to create a null-terminated string from the character value.

---

## Comparisons

Characters are compared by their code point values using integer comparison instructions:

```uranite
Char lower = 'a'
Char upper = 'A'
Boolean before = upper < lower
```

Since `Char` is `i32` at the LLVM level, character comparisons use `ICmpEQ`, `ICmpNE`, `ICmpSLT`, `ICmpSGT`, `ICmpSLE`, `ICmpSGE` — the same signed integer comparison instructions used for `I32`.

This means character ordering follows Unicode code point order: digits (`'0'`–`'9'`, 48–57) < uppercase letters (`'A'`–`'Z'`, 65–90) < lowercase letters (`'a'`–`'z'`, 97–122).

---

## Assignability Rules

The `Char` type has the following assignability characteristics:

- `Char` is assignable to `Char` — trivial identity.
- `Char` is classified by `isPrimitive()` via `Kind::Char`.
- `Char` is part of `oopWrapperQualified` — the OOP wrapper is recognized.
- `Char` is not considered numeric — `isIntegral()` and `isFloatingPoint()` return `false` for `Kind::Char`.
- Explicit casting between `Char` and integer types is supported via the `as` keyword.
- The `isComparable()` method permits comparing `Char` with `Char` via the `equals()` check.

---

## The OOP Wrapper

### Char Class

The `Char` class in `stdlibs/language/char.urn`:

```
final class Char
    protect Char value
```

It is a `final` class — no subclassing. The `value` field is typed as `Char`, which at the LLVM level is `i32`.

**Constructor:** `Char(self, Char value)` — wraps a character value.

### Classification Methods

| Method | Signature | Description | Implementation |
|---|---|---|---|
| `isAlpha()` | `() -> Boolean` | True for A–Z and a–z | Range check: `(value >= 'a' and value <= 'z') or (value >= 'A' and value <= 'Z')` |
| `isDigit()` | `() -> Boolean` | True for 0–9 | Range check: `value >= '0' and value <= '9'` |
| `isAlphanumeric()` | `() -> Boolean` | True for letters and digits | Delegates to `self.isAlpha() or self.isDigit()` |
| `isWhitespace()` | `() -> Boolean` | True for space, tab, newline, CR | Equality check: `value == ' ' or value == '\t' or value == '\n' or value == '\r'` |

These methods perform ASCII-range classification. Characters outside the ASCII range (code points > 127) return `False` for all classification methods.

### Case Conversion Methods

| Method | Signature | Description | Implementation |
|---|---|---|---|
| `toUpper()` | `() -> Char` | Convert lowercase to uppercase | If `value >= 'a' and value <= 'z'`: `(value as I32 - 32) as Char` |
| `toLower()` | `() -> Char` | Convert uppercase to lowercase | If `value >= 'A' and value <= 'Z'`: `(value as I32 + 32) as Char` |

Case conversion uses ASCII arithmetic — the distance between lowercase and uppercase ASCII letters is exactly 32 code points. `'a'` (97) minus 32 equals `'A'` (65). Characters outside the a–z/A–Z range are returned unchanged.

### Builtin Method Codegen

The codegen intercepts common methods on OOP wrappers. For the `Char` wrapper:

| Method | LLVM Implementation |
|---|---|
| `getValue()` | Identity return of `i32` value |
| `toString()` | String conversion via `snprintf`-style emission |
| `hashCode()` | `CreateZExt(selfValue, i64Type)` — zero-extends `i32` to `i64` |
| `equals(other)` | `CreateICmpEQ(self, other)` |
| `compareTo(other)` | `ICmpSLT` + `ICmpSGT` → `gt - lt` pattern |

Classification and case conversion methods delegate to the stdlib implementation, which compiles to inline integer comparisons and arithmetic — no function call overhead for simple range checks.

### Additional Methods

| Method | Signature | Description |
|---|---|---|
| `getValue()` | `() -> Char` | Return underlying character value |
| `toString()` | `() -> String` | Convert to single-character string |

---

## Examples

### Character Declarations

```uranite
Char letter = 'A'
Char digit = '0'
Char space = ' '
Char newline = '\n'
Char tab = '\t'
Char singleQuote = '\''
Char backslash = '\\'
Char nullChar = '\0'
Char hexChar = '\x41'
```

### Escape Sequences in Practice

```uranite
from uranite.io.console import puts

Char bell = '\x07'
Char escape = '\x1B'
Char delete = '\x7F'

String text = "Line one"
Char separator = '\n'
String combined = text + separator.toString() + "Line two"
puts( combined )
```

### Classification and Conversion

```uranite
from uranite.io.console import puts

Char upper = 'H'
Char lower = upper.toLower()
puts( lower.toString() )

Char digit = '5'
Boolean isNum = digit.isDigit()
puts( isNum.toString() )

Char space = ' '
Boolean isWs = space.isWhitespace()
puts( isWs.toString() )

Char letter = 'z'
Char uppercased = letter.toUpper()
puts( uppercased.toString() )

Char at = '@'
Boolean alpha = at.isAlpha()
Boolean alnum = at.isAlphanumeric()
puts( alpha.toString() )
puts( alnum.toString() )
```

### Character Arithmetic

```uranite
Char letter = 'A'
I32 codePoint = letter as I32

I32 offset = codePoint + 3
Char shifted = offset as Char

Char lowerA = 'a'
Char upperA = 'A'
I32 distance = lowerA as I32 - upperA as I32

I32 code = 9731
Char snowman = code as Char
```

### Practical Usage

```uranite
from uranite.io.console import puts

public function isVowel( Char character ) -> Boolean:
    Char lower = character.toLower()
    return lower == 'a' or lower == 'e' or lower == 'i' or lower == 'o' or lower == 'u'

public function caesarShift( Char character, I32 shift ) -> Char:
    if character.isAlpha():
        Char base = 'a'
        if character >= 'A' and character <= 'Z':
            base = 'A'
        I32 offset = character as I32 - base as I32
        I32 shifted = (offset + shift) % 26
        if shifted < 0:
            shifted = shifted + 26
        return (base as I32 + shifted) as Char
    return character

public function countDigits( String text, I64 length ) -> I64:
    I64 count = 0
    I64 index = 0
    while index < length:
        Char character = text[index] as Char
        if character.isDigit():
            count = count + 1
        index = index + 1
    return count

public function toUpperCase( Char character ) -> Char:
    if character >= 'a' and character <= 'z':
        return (character as I32 - 32) as Char
    return character

public function main() -> I32:
    Boolean vowelCheck = isVowel( 'E' )
    puts( vowelCheck.toString() )

    Char encrypted = caesarShift( 'H', 3 )
    puts( encrypted.toString() )

    Char decrypted = caesarShift( encrypted, -3 )
    puts( decrypted.toString() )

    Char upper = toUpperCase( 'q' )
    puts( upper.toString() )

    Boolean consonant = not isVowel( 'X' )
    puts( consonant.toString() )

    return 0
```

This example demonstrates vowel detection via character comparison, Caesar cipher using code point arithmetic with modular wrapping, digit counting with `isDigit()`, manual case conversion via ASCII offset, and character-to-integer round-trip casting — all compiled to `i32` constants and integer comparison/arithmetic instructions with zero OOP wrapper overhead.
