# Memory Management

- [Table of Contents](#table-of-contents)

## Table of Contents

- [Memory Management](#memory-management)
  - [Table of Contents](#table-of-contents)
  - [Overview](#overview)
  - [Memory Type](#memory-type)
  - [Bulk Copy](#bulk-copy)
  - [Explicit Free](#explicit-free)
  - [Deferred Cleanup](#deferred-cleanup)
  - [Passing Memory to Functions](#passing-memory-to-functions)
  - [Arena Allocator](#arena-allocator)
  - [Arena Lifecycle](#arena-lifecycle)

## Overview

Uranite provides two intrinsic types for direct memory management: `Memory<T>` and `Arena<T>`. Both are native classes whose methods map directly to low-level allocation operations with no runtime overhead.

`Memory<T>` manages a contiguous block of typed elements with explicit allocation, indexed access, and deallocation. `Arena<T>` provides a bump-pointer pool allocator that sub-allocates from a single contiguous region and supports constant-time bulk reset.

Both types are imported from `uranite.memory.memory` and `uranite.memory.arena` respectively.

## Memory Type

`Memory<T>` allocates a contiguous block of typed elements. Elements are accessed by index through `get` and `set`. The owner is responsible for freeing the allocation.

```uranite
package testing

from uranite.io.console import puts
from uranite.memory.memory import Memory

public function main() -> I32:
    Memory<I64> buffer = new Memory<I64>( 4 )
    buffer.set( 0, 10 )
    buffer.set( 1, 20 )
    buffer.set( 2, 30 )
    buffer.set( 3, 40 )
    puts( buffer.get( 0 ) )
    puts( buffer.get( 1 ) )
    puts( buffer.get( 2 ) )
    puts( buffer.get( 3 ) )
    delete buffer
    return 0
```

The constructor `Memory<I64>( 4 )` allocates space for 4 elements. Elements are stored and retrieved by zero-based index. The program prints `10`, `20`, `30`, `40`.

## Bulk Copy

The `copyTo` method copies elements from one memory block to another. Both blocks must have sufficient capacity.

```uranite
package testing

from uranite.io.console import puts
from uranite.memory.memory import Memory

public function main() -> I32:
    Memory<I64> source = new Memory<I64>( 3 )
    source.set( 0, 100 )
    source.set( 1, 200 )
    source.set( 2, 300 )
    Memory<I64> destination = new Memory<I64>( 3 )
    source.copyTo( destination, 3 )
    puts( destination.get( 0 ) )
    puts( destination.get( 1 ) )
    puts( destination.get( 2 ) )
    delete source
    delete destination
    return 0
```

The call `source.copyTo( destination, 3 )` copies 3 elements from `source` into `destination`. The program prints `100`, `200`, `300`.

## Explicit Free

The `free` method releases the underlying memory block. After calling `free`, the `Memory` instance is invalid and must not be used.

```uranite
package testing

from uranite.io.console import puts
from uranite.memory.memory import Memory

public function main() -> I32:
    Memory<I64> data = new Memory<I64>( 2 )
    data.set( 0, 55 )
    data.set( 1, 77 )
    puts( data.get( 0 ) )
    puts( data.get( 1 ) )
    data.free()
    puts( "freed" )
    return 0
```

The `free` method releases the allocation without destroying the wrapper object. The `delete` statement on a `Memory` variable achieves the same effect. The program prints `55`, `77`, `freed`.

## Deferred Cleanup

Combining `defer delete` with `Memory<T>` guarantees the buffer is freed when the function returns, regardless of which return path is taken.

```uranite
package testing

from uranite.io.console import puts
from uranite.memory.memory import Memory

public function processBuffer() -> I64:
    Memory<I64> buffer = new Memory<I64>( 3 )
    defer delete buffer
    buffer.set( 0, 42 )
    buffer.set( 1, 84 )
    buffer.set( 2, 126 )
    I64 total = buffer.get( 0 ) + buffer.get( 1 ) + buffer.get( 2 )
    return total

public function main() -> I32:
    I64 result = processBuffer()
    puts( result )
    return 0
```

The `defer delete buffer` schedules deallocation at function exit. The return value is computed before the deferred delete executes. The program prints `252`.

## Passing Memory to Functions

`Memory<T>` can be passed as a function parameter. The function receives direct access to the underlying buffer.

```uranite
package testing

from uranite.io.console import puts
from uranite.memory.memory import Memory

public function sumBuffer( Memory<I64> buffer, I64 length ) -> I64:
    I64 total = 0
    for I64 index in 0..length:
        total = total + buffer.get( index )
    return total

public function main() -> I32:
    Memory<I64> data = new Memory<I64>( 3 )
    data.set( 0, 10 )
    data.set( 1, 20 )
    data.set( 2, 30 )
    I64 result = sumBuffer( data, 3 )
    puts( result )
    delete data
    return 0
```

The function `sumBuffer` iterates over the buffer using indexed access. The caller retains ownership and is responsible for cleanup. The program prints `60`.

## Arena Allocator

`Arena<T>` pre-allocates a contiguous memory region and sub-allocates individual elements by bumping an internal pointer. All allocations share one underlying block.

```uranite
package testing

from uranite.io.console import puts
from uranite.memory.arena import Arena

public class Node:
    public I64 value

    public function Node( self, I64 value ) -> Void:
        self.value = value

public function main() -> I32:
    Arena<Node> pool = new Arena<Node>( 10 )
    Node first = pool.alloc()
    first.value = 1
    Node second = pool.alloc()
    second.value = 2
    puts( first.value )
    puts( second.value )
    puts( pool.count() )
    pool.freeAll()
    pool.destroy()
    return 0
```

The arena `pool` is created with capacity for 10 elements. Each `alloc` call bumps the internal pointer and returns a slot. The `count` method reports 2 allocated slots. The program prints `1`, `2`, `2`.

## Arena Lifecycle

The arena lifecycle consists of three phases: allocate the pool, sub-allocate elements, and release. The `freeAll` method resets the bump pointer in constant time, logically freeing all allocations while retaining the underlying memory for reuse. The `destroy` method releases the entire block.

```uranite
package testing

from uranite.io.console import puts
from uranite.memory.arena import Arena

public class Entry:
    public I64 key

    public function Entry( self, I64 key ) -> Void:
        self.key = key

public function main() -> I32:
    Arena<Entry> pool = new Arena<Entry>( 8 )
    puts( pool.capacity() )
    Entry first = pool.alloc()
    first.key = 100
    Entry second = pool.alloc()
    second.key = 200
    Entry third = pool.alloc()
    third.key = 300
    puts( pool.count() )
    puts( first.key )
    puts( second.key )
    puts( third.key )
    pool.freeAll()
    puts( pool.count() )
    pool.destroy()
    return 0
```

The arena starts with capacity 8. After 3 allocations, `count` returns 3. After `freeAll`, `count` returns 0 — the bump pointer resets but the underlying block is retained. The `destroy` call releases all memory. The program prints `8`, `3`, `100`, `200`, `300`, `0`.
