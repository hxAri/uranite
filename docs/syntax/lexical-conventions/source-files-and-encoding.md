# Source Files and Encoding

This document specifies how Uranite source files are interpreted at the byte level — before any tokenization begins. It covers the `.urn` file extension, character encoding requirements, line ending normalization, and the structural requirements every source file must satisfy.

---

## Table of Contents

- [Source Files and Encoding](#source-files-and-encoding)
  - [Table of Contents](#table-of-contents)
  - [The .urn File Extension](#the-urn-file-extension)
  - [Character Encoding](#character-encoding)
    - [UTF-8 Requirement](#utf-8-requirement)
    - [Byte Order Mark (BOM)](#byte-order-mark-bom)
    - [Multi-Byte Code Points](#multi-byte-code-points)
    - [Invalid Byte Sequences](#invalid-byte-sequences)
  - [Line Endings](#line-endings)
    - [LF as the Canonical Line Terminator](#lf-as-the-canonical-line-terminator)
    - [Windows Line Endings (CRLF)](#windows-line-endings-crlf)
    - [Classic Mac Line Endings (Bare CR)](#classic-mac-line-endings-bare-cr)
  - [Source File Structure](#source-file-structure)
    - [Required Elements](#required-elements)
    - [Ordering Constraints](#ordering-constraints)
    - [Minimal Valid Source File](#minimal-valid-source-file)
  - [End-of-File Behavior](#end-of-file-behavior)
  - [Module Caching](#module-caching)
  - [Edge Cases](#edge-cases)
    - [Empty Files](#empty-files)
    - [Files with Only Whitespace](#files-with-only-whitespace)
    - [Files with Only Comments](#files-with-only-comments)
    - [Deeply Nested Files That End Abruptly](#deeply-nested-files-that-end-abruptly)
    - [Mixed Tabs and Spaces](#mixed-tabs-and-spaces)

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

Uranite source files must be encoded in UTF-8. All syntactic constructs — keywords, operators, identifiers, whitespace, and delimiters — use only ASCII characters (bytes 0x00–0x7F).

UTF-8 multi-byte sequences appear in two contexts:

- **String literals** — Double-quoted strings can contain arbitrary UTF-8 text. The bytes are preserved verbatim in the compiled output. No UTF-8 decoding occurs during scanning; a string literal containing `"こんにちは"` stores the 15-byte UTF-8 encoding of those 5 characters as-is.
- **Comments** — Line comments (`#`), block comments (`#{...}#`), and doccomments (`"""..."""`) can contain arbitrary UTF-8 text. These bytes are skipped without interpretation.

Identifiers are restricted to ASCII alphanumeric characters and underscores (`[a-zA-Z_][a-zA-Z0-9_]*`). Unicode identifiers are not supported — a variable name like `café` or `数量` would produce an "unexpected character" error when the non-ASCII byte is encountered.

### Byte Order Mark (BOM)

The UTF-8 Byte Order Mark (bytes `0xEF 0xBB 0xBF`) is not recognized. If a source file begins with a BOM, the non-ASCII bytes produce "unexpected character" errors at line 1, columns 1–3.

To avoid this issue, ensure source files are saved as "UTF-8 without BOM". Most modern editors default to this setting. If your editor inserts a BOM, configure it to suppress it for `.urn` files or strip the BOM with a tool:

```bash
sed -i '1s/^\xEF\xBB\xBF//' source.urn
```

### Multi-Byte Code Points

All syntactic processing operates on single bytes. Since every syntactic token is composed entirely of ASCII characters, this works correctly. Multi-byte UTF-8 sequences only appear inside string literals and comments, where bytes are copied or skipped without interpreting their Unicode meaning:

- A string literal containing `"こんにちは"` stores all 15 raw bytes.
- A comment containing `# 注释：变量初始化` skips all bytes regardless of their UTF-8 grouping.
- A character literal containing a multi-byte code point stores the raw bytes. The 32-bit `Char` type handles the Unicode representation.

No explicit UTF-8 validation pass exists. Invalid UTF-8 byte sequences inside string literals and comments are passed through without error. Invalid bytes outside these contexts (in positions where identifiers or operators are expected) produce "unexpected character" errors because they do not match any recognized token pattern.

### Invalid Byte Sequences

If a source file contains a byte that is not valid ASCII and appears outside of a string literal, character literal, or comment, the compiler emits:

```
error: unexpected character "�"
```

The diagnostic includes the file path, line number, and column number of the offending byte. The compiler continues scanning after the error, allowing multiple issues to be reported in a single pass rather than aborting on the first invalid byte.

Common causes of unexpected characters:

| Cause | Symptom | Fix |
|---|---|---|
| File saved in Latin-1 or Windows-1252 | Accented characters in identifiers produce byte values 0x80–0xFF | Re-save as UTF-8 |
| UTF-8 BOM at file start | Three "unexpected character" errors at line 1, columns 1–3 | Save as "UTF-8 without BOM" |
| Copy-pasted "smart quotes" | `“text”` (U+201C/U+201D) instead of `"text"` (U+0022) | Replace with ASCII double quotes |
| Non-breaking space (U+00A0) | Invisible character in indentation | Replace with regular ASCII spaces |

The non-breaking space case is particularly insidious for an indentation-sensitive language. A non-breaking space (0xC2 0xA0 in UTF-8) is visually indistinguishable from a regular space but is not recognized as whitespace for indentation counting (only ASCII space 0x20 and tab 0x09 count). This produces an "unexpected character" error at what appears to be a normally-indented line.

---

## Line Endings

### LF as the Canonical Line Terminator

Uranite uses the Unix-style line feed character (`\n`, byte 0x0A) as the canonical line terminator. Only `\n` triggers a new logical line. The carriage return character `\r` (byte 0x0D) does not affect line counting.

### Windows Line Endings (CRLF)

Windows-style line endings (`\r\n`, bytes 0x0D 0x0A) are handled transparently. The carriage return character is treated as whitespace and silently consumed. When the subsequent `\n` is encountered, it triggers normal newline handling.

The result is transparent CRLF support: a file with Windows line endings produces the same token stream as the same file with Unix line endings. No explicit normalization pass is required.

Carriage returns in leading whitespace do not affect indentation calculation. Indentation counting recognizes only spaces and tabs:

- Each space counts as 1 unit
- Each tab counts as 4 units
- Carriage returns and all other characters end the whitespace count

In practice, CRLF line endings place `\r` at the end of the previous line (before `\n`), not at the start of the next line, so carriage returns in leading whitespace do not arise in well-formed files.

### Classic Mac Line Endings (Bare CR)

Files that use bare `\r` as line endings (classic Mac OS style, pre-OS X) are not supported. The compiler would not recognize line boundaries, would not process indentation, and would treat the entire file as a single logical line. Convert to LF or CRLF before compiling:

```bash
tr '\r' '\n' < source.urn > source_fixed.urn
```

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

The compiler does not enforce strict ordering between top-level declarations (a function can appear before or after a class). Forward references are supported — a class can reference another class declared later in the same file.

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

## End-of-File Behavior

When the compiler reaches the end of a source file, it performs cleanup to ensure the token stream is well-formed:

1. **Close remaining blocks.** If the file ends inside nested blocks (e.g., at indentation level 12 with three open blocks), all open blocks are closed automatically. Each open block produces a corresponding block-close boundary. This guarantees that every block-open has a matching block-close, even if the file ends mid-block.

2. **Append trailing newline.** If the file does not end with a newline character, a synthetic line boundary is appended. This normalizes the token stream so a line boundary always precedes the end-of-file marker.

3. **Append end-of-file marker.** An end-of-file marker is always the final token in the stream.

Without this cleanup, a source file ending inside a nested block would produce mismatched block boundaries, causing parsing errors.

**Example:** A file ending at four levels of nesting without a trailing newline:

```uranite
package deep

public function main() -> I32:
    if True:
        if True:
            if True:
                return 0
```

End-of-file cleanup closes all four open blocks (indentation levels 16, 12, 8, and 4), appends a line boundary, and appends the end-of-file marker. The result is a perfectly balanced token stream despite the abrupt file termination.

---

## Module Caching

When the same module is imported by multiple files in a compilation, the file is read and processed only once. Subsequent imports of the same module return the cached result directly. This prevents redundant file I/O and processing for commonly imported standard library modules like `uranite.io.console` or `uranite.collection`.

Module identity is determined by the canonical filesystem path. Two import paths that resolve to the same physical file (e.g., through symlinks or different relative paths) share a single cached entry.

---

## Edge Cases

### Empty Files

A completely empty file (zero bytes) produces only an end-of-file marker. The compiler reports an error for the missing `package` declaration.

### Files with Only Whitespace

A file containing only spaces, tabs, and newlines produces line boundary tokens for each newline but no block boundaries or content tokens. The compiler reports an error for the missing `package` declaration.

### Files with Only Comments

A file containing only comment lines:

```uranite
# This file contains only comments
# No package declaration, no code
```

Comment lines produce no content tokens. The compiler reports an error for the missing `package` declaration.

### Deeply Nested Files That End Abruptly

Covered in [End-of-File Behavior](#end-of-file-behavior). The compiler's cleanup phase guarantees balanced block boundaries regardless of how deep the nesting is when the file ends.

### Mixed Tabs and Spaces

A line indented with one tab followed by two spaces produces an indentation level of 6 (tab = 4 + 2 spaces = 6). This compiles, but the visual alignment in most editors (which display tabs at varying widths) will disagree with the compiler's interpretation. This is why spaces-only indentation with a width of 4 is the mandatory convention.

The formatter (`uranite-fmt --write`) normalizes all indentation to spaces, converting tabs and resolving mixed indentation into a consistent 4-space layout. Running the formatter on a file with mixed indentation fixes the issue permanently.
