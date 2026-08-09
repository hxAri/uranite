# Lexical Conventions

This section documents how Uranite transforms raw source text into a stream of tokens. Every syntactic construct in the language — from a simple integer literal to a multi-module import declaration — begins as a sequence of tokens. Understanding lexical conventions is essential because Uranite treats whitespace as structural: indentation is not cosmetic but produces explicit block-opening and block-closing boundaries that determine program structure.

The lexer is a single-pass, character-by-character scanner. It processes the entire source file in one sweep, producing a flat sequence of tokens with no tree structure. The tokens are then consumed by the parser to build program structure.

---

## Table of Contents

- [Lexical Conventions](#lexical-conventions)
  - [Table of Contents](#table-of-contents)
  - [Lexical Pipeline Overview](#lexical-pipeline-overview)
  - [Token Categories](#token-categories)
    - [Keywords](#keywords)
    - [Literals](#literals)
    - [Identifiers](#identifiers)
    - [Operators and Punctuation](#operators-and-punctuation)
    - [Structural Tokens](#structural-tokens)
  - [Indentation as Block Boundaries](#indentation-as-block-boundaries)
  - [Comments and Doccomments](#comments-and-doccomments)
  - [Identifier and Keyword Resolution](#identifier-and-keyword-resolution)
  - [Literal Scanning](#literal-scanning)
    - [Numbers](#numbers)
    - [Strings](#strings)
    - [Characters](#characters)
    - [Regex](#regex)
  - [Operator and Punctuation Tokens](#operator-and-punctuation-tokens)
  - [Parenthesis Depth and Implicit Line Joining](#parenthesis-depth-and-implicit-line-joining)
  - [Line Continuation](#line-continuation)
  - [End-of-File Cleanup](#end-of-file-cleanup)
  - [Subpage Index](#subpage-index)

---

## Lexical Pipeline Overview

The lexer processes source text in a single loop. At each iteration, it examines the current character and dispatches to the appropriate handler:

1. **Line start processing.** At the beginning of each line, the lexer counts leading whitespace and compares the computed level against the current nesting depth. If indentation increases, a block-open boundary is recorded. If indentation decreases, one or more block-close boundaries are recorded. Inside parenthesized contexts (`()`, `[]`, `{}`), indentation is consumed silently — no block boundaries are produced.

2. **Whitespace skipping.** Spaces, tabs, and carriage returns outside of line-start context are consumed and discarded.

3. **Newline handling.** A newline character marks a logical line boundary (unless the previous boundary was redundant, or the lexer is inside a parenthesized context) and triggers line-start processing for the next iteration.

4. **Comment handling.** A `#` character triggers either a line comment (`#`) or a block comment (`#{...}#`). Triple-quoted strings (`"""..."""` or `'''...'''`) at the top level are also consumed as comments.

5. **Literal scanning.** Double-quote characters begin string scanning. Single-quote characters begin character literal scanning. Digit characters begin number scanning. A `/` character may begin regex literal scanning depending on the preceding token.

6. **Identifier and keyword scanning.** Alphabetic characters and underscores begin identifier accumulation. The accumulated text is checked against the keyword registry — if it matches a reserved keyword, the appropriate keyword token is produced; otherwise, a generic identifier token is produced.

7. **Operator and punctuation scanning.** All remaining characters produce operator and punctuation tokens. Multi-character operators (`==`, `!=`, `->`, `**`, `..`, `...`, `::`, `=>`, `<<`, `>>`) are resolved by lookahead, always matching the longest possible sequence.

After the main loop completes, the lexer performs end-of-file cleanup: it closes all remaining open blocks, appends a final line boundary if needed, and terminates with an end-of-file marker.

---

## Token Categories

The lexer produces tokens in six categories:

### Keywords

78 reserved words that the lexer distinguishes from identifiers. Keywords are grouped by purpose:

| Category | Keywords |
|---|---|
| Control Flow | `break`, `case`, `continue`, `else`, `elif`, `for`, `if`, `match`, `return`, `switch`, `while`, `yield` |
| Declarations | `class`, `const`, `enum`, `extern`, `from`, `function`, `implements`, `import`, `interface`, `package`, `static`, `struct`, `trait`, `type` |
| Access Modifiers | `private`, `protect`, `public` |
| OOP | `abstract`, `delete`, `extends`, `final`, `native`, `new`, `override`, `parent`, `property`, `readonly`, `Readonly`, `self`, `virtual` |
| Memory and Safety | `addressof`, `move`, `mut`, `own`, `reference`, `unsafe` |
| Logic | `and`, `not`, `or` |
| Values | `False`, `None`, `True` |
| Error Handling | `except`, `finally`, `raise`, `raises`, `try` |
| Other | `as`, `asm`, `async`, `await`, `backed`, `defer`, `export`, `in`, `instanceof`, `is`, `lambda`, `pass`, `subclassof`, `unit`, `use`, `volatile`, `where` |

Note that `True`, `False`, `None`, and `Readonly` are case-sensitive — `true`, `false`, `none`, and `READONLY` are not keywords and would be parsed as identifiers.

### Literals

| Token Type | Example |
|---|---|
| Integer | `42`, `0xFF`, `0o77`, `0b1010` |
| Float | `3.14`, `1.5e10`, `2.0E-3` |
| String | `"hello"` |
| Char | `'A'`, `'\n'`, `'\x41'` |
| Regex | `/[a-z]+/` |

### Identifiers

Any sequence of alphanumeric characters and underscores starting with a letter or underscore that does not match a keyword entry. Identifiers are case-sensitive: `counter`, `Counter`, and `COUNTER` are three distinct identifiers.

### Operators and Punctuation

Multi-character operators are resolved by lookahead. The lexer checks for the longest matching sequence first:

| Tokens | Characters |
|---|---|
| Arithmetic | `+`, `-`, `*`, `/`, `%`, `**` |
| Comparison | `==`, `!=`, `<`, `>`, `<=`, `>=` |
| Assignment | `=`, `+=`, `-=`, `*=`, `/=`, `%=`, `&=`, `\|=`, `^=`, `<<=`, `>>=` |
| Bitwise | `&`, `\|`, `^`, `~`, `<<`, `>>` |
| Navigation | `.`, `..`, `...`, `->`, `=>`, `::` |
| Increment/Decrement | `++`, `--` |
| Delimiters | `(`, `)`, `[`, `]`, `{`, `}` |
| Separators | `,`, `:`, `;` |
| Other | `?`, `@`, `!` |

### Structural Tokens

| Token | Meaning |
|---|---|
| Block open | Indentation level increased — a new block begins. |
| Block close | Indentation level decreased — one block ends per decreased level. |
| Newline | Logical line boundary. Suppressed inside parenthesized contexts. |
| End of file | Always the last token in the stream. |

---

## Indentation as Block Boundaries

The defining lexical characteristic of Uranite is that indentation produces explicit block boundaries. The parser never examines whitespace — it consumes block-open and block-close tokens the same way a C parser consumes `{` and `}`.

The indentation algorithm:

1. At line start, count leading whitespace. Each space counts as 1 unit. Each tab counts as 4 units.
2. Skip blank lines (lines that contain only whitespace) and comment-only lines (lines starting with `#` after whitespace) — neither produces block boundary tokens.
3. Compare the computed indentation level against the current nesting depth:
   - **Greater than current level**: A new block opens. One block-open token is emitted.
   - **Equal to current level**: No token emitted. The line continues at the same block level.
   - **Less than current level**: One or more blocks close. One block-close token is emitted per level difference. If the decreased level does not match any enclosing level, the compiler reports an error: "inconsistent indentation — indentation does not match any outer level".

Consider this source:

```uranite
public function outer() -> Void:
    I64 count = 0
    if count == 0:
        I64 inner = 1
        if inner == 1:
            puts( "deep" )
        puts( "shallow" )
    puts( "top" )
```

The structural token stream for this fragment (showing only block boundaries):

```
Block open             (0 -> 4, entering outer body)
Newline
Newline
Block open             (4 -> 8, entering if body)
Newline
Newline
Block open             (8 -> 12, entering nested if body)
Newline
Block close            (12 -> 8, exiting nested if)
Newline
Block close            (8 -> 4, exiting if)
Newline
Block close            (4 -> 0, exiting outer body)
```

Each block-open token opens exactly one block. Each block-close token closes exactly one block. Multiple blocks can close on a single line — if the code jumps from indentation level 12 back to level 0, three block-close tokens are emitted in sequence.

---

## Comments and Doccomments

Uranite has three comment forms:

**Line comments** start with `#` and extend to the end of the line. No token is emitted.

```uranite
# This is a line comment
I64 value = 42
```

**Block comments** use `#{` to open and `}#` to close. They support nesting — a `#{` inside a block comment opens a nested comment that must be closed with its own `}#`. No token is emitted.

```uranite
#{
    This is a block comment.
    It can span multiple lines.
    #{ And it can be nested. }#
}#
```

**Triple-quoted strings** (`"""..."""` or `'''...'''`) at the top level are consumed as comments. When a triple-quoted string appears as the first statement after a declaration, it serves as a doccomment:

```uranite
public function hash( I64 value ) -> I64:

    """
    Compute a simple hash of an integer value.

    Parameters:
        value (I64):
            The integer to hash.

    Returns:
        I64:
            The computed hash value.

    Complexity:
        Time: O(1)
        Space: O(1)
    """

    return value * 2654435761
```

Doccomments follow a structured format with "Parameters:", "Returns:", and "Complexity:" sections. This format is enforced by the linter (`uranite-fmt --lint`), which flags doccomments that use `@param` style tags or omit the "Complexity:" section.

---

## Identifier and Keyword Resolution

When the lexer encounters an alphabetic character or underscore, it accumulates all following characters matching `[a-zA-Z0-9_]`, then checks the accumulated string against the keyword registry. If a match is found, the corresponding keyword token is produced. Otherwise, a generic identifier token is produced.

```uranite
I64 slotIndex = 0
```

In this line, the lexer produces:

| Position | Text | Token Type |
|---|---|---|
| 1 | `I64` | Identifier (not a keyword — type names are identifiers) |
| 2 | `slotIndex` | Identifier |
| 3 | `=` | Assignment |
| 4 | `0` | Integer literal |

Note that `I64` is an identifier, not a keyword. Type names like `I64`, `String`, `Boolean`, `ArrayList`, and `HashMap` are all identifiers resolved during semantic analysis, not reserved words. Only language constructs like `function`, `class`, `if`, `return`, `and`, `or`, `not`, `True`, `False`, `None`, and `self` are keywords.

---

## Literal Scanning

### Numbers

The lexer handles four integer bases and floating-point numbers:

- **Decimal**: `42`, `1_000_000` (underscores as digit separators, silently stripped)
- **Binary**: `0b1010`, `0B1111_0000`
- **Octal**: `0o755`, `0O644`
- **Hexadecimal**: `0xFF`, `0x0000_FFFF`

Floating-point numbers are detected when a decimal point appears followed by a digit (`3.14`), or when an exponent suffix appears (`1e10`, `2.5E-3`). The exponent can include a sign (`+` or `-`).

Number literals can include a trailing alphabetic suffix (e.g., `100u64`), which is captured as part of the token value for type inference.

### Strings

Double-quoted strings support the following escape sequences:

| Escape | Character |
|---|---|
| `\"` | Double quote |
| `\'` | Single quote |
| `\\` | Backslash |
| `\n` | Newline (LF) |
| `\r` | Carriage return (CR) |
| `\t` | Horizontal tab |
| `\0` | Null byte |
| `\xHH` | Hexadecimal byte (2 hex digits) |

String literals cannot span multiple lines. A newline character inside a string produces an "unterminated string literal" error.

### Characters

Single-quoted character literals support the same escape sequence set as strings. Character literals must contain exactly one character (or one escape sequence). An unclosed quote produces an "unterminated character literal" error.

### Regex

Regex literal scanning is context-sensitive. A `/` character is interpreted as the start of a regex literal only when the preceding token is not a value-producing expression (identifier, literal, `)`, `]`, `self`, `True`, `False`, `None`). This disambiguation prevents `/` in arithmetic from being misinterpreted as a regex delimiter.

The regex scanner handles escape sequences (`\/` to include a literal `/`), character classes (`[...]` where `/` is not a delimiter), and terminates at the closing unescaped `/`.

---

## Operator and Punctuation Tokens

The lexer resolves multi-character operators by greedy lookahead — it always matches the longest possible operator:

- `**` is matched before `*`
- `==` is matched before `=`
- `!=` is matched before `!`
- `->` is matched before `-`
- `=>` is matched before `=`
- `::` is matched before `:`
- `..` and `...` are matched before `.`
- `<<` and `<<=` are matched before `<`
- `>>` and `>>=` are matched before `>`
- `++` is matched before `+`
- `--` is matched before `-`

This is a standard maximal-munch strategy. One-character lookahead determines whether a longer operator match exists.

---

## Parenthesis Depth and Implicit Line Joining

The lexer tracks a nesting depth counter that increments on `(`, `[`, and `{`, and decrements on `)`, `]`, and `}`. When the depth is greater than zero:

- **Newlines are suppressed.** No newline token is emitted. This allows expressions to span multiple lines inside parenthesized contexts without backslash continuation.
- **Indentation is ignored.** Leading whitespace is consumed silently without producing block boundary tokens.

This means multi-line function calls, collection literals, and import blocks work naturally:

```uranite
from uranite.collection import {
    ArrayList,
    HashMap,
    HashSet,
    Pair
}

ArrayList<I64> numbers = new ArrayList<>(
    16
)

I64 result = compute(
    firstArgument,
    secondArgument,
    thirdArgument
)
```

Inside the braces and parentheses, newlines and indentation produce no tokens. The parser sees a flat sequence of identifiers, commas, and the closing delimiter.

---

## Line Continuation

A backslash (`\`) immediately followed by a newline acts as a line continuation. The lexer consumes both characters and continues scanning the next line as if the break did not exist:

```uranite
I64 total = firstValue + secondValue \
    + thirdValue + fourthValue
```

This is the explicit line-joining mechanism for contexts outside parenthesized expressions. Inside `()`, `[]`, or `{}`, line continuation is implicit and the backslash is unnecessary.

---

## End-of-File Cleanup

After the main scanning loop exhausts all source characters, the lexer performs three cleanup steps:

1. **Close remaining blocks.** While blocks remain open, the lexer emits block-close tokens for each level. This ensures every block-open has a matching block-close, even if the file ends mid-block.

2. **Append trailing newline.** If the last token in the stream is not already a line boundary, one is appended. This normalizes the token stream so the parser can always expect a line boundary before end-of-file.

3. **Append end-of-file marker.** An end-of-file token is always the final token in the stream.

---

## Subpage Index

Each lexical topic is covered in depth in its own document:

| Document | Description |
|---|---|
| [Source Files and Encoding](source-files-and-encoding.md) | File encoding requirements (UTF-8), the `.urn` extension, and source file structure rules. |
| [Comments and Doccomments](comments-and-doccomments.md) | Line comments (`#`), block comments (`#{...}#`), and triple-quoted doccomments with the "Parameters:", "Returns:", "Complexity:" format. |
| [Indentation and Blocks](indentation-and-blocks.md) | How indentation defines block boundaries: tab normalization (1 tab = 4 units), the 4-space convention, and common indentation errors. |
| [Reserved Keywords](reserved-keywords.md) | Complete enumeration of all 78 reserved keywords organized by category with descriptions and usage context. |
| [Identifiers and Naming](identifiers-and-naming.md) | Identifier character rules (`[a-zA-Z_][a-zA-Z0-9_]*`), case sensitivity, naming conventions, and linter enforcement of descriptive names. |
| [Integer Literals](integer-literals.md) | Decimal, hexadecimal (`0x`), octal (`0o`), and binary (`0b`) formats. Underscore digit separators. Numeric suffixes. |
| [Float Literals](float-literals.md) | Decimal floating-point syntax, scientific notation (`1.5e10`), and IEEE 754 representation. |
| [String Literals](string-literals.md) | Double-quoted strings, escape sequences (`\n`, `\t`, `\xHH`, etc.), and UTF-8 encoding. |
| [Char Literals](char-literals.md) | Single-quoted character literals, escape sequences, and the 32-bit `Char` type. |
| [Boolean and None Literals](boolean-and-none-literals.md) | `True`, `False`, and `None` as keyword tokens — case-sensitive, not identifiers. |
| [Regex Literals](regex-literals.md) | `/pattern/` syntax, context-sensitive disambiguation from division, escape handling, and character class support. |
