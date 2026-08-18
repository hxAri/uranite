# Droper Interface

- [Table of Contents](#table-of-contents)

## Table of Contents

- [Droper Interface](#droper-interface)
  - [Table of Contents](#table-of-contents)
  - [Overview](#overview)
  - [Implementing Droper](#implementing-droper)
  - [Manual Cleanup Pattern](#manual-cleanup-pattern)
  - [Deferred Cleanup](#deferred-cleanup)
  - [Multiple Resources](#multiple-resources)
  - [Ownership Transfer with Droper](#ownership-transfer-with-droper)

## Overview

The `Droper` interface defines a `drop` method for custom resource cleanup. Types that hold external resources such as file descriptors, network connections, locks, or memory pools implement `Droper` to release those resources before deallocation.

The `drop` method is not called automatically by `delete`. The `delete` statement frees the object's memory but does not invoke `drop`. Cleanup logic must be called explicitly, either directly or through `defer`.

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
    handle.drop()
    delete handle
    return 0
```

The `drop` method is called explicitly before `delete`. The `delete` statement frees the memory. The program prints `42`, `closing file`.

## Manual Cleanup Pattern

The standard cleanup sequence for a `Droper` type is: call `drop` to release resources, then call `delete` to free memory. Calling `drop` after `delete` is undefined behavior because the object's memory has been freed.

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
    pool.drop()
    delete pool
    puts( "after delete" )
    return 0
```

The `drop` method runs cleanup logic while the object is still valid. The subsequent `delete` frees the memory. The program prints `64`, `pool released`, `after delete`.

## Deferred Cleanup

The `defer` statement automates the cleanup sequence. Deferred statements execute in LIFO order, so `defer delete` must appear before `defer drop` to ensure `drop` runs first.

```uranite
package testing

from uranite.io.console import puts
from uranite.memory.droper import Droper

public class Connection implements Droper:
    public String host

    public function Connection( self, String host ) -> Void:
        self.host = host

    public function drop( self ) -> Void:
        puts( "disconnecting" )

public function useConnection() -> Void:
    Connection conn = new Connection( "server" )
    defer delete conn
    defer conn.drop()
    puts( conn.host )
    puts( "working" )

public function main() -> I32:
    useConnection()
    puts( "done" )
    return 0
```

The `defer delete conn` is registered first and runs last. The `defer conn.drop()` is registered second and runs first. This guarantees `drop` executes while the object is still valid, followed by memory deallocation. The program prints `server`, `working`, `disconnecting`, `done`.

## Multiple Resources

When multiple `Droper` objects are active, each must be cleaned up independently. Release resources in reverse order of acquisition.

```uranite
package testing

from uranite.io.console import puts
from uranite.memory.droper import Droper

public class Lock implements Droper:
    public String name

    public function Lock( self, String name ) -> Void:
        self.name = name
        puts( name )

    public function drop( self ) -> Void:
        puts( self.name )

public function main() -> I32:
    Lock first = new Lock( "lock-a" )
    Lock second = new Lock( "lock-b" )
    puts( "working" )
    second.drop()
    delete second
    first.drop()
    delete first
    puts( "done" )
    return 0
```

Two locks are acquired in order. They are released in reverse: `second` is dropped and deleted before `first`. The program prints `lock-a`, `lock-b`, `working`, `lock-b`, `lock-a`, `done`.

## Ownership Transfer with Droper

When a `Droper` object is moved to another function, the receiving function becomes responsible for both calling `drop` and freeing the memory.

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
    session.drop()
    delete session

public function main() -> I32:
    Session session = new Session( 7 )
    closeSession( move session )
    puts( "after close" )
    return 0
```

Ownership of the `Session` transfers to `closeSession` via `move`. The function calls `drop` to release resources, then `delete` to free memory. The caller's variable is invalidated after the move. The program prints `7`, `session closed`, `after close`.
