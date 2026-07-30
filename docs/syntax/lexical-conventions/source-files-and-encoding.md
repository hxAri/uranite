# Source Files and Encoding

This document specifies how the Uranite compiler loads, buffers, and interprets source files at the byte level — before any tokenization begins. It covers the `.urn` file extension, character encoding requirements, line ending normalization, file buffering strategy, and the end-of-file cleanup phase that ensures structural token integrity.

---

## Table of Contents

- [The .urn File Extension](#the-urn-file-extension)
- [Character Encoding](#character-encoding)
  - [UTF-8 Requirement](#utf-8-requirement)
  - [Byte Order Mark (BOM)](#byte-order-mark-bom)
  - [Multi-Byte Code Points](#multi-byte-code-points)
  - [Invalid Byte Sequences](#invalid-byte-sequences)
- [Line Endings](#line-endings)
  - [LF Normalization](#lf-normalization)
  - [Carriage Return Handling in the Lexer](#carriage-return-handling-in-the-lexer)
  - [Indentation Interaction](#indentation-interaction)
- [File Loading and Buffering](#file-loading-and-buffering)
  - [The readSource Pipeline](#the-readsource-pipeline)
  - [Module Loading Pipeline](#module-loading-pipeline)
  - [Memory Model](#memory-model)
  - [Module Caching](#module-caching)
- [Source File Structure](#source-file-structure)
  - [Required Elements](#required-elements)
  - [Ordering Constraints](#ordering-constraints)
  - [Minimal Valid Source File](#minimal-valid-source-file)
- [End-of-File Cleanup](#end-of-file-cleanup)
  - [Dedent Emission](#dedent-emission)
  - [Trailing Newline Normalization](#trailing-newline-normalization)
  - [EOF Token](#eof-token)
  - [Cleanup Example](#cleanup-example)
- [Edge Cases](#edge-cases)

---

## The .urn File Extension

All Uranite source files use the `.urn` extension. The compiler identifies source files by this extension when:

- The user passes a file path directly (`./build/uranite main.urn`)
- The module resolver searches for files during import resolution (trying `<path>.urn`, `<path>/<Name>.urn`, and `<path>/__mod__.urn`)
- The formatter and linter discover files recursively in a directory (`uranite-fmt --write src/` scans for all `*.urn` files)

The extension is a hard requirement for module resolution. A file named `utils.txt` or `helpers.py` will never be found by the import system, even if it contains valid Uranite source code. The compiler's entry point file (passed on the command line) is opened regardless of extension, but imported modules must use `.urn`.

---

## Character Encoding

### UTF-8 Requirement

Uranite source files must be encoded in UTF-8. The lexer operates on raw bytes within a `std::string`, using byte-position indexing to advance through the source. ASCII characters (bytes 0x00–0x7F) are the primary operating range for all syntactic constructs — keywords, operators, identifiers, whitespace, and delimiters are all within the ASCII subset.

UTF-8 multi-byte sequences appear in two contexts:

- **String literals** — Double-quoted strings can contain arbitrary UTF-8 text. The lexer copies bytes verbatim into the token value until it encounters the closing `"` delimiter. No UTF-8 decoding occurs during string scanning; the raw bytes are preserved in the token and passed through to LLVM IR as-is.
- **Comments** — Line comments (`#`), block comments (`#{...}#`), and doccomments (`"""..."""`) can contain arbitrary UTF-8 text. The lexer skips these bytes without interpretation.

Identifiers are restricted to ASCII alphanumeric characters and underscores (`[a-zA-Z_][a-zA-Z0-9_]*`). Unicode identifiers are not supported — a variable name like `café` or `数量` would produce an "unexpected character" diagnostic error when the lexer encounters the non-ASCII byte.

### Byte Order Mark (BOM)

The UTF-8 Byte Order Mark (bytes `0xEF 0xBB 0xBF`) is not expected or handled by the lexer. If a source file begins with a BOM, the lexer will encounter `0xEF` as the first byte. Since this byte is not an ASCII letter, digit, underscore, whitespace, or recognized operator character, it falls through to the default case in the operator switch and produces an "unexpected character" error.

To avoid this issue, ensure source files are saved as "UTF-8 without BOM". Most modern editors default to this setting. If your editor inserts a BOM, configure it to suppress it for `.urn` files or strip the BOM with a tool:

```bash
sed -i '1s/^\xEF\xBB\xBF//' source.urn
```

### Multi-Byte Code Points

The lexer's character-at-a-time scanning uses `char` values (single bytes). For ASCII-range processing (keywords, operators, whitespace, delimiters), this is correct — every syntactic token is composed entirely of single-byte characters. Multi-byte UTF-8 sequences only appear inside string literals and comments, where the lexer copies bytes without interpreting their Unicode meaning:

- In `readString()`, the loop `stringValue += currentChar` appends each raw byte. A string literal containing `"こんにちは"` stores the 15-byte UTF-8 encoding of those 5 characters as-is in the token value.
- In `skipLineComment()`, the loop advances past all bytes until `\n`. A comment containing `# 注释：变量初始化` skips all bytes regardless of their UTF-8 grouping.
- In `readChar()`, a character literal containing a multi-byte code point stores the raw bytes. The semantic analyzer and codegen handle the 32-bit `Char` type mapping.

No explicit UTF-8 validation pass exists in the lexer. Invalid UTF-8 byte sequences inside string literals and comments are passed through without error. Invalid bytes outside these contexts (in positions where identifiers or operators are expected) produce "unexpected character" errors because they do not match any recognized lexer dispatch.

### Invalid Byte Sequences

If a source file contains a byte that is not valid ASCII and appears outside of a string literal, character literal, or comment, the lexer reaches the `default` case in its main switch statement and emits:

```
error: unexpected character "�"
```

The diagnostic includes the file path, line number, and column number of the offending byte. The lexer produces an `Error` token and continues scanning, allowing the compiler to report multiple issues in a single pass rather than aborting on the first invalid byte.

Common causes of unexpected characters:

| Cause | Symptom | Fix |
|---|---|---|
| File saved in Latin-1 or Windows-1252 | Accented characters in identifiers produce byte values 0x80–0xFF | Re-save as UTF-8 |
| UTF-8 BOM at file start | Three "unexpected character" errors at line 1, columns 1–3 | Save as "UTF-8 without BOM" |
| Copy-pasted "smart quotes" | `"text"` (U+201C/U+201D) instead of `"text"` (U+0022) | Replace with ASCII double quotes |
| Non-breaking space (U+00A0) | Invisible character in indentation | Replace with regular ASCII spaces |

The non-breaking space case is particularly insidious for an indentation-sensitive language. A non-breaking space (0xC2 0xA0 in UTF-8) is visually indistinguishable from a regular space but is not recognized by the lexer's whitespace handling (`this->current() == ' '` checks for ASCII 0x20 only). This produces an "unexpected character" error at what appears to be a normally-indented line.

---

## Line Endings

### LF Normalization

Uranite uses the Unix-style line feed character (`\n`, byte 0x0A) as the canonical line terminator. The lexer's `advance()` method tracks line and column numbers by detecting `\n`:

```cpp
char Lexer::advance() {
    char processedChar = this->source[this->position++];
    if( processedChar == '\n' ) {
        this->line++;
        this->column = 1;
    }
    else {
        this->column++;
    }
    return processedChar;
}
```

Only `\n` triggers a line increment. The carriage return character `\r` (byte 0x0D) does not affect line counting.

### Carriage Return Handling in the Lexer

Windows-style line endings (`\r\n`, bytes 0x0D 0x0A) are handled by the lexer's main tokenization loop. Carriage return characters are treated as whitespace and silently consumed:

```cpp
if( character == '\t' || character == '\r' || character == ' ' ) {
    this->advance();
    continue;
}
```

This means `\r` is skipped without producing any token or affecting indentation counting. When the subsequent `\n` is encountered, it triggers normal newline handling (emitting a `Newline` token and setting the `atLineStart` flag).

The result is transparent CRLF support: a file with Windows line endings produces the same token stream as the same file with Unix line endings. No explicit normalization pass is required.

### Indentation Interaction

Carriage returns in leading whitespace do not affect indentation calculation. The `handleIndentation()` method counts only spaces and tabs:

```cpp
while( this->isAtEnd() == false && ( this->current() == ' ' || this->current() == '\t' ) ) {
    if( this->current() == '\t' ) {
        spaceCount += 4;
    }
    else {
        spaceCount++;
    }
    this->advance();
}
```

A `\r` character in leading whitespace would cause `handleIndentation()` to stop counting (since `\r` is neither `' '` nor `'\t'`), exit the whitespace loop, and treat the `\r` as the first non-whitespace character of the line. The `\r` would then be consumed by the main loop's whitespace skip. In practice, CRLF line endings place `\r` at the end of the previous line (before `\n`), not at the start of the next line, so this edge case does not arise in well-formed files.

However, a file that uses bare `\r` as line endings (classic Mac OS style, pre-OS X) would not be handled correctly. The lexer would not increment the line counter, would not set `atLineStart`, and would treat the entire file as a single logical line. This is an unsupported line ending format.

---

## File Loading and Buffering

### The readSource Pipeline

The compiler's driver loads the main source file through `Driver::readSource()`:

1. Open the file with `std::ifstream`.
2. If the file cannot be opened, print "error: could not open file" to stderr and return error code 1.
3. Read the entire file contents into a `std::ostringstream` via `rdbuf()`.
4. Store the result as a `std::string` in `this->source`.

The complete source text is buffered in memory as a single contiguous string. There is no streaming or chunked reading — the entire file is loaded before any lexing begins.

### Module Loading Pipeline

Imported modules follow an identical loading pattern in `Driver::loadModule()`:

1. Resolve the import path to a filesystem path (see [Module Resolution](../../getting-started/project-structure.md#module-resolution-algorithm)).
2. Canonicalize the path with `std::filesystem::canonical()`.
3. Check the module cache (`this->modules`) for a previously loaded module with the same canonical path.
4. If not cached: open the file, read the entire contents via `rdbuf()` into a `std::string`, create a fresh `diagnostic::Engine`, construct a `Lexer` with the source string, tokenize, parse, and cache the resulting AST.

### Memory Model

The lexer receives the source as a `const std::string&` reference and stores a copy in its `source` member. It navigates the string by byte position (`this->position`), using `this->source[this->position]` for random access. The `isAtEnd()` check compares `this->position >= this->source.size()`.

This means:

- **The entire source file resides in memory** during lexing. For typical Uranite source files (hundreds to low thousands of lines), this is negligible.
- **No memory-mapped I/O** is used. The file is read through standard C++ iostream.
- **No explicit file size limit** is enforced by the compiler. The practical limit is available heap memory. A source file that exhausts memory during the `rdbuf()` read will produce a `std::bad_alloc` exception.
- **Position tracking is byte-based**, not character-based. Column numbers in diagnostics refer to byte offsets within the line, not Unicode code points. For ASCII-only source (which is the norm for syntactic constructs), byte position and character position are identical.

### Module Caching

The driver caches loaded modules by canonical path in `this->modules` (a `std::unordered_map<std::string, ModuleInfo>`). If the same module is imported by multiple files, the file is read and parsed only once. Subsequent imports of the same path return the cached AST directly. This prevents redundant I/O and parsing for commonly imported standard library modules.

---

## Source File Structure

### Required Elements

Every Uranite source file must contain:

1. **A package declaration** — The first non-comment, non-blank statement must be `package <dotted.name>`. This declares the module's identity in the package namespace.

```uranite
package myproject.core.engine
```

2. **At least one declaration or statement** — An empty file (containing only a package declaration and nothing else) is valid but produces no useful output.

For executable programs, exactly one source file in the compilation unit must contain:

3. **A public main function** — `public function main() -> I32:` with a body that returns an `I32` exit code.

### Ordering Constraints

Within a source file, declarations must appear in this order:

1. `package` declaration (exactly one, must be first)
2. `import` / `from ... import` declarations (zero or more)
3. Top-level declarations: functions, classes, interfaces, enums, structs, constants, type aliases (any order among themselves)

```uranite
package myproject.services.auth

from uranite.io.console import puts
from uranite.collection import HashMap

const I64 MAX_RETRIES = 3

public class AuthService:

    public function AuthService( self ) -> Void:
        pass

public function main() -> I32:
    AuthService service = new AuthService()
    puts( "Authentication service started" )
    return 0
```

The compiler does not enforce strict ordering between top-level declarations (a function can appear before or after a class). Forward references are resolved by the semantic analyzer's two-pass registration system — Pass 1 registers all declarations eagerly, so Pass 2 can reference any declaration regardless of source order.

### Minimal Valid Source File

The smallest valid Uranite source file that compiles and runs:

```uranite
package minimal

public function main() -> I32:
    return 0
```

This file contains 4 lines and produces a native executable that exits with code 0.

The smallest valid Uranite source file (not necessarily executable):

```uranite
package empty
```

This is a valid library module with no exports. It can be imported by other modules (though importing it provides nothing).

---

## End-of-File Cleanup

When the lexer's main scanning loop exhausts all characters in the source string (`this->isAtEnd()` returns true), three cleanup steps execute to ensure the token stream is well-formed.

### Dedent Emission

The lexer pops all remaining levels from the indentation stack until only the base level (0) remains. For each popped level, a `Dedent` token is emitted:

```cpp
while( this->indentStack.size() > 1 ) {
    this->indentStack.pop();
    this->tokens.push_back(
        token::Token( token::Type::Dedent, this->currentSource(), "DEDENT" )
    );
}
```

This guarantees that every `Indent` token in the stream has a corresponding `Dedent`. If a file ends at indentation level 12 (three nested blocks at 4 spaces each), three `Dedent` tokens are emitted during cleanup.

Without this cleanup, a source file that ends inside a nested block would produce an `Indent`/`Dedent` imbalance. The parser expects matched pairs, so the cleanup is essential for correct parsing.

### Trailing Newline Normalization

If the last token in the stream is not already a `Newline`, one is appended:

```cpp
if( this->tokens.empty() == false && this->tokens.back().type != token::Type::Newline ) {
    this->tokens.push_back(
        token::Token( token::Type::Newline, this->currentSource(), "\\n" )
    );
}
```

This normalizes the token stream so the parser can always expect a `Newline` before `Eof`. Files that end with a trailing newline character already have this token from normal newline processing. Files that end without a trailing newline (the last line has no `\n`) receive the synthetic `Newline` here.

### EOF Token

The final token in every token stream is `Eof`:

```cpp
this->tokens.push_back(
    token::Token( token::Type::Eof, this->currentSource(), "" )
);
```

The parser uses `Eof` as its termination signal. No valid program can produce tokens after `Eof`.

### Cleanup Example

Consider a file that ends mid-block without a trailing newline:

```uranite
package example

public function main() -> I32:
    if True:
        return 0
```

Assume the file contains no trailing newline after `return 0`. The token stream before cleanup ends with:

```
... LiteralInteger("0") Newline
```

The indentation stack at this point contains `[0, 4, 8]` (base, function body, if body). Cleanup proceeds:

1. Pop level 8, emit `Dedent` — stack becomes `[0, 4]`
2. Pop level 4, emit `Dedent` — stack becomes `[0]`
3. Last token is now `Dedent`, not `Newline`, so append `Newline`
4. Append `Eof`

Final token stream tail:

```
... LiteralInteger("0") Newline Dedent Dedent Newline Eof
```

The parser sees properly closed blocks followed by a clean termination sequence.

---

## Edge Cases

### Empty Files

A completely empty file (zero bytes) produces the following token stream:

```
Newline Eof
```

The main loop never executes. Cleanup appends a `Newline` (since the token list is empty at that point, but the condition checks `tokens.empty() == false`, so the synthetic `Newline` is only appended if there are existing tokens). The `Eof` token is always appended. The parser receives this and reports an error for the missing `package` declaration.

### Files with Only Whitespace

A file containing only spaces, tabs, and newlines produces `Newline` tokens for each newline encountered at parenthesis depth zero. No `Indent` or `Dedent` tokens are emitted because `handleIndentation()` returns early when it encounters a newline or end-of-file after counting whitespace. The parser reports an error for the missing `package` declaration.

### Files with Only Comments

A file containing only comment lines:

```uranite
# This file contains only comments
# No package declaration, no code
```

The lexer's `handleIndentation()` detects the `#` character after consuming leading whitespace and calls `skipLineComment()` instead of emitting any indentation tokens. The resulting token stream contains only `Newline` tokens and the `Eof` terminator. The parser reports an error for the missing `package` declaration.

### Deeply Nested Files That End Abruptly

A file that ends at deep nesting without unwinding:

```uranite
package deep

public function main() -> I32:
    if True:
        if True:
            if True:
                return 0
```

If this file ends without a trailing newline, the indentation stack contains `[0, 4, 8, 12, 16]`. Cleanup pops four levels, emitting four `Dedent` tokens, followed by a `Newline` and `Eof`. The parser receives a perfectly balanced token stream despite the abrupt file termination.

### Mixed Tabs and Spaces

A file that uses a tab followed by spaces for indentation:

```
package mixed\n
\n
public function test() -> Void:\n
\t  puts( "mixed" )\n
```

The tab counts as 4, the two spaces count as 2, producing an indentation level of 6. This is pushed onto the stack. When the next line returns to level 0, the level 6 is popped and a `Dedent` is emitted. The code compiles, but the visual alignment in most editors (which display tabs at varying widths) will disagree with the lexer's interpretation. This is why spaces-only indentation with a width of 4 is the mandatory convention.
