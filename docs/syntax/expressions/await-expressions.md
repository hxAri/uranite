# Await Expressions

The `await` keyword suspends execution until an asynchronous operation completes, extracting the inner value from a `Future<T>`. It is only valid inside `async function` bodies. The expression `await asyncCall()` produces a value of type `T` from a `Future<T>`.

---

## Table of Contents

- [Await Expressions](#await-expressions)
  - [Table of Contents](#table-of-contents)
  - [Syntax](#syntax)
  - [Async Functions](#async-functions)
    - [Declaring Async Functions](#declaring-async-functions)
    - [Return Type](#return-type)
    - [Async Main](#async-main)
  - [Await](#await)
    - [Basic Usage](#basic-usage)
    - [Chaining Await Calls](#chaining-await-calls)
    - [Multiple Independent Awaits](#multiple-independent-awaits)
    - [Await Context Requirement](#await-context-requirement)
    - [Type Unwrapping](#type-unwrapping)
  - [Examples](#examples)
    - [Basic Async and Await](#basic-async-and-await)
    - [Chained Async Operations](#chained-async-operations)
    - [Combining Multiple Results](#combining-multiple-results)
    - [Mixed Return Types](#mixed-return-types)

---

## Syntax

```
await asyncExpression
```

The `await` keyword precedes an expression that produces a `Future<T>`. Execution suspends until the future resolves, then the `await` expression evaluates to the resolved value of type `T`.

---

## Async Functions

### Declaring Async Functions

The `async` keyword before `function` declares an asynchronous function:

```uranite
public async function fetchValue() -> Future<I64>:
    return 42
```

An async function runs as a concurrent task. When called, it returns a `Future<T>` immediately. The actual computation runs asynchronously.

---

### Return Type

Async functions must declare their return type as `Future<T>` where `T` is the type of the value being returned:

```uranite
public async function getName() -> Future<String>:
    return "Alice"

public async function getCount() -> Future<I64>:
    return 10
```

The `return` statement inside the async function body returns a value of type `T`, not `Future<T>`. The wrapping in `Future` is handled automatically.

---

### Async Main

When a program uses async functions, the `main` function must also be declared `async` with a `Future<I32>` return type:

```uranite
public async function main() -> Future<I32>:
    I64 result = await fetchValue()
    return 0
```

The async runtime is initialized automatically when `main` is async.

---

## Await

### Basic Usage

Use `await` to get the resolved value from a `Future<T>`:

```uranite
public async function main() -> Future<I32>:
    I64 result = await fetchValue()
    puts( result.toString() )
    return 0
```

The variable `result` receives the `I64` value after the future resolves. Without `await`, calling an async function returns a `Future<I64>`, not the `I64` value itself.

---

### Chaining Await Calls

The result of one `await` can be passed as an argument to another async function:

```uranite
I64 first = await stepOne()
I64 second = await stepTwo( first )
```

Each `await` suspends until its future resolves before proceeding to the next statement. The second call uses the result of the first.

---

### Multiple Independent Awaits

Multiple async functions can be awaited independently:

```uranite
I64 width = await getWidth()
I64 height = await getHeight()
I64 area = width * height
```

Each `await` resolves its future. After both complete, the results are combined using regular synchronous operations.

---

### Await Context Requirement

`await` is only valid inside `async function` bodies. Using `await` in a non-async function produces a compiler error:

```
"await" can only be used inside an async function
```

---

### Type Unwrapping

`await` unwraps the `Future<T>` type:

| Expression Type | Await Result Type |
|---|---|
| `Future<I64>` | `I64` |
| `Future<String>` | `String` |
| `Future<I32>` | `I32` |
| `Future<Boolean>` | `Boolean` |

If the operand is not a `Future<T>`, the compiler emits an error.

---

## Examples

### Basic Async and Await

```uranite
package testing

from uranite.io.console import puts

public async function fetchValue() -> Future<I64>:
    return 42

public async function main() -> Future<I32>:
    I64 result = await fetchValue()
    puts( result.toString() )
    return 0
```

Output:

```
42
```

The `fetchValue` function is declared `async` with return type `Future<I64>`. Inside the body, `return 42` returns the `I64` value. In `main`, `await fetchValue()` suspends until the future resolves, then stores the resulting `42` in `result`.

---

### Chained Async Operations

```uranite
package testing

from uranite.io.console import puts

public async function stepOne() -> Future<I64>:
    return 10

public async function stepTwo( I64 input ) -> Future<I64>:
    return input * 2

public async function main() -> Future<I32>:
    I64 first = await stepOne()
    I64 second = await stepTwo( first )
    puts( first.toString() )
    puts( second.toString() )
    return 0
```

Output:

```
10
20
```

Two async functions are chained sequentially. `stepOne` returns `10`. That result is passed to `stepTwo`, which doubles it to `20`. Each `await` completes before the next statement executes, ensuring `first` has a value before `stepTwo` is called.

---

### Combining Multiple Results

```uranite
package testing

from uranite.io.console import puts

public async function getWidth() -> Future<I64>:
    return 5

public async function getHeight() -> Future<I64>:
    return 3

public async function main() -> Future<I32>:
    I64 width = await getWidth()
    I64 height = await getHeight()
    I64 area = width * height
    puts( area.toString() )
    return 0
```

Output:

```
15
```

Two independent async operations are awaited separately. After both resolve, their results are combined with regular arithmetic. The `await` expressions extract `I64` values from the `Future<I64>` returns, enabling normal operations on the results.

---

### Mixed Return Types

```uranite
package testing

from uranite.io.console import puts

public async function getMessage() -> Future<String>:
    return "Hello from async"

public async function getCount() -> Future<I64>:
    return 7

public async function main() -> Future<I32>:
    String message = await getMessage()
    puts( message )

    I64 count = await getCount()
    puts( count.toString() )
    return 0
```

Output:

```
Hello from async
7
```

Async functions can return different types. `getMessage` returns `Future<String>` and `getCount` returns `Future<I64>`. Each `await` unwraps the appropriate type — `String` from `Future<String>` and `I64` from `Future<I64>`.
