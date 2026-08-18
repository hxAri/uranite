# Constants

Constants are immutable bindings. Once a constant is initialized, its value cannot be changed for the rest of its lifetime. Uranite provides constants at two levels: local constants inside function bodies and top-level constants that exist for the entire duration of the program.

---

## Table of Contents

- [Constants](#constants)
  - [Table of Contents](#table-of-contents)
  - [Overview](#overview)
  - [Local Constants](#local-constants)
  - [Top-Level Constants](#top-level-constants)
  - [Using Constants in Expressions](#using-constants-in-expressions)
  - [Immutability Enforcement](#immutability-enforcement)
  - [When to Use Constants](#when-to-use-constants)

---

## Overview

The `const` keyword declares a binding that cannot be reassigned after initialization. Constants provide two guarantees: the name always refers to the same value, and the compiler rejects any attempt to modify it.

```uranite
const I64 maxConnections = 100
const String defaultHost = "localhost"
const F64 gravity = 9.81
```

Constants and variables serve different purposes. Variables are mutable bindings that can change over time. Constants are fixed bindings that represent values known at the point of declaration and never modified afterward.

---

## Local Constants

A local constant is declared inside a function body using the `const` keyword followed by a type, a name, and an initializer:

```uranite
public function calculateArea( I64 width, I64 height ) -> I64:
    const I64 area = width * height
    return area
```

Local constants follow the same scoping rules as variables. They are visible from the point of declaration to the end of the enclosing block.

Multiple local constants can appear in the same function:

```uranite
from uranite.io.console import puts

public function main() -> I32:
    const I64 width = 10
    const I64 height = 20
    I64 area = width * height
    puts( area )
    return 0
```

Output:

```
200
```

Local constants accept any expression as their initializer, including function calls, arithmetic, and constructor expressions. The value is computed at runtime and then frozen for the lifetime of the binding.

---

## Top-Level Constants

A top-level constant is declared outside any function or class body using the `const` keyword. Top-level constants are visible to all functions in the same module and can be imported by other modules when declared `public`.

```uranite
package config

from uranite.io.console import puts

const I64 MAX_RETRIES = 3
const String APP_NAME = "Uranite"
const F64 PI = 3.14159

public function main() -> I32:
    puts( APP_NAME )
    puts( MAX_RETRIES )
    puts( PI )
    return 0
```

Output:

```
Uranite
3
3.14159
```

Top-level constants become global values in the compiled output. Integer and floating-point constants are compiled as LLVM global constants with fixed values. String constants are compiled as global string pointers.

By convention, top-level constant names use UPPER_SNAKE_CASE to distinguish them from local bindings and function names.

---

## Using Constants in Expressions

Constants participate in expressions the same way variables do. They can be operands in arithmetic, arguments to function calls, and elements in larger expressions:

```uranite
package testing

from uranite.io.console import puts

const I64 BASE_SCORE = 100

public function main() -> I32:
    I64 bonus = 50
    I64 totalScore = BASE_SCORE + bonus
    puts( totalScore )
    return 0
```

Output:

```
150
```

---

## Immutability Enforcement

The compiler rejects any attempt to reassign a constant. Both direct assignment and compound assignment operators produce compile-time errors:

Attempting to reassign a `const` binding:

```uranite
const I64 limit = 10
limit = 20
```

Produces:

```
Error: cannot assign to immutable variable "limit"
```

Attempting compound assignment on a `const` binding:

```uranite
const I64 value = 10
value += 5
```

Produces the same error:

```
Error: cannot assign to immutable variable "value"
```

The immutability check applies to the binding itself. For class-typed constants, the constant binding cannot be reassigned to point to a different object, but the object's own mutable fields can still be modified through method calls.

---

## When to Use Constants

Use constants for values that are determined once and never change:

- Configuration limits (`const I64 MAX_BUFFER_SIZE = 4096`)
- Mathematical values (`const F64 EULER = 2.71828`)
- Application metadata (`const String VERSION = "1.0.0"`)
- Fixed lookup values (`const I64 HASH_SEED = 0x9E3779B9`)

Use variables for values that change during execution:

- Loop counters
- Accumulators
- State trackers
- Intermediate computation results

When in doubt, start with `const`. If the compiler later rejects an assignment, change it to a variable. This approach ensures the maximum number of bindings are provably immutable, which makes code easier to reason about.
