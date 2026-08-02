# Comments and Doccomments

Uranite supports three comment forms: single-line comments (`#`), nestable block comments (`#{...}#`), and triple-quoted doccomments (`"""..."""`). Each form serves a different purpose — single-line comments annotate implementation details, block comments disable sections of code, and doccomments provide structured documentation extracted by the documentation generator. This document specifies the syntax, placement rules, and structural format for each form.

---

## Table of Contents

- [Comments and Doccomments](#comments-and-doccomments)
  - [Table of Contents](#table-of-contents)
  - [Single-Line Comments](#single-line-comments)
    - [Syntax](#syntax)
    - [Interaction with Indentation](#interaction-with-indentation)
    - [Trailing Comments](#trailing-comments)
  - [Block Comments](#block-comments)
    - [Block Comment Syntax](#block-comment-syntax)
    - [Nesting](#nesting)
    - [Inline Block Comments](#inline-block-comments)
    - [Unterminated Block Comments](#unterminated-block-comments)
  - [Triple-Quoted Doccomments](#triple-quoted-doccomments)
    - [Doccomment Syntax](#doccomment-syntax)
    - [Placement Rules](#placement-rules)
    - [Doccomment Structural Format](#doccomment-structural-format)
    - [The Summary Line](#the-summary-line)
    - [The Parameters Section](#the-parameters-section)
    - [The Returns Section](#the-returns-section)
    - [The Raises Section](#the-raises-section)
    - [The Complexity Section](#the-complexity-section)
    - [Extended Description](#extended-description)
    - [Complete Doccomment Example](#complete-doccomment-example)
  - [Indentation in Doccomments](#indentation-in-doccomments)
  - [Comment Preservation During Formatting](#comment-preservation-during-formatting)
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

There is no multi-character variant like `//` or `--`. The `#` character is the sole single-line comment delimiter. If `#` is immediately followed by `{`, it begins a block comment instead (see [Block Comments](#block-comments)).

### Interaction with Indentation

Comment-only lines do not affect block structure. A line that contains only a `#` comment (with any amount of leading whitespace) is skipped entirely during indentation processing. This means comments can appear at any indentation level without opening or closing blocks:

```uranite
public function process( I64 value ) -> I64:
    # This comment is at indentation level 4
    if value > 0:
        # This comment is at indentation level 8
        return value * 2
# This comment is at indentation level 0
    return 0
```

In this example, the comment at level 0 between the `if` body and `return 0` does not close any blocks or open new ones. The compiler skips the comment line entirely and processes `return 0` at indentation level 4 normally — closing the `if` body (from level 8 to level 4) as expected.

This behavior is important when commenting out code during debugging. You can comment out lines without worrying about disrupting the indentation structure of surrounding code.

### Trailing Comments

A comment that follows code on the same line is called a trailing comment. The code tokens are processed normally, then the `#` causes the rest of the line to be discarded:

```uranite
I64 bufferSize = 4096  # Maximum buffer allocation
Boolean isReady = False   # Will be set to True after initialization
```

The comment text is invisible to the compiler. It exists only in the source file for human readers.

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

Block comments produce no tokens. The entire content between `#{` and `}#` is discarded. Block comments are particularly useful for temporarily disabling sections of code during development:

```uranite
#{
public function oldImplementation( I64 value ) -> I64:
    return value * 2
}#

public function newImplementation( I64 value ) -> I64:
    return value * 3
```

### Nesting

Block comments support nesting. Each `#{` increases a nesting depth, and each `}#` decreases it. The block comment is closed when the depth returns to zero:

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

Nesting support means that commenting out a section of code that already contains block comments works correctly — the inner `}#` delimiters do not prematurely close the outer comment. This is a common need when iteratively disabling larger sections of code:

```uranite
#{
    # This entire block is commented out, including its own block comment

    public function helper() -> Void:
        #{
            This inner block comment was already here.
            It does not interfere with the outer one.
        }#
        pass
}#
```

### Inline Block Comments

Block comments can appear inline within a single line, though this is uncommon:

```uranite
I64 value = #{ temporary override }# 42
```

The compiler sees only `I64 value = 42`. The block comment between `#{` and `}#` is discarded entirely.

### Unterminated Block Comments

If the file ends before a block comment is closed (the nesting depth never reaches zero), the comment runs to the end of the file. Everything from the opening `#{` to the last byte of the file is treated as comment content. No explicit error is emitted for an unterminated block comment — the code that was intended to follow the comment simply does not exist in the token stream.

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
            Exponential due to redundant recursive calls without memoization.
        Space: O(n)
            Call stack depth is proportional to the position value.
    """

    if position <= 1:
        return position
    return fibonacci( position - 1 ) + fibonacci( position - 2 )
```

Triple double-quotes (`"""`) are the canonical form. Triple single-quotes (`'''`) are accepted as an equivalent alternative.

Doccomments have zero impact on compilation — they do not affect type checking, code generation, or program behavior. They exist solely for documentation purposes and are extracted by the documentation generator (`uranite-doc`) and validated by the linter (`uranite-fmt --lint`).

### Placement Rules

Doccomments are placed **after** the declaration they document — inside the function or class body, as the first statement before the implementation code. This is the opposite of many languages where documentation precedes the declaration:

```uranite
public class EventDispatcher:

    """
    Manages event registration and dispatch for application-level signals.

    Complexity:
        Time: O(1) for dispatch
            Single hash lookup to find handler list, then sequential invocation.
        Space: O(n) where n is the number of registered handlers
            Each registered handler reference stored in per-event lists.
    """

    private HashMap<String, ArrayList<Callable<Void, <String>>>> handlers

    public function EventDispatcher( self ) -> Void:

        """
        Construct a new EventDispatcher with an empty handler registry.

        Complexity:
            Time: O(1)
                Single allocation of an empty hash map.
            Space: O(1)
                Only the handler map reference is stored.
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
                Hash map insertion is constant time on average, with rare rehashing.
            Space: O(1)
                Stores one additional handler reference in the existing list.
        """

        if self.handlers.containsKey( eventName ) == False:
            self.handlers.put( eventName, new ArrayList<>() )
        self.handlers.get( eventName ).add( handler )
```

This "after" placement convention applies to all documentable entities: classes, interfaces, enums, structs, functions, methods, and constructors.

### Doccomment Structural Format

Doccomments follow a structured format with recognized sections. Each section is identified by a header ending with a colon, appearing on its own line:

| Section | Header | Required For | Description |
|---|---|---|---|
| Summary | (first non-blank line) | All public entities | One-line description of the entity's purpose. |
| Extended Description | (lines after summary, before first section) | Optional | Multi-line detailed explanation, usage notes. |
| Parameters | `Parameters:` | Functions with parameters | Typed parameter documentation. |
| Returns | `Returns:` | Functions with return values | Return type and description. |
| Raises | `Raises:` | Functions that raise exceptions | Exception types and conditions. |
| Complexity | `Complexity:` | All public entities (linter-enforced) | Big-O time and space bounds. |

Sections must appear in this order when present. Omitting an optional section is fine; reordering sections produces linter warnings.

### The Summary Line

The first non-blank line after the opening `"""` is the summary. It should be a single sentence describing the entity's purpose:

```uranite
public function hash( I64 value ) -> I64:

    """
    Compute a multiplicative hash of an integer value.
    """

    return value * 2654435761
```

If the summary spans multiple lines before a blank line separator, the lines are joined with spaces. The summary ends at the first blank line, which transitions to the Extended Description or the first section header.

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
            At most two comparisons regardless of input.
        Space: O(1)
            No additional memory allocated.
    """

    if value < minimum:
        return minimum
    if value > maximum:
        return maximum
    return value
```

The `(Type)` annotation in the parameter header is optional but recommended:

- `value (I64):` — name is "value", type is "I64"
- `value:` — name is "value", type is unspecified
- `value` — name is "value", type and colon both absent

When the type annotation is present, the documentation generator can cross-reference it against the actual function signature and warn on mismatches.

### The Returns Section

The "Returns:" section documents the function's return value. It follows the same indented structure as parameters:

```uranite
public function absolute( I64 value ) -> I64:

    """
    Compute the absolute value of an integer.

    Parameters:
        value (I64):
            The input integer.

    Returns:
        I64:
            The non-negative absolute value. If value is I64 minimum
            (-9223372036854775808), the result overflows.

    Complexity:
        Time: O(1)
            Single comparison and conditional negation.
        Space: O(1)
            No additional memory allocated.
    """

    if value < 0:
        return -value
    return value
```

The first line after "Returns:" is the return type (if it ends with `:`). Subsequent indented lines form the description. For `Void` functions, the "Returns:" section is typically omitted.

### The Raises Section

The "Raises:" section documents exceptions that the function may raise. Each entry names an exception type and the condition under which it is raised:

```uranite
public function divide( I64 dividend, I64 divisor ) -> I64:

    """
    Perform integer division.

    Parameters:
        dividend (I64):
            The number to divide.

        divisor (I64):
            The number to divide by.

    Returns:
        I64:
            The integer quotient, truncated toward zero.

    Raises:
        ZeroDivisionError:
            When the divisor is zero.

    Complexity:
        Time: O(1)
            Single hardware division instruction.
        Space: O(1)
            No additional memory allocated.
    """

    return dividend / divisor
```

Multiple exception types can be documented:

```
Raises:
    ZeroDivisionError:
        When the divisor argument is zero.

    OverflowError:
        When the result exceeds the maximum value of I64.
```

### The Complexity Section

The "Complexity:" section documents algorithmic performance using Big-O notation. It supports two sub-fields:

```
Complexity:
    Time: O(n log n)
        Dominated by the comparison-based sorting step.
    Space: O(n)
        Auxiliary storage proportional to input size.
```

Both "Time:" and "Space:" sub-fields should be present. Valid Big-O formats include `O(1)`, `O(n)`, `O(n^2)`, `O(log n)`, `O(n log n)`, `O(2^n)`, and `O(n!)`. The linter validates that complexity values match recognized Big-O patterns.

The linter enforces the presence of a "Complexity:" section on all public entities via the `missing-complexity` rule. This is an unusual requirement compared to most languages, but it reflects Uranite's emphasis on performance-aware coding. Every public function, class, and interface should document its computational characteristics.

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
            Search range halves with each iteration.
        Space: O(1)
            Only three index variables regardless of list size.
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
            Dominated by sorting intervals by start value before merging.
        Space: O(n)
            Output list may contain up to n intervals if none overlap.
    """

    if intervals.size() <= 1:
        return intervals
    ArrayList<Pair<I64, I64>> result = new ArrayList<>()
    return result
```

---

## Indentation in Doccomments

Doccomments are typically indented to align with the surrounding code. The documentation generator strips leading whitespace from each line before processing, so the absolute indentation level of the doccomment does not matter. Only the relative indentation between section headers and their content matters.

This means the following two doccomments produce identical documentation output:

```uranite
public function example() -> Void:

    """
    Summary line.

    Parameters:
        value (I64):
            Description text.

    Complexity:
        Time: O(1)
            Constant time regardless of input.
        Space: O(1)
            No additional memory allocated.
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
                    Constant time regardless of input.
                Space: O(1)
                    No additional memory allocated.
            """

            pass
```

Both produce documentation with summary "Summary line.", one parameter "value" of type "I64" with description "Description text.", and complexity "O(1)" for both time and space. The standard convention is to indent the doccomment body at the same level as the function body (typically 4 spaces from the function signature), which the formatter enforces automatically.

---

## Comment Preservation During Formatting

When `uranite-fmt` reformats a source file, all comments are preserved. The formatter extracts comments from the original source before formatting, reformats the code, and then reattaches comments at their correct logical positions in the reformatted output.

This means you can run `uranite-fmt --write` on any file without losing comments. Trailing comments remain on the same logical line as the code they annotate. Standalone comment lines maintain their position relative to the surrounding declarations. Doccomments stay inside their parent declaration body.

The formatter may adjust comment indentation to match the reformatted code's indentation level, but the comment content is never modified.

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
            Single addition operation.
        Space: O(1)
            No additional memory allocated.
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
It is treated as a top-level triple-quoted string and discarded.
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
            Constant time regardless of input.
        Space: O(1)
            No additional memory allocated.
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
    The second """ is treated as the closing delimiter.
    Everything after it is interpreted as code.
    """

    pass
```

To include literal triple-quotes in documentation text, there is no escape mechanism. Describe the syntax in prose rather than embedding it directly.

**Do not omit the Complexity section on public entities.** The linter flags public functions, classes, and interfaces without a "Complexity:" section. Even for simple constant-time operations, explicitly document `O(1)` — it confirms the author considered the algorithmic characteristics rather than forgetting about them.
