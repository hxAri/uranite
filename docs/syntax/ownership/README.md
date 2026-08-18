# Ownership and Memory

Uranite uses ownership-based memory management with no garbage collector. Every value has exactly one owner at any point in time. When the owner goes out of scope, the value's memory is released. Ownership can be transferred explicitly with `move`, resources can be freed manually with `delete`, and cleanup logic can be deferred to scope exit with `defer`.

---

## Table of Contents

- [Ownership and Memory](#ownership-and-memory)
  - [Table of Contents](#table-of-contents)
  - [Overview](#overview)
  - [Ownership Model](#ownership-model)
  - [Move Semantics](#move-semantics)
  - [Manual Deallocation](#manual-deallocation)
  - [Deferred Cleanup](#deferred-cleanup)
  - [Raw Memory](#raw-memory)
  - [Subpages](#subpages)

---

## Overview

Uranite tracks ownership at compile time through the borrow checker. The compiler verifies that every value has a single owner, that moved values are not used after transfer, and that references do not outlive the values they point to. This eliminates memory leaks and use-after-free bugs without runtime overhead.

The ownership system provides three explicit mechanisms for controlling when memory is freed:

- **`move`** — transfers ownership from one variable to another
- **`delete`** — immediately frees memory before the scope ends
- **`defer`** — schedules a statement to execute when the current scope exits

---

## Ownership Model

Every variable that holds a class or struct instance owns that instance. When the variable goes out of scope, the instance is freed:

```uranite
public class Resource:
    public String name

    public function Resource( self, String name ) -> Void:
        self.name = name

public function main() -> I32:
    Resource first = new Resource( "file" )
    puts( first.name )
    return 0
```

Output:

```
file
```

When `main` returns, `first` goes out of scope and its memory is released.

---

## Move Semantics

The `move` keyword explicitly transfers ownership from one variable to another. After a move, the source variable is invalidated and cannot be used:

```uranite
public class Resource:
    public String name

    public function Resource( self, String name ) -> Void:
        self.name = name

public function main() -> I32:
    Resource first = new Resource( "file" )
    Resource second = move first
    puts( second.name )
    return 0
```

Output:

```
file
```

After `move first`, ownership of the `Resource` transfers to `second`. The variable `first` is no longer valid — using it after the move produces a compile-time error.

---

## Manual Deallocation

The `delete` statement immediately frees the memory held by a variable before the scope ends. Use it when you need deterministic resource release at a specific point:

```uranite
public class Handle:
    public I64 descriptor

    public function Handle( self, I64 descriptor ) -> Void:
        self.descriptor = descriptor

public function main() -> I32:
    Handle handle = new Handle( 42 )
    puts( handle.descriptor )
    delete handle
    return 0
```

Output:

```
42
```

After `delete handle`, the memory is freed immediately. Using `handle` after deletion is a compile-time error.

---

## Deferred Cleanup

The `defer` statement schedules a statement to execute when the current scope exits, regardless of how the scope exits. Deferred statements execute in reverse order of their declaration:

```uranite
public function main() -> I32:
    puts( "start" )
    defer puts( "cleanup" )
    puts( "middle" )
    return 0
```

Output:

```
start
middle
cleanup
```

The `defer puts( "cleanup" )` statement runs after `puts( "middle" )` and before `main` returns. This pattern is useful for closing files, releasing locks, or any cleanup that must happen when leaving a scope.

---

## Raw Memory

The `Memory<T>` type provides direct access to a contiguous block of typed memory. It supports manual allocation, indexed access with `get` and `set`, and explicit sizing:

```uranite
public function main() -> I32:
    Memory<I64> buffer = new Memory<I64>( 3 )
    buffer.set( 0, 100 )
    buffer.set( 1, 200 )
    buffer.set( 2, 300 )
    puts( buffer.get( 0 ) )
    puts( buffer.get( 1 ) )
    puts( buffer.get( 2 ) )
    return 0
```

Output:

```
100
200
300
```

`Memory<I64>( 3 )` allocates space for 3 `I64` values. Elements are accessed by index through `get` and `set`. `Memory<T>` is an intrinsic type used to build higher-level data structures.

---

## Subpages

| Page | Description |
|---|---|
| [Ownership Model](ownership-model.md) | Single-owner semantics, `move` transfers, and `delete` deallocation |
| [Ownership Patterns](ownership-patterns.md) | Common patterns for managing ownership across functions and scopes |
| [Borrow Checking](borrow-checking.md) | Shared and mutable references, borrow rules, and lifetimes |
| [Memory Management](memory-management.md) | `Memory<T>`, `Arena`, and raw memory operations |
| [Droper Interface](droper-interface.md) | The `Droper` interface for custom cleanup logic when values are freed |
