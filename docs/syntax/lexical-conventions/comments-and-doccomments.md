# Comments and Doccomments

Uranite supports three comment forms: single-line comments (`#`), nestable block comments (`#{...}#`), and triple-quoted doccomments (`"""..."""`). Each form interacts differently with the lexer, the formatter, the linter, and the documentation generator. This document specifies how each form is parsed, where it is preserved or discarded, and the structured format required for public API documentation.

---

## Table of Contents

- [Comments and Doccomments](#comments-and-doccomments)
  - [Table of Contents](#table-of-contents)
  - [Single-Line Comments](#single-line-comments)
    - [Syntax](#syntax)
    - [Lexer Behavior](#lexer-behavior)
    - [Interaction with Indentation](#interaction-with-indentation)
    - [Trailing Comments](#trailing-comments)
  - [Block Comments](#block-comments)
    - [Block Comment Syntax](#block-comment-syntax)
    - [Nesting](#nesting)
    - [Lexer Behavior for Block Comments](#lexer-behavior-for-block-comments)
  - [Triple-Quoted Doccomments](#triple-quoted-doccomments)
    - [Doccomment Syntax](#doccomment-syntax)
    - [Placement Rules](#placement-rules)
    - [Lexer Behavior for Doccomments](#lexer-behavior-for-doccomments)
    - [Doccomment Structural Format](#doccomment-structural-format)
    - [The Summary Line](#the-summary-line)
    - [The Parameters Section](#the-parameters-section)
    - [The Returns Section](#the-returns-section)
    - [The Raises Section](#the-raises-section)
    - [The Complexity Section](#the-complexity-section)
    - [Extended Description](#extended-description)
    - [Complete Doccomment Example](#complete-doccomment-example)
  - [The Comment Processing Pipeline](#the-comment-processing-pipeline)
    - [Stage 1: Lexer Discards](#stage-1-lexer-discards)
    - [Stage 2: CommentExtractor Harvests](#stage-2-commentextractor-harvests)
    - [Stage 3: CommentReattacher Restores](#stage-3-commentreattacher-restores)
    - [Stage 4: DoccommentParser Structures](#stage-4-doccommentparser-structures)
    - [Stage 5: DoccommentValidator Verifies](#stage-5-doccommentvalidator-verifies)
  - [Indentation Stripping in Doccomments](#indentation-stripping-in-doccomments)
  - [Linter Enforcement Rules](#linter-enforcement-rules)
  - [What Not to Do](#what-not-to-do)

---

## Single-Line Comments

### Syntax

A single-line comment begins with `#` and extends to the end of the line. Everything after the `#` character on that line is comment text.

```uranite
# This is a standalone comment on its own line

I64 threshold = 100  # This is a trailing comment after code
```

There is no multi-character variant like `//` or `--`. The `#` character is the sole single-line comment delimiter.

### Lexer Behavior

When the lexer encounters `#` (and the next character is not `{`, which would start a block comment), it calls `skipLineComment()`. This method advances the cursor position to the next newline character, consuming all bytes in between. No token is emitted. The comment text is completely discarded from the token stream — the parser never sees it.

The newline character following the comment is not consumed by `skipLineComment()`. It remains for the main tokenization loop to process, which emits a `Newline` token and sets the `atLineStart` flag. This ensures that a comment at the end of a line does not suppress the line boundary.

### Interaction with Indentation

Comment-only lines do not produce `Indent` or `Dedent` tokens. The `handleIndentation()` method detects this case explicitly:

1. At line start, `handleIndentation()` counts leading whitespace.
2. After counting, it checks if the first non-whitespace character is `#`.
3. If so, it calls `skipLineComment()` and returns immediately — no indentation comparison occurs.

This means comments can appear at any indentation level without disturbing the block structure:

```uranite
public function process( I64 value ) -> I64:
    # This comment is at indentation level 4
    if value > 0:
        # This comment is at indentation level 8
        return value * 2
# This comment is at indentation level 0
    return 0
```

In this example, the comment at level 0 between the `if` body and the `return 0` does not cause a `Dedent` from level 8 to level 0 followed by an `Indent` back to level 4. The lexer skips the comment line entirely and processes the next non-comment line normally. The `return 0` at indentation level 4 produces a `Dedent` from level 8 to level 4 as expected.

### Trailing Comments

A comment that follows code on the same line is also consumed by the lexer. The code tokens are emitted normally, then the `#` triggers `skipLineComment()`, which discards the rest of the line:

```uranite
I64 bufferSize = 4096  # Maximum buffer allocation
```

The token stream for this line contains `Identifier("I64")`, `Identifier("bufferSize")`, `Assignment("=")`, `LiteralInteger("4096")`, `Newline`. The comment text "Maximum buffer allocation" is not present.

---

## Block Comments

### Block Comment Syntax

Block comments open with `#{` and close with `}#`. They can span multiple lines:

```uranite
#{
    This is a block comment.
    It spans multiple lines.
    No special prefix is needed on each line.
}#
```

Block comments can also appear inline within a single line, though this is uncommon:

```uranite
I64 value = #{ temporary override }# 42
```

### Nesting

Block comments support nesting. The lexer tracks a `nestingDepth` counter that increments on each `#{` and decrements on each `}#`. The block comment is closed when the depth returns to zero:

```uranite
#{
    Outer block comment.
    #{
        Inner nested block comment.
        This does not close the outer comment.
    }#
    Still inside the outer comment.
}#
```

The nesting support means that commenting out a section of code that already contains block comments works correctly — the inner `}#` delimiters do not prematurely close the outer comment.

### Lexer Behavior for Block Comments

When the lexer encounters `#` followed by `{`, it calls `skipBlockComment()`. This method:

1. Advances past the `{` character.
2. Sets `nestingDepth` to 1.
3. Loops through characters:
   - On `#{`: increments `nestingDepth`, advances past both characters.
   - On `}#`: decrements `nestingDepth`, advances past both characters. If depth reaches 0, returns.
   - On any other character: advances one position.
4. If the file ends before the block comment closes (`nestingDepth` never reaches 0), the comment runs to the end of the file. No explicit error is emitted for an unterminated block comment.

No token is emitted. The entire block comment content is discarded from the token stream.

---

## Triple-Quoted Doccomments

### Doccomment Syntax

Documentation comments use triple double-quotes (`"""..."""`) or triple single-quotes (`'''...'''`). They are multi-line string delimiters repurposed for documentation:

```uranite
public function fibonacci( I64 position ) -> I64:

    """
    Compute the Fibonacci number at the given position.

    Parameters:
        position (I64):
            The zero-indexed position in the Fibonacci sequence.
            Must be non-negative.

    Returns:
        I64:
            The Fibonacci number at the specified position.

    Complexity:
        Time: O(2^n)
        Space: O(n)
    """

    if position <= 1:
        return position
    return fibonacci( position - 1 ) + fibonacci( position - 2 )
```

Triple double-quotes (`"""`) are the canonical form. Triple single-quotes (`'''`) are accepted as an equivalent alternative.

### Placement Rules

Doccomments are placed **after** the declaration they document — inside the function or class body, before the implementation code. This is the opposite of many languages where documentation precedes the declaration:

```uranite
public class EventDispatcher:

    """
    Manages event registration and dispatch for application-level signals.

    Parameters:
        (none)

    Complexity:
        Time: O(1) for dispatch
        Space: O(n) where n is the number of registered handlers
    """

    private HashMap<String, ArrayList<Callable<Void, <String>>>> handlers

    public function EventDispatcher( self ) -> Void:

        """
        Construct a new EventDispatcher with an empty handler registry.

        Complexity:
            Time: O(1)
            Space: O(1)
        """

        self.handlers = new HashMap<>()

    public function register( self, String eventName, Callable<Void, <String>> handler ) -> Void:

        """
        Register a callback handler for a named event.

        Parameters:
            eventName (String):
                The unique identifier for the event to listen on.

            handler (Callable<Void, <String>>):
                The callback function invoked when the event fires.

        Returns:
            Void:
                No return value.

        Complexity:
            Time: O(1) amortized
            Space: O(1)
        """

        if self.handlers.containsKey( eventName ) == False:
            self.handlers.put( eventName, new ArrayList<>() )
        self.handlers.get( eventName ).add( handler )
```

### Lexer Behavior for Doccomments

The main compiler lexer treats triple-quoted strings as comments. When it encounters `"""` (or `'''`), it calls `skipTripleQuoteComment()`, which advances past all characters until the matching closing `"""` (or `'''`). No token is emitted.

This means doccomments are invisible to the parser and semantic analyzer. They have zero impact on compilation — they do not appear in the AST, are not type-checked, and produce no code.

The doccomment content is recovered separately by the formatter's `CommentExtractor` and the documentation generator's `DoccommentParser`, both of which re-scan the raw source text independently of the main lexer.

### Doccomment Structural Format

The `DoccommentParser` in the documentation generator (`src/uranite-doc/extractor/doccomment-parser.cpp`) parses doccomments into a structured `ParsedDoccomment` object. The parser recognizes six sections, identified by section headers ending with a colon:

| Section | Header | Required For | Description |
|---|---|---|---|
| Summary | (first non-blank line) | All public entities | One-line description of the entity's purpose. |
| Extended Description | (lines after summary, before first section) | Optional | Multi-line detailed explanation, usage notes. |
| Parameters | `Parameters:` | Functions with parameters | Typed parameter documentation. |
| Returns | `Returns:` | Functions with return values | Return type and description. |
| Raises | `Raises:` | Functions that throw | Exception types and conditions. |
| Complexity | `Complexity:` | All public entities (linter-enforced) | Big-O time and space bounds. |

The parser processes the doccomment line by line, using section headers as state transitions. Each section header must appear on its own line, left-aligned relative to the doccomment body, followed by a colon.

### The Summary Line

The first non-blank line after the opening `"""` is the summary. It should be a single sentence describing the entity's purpose:

```uranite
public function hash( I64 value ) -> I64:

    """
    Compute a multiplicative hash of an integer value.
    """

    return value * 2654435761
```

If the summary spans multiple lines before a blank line separator, the lines are joined with spaces. The summary ends at the first blank line, which transitions the parser to the Extended Description section.

### The Parameters Section

The "Parameters:" section documents each function parameter. Each parameter entry has two components:

1. **Parameter header** — The parameter name, optionally followed by its type in parentheses, followed by a colon.
2. **Parameter description** — Indented text below the header describing the parameter's purpose, constraints, and valid values.

```uranite
public function clamp( I64 value, I64 minimum, I64 maximum ) -> I64:

    """
    Restrict a value to a specified range.

    Parameters:
        value (I64):
            The input value to clamp.

        minimum (I64):
            The lower bound of the range. Must be less than or equal
            to maximum.

        maximum (I64):
            The upper bound of the range.

    Returns:
        I64:
            The clamped value, guaranteed to satisfy
            minimum <= result <= maximum.

    Complexity:
        Time: O(1)
        Space: O(1)
    """

    if value < minimum:
        return minimum
    if value > maximum:
        return maximum
    return value
```

The parser detects parameter entries by indentation level. Lines at the section's base indentation plus up to 8 spaces are treated as new parameter headers. Lines indented further are continuation text for the current parameter's description.

The `(Type)` annotation in the parameter header is optional but recommended. The parser extracts it by finding the `(` and `)` delimiters:

- `value (I64):` — name is "value", type is "I64"
- `value:` — name is "value", type is unspecified
- `value` — name is "value", type and colon both absent

### The Returns Section

The "Returns:" section documents the function's return value. It follows the same indented structure:

```
Returns:
    I64:
        The computed result.
```

The first line after "Returns:" is parsed as the return type (if it ends with `:`). Subsequent indented lines form the description. If the first line does not end with a colon, the entire text is treated as description only.

### The Raises Section

The "Raises:" section documents exceptions that the function may throw. Each entry names an exception type and the condition under which it is thrown:

```
Raises:
    ZeroDivisionError:
        When the divisor argument is zero.

    OverflowError:
        When the result exceeds the maximum value of I64.
```

The parser identifies new exception entries by the same indentation-based heuristic as parameter entries. The exception type name is extracted from the text before the colon.

The `DoccommentValidator` cross-references exception types against a registry of known error types (`registerKnownErrorType()`), warning when a documented exception type does not match any recognized type in the codebase.

### The Complexity Section

The "Complexity:" section documents algorithmic performance using Big-O notation. It supports two sub-fields:

```
Complexity:
    Time: O(n log n)
    Space: O(n)
```

The parser recognizes lines starting with "Time:" and "Space:" within this section. Each is extracted into the `ComplexityDocumentation` struct's `timeComplexity` and `spaceComplexity` fields.

The linter (`uranite-fmt --lint`) enforces the presence of a "Complexity:" section on all public entities via the `missing-complexity` rule. It also validates that the complexity values match valid Big-O notation patterns via the `invalid-complexity-format` rule.

### Extended Description

Text between the summary and the first section header is treated as the extended description. It provides detailed context, usage notes, invariants, or examples:

```uranite
public function binarySearch( ArrayList<I64> sorted, I64 target ) -> I64:

    """
    Perform binary search on a sorted list.

    The input list must be sorted in ascending order. If the list
    contains duplicate values equal to the target, any matching index
    may be returned. Returns -1 if the target is not found.

    Parameters:
        sorted (ArrayList<I64>):
            The sorted list to search. Must be in ascending order.

        target (I64):
            The value to find.

    Returns:
        I64:
            The index of the target in the list, or -1 if not found.

    Complexity:
        Time: O(log n)
        Space: O(1)
    """

    I64 low = 0
    I64 high = sorted.size() - 1
    while low <= high:
        I64 mid = low + ( high - low ) / 2
        I64 midValue = sorted.get( mid )
        if midValue == target:
            return mid
        elif midValue < target:
            low = mid + 1
        else:
            high = mid - 1
    return -1
```

### Complete Doccomment Example

A comprehensive doccomment using all sections:

```uranite
public function mergeIntervals(
    ArrayList<Pair<I64, I64>> intervals
) -> ArrayList<Pair<I64, I64>>:

    """
    Merge overlapping intervals into a minimal non-overlapping set.

    Given a list of closed intervals [start, end], produces a new list
    where all overlapping or adjacent intervals have been combined into
    single intervals. The output is sorted by start value.

    Parameters:
        intervals (ArrayList<Pair<I64, I64>>):
            A list of interval pairs where each pair represents
            a closed range [start, end]. Start must be less than
            or equal to end for each pair.

    Returns:
        ArrayList<Pair<I64, I64>>:
            A new list of merged intervals sorted by start value.
            The returned list is independent of the input; the
            original list is not modified.

    Raises:
        ArithmeticError:
            When an interval has start greater than end.

    Complexity:
        Time: O(n log n)
        Space: O(n)
    """

    if intervals.size() <= 1:
        return intervals
    ArrayList<Pair<I64, I64>> result = new ArrayList<>()
    return result
```

---

## The Comment Processing Pipeline

Comments flow through up to five processing stages depending on the tool being used. Understanding this pipeline explains why the lexer discards comments but the formatter preserves them.

### Stage 1: Lexer Discards

The main compiler lexer (`src/uranite/lexer/lexer.cpp`) discards all three comment forms. `skipLineComment()` handles `#` comments, `skipBlockComment()` handles `#{...}#` comments, and `skipTripleQuoteComment()` handles `"""..."""` doccomments. No tokens are emitted. The parser and semantic analyzer never see comment text.

This is intentional — comments have no semantic meaning and should not affect compilation.

### Stage 2: CommentExtractor Harvests

The `CommentExtractor` class (`src/uranite-fmt/comments/comment-extractor.cpp`) runs a separate micro-lexer pass over the raw source text. This pass is independent of the main lexer and operates before the formatter or linter runs.

The extractor identifies `#` comments and `"""..."""` doccomments, recording each as a `CommentEntry` with:

- **`commentKind`** — `SingleLine` or `DocComment`
- **`attachmentStrategy`** — How the comment relates to surrounding code:
  - `LeadingDeclaration` — Comment precedes a declaration (class, function, etc.)
  - `TrailingStatement` — Comment follows code on the same line
  - `FloatingBlock` — Standalone comment not attached to any specific entity
- **`commentContent`** — The raw text of the comment
- **`startLine`, `startColumn`, `endLine`, `endColumn`** — Source coordinates

The attachment strategy is determined heuristically. For doccomments, the strategy is always `LeadingDeclaration`. For single-line comments, the extractor checks whether the next non-blank line starts with a declaration keyword (`public`, `function`, `class`, `interface`, etc.) and assigns `LeadingDeclaration` if so, or checks whether code precedes the `#` on the same line for `TrailingStatement`.

### Stage 3: CommentReattacher Restores

When `uranite-fmt` formats a file, it:

1. Runs `CommentExtractor` to harvest all comments from the original source.
2. Runs the main lexer and parser to produce an AST.
3. Runs the `Formatter` visitor to pretty-print the AST into a new source string (which contains no comments, since the parser never saw them).
4. Runs `CommentReattacher` to re-inject the extracted comments into the formatted output at their correct logical positions.

The `CommentReattacher` compares the original source layout with the formatted output to calculate where each comment should be placed. This is how `uranite-fmt` preserves comments across formatting — they are extracted before formatting, then reattached after.

### Stage 4: DoccommentParser Structures

The documentation generator (`uranite-doc`) uses `DoccommentParser::parse()` to convert raw doccomment text into a `ParsedDoccomment` struct. The parser:

1. Strips the `"""` / `'''` delimiters from the raw text.
2. Splits the remaining text into lines.
3. Processes lines sequentially, using section headers ("Parameters:", "Returns:", "Raises:", "Complexity:") as state transitions.
4. Extracts parameter names and types from the `name (Type):` format.
5. Extracts return types from the `Type:` format in the Returns section.
6. Extracts time and space complexity from the "Time:" and "Space:" sub-fields.
7. Accumulates multi-line descriptions for each parameter, return value, and exception.

The result is a structured object that the documentation renderer uses to generate HTML or Markdown documentation pages.

### Stage 5: DoccommentValidator Verifies

The `DoccommentValidator` cross-references parsed doccomments against the AST to detect:

- **Missing doccomments** — Public entities without any doccomment.
- **Missing parameters** — Parameters present in the function signature but not documented.
- **Unknown exception types** — Exception types in the "Raises:" section that do not match any registered error type.
- **Structural syntax errors** — Malformed section headers, unclosed tags, or invalid notation.

Validation diagnostics are emitted with severity levels (Warning or Error) and include the source file path and line number.

---

## Indentation Stripping in Doccomments

Doccomments are typically indented to align with the surrounding code. The `DoccommentParser` handles this through its `trimLeadingWhitespace()` and `measureIndentation()` methods:

1. Each line of the doccomment is processed individually.
2. `trimLeadingWhitespace()` removes all leading spaces and tabs from each line before evaluating its content against section headers and description text.
3. `measureIndentation()` counts leading whitespace (spaces count as 1, tabs count as 4) to determine the nesting level of parameter descriptions and other sub-sections relative to their section header.

This means the following two doccomments produce identical parsed results:

```uranite
public function example() -> Void:

    """
    Summary line.

    Parameters:
        value (I64):
            Description text.

    Complexity:
        Time: O(1)
        Space: O(1)
    """

    pass
```

```uranite
public function example() -> Void:

            """
            Summary line.

            Parameters:
                value (I64):
                    Description text.

            Complexity:
                Time: O(1)
                Space: O(1)
            """

            pass
```

Both produce a `ParsedDoccomment` with summary "Summary line.", one parameter "value" of type "I64" with description "Description text.", and complexity "O(1)" for both time and space. The absolute indentation is irrelevant — only the relative indentation between section headers and their content matters.

---

## Linter Enforcement Rules

The linter (`uranite-fmt --lint`) enforces documentation standards through these rules:

| Rule ID | Severity | Description |
|---|---|---|
| `missing-doccomment` | Warning | Public classes, interfaces, enums, structs, and functions that lack a `"""..."""` doccomment. |
| `missing-complexity` | Warning | Doccomments that lack a "Complexity:" section. |
| `invalid-complexity-format` | Warning | Complexity values that do not match valid Big-O notation (e.g., "O(n)", "O(1)", "O(n log n)"). |
| `at-param-style` | Warning | Doccomments that use `@param` tags instead of the "Parameters:" section format. |

Uranite deliberately uses a section-based doccomment format ("Parameters:", "Returns:", "Complexity:") rather than inline tags ("@param", "@return", "@complexity"). The `at-param-style` rule flags the tag-based format as a style violation.

Correct format:

```uranite
public function add( I64 first, I64 second ) -> I64:

    """
    Add two integers.

    Parameters:
        first (I64):
            The first operand.

        second (I64):
            The second operand.

    Returns:
        I64:
            The sum.

    Complexity:
        Time: O(1)
        Space: O(1)
    """

    return first + second
```

Incorrect format (triggers `at-param-style` warning):

```uranite
public function add( I64 first, I64 second ) -> I64:

    """
    Add two integers.

    @param first The first operand.
    @param second The second operand.
    @return The sum.
    """

    return first + second
```

---

## What Not to Do

**Do not place doccomments before declarations.** Uranite doccomments go inside the body, not above the signature. Placing a doccomment above a declaration makes it a floating comment unattached to the entity:

```uranite
"""
This doccomment is not associated with the function below.
The lexer skips it as a top-level triple-quoted string.
"""
public function misplaced() -> Void:
    pass
```

The correct placement:

```uranite
public function correct() -> Void:

    """
    This doccomment is correctly placed inside the function body.

    Complexity:
        Time: O(1)
        Space: O(1)
    """

    pass
```

**Do not use `#` comments as doccomments.** Single-line comments are for implementation notes. They are not extracted by the documentation generator:

```uranite
# This is NOT a doccomment — uranite-doc will not extract it
public function invisible() -> Void:
    pass
```

**Do not nest triple-quoted strings.** Triple-quoted doccomments cannot be nested. A `"""` inside a doccomment closes it:

```uranite
public function broken() -> Void:

    """
    This doccomment contains a """nested""" triple-quote.
    The parser sees the second """ as the closing delimiter.
    """

    pass
```

To include literal triple-quotes in documentation text, there is no escape mechanism. Use a different quoting convention in prose (e.g., describe the syntax rather than embedding it).
