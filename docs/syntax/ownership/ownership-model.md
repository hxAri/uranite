# Ownership Model

- [Table of Contents](#table-of-contents)

## Table of Contents

- [Ownership Model](#ownership-model)
  - [Table of Contents](#table-of-contents)
  - [Overview](#overview)
  - [Single Owner](#single-owner)
  - [Move Semantics](#move-semantics)
  - [Move to Function](#move-to-function)
  - [Manual Deallocation](#manual-deallocation)
  - [Deferred Deallocation](#deferred-deallocation)
  - [Scoped Ownership](#scoped-ownership)
  - [Raw Memory Ownership](#raw-memory-ownership)

## Overview

Uranite uses ownership-based memory management. Every heap-allocated value has exactly one owner at any point in time. When the owner goes out of scope, the value's memory is eligible for release.

Ownership can be transferred between variables using `move`. Memory can be released early using `delete`. Cleanup can be scheduled for scope exit using `defer`.

There is no garbage collector. The compiler tracks ownership at compile time through the borrow checker, verifying that moved values are not used after transfer and that resources are properly managed.

## Single Owner

A variable that holds a class instance created with `new` owns that instance. The owner controls the lifetime of the allocated memory.

```uranite
package testing

from uranite.io.console import puts

public class Resource:
    public String name

    public function Resource( self, String name ) -> Void:
        self.name = name

public function main() -> I32:
    Resource res = new Resource( "file" )
    puts( res.name )
    return 0
```

The variable `res` owns the `Resource` instance. The program prints `file`. When `main` returns, `res` goes out of scope.

## Move Semantics

The `move` keyword transfers ownership from one variable to another. After a move, the source variable is invalidated.

```uranite
package testing

from uranite.io.console import puts

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

Ownership of the `Resource` transfers from `first` to `second`. The variable `second` now owns the instance and can access its fields. The program prints `file`.

## Move to Function

The `move` keyword transfers ownership to a function parameter. The function becomes the new owner of the value.

```uranite
package testing

from uranite.io.console import puts

public class Handle:
    public function Handle( self ) -> Void:
        pass

public function takeOwnership( Handle handle ) -> Void:
    puts( "took ownership" )

public function main() -> I32:
    Handle handle = new Handle()
    takeOwnership( move handle )
    return 0
```

Ownership of the `Handle` transfers to the `takeOwnership` function. The caller's `handle` variable is invalidated after the move. The program prints `took ownership`.

## Manual Deallocation

The `delete` statement frees memory immediately, before the owner goes out of scope. This provides deterministic resource release at a specific point.

```uranite
package testing

from uranite.io.console import puts

public class Handle:
    public I64 descriptor

    public function Handle( self, I64 descriptor ) -> Void:
        self.descriptor = descriptor

public function main() -> I32:
    Handle handle = new Handle( 42 )
    puts( handle.descriptor )
    delete handle
    puts( "freed" )
    return 0
```

The `Handle` is used and then explicitly freed with `delete`. The program prints `42` followed by `freed`.

## Deferred Deallocation

The `defer` statement combined with `delete` schedules deallocation for scope exit. This guarantees cleanup runs regardless of which return path the function takes.

```uranite
package testing

from uranite.io.console import puts
from uranite.memory.memory import Memory

public function main() -> I32:
    Memory<I64> buffer = new Memory<I64>( 5 )
    defer delete buffer
    buffer.set( 0, 42 )
    I64 val = buffer.get( 0 )
    puts( val )
    return 0
```

The `defer delete buffer` schedules deallocation for when `main` returns. The buffer is used normally throughout the function. The program prints `42`.

## Scoped Ownership

When a function creates and deletes an object within its body, the ownership is entirely scoped to that function. The caller is unaffected.

```uranite
package testing

from uranite.io.console import puts

public class Tracker:
    public function Tracker( self ) -> Void:
        pass

public function createAndUse() -> Void:
    Tracker tracker = new Tracker()
    puts( "created" )
    delete tracker
    puts( "deleted in scope" )

public function main() -> I32:
    createAndUse()
    puts( "after function" )
    return 0
```

The `Tracker` is created, used, and deleted entirely within `createAndUse`. The caller sees no ownership transfer. The output is `created`, `deleted in scope`, `after function`.

## Raw Memory Ownership

The `Memory<T>` type provides ownership over a raw block of typed heap memory. The owner is responsible for freeing the allocation with `delete`.

```uranite
package testing

from uranite.io.console import puts
from uranite.memory.memory import Memory

public function main() -> I32:
    Memory<I64> buffer = new Memory<I64>( 3 )
    buffer.set( 0, 100 )
    buffer.set( 1, 200 )
    buffer.set( 2, 300 )
    I64 val = buffer.get( 1 )
    puts( val )
    delete buffer
    return 0
```

A `Memory<I64>` buffer is allocated with capacity for 3 elements. Values are stored and retrieved by index. The owner frees the buffer with `delete`. The program prints `200`.
