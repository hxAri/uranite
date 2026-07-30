# Standard Library Guide

Practical usage patterns and best practices for the Uranite standard library. This is not an API reference (use `uranite-doc` to generate that). This guide shows *how* to write idiomatic Uranite using the core modules.

---

## Table of Contents

- [Module Overview](#module-overview)
- [Collections](#collections)
  - [ArrayList: The Default Sequence](#arraylist-the-default-sequence)
  - [HashMap: Key-Value Storage](#hashmap-key-value-storage)
  - [HashSet: Unique Elements](#hashset-unique-elements)
  - [Choosing the Right Collection](#choosing-the-right-collection)
  - [Iteration Patterns](#iteration-patterns)
  - [Functional Transforms](#functional-transforms)
- [Strings](#strings)
  - [Building Strings](#building-strings)
  - [Formatting](#formatting)
  - [Common Operations](#common-operations)
- [Memory Management](#memory-management)
  - [Memory Intrinsic](#memory-intrinsic)
  - [Arena Allocator](#arena-allocator)
  - [Ownership and Lifetimes](#ownership-and-lifetimes)
  - [Common Pitfalls](#common-pitfalls)
- [I/O](#io)
  - [Console Output](#console-output)
  - [Console Input](#console-input)
  - [File Operations](#file-operations)
- [Error Handling](#error-handling)
  - [Exception Hierarchy](#exception-hierarchy)
  - [Custom Exceptions](#custom-exceptions)
  - [Defensive Patterns](#defensive-patterns)
- [Concurrency](#concurrency)
  - [Async/Await](#asyncawait)
  - [Threading](#threading)
  - [Synchronization Primitives](#synchronization-primitives)
- [Testing](#testing)
  - [Writing Tests](#writing-tests)
  - [Assertions](#assertions)
  - [Running Tests](#running-tests)

---

## Module Overview

| Module | Purpose |
|---|---|
| `uranite.collection` | ArrayList, HashMap, HashSet, Tuple, Generator, Pair, Sequence interfaces |
| `uranite.language` | OOP wrappers for primitives (String, Int, Boolean, Float, etc.) |
| `uranite.memory` | Memory intrinsic, Arena allocator, Slab allocator, Object base |
| `uranite.io` | Console I/O, file reading/writing, buffered streams, filesystem ops |
| `uranite.errors` | Exception, Error, Warning, ValueError, IndexError, StateError, etc. |
| `uranite.string` | String builder, string formatting utilities |
| `uranite.iterators` | Iterable and Iterator interfaces |
| `uranite.async` | Async runtime, epoll, scheduler, tasks, timers |
| `uranite.threading` | Thread, Mutex, RwLock, Barrier, Atomic, Channel, ThreadPool |
| `uranite.coroutine` | Lightweight coroutines and fiber-based scheduling |
| `uranite.testing` | TestRunner, assertions, test case structure |
| `uranite.math` | Mathematical functions and constants |
| `uranite.datetime` | Date, time, and monotonic clock |
| `uranite.crypto` | Cryptographic primitives |
| `uranite.net` | Networking primitives |
| `uranite.encoding` | JSON, Base64, and other encoding formats |
| `uranite.regexp` | Regular expressions |
| `uranite.os` | OS-level primitives and environment access |
| `uranite.kernel` | Raw syscalls and hardware access |
| `uranite.ffi` | Foreign function interface for C interop |

---

## Collections

### ArrayList: The Default Sequence

`ArrayList<E>` is the workhorse collection. It is a resizable, contiguous array with amortized O(1) append and O(1) indexed access.

**Construction:**

```uranite
from uranite.collection import ArrayList

# Empty with default capacity (16)
ArrayList<String> names = new ArrayList<>()

# With explicit initial capacity
ArrayList<I64> buffer = new ArrayList<I64>( 1024 )

# From a collection literal
ArrayList<I64> numbers = [ 10, 20, 30 ]

# From an existing sequence
ArrayList<String> copy = new ArrayList<String>( existingSequence )

# With explicit capacity and initial elements
ArrayList<I64> preallocated = new ArrayList<I64>( 256, sourceSequence )
```

**Essential operations:**

```uranite
ArrayList<String> tags = new ArrayList<>()

# Add elements
tags.add( "urgent" )
tags.add( "review" )
tags.append( "deploy" )      # Returns self for chaining

# Access
String first = tags.get( 0 )
String head = tags.first
String tail = tags.last
Boolean hasIndex = tags.hasIndex( 5 )

# Search
Boolean found = tags.contains( "urgent" )
Int position = tags.indexOf( "review" )

# Remove
tags.remove( "deploy" )       # By value
tags.remove( 0 )              # By index

# Size
Int count = tags.size()
Boolean empty = tags.empty
```

**Best practice:** Prefer `new ArrayList<I64>( expectedSize )` when the approximate size is known. This avoids repeated resizing. The default capacity is 16; if you are building a list of thousands of elements, preallocating saves significant reallocation.

### HashMap: Key-Value Storage

`HashMap<K, V>` is an open-addressing hash table with linear probing.

**Construction:**

```uranite
from uranite.collection import HashMap, Pair

# Empty
HashMap<String, I64> scores = new HashMap<>()

# From literal
HashMap<String, I64> config = { "port": 8080, "workers": 4 }

# From sequence of pairs
ArrayList<Pair<String, I64>> entries = new ArrayList<>()
entries.add( new Pair<>( "alpha", 1 ) )
HashMap<String, I64> fromPairs = new HashMap<String, I64>( entries )
```

**Essential operations:**

```uranite
HashMap<String, I64> ages = new HashMap<>()

# Insert and update
ages.put( "Alice", 30 )
ages.putIfAbsent( "Bob", 25 )         # Only inserts if key missing
ages.replace( "Alice", 31 )           # Only updates if key exists

# Retrieve
I64 age = ages.get( "Alice" )
I64 safe = ages.getOrDefault( "Charlie", 0 )

# Check membership
Boolean has = ages.containsKey( "Alice" )
Boolean hasVal = ages.containsValue( 30 )

# Remove
ages.remove( "Bob" )
I64 popped = ages.pop( "Alice" )      # Remove and return value

# Bulk operations
ages.putAll( otherMap )
ages.merge( otherMap )

# Views
Set<String> allKeys = ages.keys
Sequence<I64> allValues = ages.values
Set<Pair<String, I64>> allEntries = ages.entries
```

**Common pitfall:** `get()` throws if the key is missing. Use `containsKey()` first or `getOrDefault()` for safe access.

### HashSet: Unique Elements

`HashSet<E>` stores unique values backed by the same open-addressing strategy as HashMap.

```uranite
from uranite.collection import HashSet

# From literal
HashSet<String> roles = { "admin", "editor", "viewer" }

# Operations
roles.add( "moderator" )
Boolean isAdmin = roles.contains( "admin" )
roles.remove( "viewer" )
Int count = roles.size()
```

### Choosing the Right Collection

| Need | Collection |
|---|---|
| Ordered list with indexed access | `ArrayList<E>` |
| Key-value lookup | `HashMap<K, V>` |
| Unique unordered elements | `HashSet<E>` |
| Fixed-size immutable group | `Tuple<E>` |
| Lazy value production | `Generator<E>` |
| Key-value pair | `Pair<K, V>` |

### Iteration Patterns

All collections implement `Iterable<E>` and work with `for-in`:

```uranite
# ArrayList iteration
ArrayList<String> items = [ "one", "two", "three" ]
for String item in items:
    puts( item )

# HashMap iteration (key-value destructuring)
HashMap<String, I64> scores = { "Alice": 95, "Bob": 87 }
for String name, I64 score in scores:
    puts( name, ": ", score )

# HashSet iteration
HashSet<I64> ids = { 1, 2, 3 }
for I64 identifier in ids:
    puts( identifier )
```

### Functional Transforms

`ArrayList` provides a functional API for transforming data:

```uranite
ArrayList<I64> numbers = [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ]

# Filter: keep only elements matching a predicate
ArrayList<I64> evens = numbers.filter( lambda I64 number: number % 2 == 0 )

# Map: transform each element
ArrayList<I64> doubled = numbers.map( lambda I64 number: number * 2 )

# ForEach: side-effect per element
numbers.forEach( lambda I64 number: puts( number ) )

# Sorted: with a custom comparator
ArrayList<I64> sorted = numbers.sorted( lambda I64 first, I64 second: first - second )

# Slice: extract a subrange
ArrayList<I64> middle = numbers.slice( 2, 5 )

# Any/All predicates
Boolean hasLarge = numbers.any( lambda Boolean condition: condition > 100 )
```

---

## Strings

### Building Strings

String concatenation uses the `+` operator:

```uranite
String greeting = "Hello" + ", " + "World"
```

For building strings from mixed types, `puts` handles variadic arguments with automatic conversion:

```uranite
I64 count = 42
puts( "Found ", count, " items" )
```

### Formatting

Use the `.format()` method with `{}` placeholders for positional substitution:

```uranite
F64 elapsed = 3.14
String message = "Completed in {} seconds".format( elapsed )
puts( message )
```

### Common Operations

```uranite
String text = "Hello, Uranite!"

Boolean starts = text.startsWith( "Hello" )
Boolean ends = text.endsWith( "!" )
Boolean has = text.contains( "Uranite" )
Int len = text.length
Boolean blank = text.isEmpty
```

---

## Memory Management

### Memory Intrinsic

`Memory<T>` is a compiler intrinsic that maps directly to LLVM memory operations. It is the foundation of all collections.

```uranite
from uranite.memory import Memory

# Allocate space for 100 I64 values
Memory<I64> buffer = new Memory<I64>( 100 )

# Read and write
buffer.set( 0, 42 )
I64 value = buffer.get( 0 )

# Free when done
buffer.free()
```

`Memory<T>` performs no bounds checking. It is raw, unmanaged memory. Prefer `ArrayList<E>` for general use.

### Arena Allocator

For batch allocations with a single deallocation point:

```uranite
from uranite.memory import Arena

Arena allocator = new Arena( 4096 )
# ... allocate many small objects from the arena
# When done, freeing the arena releases everything at once
```

### Ownership and Lifetimes

Uranite enforces ownership at compile time:

1. Each value has exactly one owner.
2. Assignment transfers ownership (move semantics).
3. After a move, the source variable is invalidated.
4. The borrow checker verifies these rules before codegen.

```uranite
ArrayList<I64> original = [ 1, 2, 3 ]
ArrayList<I64> moved = original
# 'original' is now invalid — using it is a compile-time error
```

### Common Pitfalls

**Use-after-free:** Calling methods on a `delete`d object produces undefined behavior. The compiler catches obvious cases, but dynamically aliased pointers can slip through.

**Double free:** Deleting the same object twice corrupts the heap. Let the ownership system handle deallocation when possible.

**Forgetting to free Memory:** `Memory<T>` does not auto-free. Every `new Memory<T>(n)` needs a corresponding `.free()`. Collections handle this internally; only worry about it when using `Memory<T>` directly.

---

## I/O

### Console Output

```uranite
from uranite.io import puts

# Print with automatic spacing and newline
puts( "Hello", "World" )           # "Hello World\n"

# Print multiple types (variadic, auto-converted)
puts( "Count:", 42, "Ratio:", 3.14 )
```

Available output functions:

| Function | Behavior |
|---|---|
| `puts(...)` | Print with space separator and trailing newline |
| `putsln(...)` | Same as `puts` |
| `putserr(...)` | Print to stderr |
| `putserrln(...)` | Print to stderr with newline |

### Console Input

```uranite
from uranite.io import input

String name = input( "Enter your name: " )
puts( "Hello, ", name )
```

For password input (no echo):

```uranite
from uranite.io import getpass

String password = getpass( "Password: " )
```

### File Operations

```uranite
from uranite.io import File

# Reading
File reader = new File( "data.txt", "r" )
String content = reader.read()
reader.close()

# Writing
File writer = new File( "output.txt", "w" )
writer.write( "Hello, file!" )
writer.close()
```

Use buffered readers/writers for large files:

```uranite
from uranite.io import BufferedReader, File

BufferedReader reader = new BufferedReader( new File( "large.txt", "r" ) )
```

---

## Error Handling

### Exception Hierarchy

```
Throwable
├── Exception
│   ├── ValueError
│   ├── IndexError
│   ├── StateError
│   ├── TypeError
│   └── (user-defined exceptions)
├── Error
│   ├── ArithmeticError
│   │   ├── ZeroDivisionError
│   │   ├── OverflowError
│   │   └── UnderflowError
│   └── (system-level errors)
└── Warning
```

### Custom Exceptions

```uranite
from uranite.errors.exception import Exception

class DatabaseError extends Exception:
    public function DatabaseError( self, String message, I64 code, ?Exception cause ) -> Void:
        parent( message, code, cause )
```

### Defensive Patterns

**Guard clauses with early return:**

```uranite
public function processItem( ?Item item ) -> Void:
    if item is None:
        return
    # Safe to use item here
```

**Defer for cleanup:**

```uranite
public function withConnection() -> Void:
    Connection conn = openConnection()
    defer conn.close()

    # If anything throws, conn.close() still runs
    conn.execute( "SELECT 1" )
```

**Typed exception catching:**

```uranite
try:
    processData()
except IndexError as error:
    puts( "Index out of bounds: ", error.message )
except ValueError as error:
    puts( "Invalid value: ", error.message )
except Exception as error:
    puts( "Unexpected: ", error.message )
```

---

## Concurrency

### Async/Await

Uranite has a built-in async runtime using raw Linux syscalls (epoll) with no C runtime dependency:

```uranite
async function fetchAndProcess() -> String:
    String data = await fetchRemote()
    return data

async function main() -> I32:
    String result = await fetchAndProcess()
    puts( result )
    return 0
```

The async runtime is auto-imported. `async` functions return `Future<T>`. `await` is only valid inside `async` functions.

### Threading

```uranite
from uranite.threading import Thread

function worker() -> Void:
    puts( "Running in thread" )

Thread thread = new Thread( worker )
thread.start()
thread.join()
```

### Synchronization Primitives

| Primitive | Module | Purpose |
|---|---|---|
| `Mutex` | `uranite.threading` | Mutual exclusion lock |
| `RwLock` | `uranite.threading` | Read-write lock (multiple readers, exclusive writer) |
| `Barrier` | `uranite.threading` | Synchronization point for multiple threads |
| `Atomic` | `uranite.threading` | Lock-free atomic operations |
| `Channel` | `uranite.threading` | Message-passing between threads |
| `CondVar` | `uranite.threading` | Condition variable for thread signaling |

```uranite
from uranite.threading import Mutex

Mutex lock = new Mutex()
lock.lock()
# Critical section
lock.unlock()
```

---

## Testing

### Writing Tests

Use the testing module to write structured unit tests:

```uranite
from uranite.testing import assertTrue, assertEqualI64

public function testAddition() -> Void:
    I64 result = 2 + 2
    assertEqualI64( result, 4, "basic addition" )
    assertTrue( result > 0, "result should be positive" )
```

### Assertions

| Function | Description |
|---|---|
| `assertTrue( condition, message )` | Assert a boolean condition is True |
| `assertFalse( condition, message )` | Assert a boolean condition is False |
| `assertEqualI64( actual, expected, message )` | Assert two I64 values are equal |
| `assertEqualString( actual, expected, message )` | Assert two strings are equal |

All assertions raise `AssertionError` on failure with the provided message.

### Running Tests

Structure test functions in a `.urn` file with a `main` that calls them:

```uranite
package myproject.tests

from uranite.testing import assertTrue
from uranite.io import puts

function testFeature() -> Void:
    assertTrue( 1 + 1 == 2, "math works" )
    puts( "testFeature passed" )

public function main() -> I32:
    testFeature()
    puts( "All tests passed" )
    return 0
```

Compile and run:

```bash
uranite tests/test_feature.urn -r
```

For the compiler's own test suite (C++ unit tests):

```bash
./build/uranite-tests
./build/uranite-tests --gtest_filter="ParserTest.*"
```
