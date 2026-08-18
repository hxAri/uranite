# Delete

- [Table of Contents](#table-of-contents)

## Table of Contents

- [Delete](#delete)
  - [Table of Contents](#table-of-contents)
  - [Overview](#overview)
  - [Deleting an Object](#deleting-an-object)
  - [Deleting Memory](#deleting-memory)
  - [Conditional Delete](#conditional-delete)
  - [Delete in a Function](#delete-in-a-function)
  - [Delete in a Loop](#delete-in-a-loop)

## Overview

The `delete` statement frees heap-allocated memory. It takes an expression that evaluates to a heap pointer and releases the underlying memory back to the allocator.

The general form is:

```
delete expression
```

Objects created with `new` and `Memory<T>` allocations are heap-allocated and must be freed with `delete` when they are no longer needed. After deleting a variable, the memory it pointed to is invalid and must not be accessed.

Uranite uses ownership semantics and does not have a garbage collector. The `delete` statement provides explicit manual control over when heap memory is released.

## Deleting an Object

An object created with `new` occupies heap memory. The `delete` statement frees that memory.

```uranite
package testing

from uranite.io.console import puts

public class Resource:
    public function Resource( self ) -> Void:
        pass

public function main() -> I32:
    Resource res = new Resource()
    delete res
    puts( "deleted" )
    return 0
```

The `Resource` object is allocated on the heap with `new`. The `delete` statement frees the memory. The program prints `deleted`.

## Deleting Memory

The `Memory<T>` type provides raw heap-allocated memory for a given element type. The `delete` statement frees the allocation.

```uranite
package testing

from uranite.io.console import puts
from uranite.memory.memory import Memory

public function main() -> I32:
    Memory<I64> buffer = new Memory<I64>( 10 )
    delete buffer
    puts( "memory freed" )
    return 0
```

A `Memory<I64>` buffer with capacity for 10 elements is allocated. The `delete` statement frees the buffer. The program prints `memory freed`.

## Conditional Delete

The `delete` statement can appear inside conditional branches, allowing memory to be freed only when certain conditions are met.

```uranite
package testing

from uranite.io.console import puts

public class Handle:
    public function Handle( self ) -> Void:
        pass

public function main() -> I32:
    Handle handle = new Handle()
    Boolean shouldDelete = True
    if shouldDelete:
        delete handle
        puts( "conditionally deleted" )
    return 0
```

The `Handle` is only deleted when `shouldDelete` is `True`. The program prints `conditionally deleted`.

## Delete in a Function

A function can accept a heap-allocated object and delete it, taking ownership of the deallocation responsibility.

```uranite
package testing

from uranite.io.console import puts

public class Connection:
    public function Connection( self ) -> Void:
        pass

public function closeConnection( Connection conn ) -> Void:
    delete conn
    puts( "connection closed" )

public function main() -> I32:
    Connection conn = new Connection()
    closeConnection( conn )
    return 0
```

The `closeConnection` function receives the `Connection` object and deletes it. The program prints `connection closed`.

## Delete in a Loop

When objects are allocated inside a loop, each iteration can delete the object to prevent memory from accumulating.

```uranite
package testing

from uranite.io.console import puts

public class Item:
    public function Item( self ) -> Void:
        pass

public function main() -> I32:
    I64 count = 0
    while count < 3:
        Item item = new Item()
        delete item
        count = count + 1
    puts( "loop cleanup done" )
    return 0
```

Each iteration allocates a new `Item` and immediately deletes it. No memory accumulates across iterations. The program prints `loop cleanup done`.
