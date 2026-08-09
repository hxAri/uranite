# Indentation and Blocks

Uranite uses indentation to define block structure. There are no braces (`{}`), `begin`/`end` keywords, or explicit block delimiters in the syntax. Instead, a colon at the end of a statement opens a block, and the first line indented deeper than the current level begins that block. When subsequent lines return to a shallower level, the block closes automatically. This document specifies indentation rules, block formation, line joining behavior, and every edge case that affects how the compiler interprets whitespace.

---

## Table of Contents

- [Indentation and Blocks](#indentation-and-blocks)
  - [Table of Contents](#table-of-contents)
  - [Design Principle](#design-principle)
  - [The Standard Convention](#the-standard-convention)
  - [How Blocks Open and Close](#how-blocks-open-and-close)
    - [Opening a Block](#opening-a-block)
    - [Closing a Block](#closing-a-block)
    - [Closing Multiple Blocks at Once](#closing-multiple-blocks-at-once)
  - [Indentation Width Calculation](#indentation-width-calculation)
    - [Spaces](#spaces)
    - [Tabs](#tabs)
    - [Mixed Tabs and Spaces](#mixed-tabs-and-spaces)
  - [Lines That Do Not Affect Block Structure](#lines-that-do-not-affect-block-structure)
    - [Blank Lines](#blank-lines)
    - [Whitespace-Only Lines](#whitespace-only-lines)
    - [Comment-Only Lines](#comment-only-lines)
  - [Implicit Line Joining](#implicit-line-joining)
    - [Parentheses](#parentheses)
    - [Brackets](#brackets)
    - [Braces](#braces)
    - [Nested Delimiters](#nested-delimiters)
    - [Indentation Inside Delimiters](#indentation-inside-delimiters)
  - [Explicit Line Continuation](#explicit-line-continuation)
  - [End-of-File Block Closure](#end-of-file-block-closure)
  - [The Indentation Mismatch Error](#the-indentation-mismatch-error)
  - [Worked Examples](#worked-examples)
    - [Simple Function](#simple-function)
    - [Nested Conditionals](#nested-conditionals)
    - [Class with Methods](#class-with-methods)
    - [Multi-Level Exit](#multi-level-exit)
  - [Common Pitfalls](#common-pitfalls)
    - [Inconsistent Dedent Level](#inconsistent-dedent-level)
    - [Missing Block Body](#missing-block-body)
    - [Phantom Indentation from Tabs](#phantom-indentation-from-tabs)
    - [Trailing Whitespace After Backslash](#trailing-whitespace-after-backslash)
    - [Invisible Non-Breaking Spaces](#invisible-non-breaking-spaces)
    - [Forgetting the Colon](#forgetting-the-colon)

---

## Design Principle

Indentation in Uranite is not cosmetic. It is structural. The compiler converts whitespace patterns into block boundaries that carry the same syntactic weight as `{` and `}` in C-family languages. This design guarantees that code appearance and code structure are always identical. There is no way for indentation to misrepresent nesting depth.

Every construct that introduces a block — functions, classes, conditionals, loops, switch/case arms, try/except — follows the same pattern: the introducing line ends with a colon, and the block body is indented one level deeper. The block ends when indentation returns to the enclosing level.

---

## The Standard Convention

Uranite uses **4 spaces per indentation level**. This is the mandatory convention enforced by the formatter (`uranite-fmt`). While the compiler accepts tabs and other space counts, the formatter normalizes all indentation to 4-space increments.

Configure your editor to:

- Insert spaces when pressing Tab (not tab characters)
- Set tab width to 4 spaces
- Set indentation width to 4 spaces
- Enable "trim trailing whitespace on save"

Running the formatter on any source file normalizes indentation automatically:

```bash
uranite-fmt --write source.urn
```

---

## How Blocks Open and Close

### Opening a Block

A block opens when a line ends with a colon (`:`) and the next meaningful line is indented deeper than the current level. Exactly one block opens per indentation increase, regardless of how large the jump is:

```uranite
public function greet( String name ) -> Void:
    puts( name )
```

The function signature ends with `:` at indentation level 0. The next line is at level 4 (one level deeper). This opens one block — the function body. All subsequent lines at level 4 or deeper belong to this block until indentation returns to level 0.

Blocks can nest to arbitrary depth:

```uranite
public function process( I64 value ) -> Void:
    if value > 0:
        if value > 100:
            if value > 1000:
                puts( "very large" )
```

Each `if` statement opens its own block. The nesting levels are 4, 8, 12, and 16 — four levels deep. The compiler tracks each level independently and requires that every block close cleanly.

### Closing a Block

A block closes when the next meaningful line has a shallower indentation than the current block:

```uranite
public function example() -> Void:
    I64 count = 0
    if count == 0:
        puts( "zero" )
    puts( "done" )
```

The line `puts( "done" )` is at level 4, which is shallower than the `if` body at level 8. This closes the `if` block. The line remains inside the function body because level 4 matches the function block.

### Closing Multiple Blocks at Once

When indentation drops by more than one level, multiple blocks close simultaneously. One block closes for each level exited:

```uranite
public function deep() -> Void:
    if True:
        while True:
            if True:
                puts( "innermost" )
                break
    puts( "back at function level" )
```

The line `puts( "back at function level" )` is at level 4. The previous line was at level 16 (inside three nested blocks within the function body). The compiler closes all three inner blocks — the inner `if`, the `while`, and the outer `if` — in a single step. The line then continues inside the function body at level 4.

The compiler does not care about the magnitude of the drop. It pops nesting levels one by one until the current level matches the new indentation. If the new indentation does not match any previously established level, an error is reported (see [The Indentation Mismatch Error](#the-indentation-mismatch-error)).

---

## Indentation Width Calculation

The compiler counts leading whitespace on each line and converts it to an integer width using these rules:

### Spaces

Each space character counts as **1 unit** of indentation width:

```
"    return value"        →  width 4
"        return value"    →  width 8
"            return value" → width 12
```

### Tabs

Each tab character counts as **4 units** of indentation width:

```
"→return value"           →  width 4   (1 tab)
"→→return value"          →  width 8   (2 tabs)
"→→→return value"         →  width 12  (3 tabs)
```

Where `→` represents a tab character.

### Mixed Tabs and Spaces

Tabs and spaces can be mixed in leading whitespace. The compiler applies the same conversion — each space adds 1, each tab adds 4 — and sums the total:

```
"→    return value"       →  width 8   (1 tab + 4 spaces)
"    →return value"       →  width 8   (4 spaces + 1 tab)
```

Both lines have width 8. The compiler does not distinguish how that width was reached.

While mixing tabs and spaces is technically valid, it causes visual confusion because editors display tabs at varying widths. A tab displayed as 2 columns in one editor appears as 8 columns in another, but the compiler always counts it as 4 regardless of display.

**Best practice:** Use spaces exclusively, 4 per level. The formatter enforces this:

```bash
uranite-fmt --write source.urn
```

This converts all tabs to spaces and normalizes indentation to consistent 4-space increments.

---

## Lines That Do Not Affect Block Structure

Three categories of lines are completely invisible to block structure. They do not open blocks, close blocks, or act as line boundaries. They can appear anywhere in a source file without disrupting the indentation of surrounding code.

### Blank Lines

Lines containing nothing (just a newline character) are ignored:

```uranite
public function spaced() -> Void:
    I64 first = 1

    I64 second = 2

    I64 third = 3
```

The blank lines between statements have no effect. The function body remains a single uninterrupted block at level 4. Blank lines are used freely for visual grouping within blocks.

### Whitespace-Only Lines

Lines containing only spaces and/or tabs (with no visible content) are treated identically to blank lines:

```uranite
public function example() -> Void:
    I64 value = 10
                
    I64 result = value * 2
```

The whitespace-only line (which might contain spaces invisible in the editor) does not affect block structure. It is consumed and discarded.

### Comment-Only Lines

Lines containing only a comment (with any amount of leading whitespace) are invisible to indentation:

```uranite
public function example() -> Void:
    I64 counter = 0
# This comment is at level 0 — does not close the function block
        # This comment is at level 8 — does not open a new block
    I64 result = counter + 1
```

Both comments are skipped entirely. The block structure remains unchanged — the function body stays open at level 4. The lines `I64 counter = 0` and `I64 result = counter + 1` are both at level 4, so no blocks open or close between them.

This behavior is important during debugging. You can comment out lines at any indentation level without disrupting the block structure of surrounding code:

```uranite
public function calculate( I64 base ) -> I64:
    I64 step = base * 2
    # I64 extra = step + 10
    # I64 adjusted = extra - 5
    return step
```

Commenting out the two intermediate lines does not affect the function block. The `return` statement at level 4 remains inside the function body.

---

## Implicit Line Joining

When an expression is enclosed in parentheses `()`, brackets `[]`, or braces `{}`, newlines and indentation inside those delimiters are ignored. This allows multi-line expressions without any special syntax.

The compiler tracks a nesting depth counter. Every opening delimiter (`(`, `[`, `{`) increments it, and every closing delimiter (`)`, `]`, `}`) decrements it. While the counter is above zero, newline and indentation processing is suspended entirely.

### Parentheses

Function calls, function signatures, and grouped expressions can span multiple lines freely:

```uranite
public function create(
    String name,
    I64 age,
    String email,
    Boolean isActive
) -> User:
    return new User( name, age, email, isActive )
```

The function parameters span five lines, but the compiler treats them as a single logical line. No block boundaries are produced inside the parentheses.

```uranite
I64 result = compute(
    firstArgument,
    secondArgument,
    thirdArgument
)
```

The function call argument list spans four lines. The closing `)` at any indentation level correctly terminates the group.

### Brackets

Array literals and subscript expressions also support implicit joining:

```uranite
Memory<I64> values = [
    100,
    200,
    300,
    400,
    500
]
```

All elements are at the same syntactic level regardless of indentation inside the brackets.

### Braces

Import blocks and collection literals enclosed in braces enjoy the same behavior:

```uranite
from uranite.collection import {
    ArrayList,
    HashMap,
    HashSet,
    Pair
}
```

The import list spans five lines inside the braces. The compiler sees a flat sequence of identifiers separated by commas.

### Nested Delimiters

Different delimiter types can nest freely. The compiler tracks total depth across all three delimiter types using a single counter:

```uranite
ArrayList<Pair<String, I64>> entries = new ArrayList<>(
    [
        new Pair<>( "alpha", 1 ),
        new Pair<>( "beta", 2 ),
        new Pair<>( "gamma", 3 )
    ]
)
```

The outer `(` increments the counter to 1. The `[` increments to 2. Each inner `(` increments further. All closing delimiters decrement the counter. Normal indentation processing resumes only when the counter returns to zero at the final `)`.

### Indentation Inside Delimiters

While inside any grouping delimiter, indentation is purely cosmetic. The compiler does not measure it, does not compare it against any level, and does not produce block boundaries from it:

```uranite
Memory<I64> data = [
    10,
        20,
  30,
            40
]
```

All four elements are syntactically equivalent despite wildly inconsistent indentation. The compiler ignores all whitespace structure inside the brackets.

While this is accepted, consistent indentation inside delimiters is strongly recommended for readability. The formatter normalizes indentation inside delimiters to align with the opening line:

```uranite
Memory<I64> data = [
    10,
    20,
    30,
    40
]
```

---

## Explicit Line Continuation

A backslash (`\`) immediately followed by a newline character acts as explicit line continuation. The compiler consumes both characters and continues processing the next line as part of the current logical line:

```uranite
I64 total = firstValue + secondValue \
    + thirdValue + fourthValue
```

The `\` and newline are consumed together. No line boundary is produced. The next line is not treated as a new line — indentation on the continuation line is consumed as ordinary whitespace between tokens.

Backslash continuation is primarily useful for long conditions and expressions outside of grouping delimiters:

```uranite
if longConditionAlpha and longConditionBeta \
    and longConditionGamma:
    doSomething()
```

Without the backslash, the newline after `longConditionBeta` would terminate the `if` statement, and `and longConditionGamma:` would be parsed as a separate (invalid) statement.

In most cases, implicit line joining via parentheses is cleaner than backslash continuation:

```uranite
if (longConditionAlpha and longConditionBeta
    and longConditionGamma):
    doSomething()
```

Wrapping the condition in parentheses achieves the same result without the backslash. The choice between the two is a matter of style — both compile identically.

**Important:** The backslash must be the very last character on the line. If spaces or tabs appear between the backslash and the newline, the continuation is not recognized. The backslash becomes an unexpected character, and the newline terminates the line normally.

---

## End-of-File Block Closure

When the compiler reaches the end of a source file, all remaining open blocks are closed automatically. For each open block, a corresponding block-close boundary is produced, ensuring that every block-open has a matching block-close.

This means source files do not need to "return to level 0" at the end. A file ending inside a deeply nested block compiles correctly:

```uranite
package deep

public function main() -> I32:
    if True:
        if True:
            if True:
                return 0
```

The file ends at indentation level 16 with four open blocks (function body, three nested `if` bodies). The compiler closes all four blocks automatically during end-of-file processing. The resulting structure is perfectly balanced.

A trailing newline at the end of the file is also handled automatically. If the file does not end with a newline character, the compiler appends one implicitly. This normalizes structure so the final statement always has a clean termination.

---

## The Indentation Mismatch Error

When a line's indentation does not match any previously established block level, the compiler reports an error:

```
error: inconsistent indentation
  --> source.urn:5:1
  | indentation does not match any outer level
```

This error occurs when code is indented to a width that was never used as a block boundary:

```uranite
public function broken() -> Void:
    if True:
        puts( "ok" )
      puts( "misaligned" )
```

Line 4 is at indentation width 6. The established levels are 0 (top-level), 4 (function body), and 8 (`if` body). When the compiler sees width 6, it closes the `if` body (exiting level 8) and arrives at level 4. But 6 does not equal 4, and no level 6 was ever established. The error fires because the indentation lands between two valid levels.

Valid indentation must always exactly match a level that was previously opened. The compiler tracks every indentation increase and requires that decreases return to one of those exact levels.

This error is not recoverable. Compilation fails and the remaining code in the file may produce cascading errors due to the malformed block structure.

**Fix:** Align the line to a valid level. In this case, either level 8 (inside the `if` body) or level 4 (after the `if` body):

```uranite
public function fixedInside() -> Void:
    if True:
        puts( "ok" )
        puts( "also inside if" )
```

```uranite
public function fixedOutside() -> Void:
    if True:
        puts( "ok" )
    puts( "after if" )
```

---

## Worked Examples

### Simple Function

```uranite
package example

public function square( I64 number ) -> I64:
    return number * number
```

The `package` declaration is at level 0. The function signature is at level 0. The colon opens a block, and `return number * number` at level 4 begins the function body. At end-of-file, the function block closes automatically.

Block structure:

```
Level 0: package declaration
Level 0: function signature (colon opens block)
  Level 4: function body (1 statement)
Level 0: end-of-file closes function block
```

### Nested Conditionals

```uranite
public function classify( I64 value ) -> String:
    if value > 0:
        if value > 100:
            return "large positive"
        return "small positive"
    elif value < 0:
        return "negative"
    else:
        return "zero"
```

Block structure:

```
Level 0: function signature
  Level 4: function body
    Level 8: if body (value > 0)
      Level 12: nested if body (value > 100)
        return "large positive"
      Level 12 closes (back to 8)
      return "small positive"
    Level 8 closes (back to 4)
    Level 8: elif body (value < 0)
      return "negative"
    Level 8 closes (back to 4)
    Level 8: else body
      return "zero"
    Level 8 closes
  Level 4 closes
Level 0: end-of-file
```

Each `if`, `elif`, and `else` arm opens its own block at level 8. The nested `if` inside the first arm opens a deeper block at level 12. All blocks close cleanly when indentation returns to enclosing levels.

### Class with Methods

```uranite
public class Counter:

    private I64 value

    public function Counter( self, I64 initial ) -> Void:
        self.value = initial

    public function increment( self ) -> Void:
        self.value = self.value + 1

    public function current( self ) -> I64:
        return self.value
```

Block structure:

```
Level 0: class declaration (colon opens class block)
  Level 4: class body
    Level 4: field declaration (same level, no block)
    Level 4: constructor signature (colon opens method block)
      Level 8: constructor body
    Level 8 closes (back to 4)
    Level 4: increment signature
      Level 8: method body
    Level 8 closes (back to 4)
    Level 4: current signature
      Level 8: method body
    Level 8 closes
  Level 4 closes
Level 0: end-of-file
```

The class body is at level 4. Each method body is at level 8. Field declarations (like `private I64 value`) are at level 4 within the class body — they do not open blocks because they do not end with a colon.

### Multi-Level Exit

```uranite
public function search( ArrayList<ArrayList<I64>> matrix, I64 target ) -> Boolean:
    for ArrayList<I64> row in matrix:
        for I64 element in row:
            if element == target:
                return True
    return False
```

Block structure:

```
Level 0: function signature
  Level 4: function body
    Level 8: outer for-loop body
      Level 12: inner for-loop body
        Level 16: if body
          return True (closes levels 16, 12, 8 on next meaningful line)
  Level 4: return False (all three inner blocks closed at once)
Level 0: end-of-file
```

The `return False` at level 4 causes three blocks to close simultaneously — the `if` body (16 to 12), the inner `for` body (12 to 8), and the outer `for` body (8 to 4). The compiler handles this in a single step, closing one block per level difference.

---

## Common Pitfalls

### Inconsistent Dedent Level

The most frequent indentation error. Code returns to a level that was never established:

```uranite
public function broken() -> Void:
    if True:
        I64 counter = 1
      I64 result = 2
```

Level 6 does not match any established level (0, 4, or 8). The compiler reports "inconsistent indentation".

**Fix:** Align to level 8 (inside `if`) or level 4 (after `if`):

```uranite
public function fixed() -> Void:
    if True:
        I64 counter = 1
    I64 result = 2
```

### Missing Block Body

A colon at the end of a line signals a block opening. If the next code line is not indented deeper, the compiler expects a block body and reports an error:

```uranite
public function empty() -> Void:
I64 value = 10
```

The function signature ends with `:` at level 0, but `I64 value = 10` is also at level 0. No block was opened because indentation did not increase. The compiler expects an indented block body.

**Fix:** Either indent the body or use `pass` for an intentionally empty block:

```uranite
public function withBody() -> Void:
    I64 value = 10
```

```uranite
public function intentionallyEmpty() -> Void:
    pass
```

### Phantom Indentation from Tabs

When an editor displays tabs at a width different from the compiler's count of 4 units per tab, visually aligned code can have mismatched indentation widths:

```
Editor display (tab=2):     Compiler measurement (tab=4):
→ firstLine                 width = 4
  secondLine                width = 2
```

Where `→` is a tab and spaces are used for `secondLine`. The two lines appear aligned in the editor (both at column 2), but the compiler measures them at different widths (4 versus 2). This triggers "inconsistent indentation" because the second line's width of 2 does not match the established level of 4.

**Fix:** Configure the editor to insert spaces instead of tabs:

```bash
uranite-fmt --write source.urn
```

The formatter converts all tabs to spaces and normalizes indentation, permanently eliminating tab-related alignment issues.

### Trailing Whitespace After Backslash

Backslash continuation requires `\` to be the very last character before the newline. Invisible trailing spaces break the continuation:

```uranite
I64 result = alpha + \   
    beta
```

The spaces after `\` prevent the compiler from recognizing the continuation. The backslash becomes an unexpected character, and the newline terminates the line. The second line (`beta`) becomes a separate statement, likely producing a parse error.

**Fix:** Remove all trailing whitespace after the backslash. Enable "trim trailing whitespace on save" in your editor to prevent this automatically.

### Invisible Non-Breaking Spaces

Non-breaking spaces (Unicode U+00A0, encoded as bytes 0xC2 0xA0 in UTF-8) look identical to regular spaces but are not recognized as whitespace for indentation. They produce "unexpected character" errors at positions that appear normally indented.

This commonly occurs when copy-pasting code from web pages, PDFs, or word processors that use non-breaking spaces for formatting.

**Fix:** Replace non-breaking spaces with regular ASCII spaces. Many editors have a "show invisible characters" mode that distinguishes between regular spaces and non-breaking spaces. The formatter also replaces non-breaking spaces during normalization:

```bash
uranite-fmt --write source.urn
```

### Forgetting the Colon

Every block-opening construct requires a trailing colon. Omitting the colon does not open a block, and the indented body becomes a separate (malformed) statement:

```uranite
if value > 0
    puts( "positive" )
```

Without the colon after `if value > 0`, the compiler does not expect an indented block. The `if` statement is incomplete, and the indented `puts` call is parsed as a separate top-level statement with an unexpected indentation increase.

**Fix:** Add the colon:

```uranite
if value > 0:
    puts( "positive" )
```

All block-opening constructs require the colon: `if`, `elif`, `else`, `for`, `while`, `switch`, `case`, `try`, `except`, `finally`, `function`, `class`, `interface`, `enum`, `struct`.
