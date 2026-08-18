# Droper Interface

- [Table of Contents](#table-of-contents)

## Table of Contents

- [Droper Interface](#droper-interface)
  - [Table of Contents](#table-of-contents)
  - [Overview](#overview)
  - [Implementing Droper](#implementing-droper)
  - [Automatic Cleanup on Delete](#automatic-cleanup-on-delete)
  - [Scope-Exit Cleanup](#scope-exit-cleanup)
  - [Explicit Delete Skips Scope Cleanup](#explicit-delete-skips-scope-cleanup)
  - [Ownership Transfer with Droper](#ownership-transfer-with-droper)
  - [Sink Functions](#sink-functions)

## Overview

The `Droper` interface defines a `drop` method for custom resource cleanup. Types that hold external resources such as file descriptors, network connections, locks, or memory pools implement `Droper` to release those resources when the object is freed.

The compiler automatically calls `drop` in two situations:

- When `delete` is used on an object that implements `Droper`, the compiler calls `drop` before freeing the memory.
- When a function returns, all live `Droper` objects constructed or received in that scope are automatically cleaned up in reverse construction order.

Objects that have already been explicitly deleted are tracked and skipped during scope-exit cleanup. Objects that have been moved out are also skipped.

The `Droper` interface is imported from `uranite.memory.droper`.

## Implementing Droper

A class implements `Droper` by providing a `drop` method. The method takes only `self` and returns `Void`.

```uranite
package testing

from uranite.io.console import puts
from uranite.memory.droper import Droper

public class FileHandle implements Droper:
    public I64 descriptor

    public function FileHandle( self, I64 descriptor ) -> Void:
        self.descriptor = descriptor

    public function drop( self ) -> Void:
        puts( "closing file" )

public function main() -> I32:
    FileHandle handle = new FileHandle( 42 )
    puts( handle.descriptor )
    delete handle
    return 0
```

The `delete handle` statement automatically calls `drop` before freeing the memory. The program prints `42`, `closing file`.

## Automatic Cleanup on Delete

When `delete` is used on an object whose class implements `Droper`, the compiler inserts a call to `drop` before the memory is freed. No manual `drop` call is needed.

```uranite
package testing

from uranite.io.console import puts
from uranite.memory.droper import Droper

public class Pool implements Droper:
    public I64 size

    public function Pool( self, I64 size ) -> Void:
        self.size = size

    public function drop( self ) -> Void:
        puts( "pool released" )

public function main() -> I32:
    Pool pool = new Pool( 64 )
    puts( pool.size )
    delete pool
    puts( "after delete" )
    return 0
```

The `delete pool` statement calls `drop` first, then frees the memory. The program prints `64`, `pool released`, `after delete`.

## Scope-Exit Cleanup

When a function returns, all live `Droper` objects in that scope are automatically cleaned up in reverse construction order. No explicit `delete` is required.

```uranite
package testing

from uranite.io.console import puts
from uranite.memory.droper import Droper

public class Guard implements Droper:
    public String name

    public function Guard( self, String name ) -> Void:
        self.name = name

    public function drop( self ) -> Void:
        puts( self.name )

public function work() -> Void:
    Guard first = new Guard( "guard-a" )
    Guard second = new Guard( "guard-b" )
    puts( "working" )

public function main() -> I32:
    work()
    puts( "done" )
    return 0
```

When `work` returns, both guards are cleaned up in reverse order: `second` is dropped and freed before `first`. The program prints `working`, `guard-b`, `guard-a`, `done`.

## Explicit Delete Skips Scope Cleanup

When an object is explicitly deleted with `delete`, the compiler marks it as no longer alive. Scope-exit cleanup skips objects that have already been deleted.

```uranite
package testing

from uranite.io.console import puts
from uranite.memory.droper import Droper

public class Resource implements Droper:
    public String label

    public function Resource( self, String label ) -> Void:
        self.label = label

    public function drop( self ) -> Void:
        puts( self.label )

public function work() -> Void:
    Resource first = new Resource( "alpha" )
    Resource second = new Resource( "beta" )
    delete first
    puts( "middle" )

public function main() -> I32:
    work()
    puts( "done" )
    return 0
```

The variable `first` is explicitly deleted, which calls `drop` and frees it. When `work` returns, scope cleanup only processes `second` because `first` is already dead. The program prints `alpha`, `middle`, `beta`, `done`.

## Ownership Transfer with Droper

When a `Droper` object is moved to another function, the source variable is marked as dead. The receiving function's scope becomes responsible for cleanup. The caller's scope-exit cleanup skips the moved variable.

```uranite
package testing

from uranite.io.console import puts
from uranite.memory.droper import Droper

public class Session implements Droper:
    public I64 identifier

    public function Session( self, I64 identifier ) -> Void:
        self.identifier = identifier

    public function drop( self ) -> Void:
        puts( "session closed" )

public function useSession( Session session ) -> Void:
    puts( session.identifier )
    puts( "used" )

public function main() -> I32:
    Session session = new Session( 7 )
    useSession( move session )
    puts( "after use" )
    return 0
```

Ownership transfers from `main` to `useSession` via `move`. When `useSession` returns, the parameter is cleaned up automatically — `drop` is called, then memory is freed. The caller's scope skips the moved variable. The program prints `7`, `used`, `session closed`, `after use`.

## Sink Functions

A sink function receives ownership of a `Droper` object and explicitly deletes it. The explicit `delete` calls `drop` and marks the parameter as dead, so scope-exit cleanup skips it.

```uranite
package testing

from uranite.io.console import puts
from uranite.memory.droper import Droper

public class Session implements Droper:
    public I64 identifier

    public function Session( self, I64 identifier ) -> Void:
        self.identifier = identifier

    public function drop( self ) -> Void:
        puts( "session closed" )

public function closeSession( Session session ) -> Void:
    puts( session.identifier )
    delete session

public function main() -> I32:
    Session session = new Session( 7 )
    closeSession( move session )
    puts( "after close" )
    return 0
```

The function `closeSession` receives ownership via `move` and deletes the session explicitly. The `delete` calls `drop` once, frees the memory, and marks it dead. No double-drop occurs. The program prints `7`, `session closed`, `after close`.
