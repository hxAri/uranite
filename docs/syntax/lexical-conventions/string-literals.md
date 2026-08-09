# String Literals

Uranite supports single-line string literals delimited by double quotes (`"..."`). Strings are immutable sequences of UTF-8 bytes, typed as `String`. This document specifies string literal syntax, escape sequences, the distinction between strings and doccomments, string methods, concatenation, mixed-type concatenation, and formatting.

---

## Table of Contents

- [String Literals](#string-literals)
  - [Table of Contents](#table-of-contents)
  - [Overview](#overview)
  - [String Literal Syntax](#string-literal-syntax)
  - [Escape Sequences](#escape-sequences)
    - [Standard Escapes](#standard-escapes)
    - [Hex Byte Escape](#hex-byte-escape)
    - [Unknown Escapes](#unknown-escapes)
  - [Strings Are Single-Line Only](#strings-are-single-line-only)
    - [Constructing Multi-Line Content](#constructing-multi-line-content)
  - [Empty Strings](#empty-strings)
  - [Strings vs Doccomments](#strings-vs-doccomments)
  - [Strings vs Character Literals](#strings-vs-character-literals)
  - [Unterminated Strings](#unterminated-strings)
  - [The String Type](#the-string-type)
    - [Method Access on Strings](#method-access-on-strings)
    - [Immutability](#immutability)
  - [String Concatenation](#string-concatenation)
    - [The Plus Operator](#the-plus-operator)
    - [The concat Method](#the-concat-method)
    - [Chained Concatenation](#chained-concatenation)
  - [Mixed-Type Concatenation](#mixed-type-concatenation)
  - [String Formatting](#string-formatting)
    - [Placeholder Syntax](#placeholder-syntax)
    - [Curly Brace Behavior](#curly-brace-behavior)
    - [Excess Placeholders](#excess-placeholders)
    - [Excess Arguments](#excess-arguments)
    - [No Format Specifiers](#no-format-specifiers)
    - [Escape Sequences in Format Templates](#escape-sequences-in-format-templates)
    - [Number Formatting Utilities](#number-formatting-utilities)
    - [format() vs Concatenation](#format-vs-concatenation)
  - [UTF-8 and Multi-Byte Characters](#utf-8-and-multi-byte-characters)
  - [Null Bytes in Strings](#null-bytes-in-strings)
  - [Examples](#examples)
    - [Valid String Literals](#valid-string-literals)
    - [Escape Sequence Examples](#escape-sequence-examples)
    - [Invalid String Literals](#invalid-string-literals)
    - [Practical Usage](#practical-usage)

---

## Overview

A string literal is a sequence of characters enclosed in double quotes. Escape sequences (backslash-prefixed character pairs) are resolved at compile time — the resulting string contains the unescaped bytes. Every string literal has the type `String`.

| Property | Value |
|---|---|
| Delimiter | Double quotes (`"..."`) |
| Multi-line | Not supported (newline inside a string is an error) |
| Escape processing | Yes |
| Default type | `String` |
| Encoding | UTF-8 (inherited from source file) |
| Mutability | Immutable |

---

## String Literal Syntax

A string literal begins with a double-quote character (`"`), contains zero or more characters (including escape sequences), and ends with a matching double-quote:

```uranite
String empty = ""
String greeting = "hello, world"
String path = "/usr/local/bin"
String message = "value is: 42"
```

Strings are delimited exclusively by double quotes. Single quotes are reserved for character literals (see [Char Literals](char-literals.md)). There is no alternative string delimiter — no single-quote strings, no backtick strings, no raw strings.

---

## Escape Sequences

An escape sequence begins with a backslash (`\`) followed by one or more characters that together represent a single byte or character in the output string. Escape sequences are resolved at compile time — the string stored at runtime contains the actual bytes, not the backslash-prefixed source text.

### Standard Escapes

Seven standard escape sequences are supported:

| Escape | Description | Example |
|---|---|---|
| `\"` | Double-quote character | `"she said \"hello\""` |
| `\'` | Single-quote character | `"it\'s here"` |
| `\0` | Null byte (zero byte) | `"before\0after"` |
| `\\` | Backslash character | `"C:\\Users\\Documents"` |
| `\n` | Newline (line feed) | `"line one\nline two"` |
| `\r` | Carriage return | `"overwrite\rthis"` |
| `\t` | Horizontal tab | `"col1\tcol2\tcol3"` |

Each escape sequence consumes two source characters (the backslash and the escape letter) and produces exactly one byte in the resulting string. The `\"` escape is essential for embedding double-quote characters inside a string, since an unescaped `"` would terminate the string.

### Hex Byte Escape

The `\x` escape inserts an arbitrary byte by its hexadecimal value. After `\x`, the compiler reads up to 2 hexadecimal digits (`0`-`9`, `a`-`f`, `A`-`F`) and converts them to a single byte:

| Escape | Byte | Character |
|---|---|---|
| `\x41` | 65 | `A` |
| `\x61` | 97 | `a` |
| `\x0A` | 10 | newline (same as `\n`) |
| `\x09` | 9 | tab (same as `\t`) |
| `\x00` | 0 | null byte (same as `\0`) |
| `\xFF` | 255 | byte 255 |

The hex digits are case-insensitive — `\x41` and `\x41` produce the same result. If fewer than 2 hex digits follow `\x`, the compiler reads as many as are available (minimum 1). Characters after the hex digits that are not hex digits become part of the subsequent string content:

```uranite
String letterA = "\x41"
String letterAB = "\x41\x42"
String mixed = "\x48ello"
```

The variable `letterA` contains "A". The variable `letterAB` contains "AB". The variable `mixed` contains "Hello" — `\x48` produces "H", and "ello" follows as literal text.

### Unknown Escapes

If the character following the backslash is not one of the recognized escape identifiers (`"`, `'`, `0`, `\`, `n`, `r`, `t`, `x`), the compiler emits a warning and includes the character literally without the backslash:

```
warning: unknown escape sequence "\q"
  --> source.urn:3:15
```

The string `"hello\qworld"` produces the value "helloqworld" — the backslash is consumed, the `q` is appended as a literal character, and a warning is emitted. Unknown escapes do not cause compilation errors — they produce warnings, allowing the code to compile while flagging the likely mistake.

---

## Strings Are Single-Line Only

A newline character inside a string literal is a compilation error:

```
error: unterminated string literal
  --> source.urn:3:20
```

Strings cannot span multiple source lines. The opening and closing double quotes must appear on the same line. To include a newline character in a string, use the `\n` escape sequence.

### Constructing Multi-Line Content

To create a string that contains newlines, concatenate single-line strings with embedded `\n` escapes:

```uranite
String multiLine = "first line\n" + "second line\n" + "third line"
```

Or build incrementally with `concat()`:

```uranite
String header = "Name: Alice\n"
String body = "Age: 30\n"
String footer = "City: Tokyo"
String document = header.concat( body ).concat( footer )
```

The resulting string contains newlines between the sections. When printed, it appears as three separate lines.

---

## Empty Strings

An empty string is two consecutive double quotes with nothing between them:

```uranite
String empty = ""
```

The empty string has a length of zero. It is a valid `String` value, not `None`. Testing whether a string is empty:

```uranite
String value = ""
Boolean isEmpty = value.isEmpty()
I64 length = value.length()
```

The variable `isEmpty` is `True`. The variable `length` is `0`.

---

## Strings vs Doccomments

Triple-quoted sequences (`"""..."""`) are **not** string literals. They are doccomments — metadata annotations attached to declarations. The compiler strips them during scanning and they produce no value.

This is a critical distinction from languages like Python, where triple-quoted strings serve as both multi-line strings and docstrings:

| Syntax | What It Is | Produces a Value? |
|---|---|---|
| `"hello"` | String literal | Yes — a `String` value |
| `"""hello"""` | Doccomment | No — stripped during compilation |

You cannot assign a triple-quoted sequence to a variable. Writing `String text = """hello"""` is not valid — the `"""hello"""` is consumed as a doccomment, leaving `String text =` with no right-hand side.

For details on doccomment syntax and placement, see [Comments and Doccomments](comments-and-doccomments.md).

---

## Strings vs Character Literals

Single quotes produce character literals (`Char` type), not strings. Double quotes produce string literals (`String` type):

| Syntax | Type | Value |
|---|---|---|
| `"A"` | `String` | A string containing one character |
| `'A'` | `Char` | A single character |

A single-character string and a character literal are different types. Use `"A"` when a `String` is expected and `'A'` when a `Char` is expected. For details on character literal syntax, see [Char Literals](char-literals.md).

---

## Unterminated Strings

If the compiler reaches the end of the source file without finding a closing double quote, it reports an error at the position where the opening quote was found:

```
error: unterminated string literal
  --> source.urn:3:1
```

This helps locate the start of the unterminated string. Common causes include forgetting the closing quote, or using a single backslash at the end of a string (which escapes the closing quote instead of terminating the string). To end a string with a backslash, use `\\`:

```uranite
String correct = "ends with backslash\\"
```

Writing `"ends with backslash\"` would escape the closing quote, making the string unterminated.

---

## The String Type

Every string literal has the type `String`. The `String` class is an immutable object type that provides methods for querying, transforming, and comparing string content.

### Method Access on Strings

String values support a rich set of methods:

**Query methods** — inspect the string without modifying it:

| Method | Return Type | Description |
|---|---|---|
| `length()` | `I64` | Return the byte length of the string |
| `isEmpty()` | `Boolean` | Return `True` if the string has zero length |
| `contains( String sub )` | `Boolean` | Return `True` if the string contains the given substring |
| `startsWith( String prefix )` | `Boolean` | Return `True` if the string starts with the given prefix |
| `endsWith( String suffix )` | `Boolean` | Return `True` if the string ends with the given suffix |
| `indexOf( String target )` | `I64` | Return the byte index of the first occurrence, or -1 if not found |
| `charAt( I64 index )` | `Char` | Return the character at the given byte index |
| `charCodeAt( I64 index )` | `I64` | Return the numeric byte value (0-255) at the given byte index |

**Transformation methods** — return a new string:

| Method | Return Type | Description |
|---|---|---|
| `concat( String other )` | `String` | Return a new string with `other` appended |
| `toUpper()` | `String` | Return an uppercase copy with all characters converted to uppercase |
| `toLower()` | `String` | Return a lowercase copy with all characters converted to lowercase |
| `trim()` | `String` | Return a copy with leading and trailing whitespace removed |
| `replace( String target, String replacement )` | `String` | Return a copy with all occurrences of `target` replaced by `replacement` |
| `substring( I64 start )` | `String` | Return a substring from byte index `start` to the end |
| `split( String delimiter )` | `ArrayList<String>` | Split into a list of substrings at each delimiter occurrence |
| `format( I64 args[] )` | `String` | Replace `{}` placeholders with positional arguments (see [String Formatting](#string-formatting)) |

**Object methods** — inherited from `Object`:

| Method | Return Type | Description |
|---|---|---|
| `toString()` | `String` | Return the string value unchanged |
| `hashCode()` | `I64` | Return a hash code suitable for hash-based collections |
| `hash()` | `I64` | Return a hash code for this string |
| `equals( String other )` | `Boolean` | Return `True` if both strings have identical content |
| `getValue()` | `String` | Return the underlying string value |

Usage examples:

```uranite
String text = "Hello, World!"

I64 len = text.length()
Boolean hasWorld = text.contains( "World" )
Boolean startsH = text.startsWith( "Hello" )
I64 commaPos = text.indexOf( "," )
Char first = text.charAt( 0 )
I64 byteValue = text.charCodeAt( 0 )

String upper = text.toUpper()
String lower = text.toLower()
String trimmed = "  spaces  ".trim()
String replaced = text.replace( "World", "Uranite" )
String sub = text.substring( 7 )
ArrayList<String> parts = text.split( ", " )
```

The `charCodeAt()` method returns the raw byte value at a given index as an `I64`. For ASCII characters, this is the ASCII code. For example, `"Hello".charCodeAt( 0 )` returns `72` (the ASCII code for "H"). This is distinct from `charAt()`, which returns the character itself as a `Char` type:

```uranite
String greeting = "Hello"
Char letter = greeting.charAt( 0 )
I64 code = greeting.charCodeAt( 0 )
```

The variable `letter` holds the character "H". The variable `code` holds the integer `72`.

### Immutability

The `String` class is immutable. Every transformation method returns a **new** `String` instance — the original is never modified:

```uranite
String original = "hello"
String upper = original.toUpper()
```

After these two lines, `original` still holds "hello" and `upper` holds "HELLO". The `toUpper()` call allocates a new string. This immutability guarantee holds for all transformation methods — `concat()`, `toLower()`, `trim()`, `replace()`, `substring()`, and `split()` all produce new strings without altering the receiver.

---

## String Concatenation

### The Plus Operator

The `+` operator concatenates two strings, producing a new string:

```uranite
String result = "hello" + " " + "world"
```

The variable `result` holds "hello world". Multiple `+` operations chain left-to-right — `"a" + "b" + "c"` first produces "ab", then concatenates "c" to produce "abc".

### The concat Method

The `concat()` method performs the same operation as `+`:

```uranite
String first = "hello"
String second = " world"
String combined = first.concat( second )
```

The variable `combined` holds "hello world". The `+` operator and `concat()` are interchangeable — use whichever reads more naturally in context.

### Chained Concatenation

Both `+` and `concat()` support chaining for building complex strings:

```uranite
String greeting = "Hello, " + name + "! You are " + age.toString() + " years old."

String same = "Hello, "
    .concat( name )
    .concat( "! You are " )
    .concat( age.toString() )
    .concat( " years old." )
```

Each concatenation creates a new string. For building strings from many parts, consider using `format()` instead (see [String Formatting](#string-formatting)).

---

## Mixed-Type Concatenation

When using the `+` operator, the compiler automatically converts non-string values to their string representation before concatenation. This means you can concatenate strings with integers, floats, and booleans directly:

```uranite
I64 count = 42
F64 ratio = 3.14
Boolean active = True

String message = "Count: " + count + ", Ratio: " + ratio + ", Active: " + active
```

The compiler inserts the necessary conversions automatically. Each non-string value is converted to its string representation before being appended.

The following types support automatic string conversion when used with `+`:

| Type | Conversion Example |
|---|---|
| `I64`, `I32`, `I16`, `I8` | `42` becomes "42" |
| `U64`, `U32`, `U16`, `U8` | `255` becomes "255" |
| `F64`, `F32` | `3.14` becomes "3.14" |
| `Boolean` | `True` becomes "True", `False` becomes "False" |
| `String` | Used directly, no conversion |
| Any object with `toString()` | `toString()` is called |

For explicit control over conversion, call `toString()` on the value before concatenation:

```uranite
I64 value = 42
String explicit = "The answer is: " + value.toString()
```

Both approaches produce the same result. The explicit `toString()` call makes the conversion visible in source code, which some developers prefer for clarity.

---

## String Formatting

The `String` class provides a `format()` method for positional placeholder substitution. The only recognized placeholder is `{}` — an opening curly brace immediately followed by a closing curly brace. No format specifiers, width modifiers, or named placeholders exist.

### Placeholder Syntax

The placeholder `{}` is the only format token. Each `{}` in the template is replaced with the next positional argument in left-to-right order:

```uranite
from uranite.io.console import puts

public function main() -> I32:
    String name = "Alice"
    I64 age = 30
    String city = "Tokyo"

    String message = "Hello, {}! You are {} years old and live in {}.".format( name, age, city )
    puts( message )

    return 0
```

The first `{}` receives `name`, the second receives `age`, the third receives `city`. The output is "Hello, Alice! You are 30 years old and live in Tokyo."

### Curly Brace Behavior

Curly braces have special meaning **only** when they appear as the exact two-character sequence `{}`. A lone `{` or lone `}` passes through the format method unchanged as a literal character:

| Template | Arguments | Result |
|---|---|---|
| `"value: {}"` | `42` | `"value: 42"` |
| `"array[0] = {}"` | `10` | `"array[0] = 10"` |
| `"left { brace"` | (none) | `"left { brace"` |
| `"right } brace"` | (none) | `"right } brace"` |
| `"both { and }"` | (none) | `"both { and }"` |
| `"JSON: {\"key\": {}}"` | `"val"` | `"JSON: {\"key\": val}"` |

Because only `{}` triggers substitution, you do not need to escape individual `{` or `}` characters. A `{` that is not immediately followed by `}` is always literal. A `}` that is not immediately preceded by `{` is always literal.

### Excess Placeholders

If the template contains more `{}` placeholders than there are arguments, the excess placeholders remain as literal `{}` text in the output:

```uranite
String template = "a={}, b={}, c={}"
String result = template.format( "one", "two" )
```

The variable `result` holds "a=one, b=two, c={}". The third placeholder has no matching argument, so it stays as the literal text "{}".

### Excess Arguments

If more arguments are provided than there are `{}` placeholders, the extra arguments are silently ignored:

```uranite
String template = "value: {}"
String result = template.format( "first", "second", "third" )
```

The variable `result` holds "value: first". The second and third arguments have no matching placeholder and are discarded.

### No Format Specifiers

Uranite's `format()` does not support format specifiers, width modifiers, precision controls, alignment directives, type codes, positional indexing, or named placeholders inside the braces. The implementation scans byte-by-byte and only matches the exact two-byte sequence `{` (byte 123) immediately followed by `}` (byte 125). Any character between `{` and `}` causes the `{` to pass through as a literal character.

The following table lists common format syntax patterns from other languages (Python, Rust, Java, C#) and shows that **none** of them work in Uranite — they all pass through as literal text:

**Positional and named indexing (not supported):**

| Syntax | Origin | Behavior in Uranite |
|---|---|---|
| `{}` | Python/Rust | **Supported** — the only valid placeholder |
| `{0}` | Python/C# | Literal `{0}` — `{` not followed by `}` |
| `{1}` | Python/C# | Literal `{1}` |
| `{name}` | Python | Literal `{name}` |
| `{key}` | Python | Literal `{key}` |

**Type codes (not supported):**

| Syntax | Origin | Behavior in Uranite |
|---|---|---|
| `{:d}` | Python | Literal `{:d}` |
| `{:f}` | Python | Literal `{:f}` |
| `{:s}` | Python | Literal `{:s}` |
| `{:x}` | Python/Rust | Literal `{:x}` |
| `{:o}` | Python/Rust | Literal `{:o}` |
| `{:b}` | Python/Rust | Literal `{:b}` |
| `{:e}` | Python | Literal `{:e}` |
| `{:X}` | Python | Literal `{:X}` |
| `{:c}` | Python | Literal `{:c}` |

**Precision and width (not supported):**

| Syntax | Origin | Behavior in Uranite |
|---|---|---|
| `{:.2f}` | Python | Literal `{:.2f}` |
| `{:.4}` | Rust | Literal `{:.4}` |
| `{:10}` | Python/Rust | Literal `{:10}` |
| `{:010}` | Python | Literal `{:010}` |
| `{:10.2f}` | Python | Literal `{:10.2f}` |

**Alignment and fill (not supported):**

| Syntax | Origin | Behavior in Uranite |
|---|---|---|
| `{:>10}` | Python | Literal `{:>10}` |
| `{:<10}` | Python | Literal `{:<10}` |
| `{:^10}` | Python | Literal `{:^10}` |
| `{:*>10}` | Python | Literal `{:*>10}` |
| `{:0>10}` | Python | Literal `{:0>10}` |

**Sign and prefix (not supported):**

| Syntax | Origin | Behavior in Uranite |
|---|---|---|
| `{:+}` | Python/Rust | Literal `{:+}` |
| `{:-}` | Python | Literal `{:-}` |
| `{:#x}` | Python/Rust | Literal `{:#x}` |
| `{:#o}` | Python/Rust | Literal `{:#o}` |
| `{:#b}` | Python | Literal `{:#b}` |
| `{:,}` | Python | Literal `{:,}` |
| `{:_}` | Python | Literal `{:_}` |

**Conversion flags (not supported):**

| Syntax | Origin | Behavior in Uranite |
|---|---|---|
| `{!r}` | Python | Literal `{!r}` |
| `{!s}` | Python | Literal `{!s}` |
| `{!a}` | Python | Literal `{!a}` |

**Brace escaping (not needed):**

| Syntax | Origin | Behavior in Uranite |
|---|---|---|
| `{{` | Python/Rust | Two literal `{` characters — each `{` is checked independently and neither is followed by `}` |
| `}}` | Python/Rust | Two literal `}` characters |
| `{{}}` | Python | Literal `{` followed by placeholder `{}` — the first `{` sees `{` as its next byte (not `}`), so it passes literally; the second `{` sees `}` as its next byte, triggering substitution |

In other languages, `{{` is the escape sequence for a literal `{` inside a format string. In Uranite, this is unnecessary — a lone `{` already passes through literally because the format method only triggers on the exact `{}` pair.

### Escape Sequences in Format Templates

Escape sequences (`\n`, `\t`, `\"`, `\\`, `\0`, `\r`, `\x`) are resolved at compile time during string literal scanning, **before** the `format()` method ever runs. By the time `format()` processes the template, all escape sequences have already been converted to their byte values:

```uranite
String formatted = "Name:\t{}\nAge:\t{}".format( "Alice", 30 )
```

The template string is stored internally as `Name:` + TAB + `{}` + LF + `Age:` + TAB + `{}`. The `format()` method sees raw bytes, not escape sequences. The `\t` and `\n` do not interfere with `{}` placeholder detection.

All escape sequences work correctly inside format templates:

| Template | Arguments | Result |
|---|---|---|
| `"Hello, {}!\n"` | `"World"` | `Hello, World!` + newline |
| `"col1:\t{}\tcol2:\t{}"` | `"A"`, `"B"` | Tab-separated columns with substituted values |
| `"path: \"{}\\{}\""` | `"dir"`, `"file"` | `path: "dir\file"` |
| `"\x48ello, {}!"` | `"World"` | `Hello, World!` — `\x48` resolves to "H" before format runs |
| `"null:\0{}"` | `"after"` | `null:` + null byte + `after` — `\0` does not affect placeholder scanning |

The key principle: escape sequences and format placeholders operate at different layers. Escapes are lexer-level (compile time). Placeholders are runtime (`format()` method). They never conflict.

### Number Formatting Utilities

For formatting needs beyond simple placeholder substitution — padding, zero-fill, alignment — use the utilities in `uranite.string.format`:

```uranite
from uranite.string.format import padLeft, padRight, padZero, appendTo, formatUtcOffset

String zeroPadded = padZero( 5, 3 )
String leftPadded = padLeft( "hi", 10 )
String rightPadded = padRight( "hi", 10 )
String utcOffset = formatUtcOffset( 25200 )
```

| Function | Arguments | Result | Description |
|---|---|---|---|
| `padZero( 5, 3 )` | value=5, width=3 | `"005"` | Zero-padded integer to minimum width |
| `padZero( 42, 2 )` | value=42, width=2 | `"42"` | Already at width, unchanged |
| `padLeft( "hi", 10 )` | source="hi", width=10 | `"        hi"` | Right-aligned with leading spaces |
| `padRight( "hi", 10 )` | source="hi", width=10 | `"hi        "` | Left-aligned with trailing spaces |
| `padLeft( "hello", 3 )` | source="hello", width=3 | `"hello"` | Already wider, unchanged |
| `formatUtcOffset( 25200 )` | seconds=25200 | `"+07:00"` | ISO 8601 UTC offset format |
| `formatUtcOffset( -18000 )` | seconds=-18000 | `"-05:00"` | Negative offset |
| `formatUtcOffset( 0 )` | seconds=0 | `"+00:00"` | Zero offset |

### format() vs Concatenation

Format and concatenation produce the same result. Format is more readable when multiple values are embedded in a sentence:

```uranite
String concatenated = "Hello, " + name + "! You are " + age.toString() + " years old."

String formatted = "Hello, {}! You are {} years old.".format( name, age )
```

Both produce the same output. The `format()` version keeps the template structure visible as a single string with insertion points clearly marked, while concatenation fragments the sentence across multiple `+` operations.

---

## UTF-8 and Multi-Byte Characters

Strings are stored as UTF-8 byte sequences. Since Uranite source files are UTF-8 encoded (see [Source Files and Encoding](source-files-and-encoding.md)), string literals inherit this encoding directly. Multi-byte characters — accented letters, CJK characters, emoji — are included verbatim:

```uranite
String japanese = "こんにちは"
String emoji = "Hello! 🎉"
String accented = "café résumé naïve"
String chinese = "你好世界"
```

All of these are valid string literals. The UTF-8 bytes from the source file are stored directly in the string value.

**Length is measured in bytes, not characters.** The `length()` method returns the number of bytes in the string, not the number of Unicode characters:

| String | Characters | Bytes | `length()` Returns |
|---|---|---|---|
| `"hello"` | 5 | 5 | 5 |
| `"café"` | 4 | 5 | 5 |
| `"こんにちは"` | 5 | 15 | 15 |
| `"🎉"` | 1 | 4 | 4 |

The string "café" has 4 visible characters but 5 bytes because "é" is encoded as 2 bytes in UTF-8. The string "こんにちは" has 5 characters but 15 bytes because each CJK character is 3 bytes in UTF-8. The emoji "🎉" is 1 character but 4 bytes.

This byte-length behavior affects `charAt()`, `indexOf()`, and `substring()` as well — all operate on byte indices, not character indices. When working with multi-byte content, be aware that a byte index may point to the middle of a multi-byte character.

---

## Null Bytes in Strings

A string literal can contain embedded null bytes using the `\0` escape:

```uranite
String withNull = "ab\0cd"
```

The resulting string contains 5 bytes: `a`, `b`, null, `c`, `d`. The `length()` method returns 5, counting all bytes including the null.

However, embedded null bytes can interact unexpectedly with certain operations. Functions that treat strings as null-terminated C-style strings will stop at the first null byte. The `length()` method counts all bytes, but printing a string with an embedded null may truncate the output at the null byte depending on the output function.

In general, avoid embedding null bytes in strings unless you have a specific need for binary data. For binary data handling, consider using `Memory<U8>` instead of `String`.

---

## Examples

### Valid String Literals

```uranite
String empty = ""
String single = "x"
String greeting = "hello, world"
String path = "/home/user/documents"
String url = "https://example.com/api/v1"
String quoted = "she said \"hello\""
String withTab = "column1\tcolumn2\tcolumn3"
String withNewline = "line one\nline two"
String hexChars = "\x48\x65\x6C\x6C\x6F"
```

The variable `hexChars` contains "Hello" — each hex escape produces one ASCII character (`H`, `e`, `l`, `l`, `o`).

### Escape Sequence Examples

```uranite
String backslash = "C:\\Users\\Documents"
String nullByte = "before\0after"
String carriageReturn = "overwrite\rthis"
String hexA = "\x41\x42\x43"
String hexLower = "\x61\x62\x63"
String mixed = "tab:\there\nnewline:\nend"
```

| Variable | Content |
|---|---|
| `backslash` | `C:\Users\Documents` |
| `nullByte` | `before` + null byte + `after` (5 visible characters, 12 bytes total) |
| `carriageReturn` | `overwrite` + carriage return + `this` |
| `hexA` | `ABC` |
| `hexLower` | `abc` |
| `mixed` | `tab:` + TAB + `here` + LF + `newline:` + LF + `end` |

### Invalid String Literals

The following are **not** valid string literals:

| Literal | Problem |
|---|---|
| `"unterminated` | No closing quote before end of file. |
| `"line one` (newline) `line two"` | Newline inside string. Use `\n` instead. |
| `"""multi-line"""` | Triple quotes produce a doccomment, not a string. |
| `'hello'` | Single quotes produce a `Char` literal, not a `String`. |
| `"ends with backslash\"` | Backslash escapes the closing quote, making the string unterminated. Write `"\\"` instead. |

### Practical Usage

A complete example demonstrating string operations, escape sequences, concatenation, formatting, and method calls:

```uranite
from uranite.io.console import puts

public function formatGreeting( String name, I64 age ) -> String:
    return "Hello, {}! You are {} years old.".format( name, age )

public function buildPath( String directory, String filename ) -> String:
    return directory + "/" + filename

public function demonstrateEscapes() -> Void:
    String json = "{\"name\": \"Alice\", \"age\": 30}"
    puts( json )

    String table = "ID\tName\tScore\n1\tBob\t95\n2\tEve\t88\n3\tAli\t92"
    puts( table )

    String windowsPath = "C:\\Program Files\\Uranite\\bin"
    puts( windowsPath )

public function demonstrateMethods() -> Void:
    String text = "  Hello, World!  "

    String trimmed = text.trim()
    puts( "Trimmed: \"" + trimmed + "\"" )

    String upper = trimmed.toUpper()
    puts( "Upper: " + upper )

    String lower = trimmed.toLower()
    puts( "Lower: " + lower )

    Boolean hasWorld = trimmed.contains( "World" )
    puts( "Contains World: " + hasWorld )

    String replaced = trimmed.replace( "World", "Uranite" )
    puts( "Replaced: " + replaced )

    I64 commaPos = trimmed.indexOf( "," )
    puts( "Comma at index: " + commaPos )

    ArrayList<String> parts = "one,two,three,four".split( "," )
    puts( "Split count: " + parts.size() )

public function demonstrateConcatenation() -> Void:
    I64 count = 42
    F64 temperature = 21.5
    Boolean raining = False

    String report = "Items: " + count + ", Temp: " + temperature + "C, Raining: " + raining
    puts( report )

    String multiLine = "Line 1: Introduction\n"
        + "Line 2: Details\n"
        + "Line 3: Conclusion"
    puts( multiLine )

public function main() -> I32:
    String message = formatGreeting( "Alice", 30 )
    puts( message )

    String path = buildPath( "/usr/local/bin", "uranite" )
    puts( "Path: " + path )

    demonstrateEscapes()
    demonstrateMethods()
    demonstrateConcatenation()

    return 0
```

This example demonstrates string formatting with `{}` placeholders, path construction with `+` concatenation, JSON embedding with `\"` escapes, tab-separated table formatting with `\t` and `\n`, Windows path escaping with `\\`, query and transformation methods (`trim()`, `toUpper()`, `toLower()`, `contains()`, `replace()`, `indexOf()`, `split()`), mixed-type concatenation with integers, floats, and booleans, and multi-line string construction through concatenation with embedded `\n` escapes.
