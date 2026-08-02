# Hello World

This guide walks through writing, understanding, compiling, and running your first Uranite program. Every syntactic element is explained in detail. A second, more advanced example demonstrates Uranite's type system, collections, pattern matching, and control flow in a single program.

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
    - [Inspecting the Output](#inspecting-the-output)
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

Smallest valid Uranite program. Declares a package, imports a function, defines an entry point, prints a string, returns an exit code. Every line is structurally required.

---

## Anatomy of a Uranite Program

### Package Declaration

```uranite
package hello
```

First non-comment line of every Uranite source file must be a `package` declaration. Package name establishes the module's namespace and determines how other modules reference this code.

**Single-segment names** like `package hello` or `package myapp` are valid for standalone scripts and small programs.

**Multi-segment names** use dot-separated identifiers to create hierarchical namespaces:

```uranite
package myorg.myproject.utils
```

Other modules can import public declarations with:

```uranite
from myorg.myproject.utils import someFunction
```

Package name does not need to match file path, but following a convention where `package a.b.c` lives at `a/b/c.urn` (or `a/b/c/__mod__.urn` for directories) makes projects navigable.

**Naming rules:**

- Each segment must be a valid identifier: starts with a letter or underscore, contains only letters, digits, underscores, and hyphens.
- Segments are separated by dots.
- Name `uranite` is reserved for standard library. User packages must not start with `uranite.`.

### Import Statements

```uranite
from uranite.io.console import puts
```

Import statements bring declarations from other modules into current scope. Syntax follows the pattern `from <module.path> import <names>`.

**Module resolution.** Compiler resolves `uranite.io.console` by searching the standard library directory for `io/console.urn` (or `io/console/__mod__.urn`). User-defined modules are resolved relative to current file's directory or from paths specified with the `-I` flag.

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

**Visibility.** Only declarations marked `public` (or `protect`/default within an `export {}` block) are importable. Private declarations are invisible to the import system.

### The Entry Point

```uranite
public function main() -> I32:
```

This line declares the program's entry point. Every element of this signature is semantically significant.

**`public`** — Visibility modifier. `main` must be `public` because the linker resolves it as an external symbol. If `main` were `private` or `protect`, linking would fail with "undefined reference to `main`".

**`function`** — Function declaration keyword. Uranite uses `function` for all function declarations, whether free functions, methods, or static methods. No shorthand like `fn` or `def`.

**`main`** — Function name. Program entry point must be a free function named "main" at top level. Class methods named "main" do not satisfy this requirement.

**`()`** — Parameter list. Minimal `main` takes no parameters. However, `main` optionally accepts command-line arguments through two reserved parameter names:

```uranite
public function main( I64 argc, ArrayList<String> argv ) -> I32:
    puts( "Argument count:", argc )
    for String arg in argv:
        puts( arg )
    return 0
```

Both parameters are optional. You can declare `argc` alone, `argv` alone, or both together:

```uranite
public function main( I64 argc ) -> I32:
    puts( "Got", argc, "arguments" )
    return 0
```

```uranite
public function main( ArrayList<String> argv ) -> I32:
    String programName = argv.get( 0 )
    puts( "Program:", programName )
    return 0
```

Parameter names must be exactly `argc` and `argv`. `argc` is an `I64` containing the number of command-line arguments. `argv` is an `ArrayList<String>` containing the arguments themselves (index 0 is the program name).

Alternatively, you can access the same data from anywhere in your program through the `uranite.cli` module without declaring `main` parameters:

```uranite
from uranite.cli import argc, argv

public function main() -> I32:
    puts( "Argument count:", argc )
    String programName = argv.get( 0 )
    return 0
```

`uranite.cli.argc` and `uranite.cli.argv` are global constants populated automatically at program startup. Both approaches give identical access to command-line arguments.

**`-> I32`** — Return type annotation. Every function in Uranite must declare its return type after the `->` arrow. `I32` is a 32-bit signed integer. Return value of `main` becomes the process exit code: `0` indicates success, any non-zero value indicates failure.

**`:`** — Block opener. Colon signals start of an indented block. Next line must have greater indentation. No braces, no `begin`/`end` markers, no semicolons.

### Indentation-Based Blocks

Uranite uses indentation to delimit blocks, similar to Python. This is not a cosmetic preference enforced by the formatter; it is a fundamental rule of the language.

**Practical rules:**

- Use spaces or tabs consistently within a single file. Mixing spaces and tabs within a line produces unpredictable behavior.
- Standard convention is 4 spaces per indentation level. Formatter enforces this.
- Every line after a colon (`:`) must be indented further than the line containing the colon.
- Decreasing indentation closes the current block and returns to the enclosing scope.
- Empty lines and comment-only lines are skipped during indentation processing.

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

Function body is indented by 4 spaces. `if`/`elif`/`else` branches open nested blocks at 8 spaces. When indentation returns to 4 spaces, branch block closes. When it returns to 0, function body closes.

**Inconsistent indentation** that does not match any enclosing block level produces an error: "inconsistent indentation — indentation does not match any outer level".

### Function Calls and Argument Spacing

```uranite
    puts( "Hello, World!" )
```

Uranite follows a consistent spacing convention for function calls: space after opening parenthesis and space before closing parenthesis when arguments are present. Empty argument lists use no spaces: `foo()`.

```uranite
puts( "one argument" )
add( firstValue, secondValue )
empty()
```

This is an enforced style convention. Formatter rewrites calls to match this pattern.

### The Return Statement

```uranite
    return 0
```

`return` exits the current function and provides the return value. In `main`, `return 0` sets the process exit code to 0 (success).

Every non-`Void` function must return a value on all code paths. If any branch of conditional logic does not end with a `return`, compiler emits an error.

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

Compiler reads "hello.urn", compiles it to a native executable, writes result to "hello".

### One-Step: Compile and Run

```bash
./build/uranite -r hello.urn
```

`-r` flag compiles source to a temporary executable, runs it, deletes the temporary file after execution. Fastest development workflow for single-file programs.

### Inspecting the Output

Compiler provides diagnostic flags for inspecting intermediate representations:

```bash
./build/uranite hello.urn --dump-tokens
./build/uranite hello.urn --dump-ast
./build/uranite hello.urn --dump-hir
./build/uranite hello.urn --dump-mir
```

Primarily useful for debugging or understanding how compiler interprets your code.

Emit generated IR for manual inspection:

```bash
./build/uranite hello.urn --emit-llvm -o hello.ll
```

Add `--verbose` for additional diagnostic output:

```bash
./build/uranite hello.urn --verbose -o hello
```

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
            greetedCount += 1
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

**Enums.** `Greeting` enum declares three variants using the `unit` keyword. Enum variants are accessed via enum name: `Greeting.Formal`, `Greeting.Casual`, `Greeting.Silent`. Enums are not integers; they are distinct types with identity semantics.

**Match expressions.** `match style in ...` expression maps a `Greeting` value to a `String`. Each arm uses `=>` (fat arrow) to separate pattern from result. `*` wildcard matches any value not covered by preceding arms. Backslash (`\`) at end of each line is the line continuation character, allowing a single expression to span multiple lines.

**String concatenation.** `+` operator on strings performs concatenation: `prefix + name` produces a new string.

**Generics.** `ArrayList<String>` is a generic collection parameterized with `String`. Diamond syntax `new ArrayList<>()` infers type parameter from variable's declared type.

**For-in loops.** `for String guest in guests` iterates over `ArrayList` using the iterator protocol (`.iterator()`, `.has()`, `.next()`). Loop variable `guest` is typed as `String` and scoped to loop body.

**Identity comparison.** `style is not Greeting.Silent` uses `is not` identity operator. Unlike `==` (which compares values for equality), `is` compares identity directly. For enums, `is` checks whether two values are the same variant.

**Variadic console output.** `puts( "Greeted:", formalCount + casualCount, "guests" )` passes mixed-type arguments to `puts`. Function accepts variadic arguments and converts each to its string representation before printing with space separation.

**Exception handling.** `try`/`except` block catches the error when dividing `formalCount` by `silentCount` (which is 0). Compiler auto-inserts a zero-division check before every integer division and modulo operation. When check fails, an exception is raised. `except Exception as error` clause catches it and binds it to `error`, providing access to `error.message`.

**Visibility.** Only `main` is marked `public`. `formatGreeting` and `processGuests` are package-private (default visibility). They are callable within this file but not importable by other modules.

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

**Missing package declaration.** Every source file must begin with `package <name>`. Without it, compiler fails immediately with a syntax error on the first line.

**Wrong main signature.** Entry point must be `public function main() -> I32` (or with optional `argc`/`argv` parameters). Common mistakes include forgetting `public` (linker error: "undefined reference to `main`"), using `Void` as return type (type mismatch), or using parameter names other than `argc` and `argv`.

**Inconsistent indentation.** Mixing spaces and tabs within a file, or using an indentation level that does not match any enclosing block, produces an error. Pick spaces or tabs and use them consistently. Standard is 4 spaces.

**Missing colon before a block.** Every construct that opens a block (`function`, `class`, `if`, `elif`, `else`, `for`, `while`, `try`, `except`, `finally`, `match`) requires a trailing colon. Forgetting colon produces a syntax error.

**Using braces instead of indentation.** Uranite has no `{` `}` block delimiters for code blocks. Curly braces are used exclusively for `export {}` blocks, `import {}` grouping, map literals (`{"key": "value"}`), and set literals (`{1, 2, 3}`).

**Using `&&`, `||`, `!` instead of `and`, `or`, `not`.** Uranite uses keyword-based logical operators. Symbolic operators `&&`, `||`, and `!` are not part of the language. `&`, `|`, and `^` are bitwise operators with different semantics.

**Forgetting the return value in main.** `main` must return an `I32`. If any code path does not include a `return` statement, compiler emits an error about a missing return value.
