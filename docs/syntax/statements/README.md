# Statements

Statements are executable instructions that perform actions: assigning values, controlling flow, handling errors, and managing resources. Unlike expressions, statements do not produce values.

---

## Table of Contents

- [Statements](#statements)
  - [Table of Contents](#table-of-contents)
  - [Overview](#overview)
  - [At a Glance](#at-a-glance)
  - [Subpage Index](#subpage-index)

---

## Overview

Statements fall into four groups:

**Assignment** binds a value to an existing variable. Simple assignment uses `=`, and compound assignment operators (`+=`, `-=`, `*=`, `/=`) combine an arithmetic operation with assignment in a single step.

**Control flow** directs execution through branching and looping. Conditional branches (`if`, `elif`, `else`) select code paths based on Boolean conditions. Loops (`while`, `for-in`) repeat blocks until a condition is met or an iterable is exhausted. Multi-way branching (`switch`) dispatches on a value. Flow modifiers (`break`, `continue`, `return`, `pass`) alter the path through a block.

**Error handling** provides structured exception management. A `try` block guards code that may fail. One or more `except` clauses catch specific exception types using `as` to bind the caught value. An optional `finally` clause runs cleanup code regardless of success or failure. The `raise` statement throws an exception object up the call stack.

**Resource management** controls object lifetime and cleanup ordering. The `defer` statement schedules a block to execute when the enclosing scope exits, guaranteeing cleanup runs even when exceptions occur. The `delete` statement explicitly destroys an object. The `yield` statement suspends a generator function, producing a value to the caller and resuming on the next iteration.

---

## At a Glance

```uranite
I64 total = 0
for I64 index in 0..5:
    total = total + index

I64 count = 0
while count < 3:
    count = count + 1

if total > 5:
    puts( "big" )
elif total == 5:
    puts( "five" )
else:
    puts( "small" )

try:
    I64 result = divide( 10, 0 )
except Exception as error:
    puts( "caught" )

defer:
    puts( "cleanup" )
```

---

## Subpage Index

| Document | Description |
|---|---|
| [Assignment](assignment.md) | Simple assignment (`=`), compound assignment (`+=`, `-=`, `*=`, `/=`), and augmented assignment semantics. |
| [If / Elif / Else](if-elif-else.md) | Conditional branching with `if`, `elif`, and `else` clauses. |
| [While Loops](while-loops.md) | Condition-based looping with `while`. |
| [For-In Loops](for-in-loops.md) | Iteration over ranges and iterables with `for ... in`. |
| [Switch / Case](switch-case.md) | Multi-way branching with `switch` and `case`. |
| [Break](break.md) | Exiting loops early with `break`. |
| [Continue](continue.md) | Skipping to the next loop iteration with `continue`. |
| [Return](return.md) | Returning values from functions with `return`. |
| [Pass](pass.md) | The `pass` placeholder statement for empty blocks. |
| [Yield](yield.md) | Suspending generator functions with `yield` to produce values lazily. |
| [Raise](raise.md) | Throwing exceptions with `raise` and declaring throwable functions with `raises`. |
| [Try / Except / Finally](try-except-finally.md) | Structured exception handling with `try`, typed `except` clauses using `as`, and `finally` cleanup blocks. |
| [Defer](defer.md) | Scope-exit cleanup with `defer` blocks for guaranteed resource release. |
| [Delete](delete.md) | Explicit object destruction with `delete`. |
| [Unsafe](unsafe.md) | Unsafe blocks for low-level operations that bypass safety checks. |
