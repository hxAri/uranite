# Regular Expression Literals

Uranite supports native regular expression literals delimited by forward slashes (`/pattern/`). Regex literals are first-class syntax — the lexer recognizes them during tokenization, the parser wraps them into `RegexLiteralExpression` AST nodes, and the standard library provides the `RegExp` class for backtracking pattern matching. This document specifies the lexer disambiguation logic, scanning algorithm, escape handling, the `RegExp` engine, and integration with the compilation pipeline.

---

## Table of Contents

- [Overview](#overview)
- [Regex Literal Syntax](#regex-literal-syntax)
- [Slash Disambiguation](#slash-disambiguation)
  - [The Previous-Token Rule](#the-previous-token-rule)
  - [Whitespace Guard](#whitespace-guard)
  - [Disambiguation Table](#disambiguation-table)
- [Lexer Scanning Algorithm](#lexer-scanning-algorithm)
  - [Escape Handling](#escape-handling)
  - [Character Class Tracking](#character-class-tracking)
  - [Newline Rejection](#newline-rejection)
  - [Closing Slash](#closing-slash)
- [Supported Pattern Syntax](#supported-pattern-syntax)
  - [Literal Characters](#literal-characters)
  - [Dot Wildcard](#dot-wildcard)
  - [Quantifiers](#quantifiers)
  - [Character Classes](#character-classes)
  - [Escape Sequences](#escape-sequences)
  - [Anchors](#anchors)
  - [Alternation](#alternation)
  - [Grouping](#grouping)
- [The Parsing Pipeline](#the-parsing-pipeline)
  - [Stage 1: Lexer Scanning](#stage-1-lexer-scanning)
  - [Stage 2: Parser Construction](#stage-2-parser-construction)
  - [Stage 3: Code Generation](#stage-3-code-generation)
- [The RegExp Standard Library Class](#the-regexp-standard-library-class)
  - [Construction](#construction)
  - [Methods](#methods)
  - [The Match Class](#the-match-class)
  - [Error Types](#error-types)
- [The Backtracking Engine](#the-backtracking-engine)
  - [Matching Algorithm](#matching-algorithm)
  - [Alternation Handling](#alternation-handling)
  - [Performance Characteristics](#performance-characteristics)
- [Examples](#examples)
  - [Basic Pattern Matching](#basic-pattern-matching)
  - [Character Classes and Quantifiers](#character-classes-and-quantifiers)
  - [Search and Replace](#search-and-replace)
  - [String Splitting](#string-splitting)
  - [Practical Usage](#practical-usage)

---

## Overview

| Property | Value |
|---|---|
| Delimiter | Forward slashes (`/.../ `) |
| Token type | `LiteralRegex` |
| AST node | `RegexLiteralExpression` |
| Stores | Pattern string (without delimiters) |
| Runtime class | `RegExp` (`uranite.regexp.regexp.RegExp`) |
| Engine type | Recursive backtracking |

Regex literals provide syntactic sugar for constructing `RegExp` objects. The pattern between the slashes is stored as a raw string — the lexer preserves escape sequences verbatim (unlike string literals, where escapes are processed). The `RegExp` class interprets the pattern at runtime.

---

## Regex Literal Syntax

A regex literal begins with a forward slash (`/`), contains the pattern, and ends with a closing forward slash:

```uranite
RegExp emailPattern = /[a-z]+@[a-z]+\.[a-z]+/
RegExp digits = /\d+/
RegExp greeting = /^hello$/
```

The content between slashes is the raw pattern string. Backslashes within the pattern are part of the regex syntax (e.g., `\d` for digit, `\.` for literal dot) and are preserved as-is in the token value.

---

## Slash Disambiguation

The forward slash (`/`) serves double duty in Uranite: as the division operator and as the regex literal delimiter. The lexer must decide which interpretation to apply when it encounters a `/` character.

### The Previous-Token Rule

The lexer determines whether a `/` introduces a regex literal or a division operator by examining the type of the most recently emitted token. If the previous token is a value-producing token (something that could be the left operand of a division), the `/` is a division operator. Otherwise, it is the start of a regex literal.

The following token types indicate that `/` is a **division operator** (previous token produces a value):

| Previous Token Type | Example Context |
|---|---|
| `Identifier` | `x / 2` |
| `LiteralInteger` | `10 / 5` |
| `LiteralFloat` | `3.14 / 2.0` |
| `LiteralString` | (rare, but syntactically possible) |
| `LiteralChar` | (rare, but syntactically possible) |
| `RightParenthesis` | `(x + y) / z` |
| `RightBracket` | `arr[0] / 2` |
| `KeywordSelf` | `self.value / 2` |
| `KeywordTrue` | (syntactically possible) |
| `KeywordFalse` | (syntactically possible) |
| `KeywordNone` | (syntactically possible) |

If the previous token is **any other type** (an operator, a keyword like `return`, an opening parenthesis, or if there is no previous token), the `/` begins a regex literal.

If the token list is **empty** (the `/` is the first token in the source), it is always a regex literal.

### Whitespace Guard

After the previous-token check determines that the context is a regex, one additional guard is applied: the character immediately after the `/` must not be a space or newline. If the next character is `' '` or `'\n'`, the `/` is emitted as a `Slash` (division) token instead.

This guard prevents false positives in expressions like `return / divisor` where a space follows the `/`. In practice, regex patterns immediately follow the opening slash with no whitespace.

### Disambiguation Table

| Context | Previous Token | Next Char | Result |
|---|---|---|---|
| `x / 2` | `Identifier` | `2` | Division (`Slash`) |
| `return /\d+/` | `KeywordReturn` | `\` | Regex (`LiteralRegex`) |
| `( /abc/ )` | `LeftParenthesis` | `a` | Regex (`LiteralRegex`) |
| `= /pattern/` | `Assignment` | `p` | Regex (`LiteralRegex`) |
| `/ 2` (first token) | (none) | ` ` | Division (`Slash`) — whitespace guard |
| `/pattern/` (first token) | (none) | `p` | Regex (`LiteralRegex`) |

---

## Lexer Scanning Algorithm

When the lexer determines that a `/` starts a regex literal, it enters a scanning loop that reads characters until it finds the closing `/`.

### Escape Handling

A backslash (`\`) inside the regex pattern escapes the next character. The lexer tracks an `escaped` boolean flag:

1. When `escaped` is `true`, the current character is appended to the pattern regardless of its value (including `/`, `[`, `]`, `\n`). The `escaped` flag is then reset to `false`.
2. When the current character is `\` and `escaped` is `false`, the backslash is appended to the pattern and the `escaped` flag is set to `true`.

This means `\/` inside a regex produces a literal forward slash in the pattern, and `\\` produces a literal backslash.

### Character Class Tracking

The lexer tracks whether it is inside a character class (`[...]`) using an `inCharClass` boolean flag:

- `[` sets `inCharClass` to `true`.
- `]` sets `inCharClass` to `false`.

When `inCharClass` is `true`, a `/` character does **not** terminate the regex — it is treated as a literal character within the class. This allows patterns like `/[a/b]/` where the `/` inside the brackets is part of the character class.

### Newline Rejection

If the lexer encounters a newline (`\n`) while scanning the pattern (and not in an escaped state), it immediately stops scanning. The pattern accumulated so far becomes the token value. Regex patterns cannot span multiple lines.

### Closing Slash

When the lexer encounters a `/` character that is:
- Not escaped (the `escaped` flag is `false`), and
- Not inside a character class (`inCharClass` is `false`)

...it advances past the closing slash and emits a `LiteralRegex` token. The token value contains the pattern text without the surrounding slashes.

---

## Supported Pattern Syntax

The `RegExp` engine (defined in `uranite.regexp.regexp`) supports the following pattern constructs:

### Literal Characters

Any character that is not a metacharacter matches itself. The metacharacters are: `. * + ? [ ] ( ) | ^ $ \`.

```uranite
RegExp literal = /hello/
```

### Dot Wildcard

The `.` metacharacter matches any single character (any byte):

```uranite
RegExp anyThree = /h.t/
```

### Quantifiers

Three quantifiers control repetition:

| Quantifier | Meaning | Example | Matches |
|---|---|---|---|
| `*` | Zero or more | `/ab*c/` | "ac", "abc", "abbc" |
| `+` | One or more | `/ab+c/` | "abc", "abbc" (not "ac") |
| `?` | Zero or one | `/colou?r/` | "color", "colour" |

Quantifiers apply to the immediately preceding atom (literal character, dot, escape sequence, or character class). The engine uses greedy matching with backtracking — it tries to match as many characters as possible, then backtracks if the remainder of the pattern fails.

### Character Classes

Character classes match a single character from a set:

| Syntax | Description | Example |
|---|---|---|
| `[abc]` | Match any of "a", "b", or "c" | `/[aeiou]/` |
| `[a-z]` | Match any character in range | `/[a-zA-Z]/` |
| `[^abc]` | Match any character NOT in set | `/[^0-9]/` |
| `[^a-z]` | Match any character NOT in range | `/[^a-z]/` |

Ranges use the `-` character between two characters. The `^` at the start of a class negates it. Multiple ranges and literal characters can be combined: `[a-zA-Z0-9_]`.

### Escape Sequences

Backslash escape sequences match character categories:

| Escape | Matches | Description |
|---|---|---|
| `\d` | `[0-9]` | Any ASCII digit. |
| `\D` | `[^0-9]` | Any non-digit. |
| `\w` | `[a-zA-Z0-9_]` | Any word character (alphanumeric or underscore). |
| `\W` | `[^a-zA-Z0-9_]` | Any non-word character. |
| `\s` | `[ \t\n\r\f\v]` | Any whitespace (space, tab, newline, CR, form feed, vertical tab). |
| `\S` | `[^ \t\n\r\f\v]` | Any non-whitespace. |
| `\.` | `.` | Literal dot (not wildcard). |
| `\\` | `\` | Literal backslash. |
| `\/` | `/` | Literal forward slash (used within regex literals). |

Any character preceded by `\` that is not a recognized escape identifier is matched literally (the backslash is consumed, the character matches itself).

### Anchors

Two anchors constrain where the pattern matches:

| Anchor | Description |
|---|---|
| `^` | Match at the start of the string. The `RegExp` constructor detects a leading `^` and sets the `anchored` flag. |
| `$` | Match at the end of the string. Checked during recursive matching. |

When `anchored` is `True`, the engine only attempts matching at position 0 (the start of the subject). Without `^`, the engine tries matching at every position from 0 to the end of the subject string.

### Alternation

The pipe character (`|`) separates alternative branches:

```uranite
RegExp color = /red|green|blue/
```

The engine splits the pattern at top-level pipe characters (respecting group nesting) and tries each branch left to right. The first matching branch wins.

### Grouping

Parentheses `()` group sub-patterns:

```uranite
RegExp grouped = /(abc|def)+/
```

Grouping affects quantifier scope and alternation boundaries. Parentheses inside the pattern are handled by the matching engine's recursive structure.

---

## The Parsing Pipeline

### Stage 1: Lexer Scanning

The lexer produces a `LiteralRegex` token containing the raw pattern string (no delimiters):

| Source Text | Token Value |
|---|---|
| `/\d+/` | `\d+` |
| `/[a-z]+@[a-z]+/` | `[a-z]+@[a-z]+` |
| `/^hello$/` | `^hello$` |
| `/a\/b/` | `a\/b` |

Escape sequences are preserved verbatim — the `\d` in the source becomes `\d` in the token, not a processed byte. This is different from string literals where `\n` is converted to a newline byte.

### Stage 2: Parser Construction

When the parser encounters a `LiteralRegex` token:

1. Extract the pattern string from the token value.
2. Advance past the token.
3. Return `RegexLiteralExpression(pattern, source)`.

The `RegexLiteralExpression` AST node stores a single `std::string pattern` field containing the raw pattern text.

### Stage 3: Code Generation

In the legacy AST codegen path, regex literals are emitted as global string pointers:

```
llvm::Value* value = this->builder.CreateGlobalStringPtr(
    regexExpression.pattern, "regex.pattern"
);
```

The pattern string is stored as a null-terminated global constant (identical to string literal emission). At runtime, this string pointer is passed to the `RegExp` constructor.

The MIR production pipeline does not yet have dedicated regex literal handling. Regex literal support in the MIR path is planned as future work.

---

## The RegExp Standard Library Class

The `RegExp` class (`uranite.regexp.regexp.RegExp`) provides the runtime regex engine. It is a pure Uranite implementation — no C runtime dependency. All byte-level operations use raw syscalls (`readByteAt`, `stringToPtr`, `stringLen`).

### Construction

The `RegExp` constructor takes a pattern string and prepares it for matching:

```uranite
RegExp pattern = new RegExp( "\\d+" )
```

Or using regex literal syntax:

```uranite
RegExp pattern = /\d+/
```

The constructor:

1. Stores the original pattern string.
2. Converts the pattern to a raw byte pointer via `stringToPtr()`.
3. Records the pattern length via `stringLen()`.
4. Detects the `^` anchor — if the first byte is `^` (ASCII 94), sets `anchored = True`.

### Methods

| Method | Signature | Description |
|---|---|---|
| `test` | `(String input) -> Boolean` | Return `True` if the pattern matches anywhere in the input. |
| `find` | `(String input) -> Match` | Find the first match in the input. Returns a `Match` object. |
| `findFrom` | `(String input, I64 fromOffset) -> Match` | Find the first match starting at or after the given byte offset. |
| `findAll` | `(String input) -> ArrayList<Match>` | Find all non-overlapping matches in the input. |
| `matches` | `(String input) -> Boolean` | Return `True` if the pattern matches the entire input string (full match). |
| `replaceFirst` | `(String input, String replacement) -> String` | Replace the first match with the replacement string. |
| `replaceAll` | `(String input, String replacement) -> String` | Replace all non-overlapping matches with the replacement string. |
| `split` | `(String input) -> ArrayList<String>` | Split the input at pattern matches, returning the segments between matches. |

### The Match Class

The `Match` class (`uranite.regexp.match.Match`) stores the result of a regex search:

| Field | Type | Description |
|---|---|---|
| `text` | `String` | The original input string that was searched. |
| `start` | `I64` | Byte offset where the match begins (inclusive). |
| `end` | `I64` | Byte offset where the match ends (exclusive). |
| `matched` | `Boolean` | `True` if a match was found. |

| Method | Signature | Description |
|---|---|---|
| `group` | `() -> String` | Extract the matched substring from the original text. |
| `length` | `() -> I64` | Return the length of the matched region in bytes (`end - start`). |

The `noMatch()` factory function creates a `Match` with `matched = False`, empty text, and zero positions.

### Error Types

Two error types are defined in `uranite.regexp.errors`:

| Error | Description |
|---|---|
| `RegexSyntaxError` | Raised when a pattern contains invalid syntax (unmatched brackets, trailing backslashes, malformed quantifiers). |
| `RegexRuntimeError` | Raised when a regex operation fails at runtime (excessive backtracking, stack overflow, execution limits exceeded). |

Both extend `Error` and accept a message, numeric code, and optional cause.

---

## The Backtracking Engine

### Matching Algorithm

The `RegExp` engine uses recursive backtracking. The core `matchAtom()` function processes one pattern element at a time:

1. If the pattern is exhausted, return the current subject position (success).
2. If the current pattern byte is `$` (end anchor), succeed only if the subject is also exhausted.
3. If the subject is exhausted but pattern remains, check if remaining pattern can match zero characters (via `*` or `?` quantifiers).
4. Match the current pattern atom against the current subject byte:
   - `.` (dot): matches any byte.
   - `\` (backslash): dispatch to escape handler (`\d`, `\D`, `\w`, `\W`, `\s`, `\S`, or literal).
   - `[` (bracket): dispatch to character class matcher.
   - Any other byte: match literally.
5. If the atom is followed by a quantifier (`*`, `+`, `?`), apply quantifier logic with backtracking.
6. If the atom matches, recurse on the remaining pattern and subject.

For dot wildcards with `*` quantifier, the engine uses greedy matching: it tries matching from the end of the subject backward to the current position. For `+`, it does the same but requires at least one character consumed. For `?`, it tries matching with the character consumed first, then without.

### Alternation Handling

The `handleAlternation()` function splits the pattern at top-level `|` characters. The `hasAlternation()` function checks whether the pattern contains any pipe characters outside of groups (tracking parenthesis depth). If alternation is detected, each branch is tried independently from left to right, and the first matching branch determines the result.

### Performance Characteristics

| Operation | Time Complexity | Description |
|---|---|---|
| Simple literal match | O(n * m) | n = subject length, m = pattern length. |
| Quantifier match | O(n * m) typical | Greedy with backtracking. |
| Pathological pattern | O(2^n) worst case | Exponential backtracking on ambiguous quantifiers. |
| `replaceAll` | O(n * k) | n = subject length, k = number of matches. Safety limit of 10,000 iterations. |
| `findAll` | O(n * m) | Scans left to right, advancing past each match. |

The `replaceAll` and `findAll` methods include safety limits (10,000 iterations) to prevent infinite loops on zero-length matches.

---

## Examples

### Basic Pattern Matching

```uranite
from uranite.regexp.regexp import RegExp
from uranite.io.console import puts

public function main() -> I32:
    RegExp pattern = /hello/
    Boolean found = pattern.test( "hello, world" )
    if found:
        puts( "pattern found" )

    RegExp anchored = /^start/
    Boolean atStart = anchored.test( "start of line" )
    Boolean notAtStart = anchored.test( "not at start" )

    RegExp fullMatch = /^exact$/
    Boolean exact = fullMatch.matches( "exact" )
    Boolean partial = fullMatch.matches( "not exact" )
    return 0
```

### Character Classes and Quantifiers

```uranite
from uranite.regexp.regexp import RegExp
from uranite.regexp.match import Match
from uranite.io.console import puts

public function main() -> I32:
    RegExp digits = /\d+/
    Match result = digits.find( "order 12345 confirmed" )
    if result.matched:
        puts( result.group() )

    RegExp word = /[a-zA-Z]+/
    Match firstWord = word.find( "  hello world  " )
    if firstWord.matched:
        puts( firstWord.group() )

    RegExp optional = /colou?r/
    Boolean american = optional.test( "color" )
    Boolean british = optional.test( "colour" )

    RegExp email = /[a-z]+@[a-z]+\.[a-z]+/
    Boolean valid = email.test( "user@example.com" )
    return 0
```

### Search and Replace

```uranite
from uranite.regexp.regexp import RegExp
from uranite.io.console import puts

public function main() -> I32:
    RegExp whitespace = /\s+/
    String cleaned = whitespace.replaceAll( "hello   world   foo", " " )
    puts( cleaned )

    RegExp vowel = /[aeiou]/
    String first = vowel.replaceFirst( "hello", "*" )
    puts( first )

    String all = vowel.replaceAll( "hello world", "*" )
    puts( all )
    return 0
```

### String Splitting

```uranite
from uranite.regexp.regexp import RegExp
from uranite.collection.array-list import ArrayList
from uranite.io.console import puts

public function main() -> I32:
    RegExp comma = /,\s*/
    ArrayList<String> parts = comma.split( "one, two, three, four" )
    for String part in parts:
        puts( part )

    RegExp pipe = /\|/
    ArrayList<String> fields = pipe.split( "name|age|city" )
    for String field in fields:
        puts( field )
    return 0
```

### Practical Usage

```uranite
from uranite.regexp.regexp import RegExp
from uranite.regexp.match import Match
from uranite.collection.array-list import ArrayList
from uranite.io.console import puts

public function validateEmail( String email ) -> Boolean:
    RegExp pattern = /^[a-zA-Z0-9_.]+@[a-zA-Z0-9]+\.[a-zA-Z]+$/
    return pattern.matches( email )

public function extractNumbers( String text ) -> ArrayList<String>:
    RegExp digits = /\d+/
    ArrayList<Match> matches = digits.findAll( text )
    ArrayList<String> numbers = new ArrayList<String>()
    for Match matchResult in matches:
        numbers.add( matchResult.group() )
    return numbers

public function sanitizeInput( String raw ) -> String:
    RegExp tags = /<[^>]*>/
    String noTags = tags.replaceAll( raw, "" )
    RegExp extraSpaces = /\s+/
    return extraSpaces.replaceAll( noTags, " " )

public function main() -> I32:
    Boolean validEmail = validateEmail( "user@example.com" )
    Boolean invalidEmail = validateEmail( "not-an-email" )

    if validEmail:
        puts( "valid email" )
    if not invalidEmail:
        puts( "invalid email rejected" )

    ArrayList<String> nums = extractNumbers( "order 42, item 7, qty 100" )
    for String num in nums:
        puts( num )

    String dirty = "<b>hello</b>  <i>world</i>"
    String clean = sanitizeInput( dirty )
    puts( clean )
    return 0
```

This example demonstrates regex-based email validation with `matches()` for full-string matching, number extraction with `findAll()` and `group()`, and HTML tag stripping with `replaceAll()` using negated character classes.
