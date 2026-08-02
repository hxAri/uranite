# Regular Expression Literals

- [Regular Expression Literals](#regular-expression-literals)
  - [Table of Contents](#table-of-contents)
  - [Overview](#overview)
  - [Regex Literal Syntax](#regex-literal-syntax)
    - [Literal Form vs String Construction](#literal-form-vs-string-construction)
    - [Escape Handling in Regex Literals](#escape-handling-in-regex-literals)
    - [Restrictions](#restrictions)
  - [Slash Disambiguation](#slash-disambiguation)
    - [The Rule](#the-rule)
    - [The Whitespace Guard](#the-whitespace-guard)
    - [Disambiguation Summary](#disambiguation-summary)
  - [Supported Pattern Syntax](#supported-pattern-syntax)
    - [Literal Characters](#literal-characters)
    - [Dot Wildcard](#dot-wildcard)
    - [Quantifiers](#quantifiers)
    - [Character Classes](#character-classes)
    - [Escape Sequences](#escape-sequences)
    - [Anchors](#anchors)
    - [Alternation](#alternation)
    - [Grouping](#grouping)
  - [The RegExp Class](#the-regexp-class)
    - [Importing](#importing)
    - [Construction](#construction)
    - [Methods](#methods)
  - [The Match Class](#the-match-class)
    - [Fields](#fields)
    - [Match Methods](#match-methods)
    - [The noMatch Factory Function](#the-nomatch-factory-function)
  - [Error Types](#error-types)
  - [Engine Behavior](#engine-behavior)
    - [Greedy Matching](#greedy-matching)
    - [Backtracking](#backtracking)
    - [Alternation Resolution](#alternation-resolution)
    - [Safety Limits](#safety-limits)
    - [Performance Characteristics](#performance-characteristics)
  - [Examples](#examples)
    - [Basic Pattern Matching](#basic-pattern-matching)
    - [Character Classes and Quantifiers](#character-classes-and-quantifiers)
    - [Search and Replace](#search-and-replace)
    - [String Splitting](#string-splitting)
    - [Finding All Matches](#finding-all-matches)
    - [Full-String Validation](#full-string-validation)
    - [Practical Usage](#practical-usage)

---

## Table of Contents

See above.

---

## Overview

Uranite supports native regular expression literals delimited by forward slashes. Regex literals are first-class syntax — the compiler recognizes them directly, and the standard library provides the `RegExp` class for pattern matching, searching, replacing, and splitting strings.

| Property | Value |
|---|---|
| Delimiter | Forward slashes (`/pattern/`) |
| Runtime type | `RegExp` |
| Engine | Recursive backtracking |
| Supported features | Literals, dot, quantifiers (`*`, `+`, `?`), character classes, escape sequences, anchors (`^`, `$`), alternation (`\|`), grouping (`()`) |

---

## Regex Literal Syntax

A regex literal begins with a forward slash, contains the pattern, and ends with a closing forward slash:

```uranite
RegExp emailPattern = /[a-z]+@[a-z]+\.[a-z]+/
RegExp digits = /\d+/
RegExp greeting = /^hello$/
```

The content between the slashes is the raw pattern. Backslashes within the pattern are regex metacharacters (such as `\d` for digit or `\.` for literal dot) and are preserved exactly as written.

### Literal Form vs String Construction

Regex literals and string-constructed `RegExp` objects produce identical results. The literal form avoids double-escaping:

```uranite
RegExp literalForm = /\d+/
RegExp stringForm = new RegExp( "\\d+" )
```

Both create a `RegExp` that matches one or more digits. In the string form, each backslash must be doubled because string literals process escape sequences at compile time. In the literal form, the backslash is preserved as-is — what you see in the pattern is exactly what the regex engine receives.

For simple patterns, the literal form is more readable. For patterns constructed at runtime (from user input or concatenation), the string constructor is required.

### Escape Handling in Regex Literals

Inside a regex literal, backslash escapes work differently from string literals:

- In a **string literal**, `\n` becomes a newline byte at compile time.
- In a **regex literal**, `\n` remains the two characters `\` and `n` — the regex engine interprets them at runtime.

This means regex escape sequences like `\d`, `\w`, `\s` work naturally without double-escaping. To include a literal forward slash inside a regex pattern, escape it with a backslash:

```uranite
RegExp pathPattern = /usr\/local\/bin/
```

To include a literal backslash in the pattern, use a double backslash:

```uranite
RegExp backslashPattern = /path\\to\\file/
```

### Restrictions

Regex literals cannot span multiple lines. If a newline character appears before the closing slash, the literal ends at that point. Multi-line patterns must use the string constructor.

Regex literals do not support flags (such as case-insensitive or multiline modifiers). All matching is case-sensitive and single-line by default.

---

## Slash Disambiguation

The forward slash (`/`) serves two purposes in Uranite: as the division operator and as the regex literal delimiter. The compiler decides which interpretation to apply based on context.

### The Rule

When the compiler encounters a `/`, it checks what came immediately before it. If the preceding element is something that produces a value — a variable, a number, a closing parenthesis, a closing bracket, `self`, `True`, `False`, or `None` — then `/` is the division operator. In every other context, `/` begins a regex literal.

| Preceding Element | `/` Interpreted As | Example |
|---|---|---|
| Variable name | Division | `x / 2` |
| Integer | Division | `10 / 5` |
| Float | Division | `3.14 / 2.0` |
| Closing `)` | Division | `( x + y ) / z` |
| Closing `]` | Division | `arr[0] / 2` |
| `self` | Division | `self.value / 2` |
| `True`, `False`, `None` | Division | (syntactically possible) |
| Assignment `=` | Regex | `RegExp pattern = /\d+/` |
| Opening `(` | Regex | `( /abc/ )` |
| Keyword (`return`, `if`, etc.) | Regex | `return /\d+/` |
| Comma | Regex | `call( /abc/, value )` |
| Nothing (start of expression) | Regex | `/pattern/` |

### The Whitespace Guard

After determining that `/` should begin a regex, one additional check is applied: the character immediately after the `/` must not be a space or newline. If it is, the `/` is treated as division instead.

This prevents ambiguity in expressions where a space follows the slash. In practice, regex patterns always begin immediately after the opening slash with no whitespace.

### Disambiguation Summary

The logic is straightforward in practice: if the slash could be dividing a value, it is division. If it appears where an expression is expected (after `=`, `(`, `return`, `,`, or at the start), it begins a regex literal. The whitespace guard handles the remaining edge cases.

---

## Supported Pattern Syntax

### Literal Characters

Any character that is not a metacharacter matches itself. The metacharacters are: `. * + ? [ ] ( ) | ^ $ \`

```uranite
RegExp literal = /hello/
```

This pattern matches the exact sequence "hello" anywhere in the subject string.

### Dot Wildcard

The `.` metacharacter matches any single byte:

```uranite
RegExp anyMiddle = /h.t/
```

This matches "hat", "hit", "hot", "h9t", and any other three-character sequence starting with "h" and ending with "t".

### Quantifiers

Three quantifiers control how many times the preceding element can repeat:

| Quantifier | Meaning | Example | Matches |
|---|---|---|---|
| `*` | Zero or more | `/ab*c/` | "ac", "abc", "abbc", "abbbc" |
| `+` | One or more | `/ab+c/` | "abc", "abbc" (not "ac") |
| `?` | Zero or one | `/colou?r/` | "color", "colour" |

Quantifiers apply to the immediately preceding atom — a literal character, a dot, an escape sequence, or a character class. The engine uses greedy matching by default, consuming as many characters as possible before backtracking if necessary.

### Character Classes

Character classes match a single character from a defined set:

| Syntax | Description | Example |
|---|---|---|
| `[abc]` | Match any one of "a", "b", or "c" | `/[aeiou]/` |
| `[a-z]` | Match any character in the range | `/[a-zA-Z]/` |
| `[^abc]` | Match any character NOT in the set | `/[^0-9]/` |
| `[^a-z]` | Match any character NOT in the range | `/[^a-z]/` |

The `-` character between two characters defines a range based on byte values. The `^` at the start of a class negates it, matching any character not listed. Multiple ranges and literal characters can be combined freely: `[a-zA-Z0-9_]`.

A forward slash inside a character class does not end the regex literal. The pattern `/[a/b]/` is valid — the `/` between the brackets is part of the character class.

### Escape Sequences

Backslash escape sequences match predefined character categories:

| Escape | Equivalent | Description |
|---|---|---|
| `\d` | `[0-9]` | Any ASCII digit |
| `\D` | `[^0-9]` | Any non-digit |
| `\w` | `[a-zA-Z0-9_]` | Any word character (alphanumeric or underscore) |
| `\W` | `[^a-zA-Z0-9_]` | Any non-word character |
| `\s` | `[ \t\n\r\f\v]` | Any whitespace (space, tab, newline, carriage return, form feed, vertical tab) |
| `\S` | `[^ \t\n\r\f\v]` | Any non-whitespace |
| `\.` | `.` | Literal dot (not wildcard) |
| `\\` | `\` | Literal backslash |
| `\/` | `/` | Literal forward slash |

Any character preceded by `\` that is not a recognized escape is matched literally — the backslash is consumed and the character matches itself.

### Anchors

Two anchors constrain where the pattern can match:

| Anchor | Description |
|---|---|
| `^` | Match only at the start of the string |
| `$` | Match only at the end of the string |

When `^` is present at the beginning of a pattern, the engine only attempts matching at position zero. Without `^`, the engine tries matching at every position from the beginning to the end of the subject string.

When `$` is present at the end of a pattern, the match succeeds only if the subject string is fully consumed at that point.

Combining both anchors requires the entire string to match:

```uranite
RegExp exact = /^hello$/
Boolean fullMatch = exact.test( "hello" )
Boolean partialFail = exact.test( "hello world" )
```

The first test returns `True` because "hello" matches the entire anchored pattern. The second returns `False` because the subject continues past "hello".

### Alternation

The pipe character (`|`) separates alternative branches:

```uranite
RegExp color = /red|green|blue/
```

The engine splits the pattern at top-level pipe characters (respecting group nesting) and tries each branch from left to right. The first matching branch wins. If no branch matches, the overall match fails.

### Grouping

Parentheses group sub-patterns together, affecting quantifier scope and alternation boundaries:

```uranite
RegExp repeated = /(abc)+/
RegExp choice = /(red|blue) car/
```

In the first pattern, `+` applies to the entire group "abc", matching "abc", "abcabc", "abcabcabc", and so on. Without the parentheses, `+` would apply only to the character "c".

In the second pattern, the alternation is confined within the group. The pattern matches "red car" or "blue car".

---

## The RegExp Class

### Importing

The `RegExp` class lives in the `uranite.regexp` package. Import it directly or through the package module:

```uranite
from uranite.regexp.regexp import RegExp
```

```uranite
from uranite.regexp import RegExp
```

### Construction

Create a `RegExp` from a regex literal or a string:

```uranite
RegExp fromLiteral = /\d+/
RegExp fromString = new RegExp( "\\d+" )
```

Both forms produce the same result. The constructor stores the pattern, detects whether it begins with the `^` anchor, and prepares it for matching.

### Methods

| Method | Signature | Description |
|---|---|---|
| `test` | `( String input ) -> Boolean` | Return `True` if the pattern matches anywhere in the input string. |
| `find` | `( String input ) -> Match` | Find the first match in the input. Returns a `Match` object with `matched` set to `True` if found, or a no-match `Match` if not found. |
| `findFrom` | `( String input, I64 fromOffset ) -> Match` | Find the first match starting at or after the given byte offset. |
| `findAll` | `( String input ) -> ArrayList<Match>` | Find all non-overlapping matches in the input, scanning left to right. Returns an `ArrayList` of `Match` objects. |
| `matches` | `( String input ) -> Boolean` | Return `True` if the pattern matches the **entire** input string from start to end. Unlike `test`, which succeeds on partial matches, `matches` requires the full string to conform. |
| `replaceFirst` | `( String input, String replacement ) -> String` | Replace the first occurrence of the pattern with the replacement string. Returns the original string unchanged if no match is found. |
| `replaceAll` | `( String input, String replacement ) -> String` | Replace all non-overlapping occurrences of the pattern with the replacement string. |
| `split` | `( String input ) -> ArrayList<String>` | Split the input string at every occurrence of the pattern. Returns the segments between matches as an `ArrayList<String>`. If no match is found, returns a single-element list containing the entire input. |

The distinction between `test` and `matches` is important. `test` checks whether the pattern occurs anywhere inside the input — it succeeds on partial matches. `matches` requires the entire input string to conform to the pattern from beginning to end:

```uranite
RegExp digits = /\d+/

Boolean testResult = digits.test( "abc 123 xyz" )
Boolean matchResult = digits.matches( "abc 123 xyz" )
```

Here `testResult` is `True` (the pattern finds "123" inside the string) but `matchResult` is `False` (the entire string is not composed of digits).

---

## The Match Class

The `Match` class represents the result of a regex search operation. Import it from the same package:

```uranite
from uranite.regexp.match import Match
```

### Fields

| Field | Type | Description |
|---|---|---|
| `text` | `String` | The original input string that was searched. |
| `start` | `I64` | Byte offset where the match begins (inclusive). |
| `end` | `I64` | Byte offset where the match ends (exclusive). |
| `matched` | `Boolean` | `True` if a match was found, `False` otherwise. |

The `start` and `end` fields define a half-open range: `start` is the index of the first byte in the match, and `end` is the index of the first byte after the match.

### Match Methods

| Method | Signature | Description |
|---|---|---|
| `group` | `() -> String` | Extract and return the matched substring from the original text. |
| `length` | `() -> I64` | Return the length of the matched region in bytes (`end - start`). |

Always check `matched` before calling `group` or accessing position fields:

```uranite
RegExp pattern = /\d+/
Match result = pattern.find( "order 42 confirmed" )
if result.matched:
    String matchedText = result.group()
    I64 startPosition = result.start
    I64 endPosition = result.end
    I64 matchLength = result.length()
```

### The noMatch Factory Function

The `noMatch` function creates a `Match` representing a failed search — `matched` is `False`, `text` is empty, and both `start` and `end` are zero:

```uranite
from uranite.regexp.match import noMatch

Match failed = noMatch()
```

This is primarily used internally by `RegExp` methods when no match is found. In user code, you typically receive a no-match `Match` from `find` or `findFrom` and check the `matched` field.

---

## Error Types

Two error types are defined for regex operations:

| Error | When Raised |
|---|---|
| `RegexSyntaxError` | The pattern contains invalid syntax — unmatched brackets, a trailing backslash, or malformed quantifiers. Raised during `RegExp` construction. |
| `RegexRuntimeError` | A regex operation fails at runtime — excessive backtracking, stack overflow, or execution limits exceeded. |

Both extend `Error` and accept a message, numeric code, and optional cause:

```uranite
from uranite.regexp.errors import RegexSyntaxError, RegexRuntimeError

public function compilePattern( String raw ) -> RegExp raises RegexSyntaxError:
    return new RegExp( raw )
```

Import them from `uranite.regexp.errors` or directly from `uranite.regexp`.

---

## Engine Behavior

### Greedy Matching

All three quantifiers (`*`, `+`, `?`) are greedy by default. The engine consumes as many characters as possible for the quantified element, then backtracks if the remainder of the pattern cannot match.

For example, with pattern `/a.*b/` and input "aXXbYYb", the `.*` initially consumes "XXbYY" (everything up to the end), then backtracks character by character until it finds a position where `b` can match. The result is "aXXbYYb" — the longest possible match.

There are no lazy (non-greedy) quantifiers. If you need shortest-match behavior, restructure the pattern using negated character classes:

```uranite
RegExp greedy = /a.*b/
RegExp shortest = /a[^b]*b/
```

The first matches "aXXbYYb" (longest). The second matches "aXXb" (stops at the first "b") because `[^b]*` matches any character except "b".

### Backtracking

The engine uses recursive backtracking. When a quantifier consumes characters and the rest of the pattern fails, the engine "backs up" and tries consuming fewer characters. This process repeats until either a valid match is found or all possibilities are exhausted.

Backtracking is invisible during normal use — the engine automatically finds the correct match. However, pathological patterns with nested quantifiers can cause exponential backtracking, where the engine explores an enormous number of possibilities before concluding that no match exists.

Patterns to avoid for performance reasons:

```uranite
RegExp dangerous = /(a+)+$/
RegExp alsoSlow = /(a|aa)+$/
```

These patterns cause the engine to explore exponentially many ways to partition the input among the nested quantifiers when the input almost-but-not-quite matches. Restructure such patterns to eliminate the ambiguity.

### Alternation Resolution

When a pattern contains the pipe character (`|`), the engine splits at top-level pipes (respecting parenthesis nesting) and tries each branch from left to right. The first branch that produces a match wins — remaining branches are not attempted.

```uranite
RegExp priority = /cat|catch/
Match result = priority.find( "catch" )
```

This matches "cat" (the first branch succeeds at position 0), not "catch". If you need the longer match, place longer alternatives first: `/catch|cat/`.

### Safety Limits

The `replaceAll` and `findAll` methods enforce a safety limit of 10,000 iterations to prevent infinite loops. If a zero-length match is found (a pattern that matches the empty string), the search advances by one byte to ensure progress. After 10,000 iterations, the method returns whatever results have been accumulated so far.

### Performance Characteristics

| Operation | Typical Complexity | Notes |
|---|---|---|
| Simple literal match | O(n * m) | n = subject length, m = pattern length |
| Quantifier match | O(n * m) | Greedy with backtracking |
| Pathological pattern | O(2^n) worst case | Exponential backtracking on ambiguous quantifiers |
| `replaceAll` | O(n * k) | n = subject length, k = number of matches. Safety limit of 10,000 iterations. |
| `findAll` | O(n * m) | Scans left to right, advancing past each match. Safety limit of 10,000 iterations. |
| `split` | O(n * m) | Same scan-and-advance as `findAll`. Safety limit of 10,000 iterations. |

For most real-world patterns, performance is linear or near-linear. Exponential behavior only occurs with pathological patterns containing nested quantifiers on overlapping character sets.

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

The `test` method returns `True` if the pattern matches anywhere in the input. The `^` anchor restricts matching to the beginning of the string. Combining `^` and `$` with `matches` requires the entire string to conform.

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

The `find` method returns a `Match` object. Calling `group()` on a successful match extracts the matched substring. The `?` quantifier makes the preceding character optional.

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

`replaceFirst` replaces only the first occurrence. `replaceAll` replaces every non-overlapping occurrence. Both return a new string — the original is unchanged.

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

The `split` method divides the input at each pattern match and returns the segments between matches. If no match is found, the entire input is returned as a single-element list.

### Finding All Matches

```uranite
from uranite.regexp.regexp import RegExp
from uranite.regexp.match import Match
from uranite.collection.array-list import ArrayList
from uranite.io.console import puts

public function main() -> I32:
    RegExp digits = /\d+/
    ArrayList<Match> allNumbers = digits.findAll( "item 42, qty 7, total 100" )
    for Match numberMatch in allNumbers:
        puts( numberMatch.group() )
    return 0
```

`findAll` returns every non-overlapping match as an `ArrayList<Match>`. After each match, the next search begins at the end of the previous match.

### Full-String Validation

```uranite
from uranite.regexp.regexp import RegExp
from uranite.io.console import puts

public function main() -> I32:
    RegExp integerPattern = /^\d+$/
    RegExp hexPattern = /^0x[0-9a-fA-F]+$/

    Boolean isInteger = integerPattern.matches( "12345" )
    Boolean isHex = hexPattern.matches( "0xFF0A" )
    Boolean notInteger = integerPattern.matches( "123abc" )

    if isInteger:
        puts( "valid integer" )
    if isHex:
        puts( "valid hex" )
    if not notInteger:
        puts( "not a valid integer" )
    return 0
```

Use `matches` for full-string validation. It requires the entire input to conform to the pattern. Combine with `^` and `$` anchors for explicit start-to-end matching.

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

This example demonstrates email validation using `matches` for full-string conformance, number extraction using `findAll` and `group`, and HTML tag stripping using `replaceAll` with a negated character class pattern.
