# Lexical Conventions

This section documents how the Uranite compiler transforms raw source text into a stream of tokens. Every syntactic construct in the language — from a simple integer literal to a multi-module import declaration — begins as a sequence of tokens produced by the lexer. Understanding lexical conventions is essential because Uranite treats whitespace as structural: indentation is not cosmetic but produces explicit `Indent` and `Dedent` tokens that the parser consumes as block delimiters.

The lexer (`src/uranite/lexer/lexer.cpp`, ~745 lines) is a single-pass, character-by-character scanner. It maintains an indentation stack, a parenthesis depth counter, and a position tracker with line and column coordinates. It produces a flat `std::vector<token::Token>` — no tree structure exists at this stage.

---

## Table of Contents

- [Lexical Pipeline Overview](#lexical-pipeline-overview)
- [Token Categories](#token-categories)
- [Indentation as Tokens](#indentation-as-tokens)
- [Comments and Doccomments](#comments-and-doccomments)
- [Identifier and Keyword Resolution](#identifier-and-keyword-resolution)
- [Literal Scanning](#literal-scanning)
- [Operator and Punctuation Tokens](#operator-and-punctuation-tokens)
- [Parenthesis Depth and Implicit Line Joining](#parenthesis-depth-and-implicit-line-joining)
- [Line Continuation](#line-continuation)
- [End-of-File Cleanup](#end-of-file-cleanup)
- [Subpage Index](#subpage-index)

---

## Lexical Pipeline Overview

The lexer processes source text in a single loop. At each iteration, it examines the current character and dispatches to the appropriate handler:

1. **Line start processing.** If the `atLineStart` flag is set and the parenthesis depth is zero, `handleIndentation()` runs. It counts leading whitespace, compares the computed level against the indentation stack, and emits `Indent` or `Dedent` tokens as needed. If the parenthesis depth is nonzero (inside `(`, `[`, or `{`), indentation is consumed silently — no `Indent`/`Dedent` tokens are emitted.

2. **Whitespace skipping.** Spaces, tabs, and carriage returns outside of line-start context are consumed and discarded.

3. **Newline handling.** A newline character emits a `Newline` token (unless the previous token was already a `Newline`, `Indent`, or `Dedent`, or the parenthesis depth is nonzero) and sets the `atLineStart` flag for the next iteration.

4. **Comment handling.** A `#` character triggers either a line comment (`#`) or a block comment (`#{...}#`). Triple-quoted strings (`"""..."""` or `'''...'''`) at the top level are also consumed as comments by the lexer.

5. **Literal scanning.** Double-quote characters dispatch to `readString()`. Single-quote characters dispatch to `readChar()`. Digit characters dispatch to `readNumber()`. A `/` character may dispatch to regex literal scanning depending on the preceding token.

6. **Identifier and keyword scanning.** Alphabetic characters and underscores dispatch to `readIdentifierOrKeyword()`, which accumulates characters and checks the result against the keyword registry.

7. **Operator and punctuation scanning.** All remaining characters are handled by a switch statement that produces operator and punctuation tokens, using one-character lookahead for multi-character operators (`==`, `!=`, `->`, `**`, `..`, `...`, `::`, `=>`, `<<`, `>>`).

After the main loop completes, the lexer performs end-of-file cleanup: it pops all remaining indentation levels, emits the corresponding `Dedent` tokens, appends a final `Newline` if needed, and terminates with an `Eof` token.

---

## Token Categories

The lexer produces tokens in six categories:

### Keywords (74 unique)

Reserved words that the lexer distinguishes from identifiers by checking against a static `keymaps()` registry. Keywords are grouped by purpose:

| Category | Keywords |
|---|---|
| Control Flow | `break`, `continue`, `else`, `elif`, `for`, `if`, `match`, `return`, `while`, `yield` |
| Declarations | `class`, `const`, `enum`, `extern`, `from`, `function`, `implements`, `import`, `interface`, `mut`, `package`, `static`, `struct`, `type` |
| Access Modifiers | `private`, `protect`, `public` |
| OOP | `abstract`, `delete`, `extends`, `final`, `native`, `new`, `override`, `parent`, `property`, `readonly` / `Readonly`, `self`, `virtual` |
| Memory and Safety | `addressof`, `move`, `own`, `reference`, `unsafe` |
| Logic | `and`, `not`, `or` |
| Values | `False`, `None`, `True` |
| Error Handling | `except`, `finally`, `raise`, `raises`, `try` |
| Other | `as`, `asm`, `async`, `await`, `backed`, `case`, `defer`, `export`, `in`, `instanceof`, `is`, `lambda`, `pass`, `subclassof`, `switch`, `trait`, `unit`, `use`, `volatile`, `where` |

Note that `True`, `False`, `None`, and `Readonly` are case-sensitive — `true`, `false`, `none`, and `READONLY` are not keywords and would be parsed as identifiers.

### Literals

| Token Type | Example | Scanner |
|---|---|---|
| `LiteralInteger` | `42`, `0xFF`, `0o77`, `0b1010` | `readNumber()` |
| `LiteralFloat` | `3.14`, `1.5e10`, `2.0E-3` | `readNumber()` |
| `LiteralString` | `"hello"` | `readString()` |
| `LiteralChar` | `'A'`, `'\n'`, `'\x41'` | `readChar()` |
| `LiteralRegex` | `/[a-z]+/` | Inline in operator switch |

### Identifiers

Any sequence of alphanumeric characters and underscores starting with a letter or underscore that does not match a keyword entry. Identifiers are case-sensitive: `counter`, `Counter`, and `COUNTER` are three distinct identifiers.

### Operators and Punctuation

Multi-character operators are resolved by lookahead. The lexer checks for the longest matching sequence first:

| Tokens | Characters |
|---|---|
| Arithmetic | `+`, `-`, `*`, `/`, `%`, `**` |
| Comparison | `==`, `!=`, `<`, `>`, `<=`, `>=` |
| Assignment | `=`, `+=`, `-=`, `*=`, `/=`, `%=`, `&=`, `|=`, `^=`, `<<=`, `>>=` |
| Bitwise | `&`, `|`, `^`, `~`, `<<`, `>>` |
| Navigation | `.`, `..`, `...`, `->`, `=>`, `::` |
| Increment/Decrement | `++`, `--` |
| Delimiters | `(`, `)`, `[`, `]`, `{`, `}` |
| Separators | `,`, `:`, `;` |
| Other | `?`, `@`, `!` |

### Structural Tokens

| Token | Meaning |
|---|---|
| `Indent` | Indentation level increased. Emitted by `handleIndentation()`. |
| `Dedent` | Indentation level decreased. One `Dedent` per popped level. |
| `Newline` | Logical line boundary. Suppressed inside parenthesized contexts. |
| `Eof` | End of file. Always the last token in the stream. |

---

## Indentation as Tokens

The defining lexical characteristic of Uranite is that indentation produces explicit tokens. The parser never examines whitespace — it consumes `Indent` and `Dedent` tokens the same way a C parser consumes `{` and `}`.

The `handleIndentation()` algorithm:

1. At line start, count leading whitespace. Each space counts as 1. Each tab counts as 4.
2. Skip blank lines (lines that contain only whitespace) and comment-only lines (lines starting with `#` after whitespace) — neither produces `Indent` or `Dedent` tokens.
3. Compare the computed indentation level against the top of the indentation stack:
   - **Greater than stack top**: Push the new level onto the stack. Emit one `Indent` token.
   - **Equal to stack top**: No token emitted. The line continues at the same block level.
   - **Less than stack top**: Pop levels from the stack until the stack top equals the current level. Emit one `Dedent` token per popped level. If the current level does not match any entry in the stack, emit a diagnostic error: "inconsistent indentation — indentation does not match any outer level".

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

The indentation token stream for this fragment (showing only structural tokens):

```
Indent                    # 0 -> 4 (entering outer body)
Newline
Newline
Indent                    # 4 -> 8 (entering if body)
Newline
Newline
Indent                    # 8 -> 12 (entering nested if body)
Newline
Dedent                    # 12 -> 8 (exiting nested if)
Newline
Dedent                    # 8 -> 4 (exiting if)
Newline
Dedent                    # 4 -> 0 (exiting outer body)
```

Each `Indent` opens exactly one block. Each `Dedent` closes exactly one block. Multiple blocks can close on a single line — if the code jumps from indentation level 12 back to level 0, three `Dedent` tokens are emitted in sequence.

---

## Comments and Doccomments

Uranite has three comment forms:

**Line comments** start with `#` and extend to the end of the line. The lexer's `skipLineComment()` advances the position to the newline character, consuming all characters in between. No token is emitted.

```uranite
# This is a line comment
I64 value = 42
```

**Block comments** use `#{` to open and `}#` to close. They support nesting — the lexer tracks a `nestingDepth` counter, incrementing on `#{` and decrementing on `}#`. The block comment is closed when the nesting depth reaches zero. No token is emitted.

```uranite
#{
    This is a block comment.
    It can span multiple lines.
    #{ And it can be nested. }#
}#
```

**Triple-quoted strings** (`"""..."""` or `'''...'''`) at the top level are consumed as comments by the lexer via `skipTripleQuoteComment()`. When a triple-quoted string appears inside a function or class body — specifically, as the first statement after a declaration — it serves as a doccomment. The lexer still skips it during tokenization, but the formatter's `CommentExtractor` separately extracts these blocks and associates them with their parent declarations.

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

The `readIdentifierOrKeyword()` method accumulates characters matching `[a-zA-Z0-9_]` (starting with a letter or underscore), then checks the accumulated string against the keyword registry. If a match is found, the corresponding keyword token type is returned. Otherwise, a generic `Identifier` token is returned.

```uranite
I64 slotIndex = 0
```

In this line, the lexer produces:

| Position | Text | Token Type |
|---|---|---|
| 1 | `I64` | `Identifier` (not a keyword — type names are identifiers) |
| 2 | `slotIndex` | `Identifier` |
| 3 | `=` | `Assignment` |
| 4 | `0` | `LiteralInteger` |

Note that `I64` is an identifier, not a keyword. Type names like `I64`, `String`, `Boolean`, `ArrayList`, and `HashMap` are all identifiers resolved by the semantic analyzer, not reserved words. Only language constructs like `function`, `class`, `if`, `return`, `and`, `or`, `not`, `True`, `False`, `None`, and `self` are keywords.

---

## Literal Scanning

### Numbers

The `readNumber()` method handles four integer bases and floating-point numbers:

- **Decimal**: `42`, `1_000_000` (underscores as digit separators, silently stripped)
- **Binary**: `0b1010`, `0B1111_0000`
- **Octal**: `0o755`, `0O644`
- **Hexadecimal**: `0xFF`, `0x0000_FFFF`

Floating-point numbers are detected when a decimal point appears followed by a digit (`3.14`), or when an exponent suffix appears (`1e10`, `2.5E-3`). The exponent can include a sign (`+` or `-`).

Number literals can include a trailing alphabetic suffix (e.g., `100u64`), which is captured as part of the token value for downstream processing.

### Strings

The `readString()` method handles double-quoted strings with escape sequences:

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

The `readChar()` method handles single-quoted character literals with the same escape sequence set as strings. Character literals must contain exactly one character (or one escape sequence). An unclosed quote produces an "unterminated character literal" error.

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

This is a standard maximal-munch strategy. The `match()` method performs one-character lookahead and advances the position if the expected character is found.

---

## Parenthesis Depth and Implicit Line Joining

The lexer maintains a `parenDepth` counter that increments on `(`, `[`, and `{`, and decrements on `)`, `]`, and `}`. When `parenDepth` is greater than zero:

- **Newlines are suppressed.** No `Newline` token is emitted. This allows expressions to span multiple lines inside parenthesized contexts without backslash continuation.
- **Indentation is ignored.** The `handleIndentation()` call is skipped. Leading whitespace is consumed silently.

This means multi-line function calls, collection literals, and import blocks "just work":

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

1. **Pop remaining indentation levels.** While the indentation stack has more than one entry (the base level 0), the lexer pops a level and emits a `Dedent` token. This ensures every `Indent` token has a matching `Dedent`, even if the file ends mid-block.

2. **Append trailing newline.** If the last token in the stream is not already a `Newline`, a `Newline` token is appended. This normalizes the token stream so the parser can always expect a `Newline` before `Eof`.

3. **Append EOF.** An `Eof` token is always the final token in the stream.

---

## Subpage Index

Each lexical topic is covered in depth in its own document:

| Document | Description |
|---|---|
| [Source Files and Encoding](source-files-and-encoding.md) | File encoding requirements (UTF-8), the `.urn` extension, and source file structure rules. |
| [Comments and Doccomments](comments-and-doccomments.md) | Line comments (`#`), block comments (`#{...}#`), and triple-quoted doccomments with the "Parameters:", "Returns:", "Complexity:" format. |
| [Indentation and Blocks](indentation-and-blocks.md) | The `handleIndentation()` algorithm in detail: the indentation stack, `Indent`/`Dedent` emission, tab normalization (1 tab = 4 spaces), error handling for inconsistent indentation. |
| [Reserved Keywords](reserved-keywords.md) | Complete enumeration of all 74 reserved keywords organized by category with descriptions and usage context. |
| [Identifiers and Naming](identifiers-and-naming.md) | Identifier character rules (`[a-zA-Z_][a-zA-Z0-9_]*`), case sensitivity, naming conventions, and linter enforcement of descriptive names. |
| [Integer Literals](integer-literals.md) | Decimal, hexadecimal (`0x`), octal (`0o`), and binary (`0b`) formats. Underscore digit separators. Numeric suffixes. |
| [Float Literals](float-literals.md) | Decimal floating-point syntax, scientific notation (`1.5e10`), and IEEE 754 representation. |
| [String Literals](string-literals.md) | Double-quoted strings, escape sequences (`\n`, `\t`, `\xHH`, etc.), null-terminated UTF-8 representation, and unterminated string error handling. |
| [Char Literals](char-literals.md) | Single-quoted character literals, escape sequences, and the 32-bit `Char` type. |
| [Boolean and None Literals](boolean-and-none-literals.md) | `True`, `False`, and `None` as keyword tokens — case-sensitive, not identifiers. |
| [Regex Literals](regex-literals.md) | `/pattern/` syntax, context-sensitive disambiguation from division, escape handling, and character class support. |
