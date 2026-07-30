# Character Literals

Uranite supports character literals delimited by single quotes (`'...'`). Character literal represents exactly one character — a single Unicode scalar value typed as `Char`. This document specifies lexer scanning rules, escape sequence processing, type assignment, LLVM code generation, and relationship between character literals and the `Char` OOP wrapper class.

---

## Table of Contents

- [Overview](#overview)
- [Character Literal Syntax](#character-literal-syntax)
- [Delimiter Distinction](#delimiter-distinction)
- [Escape Sequences](#escape-sequences)
  - [Standard Escapes](#standard-escapes)
  - [Hex Byte Escape](#hex-byte-escape)
  - [Unknown Escapes](#unknown-escapes)
- [Lexer Scanning Algorithm](#lexer-scanning-algorithm)
  - [Entry Point](#entry-point)
  - [Escape Branch](#escape-branch)
  - [Literal Branch](#literal-branch)
  - [Closing Quote Validation](#closing-quote-validation)
  - [Error Conditions](#error-conditions)
- [The Parsing Pipeline](#the-parsing-pipeline)
  - [Stage 1: Lexer Scanning](#stage-1-lexer-scanning)
  - [Stage 2: Parser Construction](#stage-2-parser-construction)
  - [Stage 3: Semantic Type Assignment](#stage-3-semantic-type-assignment)
  - [Stage 4: HIR Representation](#stage-4-hir-representation)
  - [Stage 5: MIR Lowering](#stage-5-mir-lowering)
  - [Stage 6: LLVM Code Generation](#stage-6-llvm-code-generation)
- [Type and Memory Representation](#type-and-memory-representation)
  - [Primitive Char Type](#primitive-char-type)
  - [LLVM Representation](#llvm-representation)
  - [Char vs I8 vs I32](#char-vs-i8-vs-i32)
- [The Char OOP Wrapper](#the-char-oop-wrapper)
  - [Class Structure](#class-structure)
  - [Character Classification Methods](#character-classification-methods)
  - [Case Conversion](#case-conversion)
- [Examples](#examples)
  - [Valid Character Literals](#valid-character-literals)
  - [Escape Sequence Examples](#escape-sequence-examples)
  - [Invalid Character Literals](#invalid-character-literals)
  - [Practical Usage](#practical-usage)

---

## Overview

Character literal is a single character enclosed in single quotes. Lexer processes escape sequences during scanning, produces a `LiteralChar` token, and parser wraps value into `CharLiteralExpression` AST node storing a C++ `char`. Semantic analyzer types it as `Char`, and MIR codegen emits an LLVM `i32` constant via `ConstantInt::get(Int32Ty, ...)`.

| Property | Value |
|---|---|
| Delimiter | Single quotes (`'...'`) |
| Content | Exactly one character (or one escape sequence) |
| Escape processing | Yes, during lexer scanning |
| Default type | `Char` (`uranite.language.char.Char`) |
| LLVM representation | `i32` constant (32-bit integer) |

---

## Character Literal Syntax

Character literal begins with single-quote (`'`), contains exactly one character or escape sequence, and ends with matching single-quote:

```uranite
Char letter = 'A'
Char digit = '7'
Char newline = '\n'
Char tab = '\t'
Char nullChar = '\0'
```

Every character literal must contain exactly one logical character. Empty character literals (`''`) and multi-character literals (`'abc'`) are not valid — the lexer expects precisely one character (or one escape sequence) before the closing quote.

---

## Delimiter Distinction

Single quotes and double quotes serve strictly different purposes in Uranite:

| Delimiter | Produces | Token Type | AST Node |
|---|---|---|---|
| `'...'` | Character literal | `LiteralChar` | `CharLiteralExpression` |
| `"..."` | String literal | `LiteralString` | `StringLiteralExpression` |

This distinction is enforced at the lexer level. When the main tokenization loop encounters a single-quote character (`'`), it dispatches to `readChar()`. When it encounters a double-quote character (`"`), it dispatches to `readString()`. The two methods are completely independent — they share no code path.

A single character in double quotes (`"x"`) produces a `String`, not a `Char`. A character in single quotes (`'x'`) produces a `Char`, not a `String`. There is no implicit conversion between them at the lexer or parser level.

---

## Escape Sequences

Character literals support the same escape sequences as string literals, with one difference: `\'` replaces `\"` as the delimiter escape.

### Standard Escapes

Seven standard escape sequences are recognized:

| Escape | Byte Value | Description |
|---|---|---|
| `\'` | `0x27` | Single-quote character. Embeds a literal `'` inside a character literal. |
| `\0` | `0x00` | Null byte. |
| `\\` | `0x5C` | Backslash. Produces a literal `\` character. |
| `\n` | `0x0A` | Newline (line feed). |
| `\r` | `0x0D` | Carriage return. |
| `\t` | `0x09` | Horizontal tab. |
| `\xHH` | `0xHH` | Hex byte escape (see below). |

Note that `\"` (double-quote escape) is **not** listed here. String literals escape double quotes with `\"`; character literals escape single quotes with `\'`. The lexer's `readChar()` method handles `\'` where `readString()` handles `\"`.

### Hex Byte Escape

The `\x` escape inserts an arbitrary byte by hexadecimal value:

```
\xHH    → single byte with value 0xHH
```

After encountering `\x`, the lexer reads up to 2 hexadecimal digits (`0`-`9`, `a`-`f`, `A`-`F`). The digits are converted via `std::stoi(hexByteValue, nullptr, 16)` and cast to `char`.

If no hex digits follow `\x`, the lexer emits an error diagnostic:

```
error: expected hex digits after \x in character literal
  --> source.urn:3:5
```

In this error case, the character value falls back to the literal character `x`.

### Unknown Escapes

If the character after the backslash is not a recognized escape identifier, the lexer treats it as a literal character. Unlike `readString()` which emits a warning for unknown escapes, `readChar()` silently accepts the character. The backslash is consumed, and the following character is stored as the literal value:

```
'\q'    → character 'q'
'\a'    → character 'a'
```

---

## Lexer Scanning Algorithm

### Entry Point

The `readChar()` method is invoked when the main tokenization loop encounters a single-quote character. The dispatch is direct:

```
if character == '\'':
    tokens.push_back( readChar() )
    continue
```

### Escape Branch

If the character after the opening quote is a backslash (`\`), the lexer enters the escape processing branch:

1. Advance past the backslash.
2. Read the next character (the escape identifier).
3. Match against the escape table (`'`, `0`, `\`, `n`, `r`, `t`, `x`).
4. For hex escapes (`x`): read up to 2 hex digits, convert via `std::stoi`, cast to `char`.
5. For standard escapes: map to the corresponding byte value.
6. For unknown escapes: use the character literally.
7. Advance past the escape identifier (except for `\x`, which handles its own advancement).

### Literal Branch

If the character after the opening quote is not a backslash, the lexer stores it directly as the character value:

1. Read the current character.
2. Create a single-character string from it.
3. Advance past the character.

### Closing Quote Validation

After processing the character (escape or literal), the lexer checks whether the current character is a closing single quote:

- If `current() == '\''`: advance past the closing quote and return a `LiteralChar` token.
- If `current() != '\''`: emit an error and return an `Error` token.

### Error Conditions

Two error conditions exist:

| Condition | Diagnostic |
|---|---|
| Missing closing quote | `"unterminated character literal"` |
| No hex digits after `\x` | `"expected hex digits after \x in character literal"` |

The unterminated character literal error catches both truly unterminated literals (EOF before closing quote) and multi-character literals (the second character occupies the position where the closing quote should be, causing a mismatch).

For example, `'ab'` processes `a` as the character value, then checks whether `b` is a closing quote. Since `b` is not `'`, the lexer emits "unterminated character literal" and returns an `Error` token.

---

## The Parsing Pipeline

### Stage 1: Lexer Scanning

The `readChar()` method produces a `LiteralChar` token. The token value is a single-character `std::string` containing the unescaped byte:

| Source Text | Token Value |
|---|---|
| `'A'` | `A` |
| `'\n'` | newline byte (`0x0A`) |
| `'\t'` | tab byte (`0x09`) |
| `'\0'` | null byte (`0x00`) |
| `'\x41'` | `A` (`0x41`) |
| `'\''` | `'` |

### Stage 2: Parser Construction

When the parser encounters a `LiteralChar` token, it extracts the first character from the token's string value and constructs a `CharLiteralExpression`:

1. Initialize `value` to `'\0'`.
2. If the token value is not empty, set `value = token.value[0]`.
3. Advance past the token.
4. Return `CharLiteralExpression(value, source)`.

The `CharLiteralExpression` AST node stores a single `char value` field — a C++ `char` (typically 8-bit signed integer). This is the internal pipeline representation; semantic analysis promotes it to the `Char` type.

### Stage 3: Semantic Type Assignment

The semantic analyzer processes `CharLiteral` expression nodes:

1. Look up the `Char` class type via `typeRegistry.lookupType("Char")` using the qualified name from `qualname::classes::Char::Name`.
2. If found, use the class type (OOP wrapper `uranite.language.char.Char`).
3. If not found (fallback), use the primitive char type `typeRegistry.getChar()` (primitive `uranite.builtin.char`).

This dual-lookup pattern mirrors string literal type assignment — OOP wrapper first, primitive fallback second.

### Stage 4: HIR Representation

HIR preserves the character value in an `HIRCharLiteral` node:

| Field | Type | Description |
|---|---|---|
| `charValue` | `char` | The character byte. |
| `resolvedType` | `TypeSharedPointer` | The `Char` type from semantic analysis. |
| `sourceLocation` | `SourceSharedPointer` | Source file position. |

HIR lowering is straightforward: extract `charLiteral.value` from the AST node, pass it with the resolved type to the `HIRCharLiteral` constructor.

### Stage 5: MIR Lowering

MIR lowering creates a `ConstantChar` instruction:

1. Extract `charValue` from the `HIRCharLiteral` node.
2. Create a `MIRInstruction` with kind `ConstantChar`.
3. Set `charConstantValue` to the character byte.
4. Set `operandType` to the resolved `Char` type.
5. Allocate a variable named `_const_char` to hold the result.
6. Emit the instruction.

The `charConstantValue` field on `MIRInstruction` is typed as C++ `char` and defaults to `'\0'`.

### Stage 6: LLVM Code Generation

The `generateConstantChar()` method emits the LLVM IR:

```
llvm::Value* constValue = llvm::ConstantInt::get(
    llvm::Type::getInt32Ty( this->llvmContext ),
    static_cast<uint32_t>( instruction.charConstantValue )
);
```

The character byte is cast from `char` to `uint32_t` and stored as a 32-bit LLVM integer constant. This 32-bit representation accommodates the full Unicode scalar value range (U+0000 through U+10FFFF), even though the current lexer only processes single-byte characters.

---

## Type and Memory Representation

### Primitive Char Type

The type registry initializes the primitive char type during construction:

```
charType = Type( Kind::Char, "char", "uranite.builtin", "uranite.builtin.char" )
```

| Property | Value |
|---|---|
| Kind | `Type::Kind::Char` |
| Short name | `char` |
| Package | `uranite.builtin` |
| Qualified name | `uranite.builtin.char` |

The OOP wrapper class `uranite.language.char.Char` wraps this primitive. Both resolve to the same LLVM representation.

### LLVM Representation

Character values are represented as `i32` (32-bit signed integer) in LLVM IR:

| Uranite | LLVM Type | LLVM Constant |
|---|---|---|
| `'A'` | `i32` | `i32 65` |
| `'\n'` | `i32` | `i32 10` |
| `'\0'` | `i32` | `i32 0` |
| `'\x7F'` | `i32` | `i32 127` |

The 32-bit width matches the Unicode scalar value representation used in languages like Rust (`char` = 4 bytes) and Go (`rune` = `int32`). A single `i32` can represent any Unicode code point without surrogate encoding.

### Char vs I8 vs I32

| Type | Width | Range | Use |
|---|---|---|---|
| `Char` | 32 bits (`i32`) | Unicode scalar values (U+0000 to U+10FFFF) | Character data. |
| `I8` | 8 bits (`i8`) | -128 to 127 | Raw byte data. |
| `I32` | 32 bits (`i32`) | -2,147,483,648 to 2,147,483,647 | Signed integer arithmetic. |

While `Char` and `I32` share the same LLVM width, they are distinct types in the semantic type system. A `Char` carries character semantics — the `Char` OOP wrapper provides classification methods like `isAlpha()` and `isDigit()` that have no meaning on arbitrary integers. Conversion between `Char` and integer types uses the `as` cast operator:

```uranite
Char letter = 'A'
I32 codePoint = letter as I32
Char fromCode = ( codePoint + 1 ) as Char
```

---

## The Char OOP Wrapper

### Class Structure

The `Char` class (`uranite.language.char.Char`) is a `final` class — it cannot be subclassed. It wraps a primitive `Char` value and provides character classification and case conversion methods:

```uranite
final class Char:
    protect Char value
    public function Char( self, Char value ) -> Void
```

The `value` field is `protect`, accessible only within the class itself. The constructor takes a `Char` argument and stores it.

### Character Classification Methods

Four classification methods query character properties using ASCII range comparisons:

| Method | Signature | Description |
|---|---|---|
| `isAlpha` | `() -> Boolean` | Return `True` if the character is an alphabetic letter (`a`-`z` or `A`-`Z`). |
| `isDigit` | `() -> Boolean` | Return `True` if the character is a decimal digit (`0`-`9`). |
| `isAlphanumeric` | `() -> Boolean` | Return `True` if the character is alphabetic or a digit. |
| `isWhitespace` | `() -> Boolean` | Return `True` if the character is space, tab, newline, or carriage return. |

These methods operate on ASCII ranges. `isAlpha()` checks `(value >= 'a' and value <= 'z') or (value >= 'A' and value <= 'Z')`. `isDigit()` checks `value >= '0' and value <= '9'`. `isAlphanumeric()` delegates to `isAlpha() or isDigit()`. `isWhitespace()` checks against four whitespace characters: space (`' '`), tab (`'\t'`), newline (`'\n'`), and carriage return (`'\r'`).

### Case Conversion

Two methods convert letter case using ASCII arithmetic:

| Method | Signature | Description |
|---|---|---|
| `toUpper` | `() -> Char` | Return uppercase copy. Non-lowercase letters pass through unchanged. |
| `toLower` | `() -> Char` | Return lowercase copy. Non-uppercase letters pass through unchanged. |

Case conversion uses the ASCII offset of 32 between uppercase and lowercase letters. `toUpper()` subtracts 32 from the character's integer value when the character is in the range `'a'` to `'z'`. `toLower()` adds 32 when the character is in the range `'A'` to `'Z'`. Both return new `Char` instances — the original is never modified.

The conversion casts through `I32` for arithmetic:

```uranite
( self.value as I32 - 32 ) as Char
```

This two-step cast (Char to I32, arithmetic, I32 back to Char) is necessary because arithmetic operators are not defined directly on `Char`. The `as` operator handles the type boundary.

### Additional Methods

| Method | Signature | Description |
|---|---|---|
| `getValue` | `() -> Char` | Return the underlying character value. |
| `toString` | `() -> String` | Return the character as a single-character `String` via `self.value as String`. |

---

## Examples

### Valid Character Literals

```uranite
Char letterA = 'A'
Char letterZ = 'z'
Char digit0 = '0'
Char space = ' '
Char exclamation = '!'
Char at = '@'
Char tilde = '~'
```

### Escape Sequence Examples

```uranite
Char newline = '\n'
Char tab = '\t'
Char carriageReturn = '\r'
Char nullChar = '\0'
Char backslash = '\\'
Char singleQuote = '\''
Char hexA = '\x41'
Char hexNewline = '\x0A'
Char hexNull = '\x00'
```

| Variable | Value | Integer Equivalent |
|---|---|---|
| `newline` | LF | 10 |
| `tab` | TAB | 9 |
| `carriageReturn` | CR | 13 |
| `nullChar` | NUL | 0 |
| `backslash` | `\` | 92 |
| `singleQuote` | `'` | 39 |
| `hexA` | `A` | 65 |
| `hexNewline` | LF | 10 |
| `hexNull` | NUL | 0 |

### Invalid Character Literals

The following are **not** valid character literals:

```
''          → empty character literal (nothing between quotes)
'ab'        → multi-character literal (error: unterminated character literal)
"x"         → double quotes produce a String, not a Char
'\x'        → no hex digits after \x (error: expected hex digits)
```

The empty character literal `''` does not produce an error at the lexer level — the lexer reads the opening quote, immediately encounters the closing quote, and returns a `LiteralChar` token with an empty string value. The parser then sets the character value to `'\0'` (the fallback when the token value is empty). This behavior means `''` is syntactically accepted but semantically equivalent to `'\0'`.

### Practical Usage

```uranite
from uranite.io.console import puts

public function classifyChar( Char character ) -> String:
    if character.isAlpha():
        return "alphabetic"
    if character.isDigit():
        return "digit"
    if character.isWhitespace():
        return "whitespace"
    return "symbol"

public function caesarShift( Char character, I32 shift ) -> Char:
    if character.isAlpha() == false:
        return character
    I32 base = 0
    if character >= 'a' and character <= 'z':
        base = 'a' as I32
    else:
        base = 'A' as I32
    I32 code = character as I32
    I32 shifted = ( ( code - base + shift ) % 26 + 26 ) % 26 + base
    return shifted as Char

public function main() -> I32:
    Char letter = 'H'
    Char digit = '5'
    Char whitespace = '\t'
    Char symbol = '@'

    puts( classifyChar( letter ) )
    puts( classifyChar( digit ) )
    puts( classifyChar( whitespace ) )
    puts( classifyChar( symbol ) )

    Char upper = letter.toUpper()
    Char lower = letter.toLower()

    Char shifted = caesarShift( 'A', 3 )
    puts( shifted.toString() )

    return 0
```

This example demonstrates character classification using the `Char` OOP wrapper methods, case conversion via `toUpper()`/`toLower()`, integer-to-character casting with the `as` operator, and character arithmetic for a Caesar cipher shift.
