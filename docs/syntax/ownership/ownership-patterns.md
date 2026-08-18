# Ownership Patterns

- [Table of Contents](#table-of-contents)

## Table of Contents

- [Ownership Patterns](#ownership-patterns)
  - [Table of Contents](#table-of-contents)
  - [Overview](#overview)
  - [Factory Functions](#factory-functions)
  - [Sink Functions](#sink-functions)
  - [Ownership Chaining](#ownership-chaining)
  - [Reassignment](#reassignment)
  - [Conditional Ownership](#conditional-ownership)
  - [Loop-Scoped Ownership](#loop-scoped-ownership)
  - [Deferred Cleanup](#deferred-cleanup)

## Overview

Uranite ownership rules produce recurring patterns for allocating, transferring, and releasing resources. Each pattern addresses a specific lifecycle shape: who creates a value, who uses it, and who is responsible for cleanup.

These patterns build on the fundamentals covered in the ownership model. Variables are mutable by default and can be reassigned freely. The `const` keyword makes a variable immutable.

## Factory Functions

A function allocates an object and returns it. The caller receives ownership and becomes responsible for the lifetime.

```uranite
package testing

from uranite.io.console import puts

public class Connection:
    public String host

    public function Connection( self, String host ) -> Void:
        self.host = host

public function createConnection( String host ) -> Connection:
    Connection conn = new Connection( host )
    return conn

public function main() -> I32:
    Connection conn = createConnection( "localhost" )
    puts( conn.host )
    delete conn
    return 0
```

The function `createConnection` allocates a `Connection` and returns it. The variable `conn` in `main` receives ownership through the return value. The program prints `localhost`.

## Sink Functions

A function receives ownership of a value through `move` and is responsible for its cleanup. The caller relinquishes all access.

```uranite
package testing

from uranite.io.console import puts

public class Resource:
    public function Resource( self ) -> Void:
        pass

public function consume( Resource res ) -> Void:
    puts( "consumed" )
    delete res

public function main() -> I32:
    Resource res = new Resource()
    consume( move res )
    puts( "after consume" )
    return 0
```

The caller creates a `Resource` and moves it into `consume`. The function takes ownership, uses it, and deletes it. The caller's variable is invalidated after the move. The program prints `consumed` followed by `after consume`.

## Ownership Chaining

A function takes ownership of one value, creates a new value from it, and returns the new value. The input is consumed and the output is transferred to the caller.

```uranite
package testing

from uranite.io.console import puts

public class Data:
    public I64 value

    public function Data( self, I64 value ) -> Void:
        self.value = value

public function transform( Data input ) -> Data:
    I64 doubled = input.value * 2
    delete input
    Data result = new Data( doubled )
    return result

public function main() -> I32:
    Data original = new Data( 5 )
    Data transformed = transform( move original )
    puts( transformed.value )
    delete transformed
    return 0
```

The function `transform` takes ownership of `input`, reads its value, deletes it, and returns a new `Data`. The caller moves `original` in and receives `transformed` back. The program prints `10`.

## Reassignment

Assigning a new value to a variable implicitly drops the previous value. No explicit `delete` is needed before the reassignment.

```uranite
package testing

from uranite.io.console import puts

public class Item:
    public String label

    public function Item( self, String label ) -> Void:
        self.label = label

public function main() -> I32:
    Item current = new Item( "alpha" )
    puts( current.label )
    current = new Item( "beta" )
    puts( current.label )
    delete current
    return 0
```

The variable `current` first holds an `Item` with label `alpha`. On reassignment, the previous instance is implicitly released. The final `delete` frees the last assigned value. The program prints `alpha` followed by `beta`.

## Conditional Ownership

When a branch replaces a value, reassignment within the branch implicitly drops the previous value.

```uranite
package testing

from uranite.io.console import puts

public class Handle:
    public String name

    public function Handle( self, String name ) -> Void:
        self.name = name

public function main() -> I32:
    I64 choice = 1
    Handle handle = new Handle( "default" )
    if choice == 1:
        handle = new Handle( "selected" )
    puts( handle.name )
    delete handle
    return 0
```

The variable `handle` starts with a default value. Inside the branch, the reassignment implicitly drops the previous instance and assigns a new one. The final `delete` frees whichever value `handle` holds at that point. The program prints `selected`.

## Loop-Scoped Ownership

Each loop iteration allocates and deallocates its own resources. No ownership leaks between iterations.

```uranite
package testing

from uranite.io.console import puts

public class Worker:
    public I64 index

    public function Worker( self, I64 index ) -> Void:
        self.index = index

public function main() -> I32:
    for I64 counter in 0..3:
        Worker worker = new Worker( counter )
        puts( worker.index )
        delete worker
    return 0
```

Each iteration creates a `Worker`, uses it, and deletes it. The variable `worker` is scoped to the loop body. The program prints `0`, `1`, `2`.

## Deferred Cleanup

The `defer delete` pattern schedules deallocation at scope exit. This combines a factory function with guaranteed cleanup regardless of the return path.

```uranite
package testing

from uranite.io.console import puts

public class Lock:
    public function Lock( self ) -> Void:
        puts( "acquired" )

public function withLock() -> Void:
    Lock lock = new Lock()
    defer delete lock
    puts( "working" )

public function main() -> I32:
    withLock()
    puts( "done" )
    return 0
```

The function `withLock` creates a `Lock` and immediately defers its deletion. The lock is cleaned up when the function returns, after all statements complete. The program prints `acquired`, `working`, `done`.
