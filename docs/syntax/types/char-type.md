# Char Type

---

## Table of Contents

- [Char Type](#char-type)
  - [Table of Contents](#table-of-contents)
  - [Overview](#overview)
  - [Character Literals](#character-literals)
    - [Basic Syntax](#basic-syntax)
    - [Escape Sequences](#escape-sequences)
    - [Hex Escapes](#hex-escapes)
    - [Single Quotes vs Double Quotes](#single-quotes-vs-double-quotes)
  - [Character Classification](#character-classification)
    - [isAlpha](#isalpha)
    - [isDigit](#isdigit)
    - [isAlphanumeric](#isalphanumeric)
    - [isWhitespace](#iswhitespace)
    - [Classification Summary](#classification-summary)
  - [Case Conversion](#case-conversion)
    - [toUpper](#toupper)
    - [toLower](#tolower)
    - [Case Conversion Scope](#case-conversion-scope)
  - [Character Comparisons](#character-comparisons)
    - [Equality and Inequality](#equality-and-inequality)
    - [Relational Comparisons](#relational-comparisons)
    - [Character Ordering](#character-ordering)
  - [Casting and Conversions](#casting-and-conversions)
    - [Char to Integer](#char-to-integer)
    - [Integer to Char](#integer-to-char)
    - [Character Arithmetic](#character-arithmetic)
  - [Char Methods](#char-methods)
    - [Method Reference](#method-reference)
    - [Using toString](#using-tostring)
    - [Using getValue](#using-getvalue)
    - [Using equals](#using-equals)
    - [Using hash](#using-hash)
  - [Practical Examples](#practical-examples)
    - [Vowel Detection](#vowel-detection)
    - [Caesar Cipher](#caesar-cipher)
    - [Character Classifier](#character-classifier)
    - [Manual Case Conversion](#manual-case-conversion)

---

## Overview

`Char` represents a single character. Each `Char` value holds one Unicode code point stored as a 32-bit integer, wide enough to represent any character from the basic ASCII range through the full Unicode scalar range (U+0000 through U+10FFFF).

| Property | Value |
|---|---|
| Type name | `Char` |
| Storage size | 4 bytes |
| Value range | U+0000 to U+10FFFF |
| Literal delimiter | Single quotes (`'...'`) |
| Subclassable | No (`final` class) |

`Char` is distinct from both integers and strings. `'A'` is a `Char`, while `"A"` is a `String`. Although `Char` is stored as a 32-bit integer internally, it is not treated as a numeric type — you cannot perform arithmetic directly on `Char` values. To do math with character code points, cast to an integer type first with `as`.

---

## Character Literals

### Basic Syntax

Character literals are enclosed in single quotes. Each literal contains exactly one character or one escape sequence:

```uranite
Char letter = 'A'
Char digit = '7'
Char space = ' '
Char at = '@'
```

Every character literal must contain exactly one character. Empty character literals (`''`) and multi-character literals (`'AB'`) are not valid.

### Escape Sequences

When a character cannot be typed directly — because it is a control character, or because it conflicts with the delimiter — use an escape sequence. An escape sequence begins with a backslash (`\`) followed by a specific character:

| Escape | Character | Description |
|---|---|---|
| `\'` | `'` | Single quote (the delimiter itself) |
| `\\` | `\` | Backslash |
| `\0` | NUL | Null character (code point 0) |
| `\n` | LF | Line feed (newline) |
| `\r` | CR | Carriage return |
| `\t` | TAB | Horizontal tab |

```uranite
Char singleQuote = '\''
Char backslash = '\\'
Char nullChar = '\0'
Char newline = '\n'
Char tab = '\t'
Char carriageReturn = '\r'
```

The `\'` escape is necessary because an unescaped single quote inside a character literal would be interpreted as the closing delimiter.

### Hex Escapes

The `\x` escape allows specifying a character by its hexadecimal code point value. It reads exactly two hexadecimal digits:

```uranite
Char hexA = '\x41'
Char hexSpace = '\x20'
Char hexTilde = '\x7E'
```

The hex escape `\x41` produces the same character as `'A'` — both represent the character at code point 65 (0x41). The hex range is 0x00 through 0xFF (0 through 255), covering the full ASCII range and the Latin-1 supplement.

Some common hex escape values:

| Escape | Code Point | Character |
|---|---|---|
| `'\x00'` | 0 | NUL (same as `'\0'`) |
| `'\x07'` | 7 | BEL (terminal bell) |
| `'\x09'` | 9 | TAB (same as `'\t'`) |
| `'\x0A'` | 10 | LF (same as `'\n'`) |
| `'\x1B'` | 27 | ESC (terminal escape) |
| `'\x20'` | 32 | Space |
| `'\x41'` | 65 | `A` |
| `'\x7F'` | 127 | DEL |

For characters beyond the 0x00–0xFF range, use integer-to-Char casting instead of hex escapes.

### Single Quotes vs Double Quotes

Single quotes produce a `Char`. Double quotes produce a `String`. This distinction is absolute — there is no implicit conversion between them:

```uranite
Char letter = 'A'
String text = "A"
```

These are different types. `letter` is a single character value. `text` is a string containing one character. To convert between them, use the `toString()` method on `Char` values.

---

## Character Classification

`Char` provides four classification methods that test whether a character falls into specific categories. All classification operates on the ASCII range — characters with code points above 127 return `False` for all classification methods.

### isAlpha

`isAlpha()` returns `True` if the character is an ASCII letter — uppercase `A` through `Z` or lowercase `a` through `z`:

```uranite
from uranite.io.console import puts

public function main() -> I32:
    Char letter = 'A'
    Boolean result = letter.isAlpha()
    puts( "A isAlpha: " + result.toString() )
    Char digit = '5'
    Boolean digitResult = digit.isAlpha()
    puts( "5 isAlpha: " + digitResult.toString() )
    Char bang = '!'
    Boolean bangResult = bang.isAlpha()
    puts( "! isAlpha: " + bangResult.toString() )
    return 0
```

This outputs:

```
A isAlpha: True
5 isAlpha: False
! isAlpha: False
```

### isDigit

`isDigit()` returns `True` if the character is an ASCII digit — `0` through `9`:

```uranite
from uranite.io.console import puts

public function main() -> I32:
    Char digit = '5'
    Boolean result = digit.isDigit()
    puts( "5 isDigit: " + result.toString() )
    Char letter = 'A'
    Boolean letterResult = letter.isDigit()
    puts( "A isDigit: " + letterResult.toString() )
    return 0
```

This outputs:

```
5 isDigit: True
A isDigit: False
```

### isAlphanumeric

`isAlphanumeric()` returns `True` if the character is either a letter or a digit — equivalent to `isAlpha() or isDigit()`:

```uranite
from uranite.io.console import puts

public function main() -> I32:
    Char letter = 'A'
    Boolean letterResult = letter.isAlphanumeric()
    puts( "A isAlphanumeric: " + letterResult.toString() )
    Char digit = '5'
    Boolean digitResult = digit.isAlphanumeric()
    puts( "5 isAlphanumeric: " + digitResult.toString() )
    Char bang = '!'
    Boolean bangResult = bang.isAlphanumeric()
    puts( "! isAlphanumeric: " + bangResult.toString() )
    return 0
```

This outputs:

```
A isAlphanumeric: True
5 isAlphanumeric: True
! isAlphanumeric: False
```

### isWhitespace

`isWhitespace()` returns `True` if the character is one of four ASCII whitespace characters: space (` `), horizontal tab (`\t`), line feed (`\n`), or carriage return (`\r`):

```uranite
from uranite.io.console import puts

public function main() -> I32:
    Char spaceChar = ' '
    Boolean spaceResult = spaceChar.isWhitespace()
    puts( "space isWhitespace: " + spaceResult.toString() )
    Char tabChar = '\t'
    Boolean tabResult = tabChar.isWhitespace()
    puts( "tab isWhitespace: " + tabResult.toString() )
    Char newlineChar = '\n'
    Boolean nlResult = newlineChar.isWhitespace()
    puts( "newline isWhitespace: " + nlResult.toString() )
    Char crChar = '\r'
    Boolean crResult = crChar.isWhitespace()
    puts( "CR isWhitespace: " + crResult.toString() )
    Char letter = 'A'
    Boolean letterResult = letter.isWhitespace()
    puts( "A isWhitespace: " + letterResult.toString() )
    return 0
```

This outputs:

```
space isWhitespace: True
tab isWhitespace: True
newline isWhitespace: True
CR isWhitespace: True
A isWhitespace: False
```

### Classification Summary

| Method | Returns `True` For | Range |
|---|---|---|
| `isAlpha()` | `A`–`Z`, `a`–`z` | 52 characters |
| `isDigit()` | `0`–`9` | 10 characters |
| `isAlphanumeric()` | Letters and digits | 62 characters |
| `isWhitespace()` | Space, tab, newline, carriage return | 4 characters |

All four methods return `False` for characters outside their defined ranges. Punctuation, symbols, and characters above code point 127 return `False` for every classification method.

---

## Case Conversion

### toUpper

`toUpper()` converts a lowercase ASCII letter to its uppercase equivalent. Characters that are not lowercase ASCII letters are returned unchanged:

```uranite
from uranite.io.console import puts

public function main() -> I32:
    Char lower = 'z'
    Char upper = lower.toUpper()
    puts( "z toUpper: " + upper.toString() )
    Char already = 'A'
    Char still = already.toUpper()
    puts( "A toUpper: " + still.toString() )
    Char digit = '5'
    Char sameDigit = digit.toUpper()
    puts( "5 toUpper: " + sameDigit.toString() )
    return 0
```

This outputs:

```
z toUpper: Z
A toUpper: A
5 toUpper: 5
```

### toLower

`toLower()` converts an uppercase ASCII letter to its lowercase equivalent. Characters that are not uppercase ASCII letters are returned unchanged:

```uranite
from uranite.io.console import puts

public function main() -> I32:
    Char upper = 'H'
    Char lower = upper.toLower()
    puts( "H toLower: " + lower.toString() )
    Char already = 'h'
    Char still = already.toLower()
    puts( "h toLower: " + still.toString() )
    Char symbol = '@'
    Char sameSymbol = symbol.toLower()
    puts( "@ toLower: " + sameSymbol.toString() )
    return 0
```

This outputs:

```
H toLower: h
h toLower: h
@ toLower: @
```

### Case Conversion Scope

Case conversion operates strictly within the ASCII letter ranges. The uppercase range is code points 65 (`A`) through 90 (`Z`). The lowercase range is code points 97 (`a`) through 122 (`z`). The distance between corresponding uppercase and lowercase letters is exactly 32 code points — `'A'` (65) and `'a'` (97) differ by 32.

Characters outside these two ranges — digits, punctuation, symbols, control characters, and non-ASCII characters — pass through `toUpper()` and `toLower()` unchanged.

---

## Character Comparisons

Characters are compared by their code point values. All six comparison operators produce `Boolean` results.

### Equality and Inequality

```uranite
from uranite.io.console import puts

public function main() -> I32:
    Char first = 'M'
    Char second = 'M'
    Boolean eqCheck = first == second
    puts( "M == M: " + eqCheck.toString() )
    Char third = 'N'
    Boolean neqCheck = first != third
    puts( "M != N: " + neqCheck.toString() )
    return 0
```

This outputs:

```
M == M: True
M != N: True
```

### Relational Comparisons

The `<`, `>`, `<=`, and `>=` operators compare characters by their code point values:

```uranite
from uranite.io.console import puts

public function main() -> I32:
    Char lower = 'a'
    Char upper = 'A'
    Boolean lessCheck = upper < lower
    puts( "A < a: " + lessCheck.toString() )
    Boolean geCheck = lower >= upper
    puts( "a >= A: " + geCheck.toString() )
    Char digitZero = '0'
    Char digitNine = '9'
    Boolean digitOrder = digitZero < digitNine
    puts( "0 < 9: " + digitOrder.toString() )
    return 0
```

This outputs:

```
A < a: True
a >= A: True
0 < 9: True
```

`'A'` (code point 65) is less than `'a'` (code point 97) because comparisons use the numeric code point value, not alphabetical order.

### Character Ordering

Characters follow Unicode code point order, which matches ASCII ordering for the common character ranges:

| Range | Characters | Code Points |
|---|---|---|
| Digits | `'0'` through `'9'` | 48 through 57 |
| Uppercase letters | `'A'` through `'Z'` | 65 through 90 |
| Lowercase letters | `'a'` through `'z'` | 97 through 122 |

This means digits come before uppercase letters, and uppercase letters come before lowercase letters:

```uranite
from uranite.io.console import puts

public function main() -> I32:
    Boolean digitBeforeLetter = '9' < 'A'
    puts( "9 < A: " + digitBeforeLetter.toString() )
    Boolean upperBeforeLower = 'Z' < 'a'
    puts( "Z < a: " + upperBeforeLower.toString() )
    return 0
```

This outputs:

```
9 < A: True
Z < a: True
```

---

## Casting and Conversions

### Char to Integer

The `as` keyword casts a `Char` to an integer type, extracting the numeric code point value:

```uranite
from uranite.io.console import puts

public function main() -> I32:
    Char letter = 'A'
    I32 codePoint = letter as I32
    puts( "A as I32: " + codePoint.toString() )
    I64 wideCode = letter as I64
    puts( "A as I64: " + wideCode.toString() )
    I32 zCode = 'Z' as I32
    puts( "Z as I32: " + zCode.toString() )
    return 0
```

This outputs:

```
A as I32: 65
A as I64: 65
Z as I32: 90
```

`Char` can be cast to any integer type: `I8`, `I16`, `I32`, `I64`, and their unsigned counterparts. When casting to a smaller integer type, the value is truncated — only use smaller types if you know the code point fits within the target range.

### Integer to Char

Casting an integer to `Char` interprets the integer value as a Unicode code point:

```uranite
from uranite.io.console import puts

public function main() -> I32:
    I32 code = 66
    Char fromCode = code as Char
    puts( "66 as Char: " + fromCode.toString() )
    Char exclaim = 33 as Char
    puts( "33 as Char: " + exclaim.toString() )
    return 0
```

This outputs:

```
66 as Char: B
33 as Char: !
```

Code point 66 is the letter `B`. Code point 33 is the exclamation mark `!`. Any integer value within the valid Unicode range can be cast to `Char`.

### Character Arithmetic

`Char` values cannot be used directly in arithmetic expressions. To perform arithmetic with character code points, cast to an integer, do the math, then cast back:

```uranite
from uranite.io.console import puts

public function main() -> I32:
    Char letter = 'A'
    I32 codePoint = letter as I32
    I32 offset = codePoint + 3
    Char shifted = offset as Char
    puts( "A + 3: " + shifted.toString() )
    Char lowerA = 'a'
    Char upperA = 'A'
    I32 distance = lowerA as I32 - upperA as I32
    puts( "a - A distance: " + distance.toString() )
    Char lowerZ = 'z'
    I32 lowerCode = lowerZ as I32
    I32 upperCode = lowerCode - 32
    Char upperZ = upperCode as Char
    puts( "z to upper: " + upperZ.toString() )
    return 0
```

This outputs:

```
A + 3: D
a - A distance: 32
z to upper: Z
```

The pattern is always: cast `Char` to integer with `as`, perform arithmetic on the integer, cast the result back to `Char` with `as`. The intermediate integer variables are necessary — you cannot chain `as` casts in a single expression.

---

## Char Methods

`Char` is a `final` class — it cannot be subclassed. It provides classification, conversion, and utility methods.

**Important:** When using the result of a method call in another method call, always store intermediate results in variables. Do not chain method calls.

### Method Reference

| Method | Return Type | Description |
|---|---|---|
| `getValue()` | `Char` | Returns the character value |
| `toString()` | `String` | Converts to a single-character string |
| `hash()` | `I64` | Returns the code point as a hash value |
| `equals( Char other )` | `Boolean` | Returns `True` if both characters are identical |
| `isAlpha()` | `Boolean` | Returns `True` for ASCII letters |
| `isDigit()` | `Boolean` | Returns `True` for ASCII digits |
| `isAlphanumeric()` | `Boolean` | Returns `True` for ASCII letters or digits |
| `isWhitespace()` | `Boolean` | Returns `True` for space, tab, newline, or carriage return |
| `toUpper()` | `Char` | Converts lowercase ASCII letter to uppercase |
| `toLower()` | `Char` | Converts uppercase ASCII letter to lowercase |

### Using toString

`toString()` converts a `Char` to a `String` containing that single character. This is essential when you need to include a character in string concatenation:

```uranite
from uranite.io.console import puts

public function main() -> I32:
    Char letter = 'A'
    puts( letter.toString() )
    Char digit = '7'
    String message = "digit is: " + digit.toString()
    puts( message )
    return 0
```

This outputs:

```
A
digit is: 7
```

String concatenation with `+` requires both operands to be strings, so `toString()` is needed whenever a `Char` value appears in a concatenated expression.

### Using getValue

`getValue()` returns the character value itself. It exists for consistency with other wrapper types:

```uranite
from uranite.io.console import puts

public function main() -> I32:
    Char letter = 'G'
    Char value = letter.getValue()
    puts( "getValue G: " + value.toString() )
    return 0
```

This outputs `getValue G: G`.

### Using equals

`equals()` compares two `Char` values for equality, returning a `Boolean`:

```uranite
from uranite.io.console import puts

public function main() -> I32:
    Char first = 'M'
    Char second = 'M'
    Boolean eq = first.equals( second )
    puts( "M equals M: " + eq.toString() )
    Char third = 'N'
    Boolean neq = first.equals( third )
    puts( "M equals N: " + neq.toString() )
    return 0
```

This outputs:

```
M equals M: True
M equals N: False
```

The `equals()` method produces the same result as the `==` operator. Use whichever reads more clearly in context.

### Using hash

`hash()` returns the code point value as an `I64`, suitable for use as a hash code:

```uranite
from uranite.io.console import puts

public function main() -> I32:
    Char letter = 'G'
    I64 hashVal = letter.hash()
    puts( "hash G: " + hashVal.toString() )
    return 0
```

This outputs `hash G: 71`, which is the code point of `G`.

---

## Practical Examples

### Vowel Detection

Detect whether a character is an English vowel, handling both uppercase and lowercase input:

```uranite
from uranite.io.console import puts

public function isVowel( Char character ) -> Boolean:
    Char lower = character.toLower()
    if lower == 'a':
        return True
    if lower == 'e':
        return True
    if lower == 'i':
        return True
    if lower == 'o':
        return True
    if lower == 'u':
        return True
    return False

public function main() -> I32:
    Boolean vowelE = isVowel( 'E' )
    puts( "E is vowel: " + vowelE.toString() )
    Boolean vowelX = isVowel( 'X' )
    puts( "X is vowel: " + vowelX.toString() )
    Boolean vowelA = isVowel( 'a' )
    puts( "a is vowel: " + vowelA.toString() )
    return 0
```

This outputs:

```
E is vowel: True
X is vowel: False
a is vowel: True
```

The function converts to lowercase first with `toLower()`, then checks against each vowel individually. This handles both `'E'` and `'e'` with the same code path.

### Caesar Cipher

Encrypt and decrypt characters using a Caesar cipher — shifting each letter by a fixed number of positions in the alphabet, wrapping around at the boundaries:

```uranite
from uranite.io.console import puts

public function caesarShift( Char character, I32 shift ) -> Char:
    Boolean isAlph = character.isAlpha()
    if isAlph:
        Char base = 'a'
        Boolean isUpper = character >= 'A' and character <= 'Z'
        if isUpper:
            base = 'A'
        I32 charCode = character as I32
        I32 baseCode = base as I32
        I32 offset = charCode - baseCode
        I32 shifted = (offset + shift) % 26
        if shifted < 0:
            shifted = shifted + 26
        I32 resultCode = baseCode + shifted
        return resultCode as Char
    return character

public function main() -> I32:
    Char encrypted = caesarShift( 'H', 3 )
    puts( "H + 3: " + encrypted.toString() )
    Char decrypted = caesarShift( encrypted, -3 )
    puts( "decrypt: " + decrypted.toString() )
    Char wrapAround = caesarShift( 'z', 1 )
    puts( "z + 1: " + wrapAround.toString() )
    Char nonAlpha = caesarShift( '!', 5 )
    puts( "! + 5: " + nonAlpha.toString() )
    return 0
```

This outputs:

```
H + 3: K
decrypt: H
z + 1: a
! + 5: !
```

The function determines the base character (`'A'` for uppercase, `'a'` for lowercase), computes the offset from that base, applies the shift with modular arithmetic to wrap around the 26-letter alphabet, and converts back to a character. Non-alphabetic characters pass through unchanged.

Negative shifts decrypt — shifting `'K'` by -3 returns it to `'H'`. The `if shifted < 0` guard handles negative modulo results by adding 26 to bring the value back into the 0–25 range.

### Character Classifier

Classify a character into one of several categories using the classification methods:

```uranite
from uranite.io.console import puts

public function classify( Char character ) -> String:
    Boolean isAlph = character.isAlpha()
    if isAlph:
        Boolean isUp = character >= 'A' and character <= 'Z'
        if isUp:
            return "uppercase letter"
        return "lowercase letter"
    Boolean isDig = character.isDigit()
    if isDig:
        return "digit"
    Boolean isWs = character.isWhitespace()
    if isWs:
        return "whitespace"
    return "symbol"

public function main() -> I32:
    puts( "A: " + classify( 'A' ) )
    puts( "m: " + classify( 'm' ) )
    puts( "7: " + classify( '7' ) )
    puts( "space: " + classify( ' ' ) )
    puts( "@: " + classify( '@' ) )
    return 0
```

This outputs:

```
A: uppercase letter
m: lowercase letter
7: digit
space: whitespace
@: symbol
```

The function tests each category in order — letters first (subdivided into uppercase and lowercase), then digits, then whitespace. Anything that does not match any category falls through to "symbol".

### Manual Case Conversion

Convert between uppercase and lowercase using code point arithmetic instead of the built-in `toUpper()` and `toLower()` methods, demonstrating how character arithmetic works at a fundamental level:

```uranite
from uranite.io.console import puts

public function manualToUpper( Char character ) -> Char:
    Boolean isLower = character >= 'a' and character <= 'z'
    if isLower:
        I32 code = character as I32
        I32 upperCode = code - 32
        return upperCode as Char
    return character

public function manualToLower( Char character ) -> Char:
    Boolean isUp = character >= 'A' and character <= 'Z'
    if isUp:
        I32 code = character as I32
        I32 lowerCode = code + 32
        return lowerCode as Char
    return character

public function main() -> I32:
    Char upper = manualToUpper( 'q' )
    puts( "q to upper: " + upper.toString() )
    Char lower = manualToLower( 'Q' )
    puts( "Q to lower: " + lower.toString() )
    Char unchanged = manualToUpper( '5' )
    puts( "5 to upper: " + unchanged.toString() )
    return 0
```

This outputs:

```
q to upper: Q
Q to lower: q
5 to upper: 5
```

The ASCII code point distance between uppercase and lowercase letters is always 32. Subtracting 32 from a lowercase letter's code point produces its uppercase equivalent. Adding 32 to an uppercase letter produces its lowercase equivalent. Characters outside the letter ranges are returned unchanged.

In practice, use the built-in `toUpper()` and `toLower()` methods instead of manual arithmetic. This example demonstrates the underlying principle for educational purposes.
