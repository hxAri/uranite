# Indentation and Blocks

Uranite uses indentation to define block structure. There are no braces (`{}`), `begin`/`end` keywords, or explicit block delimiters. Instead, the lexer converts changes in leading whitespace into `Indent` and `Dedent` tokens that the parser consumes as block boundaries. This document specifies the exact algorithm, the data structures behind it, and every edge case the lexer handles.

---

## Table of Contents

- [Design Principle](#design-principle)
- [The Indentation Stack](#the-indentation-stack)
- [The handleIndentation Algorithm](#the-handleindentation-algorithm)
  - [Step 1: Count Leading Whitespace](#step-1-count-leading-whitespace)
  - [Step 2: Skip Non-Structural Lines](#step-2-skip-non-structural-lines)
  - [Step 3: Compare and Emit](#step-3-compare-and-emit)
  - [Step 4: Clear Line-Start Flag](#step-4-clear-line-start-flag)
- [Token Emission: Indent](#token-emission-indent)
- [Token Emission: Dedent](#token-emission-dedent)
- [The Indentation Mismatch Error](#the-indentation-mismatch-error)
- [Newline Token Emission](#newline-token-emission)
- [Ignored Lines](#ignored-lines)
  - [Blank Lines](#blank-lines)
  - [Whitespace-Only Lines](#whitespace-only-lines)
  - [Comment-Only Lines](#comment-only-lines)
- [Implicit Line Joining](#implicit-line-joining)
  - [The parenDepth Counter](#the-parendepth-counter)
  - [Delimiters That Increment Depth](#delimiters-that-increment-depth)
  - [Behavior Inside Grouped Expressions](#behavior-inside-grouped-expressions)
- [Explicit Line Continuation](#explicit-line-continuation)
- [EOF Cleanup Phase](#eof-cleanup-phase)
- [Worked Example: Full Token Trace](#worked-example-full-token-trace)
- [Tabs and Spaces](#tabs-and-spaces)
- [Common Pitfalls](#common-pitfalls)

---

## Design Principle

Indentation in Uranite is not cosmetic — it is structural. The lexer converts whitespace patterns into tokens that carry the same syntactic weight as `{` and `}` in C-family languages. A colon at the end of a statement opens a block; the first line indented deeper than the current level emits an `Indent` token; the first line that returns to a shallower level emits one or more `Dedent` tokens.

This design means that code appearance and code structure are always identical. There is no way for indentation to lie about nesting depth.

---

## The Indentation Stack

The lexer maintains an `indentStack` — a `std::stack<int>` that records every active indentation level. On construction, the stack is initialized with a single entry of `0`, representing the top-level (zero-indentation) scope:

```
indentStack: [0]       ← initial state at file start
```

Throughout tokenization, the stack grows when the lexer enters a deeper block and shrinks when it exits. The top of the stack always represents the current expected indentation width. At the end of the file, the stack is drained back to its initial `[0]` state, emitting a `Dedent` token for every popped level.

The stack is never empty during tokenization. The bottom entry of `0` is never popped — it serves as the sentinel for top-level scope.

---

## The handleIndentation Algorithm

The `handleIndentation()` method is called at the start of every new line (when `atLineStart` is `true`) and only when `parenDepth` is `0`. It performs four steps in sequence.

### Step 1: Count Leading Whitespace

The lexer scans forward through spaces and tabs, accumulating a `spaceCount` integer:

- Each **space** (`' '`) adds **1** to the count.
- Each **tab** (`'\t'`) adds **4** to the count.

The lexer advances past every whitespace character it counts. After this step, the cursor sits on the first non-whitespace character of the line (or at end-of-file).

```
Input line:  "        return value\n"
             ^^^^^^^^
             8 spaces → spaceCount = 8
```

```
Input line:  "\t\treturn value\n"
             ^^^^
             2 tabs → spaceCount = 8
```

```
Input line:  "    \treturn value\n"
             ^^^^^
             4 spaces + 1 tab → spaceCount = 8
```

All three examples produce `spaceCount = 8`. The lexer does not distinguish between tabs and spaces — it converts both into a unified integer width using the rule above.

### Step 2: Skip Non-Structural Lines

After counting whitespace, the lexer checks whether the line contains meaningful content:

1. **End of file** — If the cursor has reached the end of the source, `handleIndentation()` returns immediately. No tokens are emitted.

2. **Blank or whitespace-only line** — If the current character is `'\n'`, the line contained only whitespace. The method returns immediately. No `Indent`, `Dedent`, or `Newline` tokens are emitted for this line.

3. **Comment-only line** — If the current character is `'#'`, the line contains only a comment (possibly preceded by whitespace). The lexer calls `skipLineComment()` to advance past the comment text and returns immediately. No indentation tokens are emitted.

These three checks ensure that blank lines, whitespace-only lines, and comment-only lines are completely invisible to the indentation tracker.

### Step 3: Compare and Emit

The lexer compares `spaceCount` against `previousIndent` (the value on top of `indentStack`). Three outcomes are possible:

**Case A — Deeper indentation** (`spaceCount > previousIndent`):

The lexer pushes `spaceCount` onto the `indentStack` and emits one `Indent` token.

```
Before:  indentStack = [0, 4]     previousIndent = 4
Line:    "        x = 1"          spaceCount = 8
Action:  push(8), emit Indent
After:   indentStack = [0, 4, 8]
```

Only one `Indent` token is ever emitted per line, regardless of how large the jump is. Jumping from level 0 to level 8 emits a single `Indent` — the parser does not care about the magnitude, only the presence of the transition.

**Case B — Shallower indentation** (`spaceCount < previousIndent`):

The lexer pops levels from the stack and emits one `Dedent` token per pop, until the stack top matches `spaceCount`:

```
Before:  indentStack = [0, 4, 8]  top = 8
Line:    "    y = 2"              spaceCount = 4
Action:  pop(8) → emit Dedent    stack = [0, 4]  top = 4
         4 == spaceCount → stop
After:   indentStack = [0, 4]
```

Multiple `Dedent` tokens can be emitted for a single line when the code exits several nesting levels at once:

```
Before:  indentStack = [0, 4, 8, 12]  top = 12
Line:    "return 0"                    spaceCount = 0
Action:  pop(12) → emit Dedent        stack = [0, 4, 8]
         pop(8)  → emit Dedent        stack = [0, 4]
         pop(4)  → emit Dedent        stack = [0]
         0 == spaceCount → stop
After:   indentStack = [0]
Emitted: 3 Dedent tokens
```

**Case C — Same indentation** (`spaceCount == previousIndent`):

No tokens are emitted. The line continues at the same block level. The lexer proceeds to tokenize the line content.

### Step 4: Clear Line-Start Flag

After completing indentation processing, the lexer sets `atLineStart = false`. This prevents `handleIndentation()` from running again until the next `'\n'` character is consumed, which resets `atLineStart` back to `true`.

---

## Token Emission: Indent

An `Indent` token signals the parser that a new nested block has begun. It appears in the token stream immediately before the first token of the indented line:

```uranite
public function greet( String name ) -> Void:
    puts( name )
```

Token stream:

```
Identifier("public")  Identifier("function")  Identifier("greet")
LeftParenthesis  Identifier("String")  Identifier("name")  RightParenthesis
Arrow  Identifier("Void")  Colon  Newline
Indent
Identifier("puts")  LeftParenthesis  Identifier("name")  RightParenthesis
Newline
```

The `Indent` token carries no meaningful value — its token text is the string literal "INDENT". Its sole function is to mark a structural transition to a deeper nesting level.

---

## Token Emission: Dedent

A `Dedent` token signals the parser that the current block has ended. One `Dedent` is emitted per block level exited. The following example demonstrates two blocks closing on a single line:

```uranite
public function outer() -> Void:
    if True:
        puts( "deep" )
    puts( "shallow" )
```

Token stream at the transition from "deep" to "shallow":

```
...  Identifier("puts")  LeftParenthesis
LiteralString("deep")  RightParenthesis  Newline
Dedent
Identifier("puts")  LeftParenthesis
LiteralString("shallow")  RightParenthesis  Newline
```

One `Dedent` is emitted because the indentation dropped from level 8 (inside `if`) back to level 4 (inside `outer`). The function body is still open — `[0, 4]` remains on the stack.

When the file ends or the next top-level declaration appears, another `Dedent` closes the function body:

```
...  Newline
Dedent
```

---

## The Indentation Mismatch Error

After the `Dedent` emission loop in Step 3 (Case B), the lexer verifies that the final stack top equals `spaceCount`. If it does not, the indentation level does not match any enclosing block, and the lexer emits a diagnostic error:

```
error: inconsistent indentation
  --> source.urn:5:1
  | indentation does not match any outer level
```

This error occurs when code is indented to a level that was never established:

```uranite
public function broken() -> Void:
    if True:
        puts( "ok" )
      puts( "misaligned" )
```

Here, line 4 has `spaceCount = 6`. The stack after processing line 3 is `[0, 4, 8]`. The lexer pops `8` (emitting `Dedent`), and the new top is `4`. But `6 != 4`, so the error fires. There is no level `6` on the stack — the code is indented to a width that does not correspond to any outer block boundary.

This error is not recoverable. The lexer reports it through the diagnostic engine and continues tokenizing, but the resulting token stream may be malformed.

---

## Newline Token Emission

The `Newline` token marks the end of a logical line. It is emitted when the lexer encounters `'\n'` and the following conditions are all satisfied:

1. `parenDepth` is `0` — newlines inside grouped expressions are suppressed.
2. The token list is not empty.
3. The last token is not `Dedent`, `Indent`, or `Newline` — this prevents duplicate or redundant newlines.

```
Character:  '\n'
Conditions: parenDepth == 0, last token is Identifier
Action:     emit Newline, set atLineStart = true
```

The `Newline` token acts as a statement terminator. In the parser, it separates consecutive statements within a block. The deduplication rule (condition 3) ensures that the token stream never contains consecutive `Newline` tokens or a `Newline` immediately after a structural token like `Indent` or `Dedent`.

---

## Ignored Lines

Three categories of lines are invisible to the indentation tracker. They produce no `Indent`, `Dedent`, or `Newline` tokens and have zero effect on block structure.

### Blank Lines

A line containing nothing — just a `'\n'` character — is detected at the very top of the tokenization loop, before `handleIndentation()` runs:

```
if atLineStart and current() == '\n':
    advance()
    continue
```

The newline is consumed and the loop restarts. No tokens are emitted.

### Whitespace-Only Lines

A line containing only spaces and/or tabs is detected inside `handleIndentation()`. After counting whitespace, the cursor lands on `'\n'` (or end-of-file), and the method returns immediately:

```
Count whitespace → spaceCount = 12
current() == '\n' → return (no tokens)
```

### Comment-Only Lines

A line containing only whitespace followed by a `#` comment is detected inside `handleIndentation()`. After counting whitespace, the cursor lands on `'#'`, and the method calls `skipLineComment()` and returns:

```
Count whitespace → spaceCount = 4
current() == '#' → skipLineComment(), return (no tokens)
```

This allows comments to appear at any indentation level without disrupting blocks:

```uranite
public function example() -> Void:
    I64 x = 10
# This comment is at level 0 — does not close the function block
        # This comment is at level 8 — does not open a new block
    I64 y = 20
```

The indentation stack remains `[0, 4]` throughout. Both comments are skipped entirely.

---

## Implicit Line Joining

### The parenDepth Counter

The lexer maintains a `parenDepth` integer (initialized to `0`) that tracks the nesting depth of grouped expressions. When `parenDepth` is greater than zero, two behaviors change:

1. **Newline tokens are suppressed** — the `'\n'` character does not emit a `Newline` token.
2. **Indentation tracking is bypassed** — `handleIndentation()` is not called. Instead, leading whitespace on the new line is consumed silently, and `atLineStart` is cleared.

This allows expressions to span multiple lines freely when enclosed in grouping delimiters.

### Delimiters That Increment Depth

Three opening delimiters increment `parenDepth`, and their closing counterparts decrement it:

| Opening | Closing | Token Types |
|---|---|---|
| `(` | `)` | `LeftParenthesis` / `RightParenthesis` |
| `[` | `]` | `LeftBracket` / `RightBracket` |
| `{` | `}` | `LeftBrace` / `RightBrace` |

All three delimiter pairs share the same `parenDepth` counter. They nest correctly because the counter simply tracks total depth, and the parser separately validates matching:

```uranite
public function compute(
    I64 alpha,
    I64 beta,
    I64 gamma
) -> I64:
    return alpha + beta + gamma
```

On `(`: `parenDepth` becomes `1`. Lines 2-4 are inside the group — no `Newline`, `Indent`, or `Dedent` tokens are emitted for them. On `)`: `parenDepth` returns to `0`, restoring normal tokenization.

### Behavior Inside Grouped Expressions

When `parenDepth > 0` and `atLineStart` is `true`, the lexer enters a special whitespace-consumption path:

```
while not at end and (current is space or tab):
    advance()
atLineStart = false
```

This consumes all leading whitespace without measuring it. The indentation stack is not consulted. The line content is tokenized normally, but no structural tokens are generated from the whitespace.

This means indentation inside grouping delimiters is purely cosmetic:

```uranite
Memory<I64> values = [
    10,
        20,
  30,
            40
]
```

All four elements are at the same syntactic level despite wildly different indentation. The lexer does not care — `parenDepth` is `1` throughout, so indentation tracking is bypassed.

---

## Explicit Line Continuation

The backslash-newline sequence (`\` immediately followed by `\n`) provides explicit line continuation outside of grouping delimiters. When the lexer encounters this sequence, it consumes both characters and continues tokenizing the next line as part of the current logical line:

```uranite
I64 result = alpha + beta \
    + gamma + delta
```

The `\` and `\n` are consumed together. No `Newline` token is emitted. The next line is not treated as a new line start — `atLineStart` remains `false`, so `handleIndentation()` does not run. The leading whitespace on the continuation line is consumed as inter-token whitespace.

Backslash continuation is rarely needed in practice because implicit line joining via parentheses, brackets, and braces covers most multi-line expression scenarios. It exists for cases where no grouping delimiter is naturally present:

```uranite
if longConditionA and longConditionB \
    and longConditionC:
    doSomething()
```

Without the backslash, the `'\n'` after `longConditionB` would emit a `Newline` token, and the parser would treat `and longConditionC:` as a separate statement.

---

## EOF Cleanup Phase

When the main tokenization loop exits (the cursor has reached the end of the source), the lexer performs cleanup:

**Step 1 — Drain the indentation stack.** While the stack contains more than one entry (the sentinel `0`), the lexer pops each entry and emits a `Dedent` token:

```
indentStack before cleanup: [0, 4, 8]
pop(8) → emit Dedent
pop(4) → emit Dedent
indentStack after cleanup:  [0]
Total Dedent tokens: 2
```

This ensures every `Indent` token in the stream has a matching `Dedent`, even when the file ends inside a deeply nested block. The parser relies on this guarantee — it never encounters an unclosed block.

**Step 2 — Trailing Newline.** If the last token in the stream is not already a `Newline`, one is appended. This ensures the final statement has a terminator.

**Step 3 — Eof token.** An `Eof` token is appended as the absolute last token in the stream, signaling the parser that no more input exists.

The final token stream for any valid source file always ends with the pattern:

```
... Newline  Dedent  Dedent  ...  Dedent  Newline  Eof
          ↑                          ↑       ↑      ↑
    last statement          stack drain   final   end
```

---

## Worked Example: Full Token Trace

Consider this complete program:

```uranite
package testing

public function main() -> I32:
    I64 x = 10
    if x > 5:
        puts( "big" )
    return 0
```

The lexer processes this line by line. The `indentStack` starts as `[0]`.

**Line 1:** `"package testing\n"`
- `atLineStart = true`, `spaceCount = 0`, `previousIndent = 0`. Same level — no indentation tokens.
- Tokens: `Identifier("package")` `Identifier("testing")` `Newline`
- Stack: `[0]`

**Line 2:** `"\n"`
- Blank line. Consumed, no tokens emitted.
- Stack: `[0]`

**Line 3:** `"public function main() -> I32:\n"`
- `spaceCount = 0`, `previousIndent = 0`. Same level.
- Tokens: `Identifier("public")` `Identifier("function")` `Identifier("main")` `LeftParenthesis` `RightParenthesis` `Arrow` `Identifier("I32")` `Colon` `Newline`
- Stack: `[0]`

**Line 4:** `"    I64 x = 10\n"`
- `spaceCount = 4`, `previousIndent = 0`. Deeper — push `4`, emit `Indent`.
- Tokens: `Indent` `Identifier("I64")` `Identifier("x")` `Assignment` `LiteralInteger("10")` `Newline`
- Stack: `[0, 4]`

**Line 5:** `"    if x > 5:\n"`
- `spaceCount = 4`, `previousIndent = 4`. Same level.
- Tokens: `Identifier("if")` `Identifier("x")` `GreaterThan` `LiteralInteger("5")` `Colon` `Newline`
- Stack: `[0, 4]`

**Line 6:** `"        puts( \"big\" )\n"`
- `spaceCount = 8`, `previousIndent = 4`. Deeper — push `8`, emit `Indent`.
- Tokens: `Indent` `Identifier("puts")` `LeftParenthesis` `LiteralString("big")` `RightParenthesis` `Newline`
- Stack: `[0, 4, 8]`

**Line 7:** `"    return 0\n"`
- `spaceCount = 4`, `previousIndent = 8`. Shallower — pop `8` (emit `Dedent`), top is now `4`, matches. Stop.
- Tokens: `Dedent` `Identifier("return")` `LiteralInteger("0")` `Newline`
- Stack: `[0, 4]`

**EOF Cleanup:**
- Stack has `[0, 4]`. Pop `4`, emit `Dedent`. Stack is `[0]`.
- Last token is `Newline` — no additional `Newline` needed. Wait — the `Dedent` is now last, so one final `Newline` is appended.
- Emit `Eof`.

**Final token stream** (37 tokens):

```
Identifier("package")  Identifier("testing")  Newline
Identifier("public")  Identifier("function")  Identifier("main")
LeftParenthesis  RightParenthesis  Arrow  Identifier("I32")  Colon  Newline
Indent
Identifier("I64")  Identifier("x")  Assignment  LiteralInteger("10")  Newline
Identifier("if")  Identifier("x")  GreaterThan  LiteralInteger("5")  Colon  Newline
Indent
Identifier("puts")  LeftParenthesis  LiteralString("big")  RightParenthesis  Newline
Dedent
Identifier("return")  LiteralInteger("0")  Newline
Dedent
Newline
Eof
```

Every `Indent` has a matching `Dedent`. Every statement ends with `Newline`. The stream terminates with `Eof`.

---

## Tabs and Spaces

The lexer accepts both tabs and spaces in leading whitespace, converting them to a single integer width using the rule:

| Character | Width |
|---|---|
| Space (`' '`) | 1 |
| Tab (`'\t'`) | 4 |

This conversion means one tab is equivalent to four spaces for indentation measurement purposes. The following lines are all measured as `spaceCount = 8`:

- Eight spaces: `"        "`
- Two tabs: `"\t\t"`
- One tab and four spaces: `"\t    "`
- Four spaces and one tab: `"    \t"`

While mixing tabs and spaces is technically accepted by the lexer, it is strongly discouraged. The `uranite-fmt` formatter normalizes all indentation to spaces (4 spaces per level), and mixed indentation creates visual ambiguity depending on the editor's tab-width setting.

The recommended practice is to use spaces exclusively, configured as 4 spaces per indentation level. The formatter enforces this:

```
uranite-fmt --write source.urn
```

---

## Common Pitfalls

### Inconsistent Dedent Level

The most common indentation error occurs when code returns to a level that was never established:

```uranite
public function broken() -> Void:
    if True:
        x = 1
      y = 2
```

Line 4 has `spaceCount = 6`. The stack is `[0, 4, 8]`. Popping `8` leaves top at `4`, but `6 != 4`. The lexer emits "inconsistent indentation" and the compilation fails.

**Fix:** Align `y = 2` to either level `8` (inside the `if` block) or level `4` (after the `if` block):

```uranite
public function fixed() -> Void:
    if True:
        x = 1
        y = 2
```

### Accidental Block Opening

A colon at the end of a line signals the parser that the next line should begin an indented block. If the next code line is not indented deeper, the parser produces an error:

```uranite
public function missing() -> Void:
I64 x = 10
```

The `Colon` token is followed by `Newline`, then `Identifier("I64")` at indentation level `0` — no `Indent` token was emitted. The parser expects `Indent` after `Colon Newline` and reports an error.

### Phantom Indentation from Tabs

When an editor displays tabs as 2 spaces but the lexer counts them as 4, visually aligned code can have mismatched indentation widths:

```
Visual (tab=2):        Lexer (tab=4):
→ x = 1    (2 cols)    spaceCount = 4
  y = 2    (2 cols)    spaceCount = 2
```

The two lines appear aligned in the editor, but the lexer measures them at different widths. This triggers "inconsistent indentation" because the stack has `[0, 4]` and `spaceCount = 2` matches no entry.

**Fix:** Configure the editor to insert spaces instead of tabs, or set tab width to 4.

### Trailing Whitespace After Backslash

The backslash continuation requires that `\` is immediately followed by `\n`. If there are spaces between the backslash and the newline, the lexer does not recognize the continuation — the backslash is tokenized as an unexpected character, and the newline emits a `Newline` token:

```uranite
I64 result = alpha + \   
    beta
```

The spaces after `\` prevent the `\` + `\n` pattern from matching. The line is split into two separate statements. The second line (`beta`) becomes an isolated identifier statement, which the parser may reject or misinterpret.

**Fix:** Ensure nothing follows the backslash on the same line — no trailing spaces or tabs.
