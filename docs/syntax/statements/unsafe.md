# Unsafe

- [Table of Contents](#table-of-contents)

## Table of Contents

- [Unsafe](#unsafe)
  - [Table of Contents](#table-of-contents)
  - [Overview](#overview)
  - [Basic Unsafe Block](#basic-unsafe-block)
  - [Variables in Unsafe](#variables-in-unsafe)
  - [Control Flow in Unsafe](#control-flow-in-unsafe)
  - [Unsafe in Functions](#unsafe-in-functions)
  - [Loops in Unsafe](#loops-in-unsafe)
  - [Multiple Unsafe Blocks](#multiple-unsafe-blocks)

## Overview

The `unsafe` block marks a region of code where certain safety restrictions are relaxed. Operations that bypass the normal safety guarantees of the language must appear inside an `unsafe` block.

The general form is:

```
unsafe:
    statements
```

Uranite enforces memory safety by default. Some low-level operations, such as dereferencing raw pointers, are forbidden outside of an `unsafe` block. The `unsafe` keyword signals that the programmer takes responsibility for ensuring correctness of the enclosed operations.

Code inside an `unsafe` block has access to the same language features as normal code, plus additional operations that the compiler would otherwise reject. Nested control flow, function calls, and variable declarations all work normally within `unsafe` blocks.

## Basic Unsafe Block

An `unsafe` block begins with the `unsafe` keyword followed by a colon and an indented body.

```uranite
package testing

from uranite.io.console import puts

public function main() -> I32:
    unsafe:
        puts( "inside unsafe" )
    return 0
```

The `unsafe` block executes its body normally. The program prints `inside unsafe`.

## Variables in Unsafe

Variables can be declared and used inside an `unsafe` block. These variables follow the same scoping rules as variables in other block statements.

```uranite
package testing

from uranite.io.console import puts

public function main() -> I32:
    unsafe:
        I64 value = 42
        puts( "unsafe with vars" )
    return 0
```

The variable `value` is declared inside the `unsafe` block. The program prints `unsafe with vars`.

## Control Flow in Unsafe

Control flow statements such as `if`, `while`, and `for` can appear inside an `unsafe` block. The unsafe context extends to all nested code within the block.

```uranite
package testing

from uranite.io.console import puts

public function main() -> I32:
    unsafe:
        if True:
            puts( "nested in unsafe" )
    return 0
```

The `if` statement inside the `unsafe` block executes normally. The unsafe context covers all nested statements. The program prints `nested in unsafe`.

## Unsafe in Functions

An `unsafe` block can appear inside any function body. The unsafe context is local to the block and does not affect code outside of it.

```uranite
package testing

from uranite.io.console import puts

public function doUnsafe() -> Void:
    unsafe:
        puts( "unsafe in function" )

public function main() -> I32:
    doUnsafe()
    return 0
```

The `doUnsafe` function contains an `unsafe` block. Calling the function from `main` executes the unsafe code. The program prints `unsafe in function`.

## Loops in Unsafe

Loops can run inside an `unsafe` block, allowing repeated execution of low-level operations within a single unsafe context.

```uranite
package testing

from uranite.io.console import puts

public function main() -> I32:
    I64 count = 0
    unsafe:
        while count < 3:
            count = count + 1
    puts( "loop in unsafe done" )
    return 0
```

The `while` loop runs inside the `unsafe` block. After the loop completes, execution continues after the block. The program prints `loop in unsafe done`.

## Multiple Unsafe Blocks

A function can contain multiple `unsafe` blocks. Each block independently marks a region of unsafe code. Code between the blocks runs in the normal safe context.

```uranite
package testing

from uranite.io.console import puts

public function main() -> I32:
    unsafe:
        puts( "first unsafe" )
    unsafe:
        puts( "second unsafe" )
    return 0
```

Two separate `unsafe` blocks appear in the same function. Each runs independently. The program prints `first unsafe` followed by `second unsafe`.
