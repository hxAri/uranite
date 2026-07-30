# Hello World

This guide walks through writing, understanding, compiling, and running your first Uranite program. Every syntactic and semantic element is explained in detail. A second, more advanced example demonstrates Uranite's type system, collections, pattern matching, and control flow in a single program.

---

## Table of Contents

- [Hello World](#hello-world)
  - [Table of Contents](#table-of-contents)
  - [The Minimal Program](#the-minimal-program)
  - [Anatomy of a Uranite Program](#anatomy-of-a-uranite-program)
    - [Package Declaration](#package-declaration)
    - [Import Statements](#import-statements)
    - [The Entry Point](#the-entry-point)
    - [Indentation-Based Blocks](#indentation-based-blocks)
    - [Function Calls and Argument Spacing](#function-calls-and-argument-spacing)
    - [The Return Statement](#the-return-statement)
  - [Compiling and Running](#compiling-and-running)
    - [Two-Step: Compile Then Run](#two-step-compile-then-run)
    - [One-Step: Compile and Run](#one-step-compile-and-run)
    - [Emitting LLVM IR](#emitting-llvm-ir)
    - [Inspecting the Pipeline](#inspecting-the-pipeline)
  - [What the Compiler Does](#what-the-compiler-does)
  - [A More Complete Example](#a-more-complete-example)
    - [The Program](#the-program)
    - [Walkthrough](#walkthrough)
    - [Running It](#running-it)
  - [Common Beginner Mistakes](#common-beginner-mistakes)

---

## The Minimal Program

Create a file called "hello.urn":

```uranite
package hello

from uranite.io.console import puts

public function main() -> I32:
    puts( "Hello, World!" )
    return 0
```

This is the smallest valid Uranite program. It declares a package, imports a function, defines an entry point, prints a string, and returns an exit code. Every line is structurally required.

---

## Anatomy of a Uranite Program

### Package Declaration

```uranite
package hello
```

The first non-comment line of every Uranite source file must be a `package` declaration. The package name establishes the module's namespace for the import system and determines how other modules reference this code.

**Single-segment names** like `package hello` or `package myapp` are valid for standalone scripts and small programs. They create a flat namespace with no hierarchy.

**Multi-segment names** use dot-separated identifiers to create a hierarchical namespace:

```uranite
package myorg.myproject.utils
```

This declares that the current file belongs to the `myorg.myproject.utils` namespace. Other modules can import its public declarations with:

```uranite
from myorg.myproject.utils import someFunction
```

The package name does not need to match the file path, but following a convention where `package a.b.c` lives at `a/b/c.urn` (or `a/b/c/__mod__.urn` for directories) makes the project navigable.

**Naming rules:**

- Each segment must be a valid identifier: starts with a letter or underscore, contains only letters, digits, underscores, and hyphens.
- Segments are separated by dots.
- The name `uranite` is reserved for the standard library. User packages must not start with `uranite.`.

### Import Statements

```uranite
from uranite.io.console import puts
```

Import statements bring declarations from other modules into the current scope. The syntax follows the pattern `from <module.path> import <names>`.

**Module resolution.** The compiler resolves `uranite.io.console` by searching the standard library directory for `io/console.urn` (or `io/console/__mod__.urn`). User-defined modules are resolved relative to the current file's directory or from paths specified with the `-I` flag.

**Single import:**

```uranite
from uranite.collection.array-list import ArrayList
```

**Multiple imports** use comma separation:

```uranite
from uranite.io.console import puts, input
```

**Brace-wrapped imports** for long lists:

```uranite
from uranite.collection.array-list import {
    ArrayList,
    ArrayListIterator
}
```

**Visibility.** Only declarations marked `public` (or `protect`/default within an `export {}` block) are importable. Private declarations are invisible to the import system regardless of the module path.

**Eager analysis.** When the compiler encounters an import, it fully analyzes the imported module (registration pass) before continuing with the current file. This means imported types and symbols are available immediately for type checking and semantic validation. Analyzed modules are cached so that multiple imports of the same module do not trigger re-analysis.

### The Entry Point

```uranite
public function main() -> I32:
```

This line declares the program's entry point. Every element of this signature is semantically significant.

**`public`** — The visibility modifier. The `main` function must be `public` because the linker resolves it as an external symbol. During code generation, the compiler emits `main` as a globally visible LLVM function with external linkage. If `main` were `private` or `protect`, the linker would not find the entry point and would produce an "undefined reference to `main`" error.

**`function`** — The function declaration keyword. Uranite uses `function` for all function declarations, whether free functions, methods, or static methods. There is no shorthand like `fn` or `def`.

**`main`** — The function name. The compiler checks for a top-level function named "main" (referenced via the centralized qualnames registry as `semantic::qualname::functions::main::Name`) to serve as the program entry point. Class methods named "main" do not satisfy this requirement; the entry point must be a free function with no owner class.

**`()`** — Empty parameter list. The `main` function takes no parameters. Unlike C's `main(int argc, char** argv)`, Uranite's entry point does not receive command-line arguments directly. Access command-line arguments through the `uranite.sys` or `uranite.cli` standard library modules.

**`-> I32`** — The return type annotation. Every function in Uranite must declare its return type after the `->` arrow. `I32` is a 32-bit signed integer. The return value of `main` becomes the process exit code: `0` indicates success, and any non-zero value indicates failure. The operating system captures this value and makes it available to the parent process (e.g., `$?` in bash).

**`:`** — The block opener. The colon at the end of the line signals the start of an indented block. The next line must have greater indentation than the current line. There are no braces, no `begin`/`end` markers, and no semicolons.

### Indentation-Based Blocks

Uranite uses indentation to delimit blocks, similar to Python. This is not a cosmetic preference enforced by the formatter; it is a lexical rule enforced by the compiler's tokenizer.

**How the lexer handles indentation.** The lexer maintains an indentation stack (a stack of integer column positions, initialized with `0` at the bottom). At the start of each line, the lexer's `handleIndentation()` method counts leading whitespace characters (spaces count as 1, tabs count as 4) and compares the total against the top of the stack:

- If the new indentation is **greater** than the stack top, the lexer pushes the new level onto the stack and emits an `Indent` token. This opens a new block.
- If the new indentation is **less** than the stack top, the lexer pops levels from the stack until it finds a matching level, emitting one `Dedent` token for each popped level. This closes one or more blocks.
- If the new indentation **equals** the stack top, no indentation token is emitted. The line continues the current block.
- If the new indentation does not match any level on the stack, the lexer emits a diagnostic error: "inconsistent indentation — indentation does not match any outer level".

At the end of the source file, the lexer pops all remaining levels from the stack (except the base `0`), emitting `Dedent` tokens for each. This ensures all blocks are properly closed.

**Practical rules:**

- Use spaces or tabs consistently within a single file. Mixing spaces and tabs within a line produces unpredictable column counts.
- The standard convention is 4 spaces per indentation level. The formatter enforces this.
- Every line after a colon (`:`) must be indented further than the line containing the colon.
- Empty lines and lines containing only comments are skipped during indentation processing.

**Example with two nesting levels:**

```uranite
public function classify( I64 value ) -> String:
    if value > 100:
        return "large"
    elif value > 0:
        return "small"
    else:
        return "non-positive"
```

The lexer produces the following indentation token sequence for this code:

```
function header       (column 0)
    INDENT            (column 4, opens function body)
    if header         (column 4)
        INDENT        (column 8, opens if body)
        return        (column 8)
        DEDENT        (column 4, closes if body)
    elif header       (column 4)
        INDENT        (column 8, opens elif body)
        return        (column 8)
        DEDENT        (column 4, closes elif body)
    else header       (column 4)
        INDENT        (column 8, opens else body)
        return        (column 8)
        DEDENT        (column 4, closes else body)
    DEDENT            (column 0, closes function body)
```

The parser consumes `Indent` and `Dedent` tokens to build the AST's block structure. A missing `Indent` after a colon, or a `Dedent` that does not align with any previous indentation level, is a syntax error.

### Function Calls and Argument Spacing

```uranite
    puts( "Hello, World!" )
```

Uranite follows a consistent spacing convention for function calls: a space after the opening parenthesis and a space before the closing parenthesis when arguments are present. Empty argument lists use no spaces: `foo()`.

```uranite
puts( "one argument" )
add( firstValue, secondValue )
empty()
```

This is an enforced style convention. The formatter rewrites calls to match this pattern. The compiler accepts calls without spaces (`puts("Hello")`), but the canonical form includes them.

### The Return Statement

```uranite
    return 0
```

The `return` statement exits the current function and provides the return value. In `main`, `return 0` sets the process exit code to 0 (success).

Every non-`Void` function must return a value on all code paths. The HIR validator checks that all branches of conditional logic terminate with either a `return` statement or an expression that produces a value.

For `Void` functions, `return` with no value is optional. Control flow falls off the end of the function body implicitly.

---

## Compiling and Running

### Two-Step: Compile Then Run

```bash
./build/uranite hello.urn -o hello
./hello
```

Output:

```
Hello, World!
```

The compiler reads "hello.urn", runs it through all twelve pipeline stages (lexer through linker), and writes a native executable to "hello". The second command executes the binary directly.

### One-Step: Compile and Run

```bash
./build/uranite -r hello.urn
```

The `-r` flag compiles the source to a temporary executable, runs it, and deletes the temporary file after execution. This is the fastest development workflow for single-file programs.

### Emitting LLVM IR

```bash
./build/uranite hello.urn --emit-llvm -o hello.ll
```

This writes the LLVM IR representation to "hello.ll" instead of compiling to a native binary. Inspecting the IR is useful for understanding how Uranite constructs map to LLVM instructions.

### Inspecting the Pipeline

Dump the output of any compilation stage:

```bash
./build/uranite hello.urn --dump-tokens
```

Shows the lexer's token stream, including `Indent`, `Dedent`, and `Newline` tokens that delimit blocks.

```bash
./build/uranite hello.urn --dump-ast
```

Shows the parsed abstract syntax tree with node types, positions, and resolved identifiers.

```bash
./build/uranite hello.urn --dump-hir
```

Shows the high-level IR after semantic analysis. Types are resolved and annotated.

```bash
./build/uranite hello.urn --dump-mir
```

Shows the mid-level IR with flattened control flow. Loops are desugared into basic blocks with explicit branch and terminator instructions.

Combine any dump flag with `--verbose` for additional logging from each pipeline stage:

```bash
./build/uranite hello.urn --dump-mir --verbose
```

---

## What the Compiler Does

When you run `./build/uranite hello.urn -o hello`, the compiler executes the following stages:

1. **Lexing.** The lexer reads the source text character by character and produces a flat token stream. Keywords (`package`, `from`, `import`, `public`, `function`, `return`), identifiers (`hello`, `main`, `puts`), literals (`"Hello, World!"`, `0`), operators (`->`), punctuation (`(`, `)`, `,`, `:`), and indentation tokens (`Indent`, `Dedent`) are all emitted. The `0` on the indentation stack is the baseline; the function body pushes `4`.

2. **Parsing.** The recursive descent parser consumes the token stream and builds an AST. The `package` declaration becomes a `PackageDeclaration` node. The import becomes an `ImportDeclaration` node. The function becomes a `FunctionDeclaration` node containing a `Block` statement with two children: a `CallExpression` (for `puts`) and a `ReturnStatement` (for `return 0`).

3. **Semantic analysis (pass 1).** The registration pass scans the program's declarations and registers the `main` function's name and signature in the symbol table. If this file were imported by another module, its public declarations would become available at this stage.

4. **Semantic analysis (pass 2).** The validation pass type-checks every expression. It resolves `puts` to the imported function from `uranite.io.console`, verifies that `"Hello, World!"` is a `String` (matching `puts`'s parameter type), and confirms that `0` is an `I32` (matching `main`'s return type).

5. **Borrow checking.** The AST-level borrow checker verifies ownership rules. In this simple program, no ownership transfers or mutable borrows occur, so the checker passes trivially.

6. **HIR lowering.** The AST is lowered to HIR nodes with resolved types. The function body becomes a sequence of HIR statements.

7. **HIR validation.** Structural correctness checks confirm that the function body terminates with a return statement.

8. **MIR lowering.** HIR is lowered to MIR instructions in basic blocks. The function body becomes a single basic block containing a `CallFunction` instruction (for `puts`) and a `Return` instruction (for `return 0`).

9. **MIR liveness, borrow checking, and optimization.** Liveness analysis, MIR-level borrow checking, and dead code elimination run on the MIR. This program has no dead code or ownership issues.

10. **MIR codegen.** The MIR is translated to LLVM IR. The `main` function is emitted as an LLVM function with `i32` return type. A call to `__uranite_push_frame` is inserted at entry, and `__uranite_pop_frame` is inserted before the return. The `puts` call is emitted as a call to the compiled `puts` function from the standard library.

11. **LLVM optimization and code generation.** The LLVM `opt` tool runs optimization passes on the IR. Then `llc` lowers the optimized IR to a native object file for the host architecture.

12. **Linking.** The system C compiler (`cc`) links the object file with the six C runtime libraries (`liburanite-exception-runtime.a`, `liburanite-async-runtime.a`, `liburanite-thread-runtime.a`, `liburanite-subprocess-runtime.a`, `liburanite-ipc-runtime.a`, `liburanite-ffi-runtime.a`) and the system C library to produce the final executable.

---

## A More Complete Example

### The Program

Create a file called "greetings.urn":

```uranite
package greetings

from uranite.io.console import puts
from uranite.collection.array-list import ArrayList
from uranite.errors.exception import Exception

enum Greeting:
    unit Formal
    unit Casual
    unit Silent

function formatGreeting( String name, Greeting style ) -> String:
    String prefix = match style in \
        Greeting.Formal => "Good evening, ", \
        Greeting.Casual => "Hey, ", \
        Greeting.Silent => "", \
        * => "Hello, "
    return prefix + name

function processGuests( ArrayList<String> guests, Greeting style ) -> I64:
    I64 greetedCount = 0
    for String guest in guests:
        String message = formatGreeting( guest, style )
        if style is not Greeting.Silent:
            puts( message )
            greetedCount+= 1
    return greetedCount

public function main() -> I32:
    ArrayList<String> guests = new ArrayList<>()
    guests.add( "Alice" )
    guests.add( "Bob" )
    guests.add( "Charlie" )

    puts( "--- Formal Greetings ---" )
    I64 formalCount = processGuests( guests, Greeting.Formal )

    puts( "--- Casual Greetings ---" )
    I64 casualCount = processGuests( guests, Greeting.Casual )

    I64 silentCount = processGuests( guests, Greeting.Silent )

    puts( "Greeted:", formalCount + casualCount, "guests" )
    puts( "Skipped:", silentCount, "silent greetings" )

    try:
        I64 ratio = formalCount / silentCount
    except Exception as error:
        puts( "Cannot compute ratio:", error.message )

    return 0
```

### Walkthrough

This program demonstrates nine language features in 45 lines.

**Enums.** The `Greeting` enum declares three variants using the `unit` keyword. Enum variants are accessed via the enum name: `Greeting.Formal`, `Greeting.Casual`, `Greeting.Silent`. Enums are not integers; they are distinct types with identity semantics.

**Match expressions.** The `match style in ...` expression maps a `Greeting` value to a `String`. Each arm uses `=>` (fat arrow) to separate the pattern from the result. The `*` wildcard matches any value not covered by the preceding arms. The backslash (`\`) at the end of each line is the line continuation character, allowing a single expression to span multiple lines.

**String concatenation.** The `+` operator on strings performs concatenation: `prefix + name` produces a new string. Under the hood, this invokes the `String.Concat` codegen path, which computes the combined length, allocates a buffer, and copies both operands.

**Generics.** `ArrayList<String>` is a generic collection parameterized with `String`. The diamond syntax `new ArrayList<>()` infers the type parameter from the variable's declared type. Generic containers store all elements as opaque pointers (`ptr` in LLVM IR) through type erasure; the semantic type is preserved in the variable descriptor table for type-safe operations.

**For-in loops.** `for String guest in guests` iterates over the `ArrayList` using the iterator protocol. The compiler desugars this into calls to `.iterator()`, `.has()`, and `.next()` on the collection. The loop variable `guest` is typed as `String` and is scoped to the loop body.

**Identity comparison.** `style is not Greeting.Silent` uses the `is not` identity operator. Unlike `==` (which calls the `Equatable.equals()` interface method), `is` compares object identity directly. For enums, `is` checks whether two values are the same variant.

**Variadic console output.** `puts( "Greeted:", formalCount + casualCount, "guests" )` passes mixed-type arguments to `puts`. The function accepts variadic arguments and converts each to its string representation before printing with space separation.

**Exception handling.** The `try`/`except` block catches the `ZeroDivisionError` that occurs when dividing `formalCount` by `silentCount` (which is 0). The compiler auto-inserts a zero-division check before every integer division and modulo operation. When the check fails, it raises an exception through the shadow stack runtime. The `except Exception as error` clause catches the exception and binds it to `error`, providing access to `error.message`.

**Visibility.** Only `main` is marked `public`. The `formatGreeting` and `processGuests` functions are package-private (default visibility). They are callable within this file but not importable by other modules.

### Running It

```bash
./build/uranite -r greetings.urn
```

Expected output:

```
--- Formal Greetings ---
Good evening, Alice
Good evening, Bob
Good evening, Charlie
--- Casual Greetings ---
Hey, Alice
Hey, Bob
Hey, Charlie
Greeted: 6 guests
Skipped: 0 silent greetings
Cannot compute ratio: division by zero
```

---

## Common Beginner Mistakes

**Missing package declaration.** Every source file must begin with `package <name>`. Without it, the parser fails immediately with a syntax error on the first line.

**Wrong main signature.** The entry point must be exactly `public function main() -> I32`. Common mistakes include forgetting `public` (linker error: "undefined reference to `main`"), using `Void` as the return type (type mismatch), or adding parameters (signature mismatch).

**Inconsistent indentation.** Mixing spaces and tabs within a file, or using an indentation level that does not match any enclosing block, produces the error "inconsistent indentation — indentation does not match any outer level". Pick spaces or tabs and use them consistently. The standard is 4 spaces.

**Missing colon before a block.** Every construct that opens a block (function declarations, class declarations, `if`, `elif`, `else`, `for`, `while`, `try`, `except`, `finally`, `match`) requires a trailing colon. Forgetting the colon produces a syntax error at the next line.

**Using braces instead of indentation.** Uranite has no `{` `}` block delimiters. Curly braces are used exclusively for `export {}` blocks, `import {}` grouping, map literals (`{"key": "value"}`), and set literals (`{1, 2, 3}`). Using them as block delimiters produces a syntax error.

**Using `&&`, `||`, `!` instead of `and`, `or`, `not`.** Uranite uses keyword-based logical operators. The symbolic operators `&&`, `||`, and `!` are not part of the language. `&`, `|`, and `^` are bitwise operators with different semantics.

**Forgetting the return value in main.** `main` must return an `I32`. If the last code path does not include a `return` statement, the HIR validator emits an error about a missing return value.
