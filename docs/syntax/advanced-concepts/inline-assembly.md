# Inline Assembly

Uranite supports inline assembly through the `asm` keyword, allowing direct hardware access and low-level system calls within Uranite source files. The `volatile` modifier prevents the compiler from optimizing away assembly blocks. A multi-architecture block form enables writing platform-specific assembly variants in a single declaration.

---

## Table of Contents

- [Inline Assembly](#inline-assembly)
  - [Table of Contents](#table-of-contents)
  - [Overview](#overview)
  - [Basic Syntax](#basic-syntax)
  - [Volatile Assembly](#volatile-assembly)
  - [Operands](#operands)
  - [Multi-Architecture Blocks](#multi-architecture-blocks)
  - [Clobber Lists](#clobber-lists)

---

## Overview

Inline assembly embeds raw machine instructions directly in Uranite code. It is used for:

- Raw system calls (syscall instructions)
- Hardware register access
- Performance-critical inner loops
- Platform-specific operations with no Uranite equivalent

Assembly blocks interact with Uranite variables through typed operand lists that bind constraints to expressions.

---

## Basic Syntax

The `asm` keyword begins an inline assembly statement. A string literal contains the assembly template:

```uranite
asm "nop"
```

The template string uses the same syntax as LLVM inline assembly — register references use `$0`, `$1`, etc., corresponding to the operand list positions.

---

## Volatile Assembly

The `volatile` modifier after `asm` tells the compiler that the assembly block has side effects and must not be removed or reordered:

```uranite
asm volatile "mfence"
```

Without `volatile`, the compiler may eliminate assembly blocks that appear to have no observable effect. Use `volatile` for memory barriers, I/O operations, and any assembly with side effects beyond the declared operands.

---

## Operands

Assembly blocks communicate with Uranite code through three operand sections separated by colons:

- **output** — variables written by the assembly
- **input** — values read by the assembly
- **clobber** — registers modified by the assembly that the compiler must preserve

Each output and input operand is a pair of a constraint string and a Uranite expression:

```uranite
I64 result = 0
asm volatile "syscall" : output( "={rax}" result ) : input( "{rax}" 231, "{rdi}" 0 )
```

In this example:
- `output( "={rax}" result )` — the value in `rax` after execution is stored into `result`
- `input( "{rax}" 231, "{rdi}" 0 )` — register `rax` receives `231`, register `rdi` receives `0`

Constraint strings follow LLVM constraint syntax. The `=` prefix marks output operands. Register names in `{}` specify exact register bindings. The `"r"` constraint allows the compiler to choose any general-purpose register.

---

## Multi-Architecture Blocks

The multi-architecture form uses an indented block after `asm volatile:` with one line per target architecture. Each line names the target, provides an assembly template, and lists operands:

```uranite
I64 result = 0
I64 functionAddress = 0

asm volatile:
    x86-64 "call *$2" : output( "={rax}" result ) : input( "{rdi}" 0, "r" functionAddress ) : clobber( "memory", "rsi", "rcx" )
    aarch64 "blr $2" : output( "={x0}" result ) : input( "{x0}" 0, "r" functionAddress ) : clobber( "memory", "x1", "x2", "x3" )
```

The compiler selects the variant matching the compilation target architecture. Supported architecture identifiers include `x86-64` and `aarch64`. Each variant has its own independent operand and clobber lists, since register names and calling conventions differ between architectures.

This form is used extensively in the standard library for portable low-level operations like system calls, task scheduling, and memory management.

---

## Clobber Lists

The `clobber` section declares registers that the assembly modifies beyond the declared outputs. The compiler saves and restores these registers around the assembly block:

```uranite
asm volatile "cpuid" : output( "={eax}" leaf ) : input( "{eax}" 0 ) : clobber( "ebx", "ecx", "edx" )
```

The special clobber `"memory"` tells the compiler that the assembly reads or writes memory beyond the declared operands, preventing the compiler from caching memory values across the assembly block.

