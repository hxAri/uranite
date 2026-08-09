# Character Literals

Uranite supports character literals delimited by single quotes (`'...'`). A character literal represents exactly one character — a single value typed as `Char`. This document specifies character literal syntax, the distinction between characters and strings, escape sequences, the `Char` type and its methods, character-to-integer conversion, and edge cases.

---

## Table of Contents

- [Character Literals](#character-literals)
  - [Table of Contents](#table-of-contents)
  - [Overview](#overview)
  - [Character Literal Syntax](#character-literal-syntax)
  - [Single Quotes vs Double Quotes](#single-quotes-vs-double-quotes)
  - [Escape Sequences](#escape-sequences)
    - [Standard Escapes](#standard-escapes)
    - [Hex Byte Escape](#hex-byte-escape)
    - [Unknown Escapes](#unknown-escapes)
  - [Exactly One Character](#exactly-one-character)
    - [Empty Character Literals](#empty-character-literals)
    - [Multi-Character Literals](#multi-character-literals)
  - [The Char Type](#the-char-type)
    - [32-Bit Representation](#32-bit-representation)
    - [Method Access on Characters](#method-access-on-characters)
    - [Character Classification Methods](#character-classification-methods)
    - [Case Conversion Methods](#case-conversion-methods)
    - [Object Methods](#object-methods)
  - [Character and Integer Conversion](#character-and-integer-conversion)
    - [Char to Integer](#char-to-integer)
    - [Integer to Char](#integer-to-char)
    - [Character Arithmetic](#character-arithmetic)
  - [Char vs String](#char-vs-string)
  - [Examples](#examples)
    - [Valid Character Literals](#valid-character-literals)
    - [Escape Sequence Examples](#escape-sequence-examples)
    - [Invalid Character Literals](#invalid-character-literals)
    - [Practical Usage](#practical-usage)

---

## Overview

A character literal is a single character enclosed in single quotes. Escape sequences are resolved at compile time — the resulting character contains the actual byte value, not the backslash-prefixed source text. Every character literal has the type `Char`.

| Property | Value |
|---|---|
| Delimiter | Single quotes (`'...'`) |
| Content | Exactly one character or one escape sequence |
| Escape processing | Yes |
| Default type | `Char` |
| Width | 32 bits |
| Mutability | Immutable (final class) |

---

## Character Literal Syntax

A character literal begins with a single-quote character (`'`), contains exactly one character or escape sequence, and ends with a matching single-quote:

```uranite
Char letter = 'A'
Char digit = '7'
Char space = ' '
Char exclamation = '!'
Char at = '@'
```

Every character literal must contain exactly one logical character. The content between the quotes is either a single visible character or a single escape sequence (which itself represents one character).

---

## Single Quotes vs Double Quotes

Single quotes and double quotes serve strictly different purposes in Uranite:

| Delimiter | Type | Example | Produces |
|---|---|---|---|
| `'...'` | `Char` | `'A'` | A single character value |
| `"..."` | `String` | `"A"` | A string containing one character |

A single character in double quotes (`"x"`) produces a `String`, not a `Char`. A character in single quotes (`'x'`) produces a `Char`, not a `String`. There is no implicit conversion between them at the syntax level.

This distinction matters when calling functions that expect a specific type:

```uranite
Char letter = 'A'
String text = "A"
```

The variable `letter` is a `Char`. The variable `text` is a `String`. They hold the same character content but are different types. Use single quotes when a `Char` is expected, double quotes when a `String` is expected.

---

## Escape Sequences

Character literals support escape sequences using the backslash (`\`) prefix, similar to string literals.

### Standard Escapes

Six standard escape sequences are recognized in character literals:

| Escape | Description | Example |
|---|---|---|
| `\'` | Single-quote character | `'\''` |
| `\0` | Null byte (zero value) | `'\0'` |
| `\\` | Backslash character | `'\\'` |
| `\n` | Newline (line feed) | `'\n'` |
| `\r` | Carriage return | `'\r'` |
| `\t` | Horizontal tab | `'\t'` |

Each escape sequence consumes two source characters (the backslash and the escape letter) and produces exactly one character value. The `\'` escape is essential for representing a single-quote character, since an unescaped `'` would terminate the character literal.

Note that `\"` (double-quote escape) is not listed here. String literals use `\"` to embed double quotes. Character literals use `\'` to embed single quotes. A double quote inside a character literal does not need escaping — `'"'` is valid and produces the `"` character directly.

### Hex Byte Escape

The `\x` escape inserts a character by its hexadecimal byte value. After `\x`, the compiler reads up to 2 hexadecimal digits (`0`-`9`, `a`-`f`, `A`-`F`) and converts them to a single character:

| Escape | Decimal | Character |
|---|---|---|
| `'\x41'` | 65 | `A` |
| `'\x61'` | 97 | `a` |
| `'\x0A'` | 10 | newline (same as `'\n'`) |
| `'\x09'` | 9 | tab (same as `'\t'`) |
| `'\x00'` | 0 | null (same as `'\0'`) |
| `'\x7F'` | 127 | DEL |
| `'\x20'` | 32 | space |

The hex digits are case-insensitive — `'\x41'` and `'\x41'` produce the same result. If no hex digits follow `\x`, the compiler emits an error:

```
error: expected hex digits after \x in character literal
  --> source.urn:3:5
```

### Unknown Escapes

If the character after the backslash is not one of the recognized escape identifiers (`'`, `0`, `\`, `n`, `r`, `t`, `x`), the compiler silently treats it as a literal character. The backslash is consumed and the following character becomes the value:

| Source | Value | Notes |
|---|---|---|
| `'\q'` | `q` | Unknown escape, `q` used literally |
| `'\a'` | `a` | Unknown escape, `a` used literally |
| `'\"'` | `"` | Double quote does not need escaping in char literals |

Unlike string literals, which emit a warning for unknown escape sequences, character literals accept unknown escapes silently. This means `'\a'` compiles without any diagnostic and produces the character `a`.

---

## Exactly One Character

A character literal must contain exactly one logical character — one visible character or one escape sequence. Violations of this rule produce errors.

### Empty Character Literals

An empty character literal (`''`) with nothing between the quotes is accepted syntactically but produces a null character (`'\0'`). The compiler does not emit an error or warning for this case. The result is equivalent to writing `'\0'` explicitly:

```uranite
Char empty = ''
Char null = '\0'
```

Both variables hold the null character (value 0). In practice, prefer `'\0'` over `''` for clarity.

### Multi-Character Literals

Multi-character literals are not valid. Writing more than one character between the quotes causes a compilation error:

```
error: unterminated character literal
  --> source.urn:3:5
```

The compiler reads the first character as the value, then expects the closing quote. When the second character occupies the position where the closing quote should be, the compiler reports an unterminated literal. For example, `'ab'` processes `a` as the character, then finds `b` where `'` is expected, producing the error.

To represent multi-character content, use a string literal: `"ab"`.

---

## The Char Type

### 32-Bit Representation

The `Char` type is stored as a 32-bit integer value. This width accommodates the full Unicode scalar value range (U+0000 through U+10FFFF), even though character literals in source code currently process single-byte characters.

The 32-bit width matches character representations in languages like Rust (`char` = 4 bytes) and Go (`rune` = `int32`). A single `Char` can represent any Unicode code point without surrogate encoding.

### Method Access on Characters

Because `Char` is an object type (a `final` class that cannot be subclassed), character values have method access. Every character literal supports classification, conversion, and object methods.

### Character Classification Methods

Four classification methods query character properties using ASCII range comparisons:

| Method | Return Type | Description |
|---|---|---|
| `isAlpha()` | `Boolean` | Return `True` if the character is an alphabetic letter (`a`-`z` or `A`-`Z`) |
| `isDigit()` | `Boolean` | Return `True` if the character is a decimal digit (`0`-`9`) |
| `isAlphanumeric()` | `Boolean` | Return `True` if the character is alphabetic or a digit |
| `isWhitespace()` | `Boolean` | Return `True` if the character is space, tab, newline, or carriage return |

These methods operate on ASCII ranges:

```uranite
Char letter = 'A'
Char digit = '5'
Char space = ' '
Char symbol = '@'

Boolean alpha = letter.isAlpha()
Boolean numeric = digit.isDigit()
Boolean alnum = letter.isAlphanumeric()
Boolean white = space.isWhitespace()
Boolean symbolAlpha = symbol.isAlpha()
```

The variable `alpha` is `True` (A is alphabetic). The variable `numeric` is `True` (5 is a digit). The variable `alnum` is `True` (A is alphanumeric). The variable `white` is `True` (space is whitespace). The variable `symbolAlpha` is `False` (@ is not alphabetic).

The `isWhitespace()` method recognizes exactly four characters: space (`' '`), tab (`'\t'`), newline (`'\n'`), and carriage return (`'\r'`). Other Unicode whitespace characters (non-breaking space, em space, etc.) are not recognized by this method.

The `isAlphanumeric()` method is equivalent to `isAlpha() or isDigit()` — it does not recognize underscores, hyphens, or other characters that some definitions of "alphanumeric" might include.

### Case Conversion Methods

Two methods convert letter case using ASCII arithmetic:

| Method | Return Type | Description |
|---|---|---|
| `toUpper()` | `Char` | Return uppercase copy. Non-lowercase letters pass through unchanged. |
| `toLower()` | `Char` | Return lowercase copy. Non-uppercase letters pass through unchanged. |

Case conversion uses the ASCII offset of 32 between uppercase and lowercase letters:

```uranite
Char lower = 'a'
Char upper = lower.toUpper()

Char capital = 'Z'
Char small = capital.toLower()

Char digit = '5'
Char unchanged = digit.toUpper()
```

The variable `upper` holds `'A'`. The variable `small` holds `'z'`. The variable `unchanged` holds `'5'` — non-letter characters pass through case conversion unchanged.

Both methods return new `Char` instances. The original character is never modified (the `Char` class is immutable).

Internally, `toUpper()` converts by subtracting 32 from the character's integer value when the character is in the range `'a'` to `'z'`. `toLower()` adds 32 when the character is in the range `'A'` to `'Z'`. This arithmetic uses the `as` cast operator to cross the type boundary between `Char` and `I32`:

```uranite
Char lowerA = 'a'
Char upperA = ( lowerA as I32 - 32 ) as Char
```

The two-step cast (Char to I32, arithmetic, I32 back to Char) is necessary because arithmetic operators are not defined directly on `Char`.

### Object Methods

| Method | Return Type | Description |
|---|---|---|
| `getValue()` | `Char` | Return the underlying character value |
| `toString()` | `String` | Return the character as a single-character `String` |

The `toString()` method converts the character to a string via the `as` cast operator. This is useful for concatenation:

```uranite
Char initial = 'J'
String greeting = "Hello, " + initial.toString() + "!"
```

The variable `greeting` holds "Hello, J!".

---

## Character and Integer Conversion

Characters and integers are convertible using the `as` cast operator. The `Char` type stores a 32-bit integer value representing the character's code point.

### Char to Integer

The `as` operator converts a `Char` to its numeric code point value:

```uranite
Char letter = 'A'
I32 codePoint = letter as I32
```

The variable `codePoint` holds `65` (the ASCII code for "A").

Common ASCII values:

| Character | Code Point | Description |
|---|---|---|
| `'A'` | 65 | First uppercase letter |
| `'Z'` | 90 | Last uppercase letter |
| `'a'` | 97 | First lowercase letter |
| `'z'` | 122 | Last lowercase letter |
| `'0'` | 48 | First digit |
| `'9'` | 57 | Last digit |
| `' '` | 32 | Space |
| `'\n'` | 10 | Newline |
| `'\t'` | 9 | Tab |
| `'\0'` | 0 | Null |

### Integer to Char

The reverse conversion creates a `Char` from an integer code point:

```uranite
I32 code = 72
Char letter = code as Char
```

The variable `letter` holds `'H'` (ASCII code 72).

No range checking is performed during this conversion. Any 32-bit integer can be cast to `Char`, even values outside the valid Unicode range. It is the programmer's responsibility to ensure the integer represents a valid character.

### Character Arithmetic

Because `Char` does not support arithmetic operators directly, character arithmetic requires casting through an integer type:

```uranite
Char letter = 'A'
Char next = ( letter as I32 + 1 ) as Char
Char tenth = ( letter as I32 + 9 ) as Char
```

The variable `next` holds `'B'` (65 + 1 = 66). The variable `tenth` holds `'J'` (65 + 9 = 74).

This pattern is common for:

- **Iterating through a character range**: increment by 1 to walk from `'A'` to `'Z'`
- **Computing offsets**: subtract `'A'` to get 0-based letter indices (0-25)
- **Caesar cipher**: shift characters by a fixed offset with modular wrapping
- **Case conversion**: add or subtract 32 to switch between uppercase and lowercase

```uranite
Char start = 'a'
I32 index = start as I32 - 'a' as I32
```

The variable `index` holds `0` — the zero-based position of `'a'` in the alphabet.

---

## Char vs String

`Char` and `String` are distinct types that serve different purposes:

| Property | `Char` | `String` |
|---|---|---|
| Delimiter | Single quotes (`'...'`) | Double quotes (`"..."`) |
| Content | Exactly one character | Zero or more characters |
| Width | 32 bits (fixed) | Variable length (byte sequence) |
| Mutability | Immutable | Immutable |
| Classification | `isAlpha()`, `isDigit()`, etc. | Not available |
| Concatenation | Not directly (convert via `toString()` first) | `+` operator and `concat()` |

A single-character string (`"A"`) and a character literal (`'A'`) are not interchangeable. Functions expecting `Char` require single quotes, functions expecting `String` require double quotes:

```uranite
Char letterChar = 'A'
String letterString = "A"
String fromChar = letterChar.toString()
```

The `toString()` method bridges the gap when a `String` representation of a `Char` is needed.

The `charAt()` method on `String` returns a `Char` — this is the primary way to extract individual characters from a string:

```uranite
String text = "Hello"
Char first = text.charAt( 0 )
Char last = text.charAt( 4 )
```

The variable `first` holds `'H'`. The variable `last` holds `'o'`.

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
Char hash = '#'
Char dollar = '$'
Char underscore = '_'
Char doubleQuote = '"'
```

All printable ASCII characters can appear directly between single quotes without escaping (except the single quote itself, which requires `\'`).

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
Char hexSpace = '\x20'
Char hexTilde = '\x7E'
```

| Variable | Character | Integer Value |
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
| `hexSpace` | space | 32 |
| `hexTilde` | `~` | 126 |

### Invalid Character Literals

The following are **not** valid character literals:

| Literal | Problem |
|---|---|
| `'ab'` | Multi-character literal. Only one character allowed. |
| `'abc'` | Multi-character literal. Use `"abc"` for a string. |
| `'\x'` | Hex escape with no digits. At least one hex digit required after `\x`. |
| `"x"` | Double quotes produce a `String`, not a `Char`. Use `'x'` for a `Char`. |

### Practical Usage

A complete example demonstrating character classification, case conversion, character arithmetic, and integration with strings:

```uranite
from uranite.io.console import puts

public function classifyChar( Char character ) -> String:
    if character.isAlpha():
        if character as I32 >= 'A' as I32 and character as I32 <= 'Z' as I32:
            return "uppercase letter"
        return "lowercase letter"
    if character.isDigit():
        return "digit"
    if character.isWhitespace():
        return "whitespace"
    return "symbol"

public function caesarShift( Char character, I32 shift ) -> Char:
    if character.isAlpha() == False:
        return character
    I32 base = 0
    if character as I32 >= 'a' as I32 and character as I32 <= 'z' as I32:
        base = 'a' as I32
    else:
        base = 'A' as I32
    I32 code = character as I32
    I32 shifted = ( ( code - base + shift ) % 26 + 26 ) % 26 + base
    return shifted as Char

public function charToDigit( Char character ) -> I32:
    return character as I32 - '0' as I32

public function isVowel( Char character ) -> Boolean:
    Char lower = character.toLower()
    return lower == 'a' or lower == 'e' or lower == 'i' or lower == 'o' or lower == 'u'

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
    puts( "Upper: " + upper.toString() )
    puts( "Lower: " + lower.toString() )

    Char shifted = caesarShift( 'A', 3 )
    puts( "Caesar A+3: " + shifted.toString() )

    Char shifted2 = caesarShift( 'Z', 1 )
    puts( "Caesar Z+1: " + shifted2.toString() )

    I32 digitValue = charToDigit( '7' )
    puts( "Digit value: " + digitValue )

    Char testVowel = 'E'
    if isVowel( testVowel ):
        puts( testVowel.toString() + " is a vowel" )

    I32 code = letter as I32
    puts( "Code point of H: " + code )

    Char fromCode = 74 as Char
    puts( "Char from 74: " + fromCode.toString() )

    return 0
```

This example demonstrates character classification with detailed subcategories, case conversion via `toUpper()` and `toLower()`, Caesar cipher shifting with modular wrapping through integer arithmetic, digit-to-integer conversion by subtracting the base character `'0'`, vowel checking using `toLower()` for case-insensitive comparison, `as` casting between `Char` and `I32` in both directions, and `toString()` for converting characters to strings for output.
