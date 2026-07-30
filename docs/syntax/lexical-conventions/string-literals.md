# String Literals

Uranite supports single-line string literals delimited by double quotes (`"..."`). Strings are immutable sequences of characters, typed as `String` during semantic analysis, and emitted as null-terminated UTF-8 byte arrays in LLVM IR. This document specifies the lexer scanning rules, escape sequence processing, the relationship between string literals and triple-quoted doccomments, type assignment, memory representation, and string operations.

---

## Table of Contents

- [Overview](#overview)
- [String Literal Syntax](#string-literal-syntax)
- [Escape Sequences](#escape-sequences)
  - [Standard Escapes](#standard-escapes)
  - [Hex Byte Escape](#hex-byte-escape)
  - [Unknown Escapes](#unknown-escapes)
  - [Escape Processing Order](#escape-processing-order)
- [Lexer Scanning Algorithm](#lexer-scanning-algorithm)
  - [Entry and Exit](#entry-and-exit)
  - [Newline Rejection](#newline-rejection)
  - [Unterminated Strings](#unterminated-strings)
- [Triple-Quoted Strings](#triple-quoted-strings)
- [The Parsing Pipeline](#the-parsing-pipeline)
  - [Stage 1: Lexer Scanning](#stage-1-lexer-scanning)
  - [Stage 2: Parser Construction](#stage-2-parser-construction)
  - [Stage 3: Semantic Type Assignment](#stage-3-semantic-type-assignment)
  - [Stage 4: HIR Representation](#stage-4-hir-representation)
  - [Stage 5: MIR Code Generation](#stage-5-mir-code-generation)
- [Memory Representation](#memory-representation)
  - [LLVM Global String Pointers](#llvm-global-string-pointers)
  - [Null Termination](#null-termination)
  - [UTF-8 Encoding](#utf-8-encoding)
- [The String OOP Wrapper](#the-string-oop-wrapper)
  - [Core Methods](#core-methods)
  - [Immutability](#immutability)
- [String Concatenation](#string-concatenation)
  - [The concat Method](#the-concat-method)
  - [The Plus Operator](#the-plus-operator)
  - [Codegen-Level Concatenation](#codegen-level-concatenation)
  - [Mixed-Type Concatenation](#mixed-type-concatenation)
- [String Formatting](#string-formatting)
- [Examples](#examples)
  - [Valid String Literals](#valid-string-literals)
  - [Escape Sequence Examples](#escape-sequence-examples)
  - [Invalid String Literals](#invalid-string-literals)
  - [Practical Usage](#practical-usage)

---

## Overview

A string literal is a sequence of characters enclosed in double quotes. The lexer processes escape sequences during scanning, producing a `LiteralString` token whose value contains the unescaped byte sequence. The parser wraps this value in a `StringLiteralExpression` AST node. The semantic analyzer types it as `String`, and the MIR codegen emits a null-terminated global constant via LLVM's `CreateGlobalStringPtr`.

| Property | Value |
|---|---|
| Delimiter | Double quotes (`"..."`) |
| Multi-line | Not supported (newline inside string is an error) |
| Escape processing | Yes, during lexer scanning |
| Default type | `String` (`uranite.language.string.String`) |
| LLVM representation | `i8*` pointer to null-terminated global constant |

---

## String Literal Syntax

A string literal begins with a double-quote character (`"`), contains zero or more characters (including escape sequences), and ends with a matching double-quote:

```uranite
String empty = ""
String greeting = "hello, world"
String path = "/usr/local/bin"
String message = "value is: 42"
```

Strings are delimited exclusively by double quotes. There is no single-quote string syntax — single quotes are reserved for character literals (covered in the char literals document).

---

## Escape Sequences

The lexer recognizes escape sequences within string literals. An escape sequence begins with a backslash (`\`) followed by one or more characters that together represent a single byte or character in the output string.

### Standard Escapes

Seven standard escape sequences are supported:

| Escape | Byte Value | Description |
|---|---|---|
| `\"` | `0x22` | Double-quote character. Allows embedding a literal `"` inside a string. |
| `\'` | `0x27` | Single-quote character. Included for symmetry with char literals. |
| `\0` | `0x00` | Null byte. Embeds a zero byte in the string. |
| `\\` | `0x5C` | Backslash. Produces a literal `\` character. |
| `\n` | `0x0A` | Newline (line feed). |
| `\r` | `0x0D` | Carriage return. |
| `\t` | `0x09` | Horizontal tab. |

Each escape sequence consumes two source characters (the backslash and the escape identifier) and produces exactly one byte in the token value.

### Hex Byte Escape

The `\x` escape allows inserting an arbitrary byte by its hexadecimal value:

```
\xHH    → single byte with value 0xHH
```

After encountering `\x`, the lexer reads up to 2 hexadecimal digits (`0`-`9`, `a`-`f`, `A`-`F`). The digits are converted to an integer via `std::stoi(hexSequence, nullptr, 16)` and cast to a `char`:

| Escape | Byte Value | Character |
|---|---|---|
| `\x41` | `0x41` | `A` |
| `\x0A` | `0x0A` | newline (same as `\n`) |
| `\x00` | `0x00` | null byte (same as `\0`) |
| `\xFF` | `0xFF` | byte 255 |
| `\x9` | `0x09` | tab (single hex digit accepted) |

The hex escape reads a maximum of 2 hex digits. If fewer than 2 hex digits follow `\x`, the lexer reads as many as are available (minimum 1). Characters after the hex digits that are not hex digits are not consumed — they become part of the subsequent string content.

Note that the `\x` escape handles the cursor advancement internally and uses `continue` to skip the normal `advance()` call at the end of the scanning loop. This is a lexer implementation detail that ensures the character after the hex digits is not inadvertently consumed.

### Unknown Escapes

If the character following the backslash is not one of the recognized escape identifiers (`"`, `'`, `0`, `\\`, `n`, `r`, `t`, `x`), the lexer emits a warning diagnostic and includes the character literally (without the backslash):

```
warning: unknown escape sequence "\q"
  --> source.urn:3:15
```

The string `"hello\qworld"` produces the token value `"helloqworld"` — the backslash is consumed, the `q` is appended as a literal character, and a warning is emitted.

This permissive behavior means unknown escapes do not cause compilation errors. They produce warnings, allowing the code to compile while flagging the likely mistake.

### Escape Processing Order

The lexer processes escape sequences character by character as it scans the string. Each character is evaluated in order:

1. If the current character matches the closing quote: the string is complete.
2. If the current character is `\n`: the string is unterminated (error).
3. If the current character is `\`: consume the backslash, read the next character, and apply the escape table.
4. Otherwise: append the character literally.

Escape sequences are fully resolved at the lexer level. The parser and semantic analyzer never see backslash-prefixed escape sequences — they receive the already-unescaped byte string.

---

## Lexer Scanning Algorithm

### Entry and Exit

The `readString()` method is called when the lexer encounters a `"` character in the main tokenization loop. The method:

1. Records the current source position for diagnostic reporting.
2. Advances past the opening quote character. The quote character is consumed but not appended to the token value.
3. Enters a scanning loop that processes characters until the closing quote is found or an error occurs.
4. When the closing quote is encountered, advances past it and returns a `LiteralString` token.

The token value contains only the string content — no quote delimiters.

### Newline Rejection

If the lexer encounters a newline character (`\n`) inside a string literal, it emits an error and returns an `Error` token:

```
error: unterminated string literal
  --> source.urn:3:20
```

Strings cannot span multiple lines. A newline inside a string is always an error. To include a newline character in a string, use the `\n` escape sequence.

### Unterminated Strings

If the lexer reaches the end of the source file without finding a closing quote, it emits an error and returns an `Error` token:

```
error: unterminated string literal
  --> source.urn:3:1
```

The error is reported at the position where the opening quote was found, helping the developer locate the start of the unterminated string.

---

## Triple-Quoted Strings

Triple-quoted sequences (`"""..."""` and `'''...'''`) are **not** string literals in Uranite. They are doccomments — the lexer's `skipTripleQuoteComment()` method consumes their entire content without producing any token.

This is a critical distinction from languages like Python, where triple-quoted strings serve as both multi-line strings and docstrings. In Uranite:

- `"hello"` is a string literal. It produces a `LiteralString` token.
- `"""hello"""` is a doccomment. It produces no token. The parser never sees it.

There is no multi-line string literal syntax in Uranite. To construct a multi-line string, concatenate single-line strings with embedded `\n` escapes:

```uranite
String multiLine = "first line\n" + "second line\n" + "third line"
```

Or use the `concat()` method:

```uranite
String header = "Name: Alice\n"
String body = "Age: 30\n"
String footer = "City: Tokyo"
String document = header.concat( body ).concat( footer )
```

---

## The Parsing Pipeline

### Stage 1: Lexer Scanning

The `readString()` method produces a `LiteralString` token with the fully unescaped string content:

| Source Text | Token Value |
|---|---|
| `"hello"` | `hello` |
| `"line1\nline2"` | `line1` + newline byte + `line2` |
| `"tab\there"` | `tab` + tab byte + `here` |
| `"quote: \""` | `quote: "` |
| `"hex: \x41"` | `hex: A` |
| `""` | (empty string) |

### Stage 2: Parser Construction

When the parser encounters a `LiteralString` token, it constructs a `StringLiteralExpression` AST node containing the string value:

```
StringLiteralExpression {
    value: "hello"        ← the unescaped content
    source: (file, line, column)
}
```

The parser performs no additional processing on the string content — it stores the lexer's output directly.

### Stage 3: Semantic Type Assignment

The semantic analyzer processes `StringLiteral` expression nodes:

1. Look up the `String` class type via `typeRegistry.lookupType("String")`.
2. If found, use the class type (OOP wrapper `uranite.language.string.String`).
3. If not found (fallback), use the primitive string type `typeRegistry.getString()` (primitive `uranite.builtin.str`).

Every string literal receives the type `String`.

### Stage 4: HIR Representation

The HIR preserves the string value in an `HIRStringLiteral` node:

| Field | Type | Description |
|---|---|---|
| `stringValue` | `std::string` | The unescaped string content. |
| `resolvedType` | `TypeSharedPointer` | The `String` type from semantic analysis. |

### Stage 5: MIR Code Generation

The `generateConstantString()` method maps the string to an LLVM global constant:

```
llvm::Value* constValue = irBuilder.CreateGlobalStringPtr(
    instruction.stringConstantValue, "str"
)
```

`CreateGlobalStringPtr` creates a global constant byte array containing the string bytes followed by a null terminator, and returns an `i8*` pointer to it. The pointer is stored as the variable's runtime value.

---

## Memory Representation

### LLVM Global String Pointers

String literals are emitted as LLVM global constants. The `CreateGlobalStringPtr` call produces:

1. A global `[N x i8]` constant array containing the string bytes and a trailing null byte.
2. A `getelementptr` instruction that returns an `i8*` pointer to the first byte.

For the string literal `"hello"`:

```llvm
@str = private unnamed_addr constant [6 x i8] c"hello\00"
```

The constant is 6 bytes: 5 characters plus 1 null terminator. The pointer passed to runtime code points to the first byte.

### Null Termination

All string literals are null-terminated. This makes them compatible with C library functions (`strlen`, `strcpy`, `strcat`, `snprintf`) that the runtime uses for string operations.

A string literal containing an embedded `\0` escape has a null byte within the content **and** an additional null terminator appended by LLVM:

```uranite
String withNull = "ab\0cd"
```

The LLVM constant is `[6 x i8] c"ab\00cd\00"` — the `\0` at position 2 is the embedded null, and the `\0` at position 5 is the LLVM-appended terminator. C-style string functions like `strlen` will report a length of 2 (stopping at the first null), but the full 5-byte content is present in memory.

### UTF-8 Encoding

Strings are stored as raw byte sequences. The lexer does not validate UTF-8 encoding — it copies source bytes verbatim into the token value. Since Uranite source files must be valid UTF-8 (see [Source Files and Encoding](source-files-and-encoding.md)), string literals inherit UTF-8 encoding from the source.

Multi-byte UTF-8 characters (accented letters, CJK characters, emoji) pass through the lexer unchanged. The lexer treats them as a sequence of individual bytes, each of which is a non-special character appended literally:

```uranite
String greeting = "こんにちは"
String emoji = "🎉"
```

The string `"こんにちは"` is 15 bytes in UTF-8 (3 bytes per character). The lexer copies all 15 bytes into the token value. The `String` class's `length()` method returns the byte length, not the character count.

---

## The String OOP Wrapper

### Core Methods

The `String` class (`uranite.language.string.String`) provides methods for querying, transforming, and comparing string values:

| Method | Signature | Description |
|---|---|---|
| `length` | `() -> I64` | Return the length of the string. |
| `isEmpty` | `() -> Boolean` | Return `True` if the string has zero length. |
| `concat` | `(String other) -> String` | Return a new string formed by appending "other". |
| `equals` | `(String other) -> Boolean` | Return `True` if both strings contain the same content. |
| `contains` | `(String substring) -> Boolean` | Return `True` if the string contains the given substring. |
| `startsWith` | `(String prefix) -> Boolean` | Return `True` if the string starts with the given prefix. |
| `endsWith` | `(String suffix) -> Boolean` | Return `True` if the string ends with the given suffix. |
| `indexOf` | `(String target) -> I64` | Return the index of the first occurrence, or -1. |
| `charAt` | `(I64 index) -> Char` | Return the character at the given byte index. |
| `substring` | `(I64 start) -> String` | Return a substring from start to end. |
| `toUpper` | `() -> String` | Return an uppercase copy. |
| `toLower` | `() -> String` | Return a lowercase copy. |
| `trim` | `() -> String` | Return a copy with leading and trailing whitespace removed. |
| `replace` | `(String target, String replacement) -> String` | Return a copy with occurrences replaced. |
| `split` | `(String delimiter) -> ArrayList<String>` | Split into a list of substrings. |
| `format` | `(I64 args[]) -> String` | Format with positional `{}` placeholders. |
| `toString` | `() -> String` | Return the string value unchanged. |
| `hashCode` | `() -> I64` | Return a hash code for the string. |

### Immutability

The `String` class is immutable. All transformation methods (`concat`, `toUpper`, `toLower`, `trim`, `replace`, `split`) return new `String` instances — they never modify the original. The underlying `value` field is `protect` (accessible only within the class and subclasses).

```uranite
String original = "hello"
String upper = original.toUpper()
```

After these two lines, `original` still holds `"hello"` and `upper` holds `"HELLO"`. The `toUpper()` call allocates a new string.

---

## String Concatenation

### The concat Method

The `concat()` method creates a new string by appending the argument to the receiver:

```uranite
String first = "hello"
String second = " world"
String combined = first.concat( second )
```

The result is `"hello world"`.

### The Plus Operator

The `+` operator on strings dispatches to the `Addable` interface's `add()` method, which for the `String` class delegates to concatenation. The codegen intercepts string `+` operations and emits the same concatenation code as `concat()`:

```uranite
String result = "hello" + " " + "world"
```

This is equivalent to `"hello".concat( " " ).concat( "world" )`.

### Codegen-Level Concatenation

The MIR codegen detects calls to `String.concat()` and emits optimized inline concatenation code instead of a general method call:

1. Compute the length of both strings using `strlen`.
2. Add the lengths plus 1 (for the null terminator).
3. Allocate a buffer using `malloc`.
4. Copy the first string into the buffer using `strcpy`.
5. Append the second string using `strcat`.
6. Store the buffer pointer as the result.

This inline expansion avoids the overhead of a full method dispatch for the most common string operation.

### Mixed-Type Concatenation

The codegen supports concatenating strings with non-string values. When `concat()` is called with a non-string operand, the codegen automatically converts the operand to a string representation:

| Operand Type | Conversion |
|---|---|
| `String` (pointer) | Used directly — no conversion needed. |
| `F64` / `F32` | Formatted via `snprintf` with `"%.6g"` format into a malloc'd buffer. |
| `Boolean` (`i1`) | Converted to the string `"True"` or `"False"` via `select`. |
| Integer (`i8`-`i64`) | Formatted via `snprintf` with `"%ld"` format into a malloc'd buffer. |
| Generic parameter (pointer) | Runtime branch: if the pointer value >= 4096, treated as a string pointer; otherwise, treated as an integer and formatted via `snprintf`. |

This type-aware concatenation means expressions like `"count: " + value` work when `value` is an integer — the codegen inserts the `snprintf` conversion automatically.

---

## String Formatting

The `String` class provides a `format()` method for positional placeholder substitution:

```uranite
String template = "Hello, {}! You are {} years old."
String result = template.format( name, age )
```

The `format()` method replaces `{}` placeholders with positional arguments in order. This provides a safer alternative to concatenation for constructing complex strings with multiple variable insertions.

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
```

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
| `nullByte` | `before` + null byte + `after` |
| `carriageReturn` | `overwrite` + CR + `this` |
| `hexA` | `ABC` |
| `hexLower` | `abc` |
| `mixed` | `tab:` + TAB + `here` + LF + `newline:` + LF + `end` |

### Invalid String Literals

The following are **not** valid string literals:

```
"unterminated        → no closing quote before end of file
"line one            → newline inside string (error: unterminated string literal)
line two"
"""multi-line"""     → triple-quoted, treated as doccomment, not a string literal
'hello'              → single quotes produce a char literal, not a string
```

To create a string from a single character, use the double-quote form:

```uranite
String singleChar = "x"
```

### Practical Usage

```uranite
from uranite.io.console import puts

public function formatGreeting( String name, I64 age ) -> String:
    String greeting = "Hello, " + name + "!"
    String info = " Age: " + age.toString()
    return greeting.concat( info )

public function buildPath( String directory, String filename ) -> String:
    return directory + "/" + filename

public function escapeDemo() -> Void:
    String json = "{\"name\": \"Alice\", \"age\": 30}"
    puts( json )

    String table = "ID\tName\tScore\n1\tBob\t95\n2\tEve\t88"
    puts( table )

    String windowsPath = "C:\\Program Files\\Uranite\\bin"
    puts( windowsPath )

public function main() -> I32:
    String message = formatGreeting( "Alice", 30 )
    puts( message )

    String path = buildPath( "/usr/local", "uranite" )
    puts( path )

    escapeDemo()
    return 0
```

This example demonstrates string concatenation with `+` and `concat()`, escape sequences for JSON embedding (`\"`), tab-separated table formatting (`\t`, `\n`), Windows path escaping (`\\`), and mixed-type concatenation via `toString()`.
