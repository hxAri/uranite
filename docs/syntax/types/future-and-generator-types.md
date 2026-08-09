# Future and Generator Types

---

## Table of Contents

- [Future and Generator Types](#future-and-generator-types)
  - [Table of Contents](#table-of-contents)
  - [Overview](#overview)
  - [Future Types](#future-types)
    - [Declaring Async Functions](#declaring-async-functions)
    - [The await Keyword](#the-await-keyword)
    - [Async Main](#async-main)
    - [Async Functions with Parameters](#async-functions-with-parameters)
    - [Chaining Async Calls](#chaining-async-calls)
    - [Nested Await](#nested-await)
    - [Async Methods on Classes](#async-methods-on-classes)
    - [Async with Different Return Types](#async-with-different-return-types)
    - [Calling Sync Functions from Async](#calling-sync-functions-from-async)
    - [Async Error Handling](#async-error-handling)
  - [Generator Types](#generator-types)
    - [Declaring Generator Functions](#declaring-generator-functions)
    - [Iterating Over Generators](#iterating-over-generators)
    - [Generator with Parameters](#generator-with-parameters)
    - [Multiple Sequential Yields](#multiple-sequential-yields)
    - [Infinite Generators](#infinite-generators)
    - [Conditional Yield](#conditional-yield)
    - [Generator with Expressions in Yield](#generator-with-expressions-in-yield)
    - [String Generators](#string-generators)
    - [Multiple Generators](#multiple-generators)
  - [Comparison](#comparison)
  - [Method Reference](#method-reference)
    - [Future Syntax](#future-syntax)
    - [Generator Syntax](#generator-syntax)
  - [Examples](#examples)
    - [Async Computation Pipeline](#async-computation-pipeline)
    - [Fibonacci Generator](#fibonacci-generator)
    - [Async Class Integration](#async-class-integration)
    - [Number Sequence Generators](#number-sequence-generators)

---

## Overview

`Future<T>` and `Generator<T>` represent deferred computation. A `Future<T>` wraps a value that becomes available after asynchronous execution completes. A `Generator<T>` produces a lazy sequence of values one at a time using `yield`.

Async functions use the `async` keyword in their declaration and `await` to extract values from futures. Generator functions use `yield` to produce values and are consumed through `for-in` loops.

The async runtime is automatically imported when a program contains async functions.

---

## Future Types

### Declaring Async Functions

An async function starts with `async function`, declares its return type as `Future<T>`, and returns values of type `T` in its body.

```uranite
async function fetchValue() -> Future<I64>:
    return 42
```

The `async` keyword must appear before `function`. The return type must be `Future<T>` where `T` is the type of value the function produces. Inside the body, `return` provides a value of type `T`, not `Future<T>`.

### The await Keyword

The `await` keyword extracts the inner value from a `Future<T>`, producing a value of type `T`. It can only be used inside an `async` function.

```uranite
from uranite.io.console import puts

async function fetchValue() -> Future<I64>:
    return 42

public async function main() -> Future<I32>:
    I64 value = await fetchValue()
    puts( value.toString() )
    return 0
```

Output:

```
42
```

The expression `await fetchValue()` calls the async function and extracts the `I64` value from the returned `Future<I64>`.

### Async Main

When a program uses async functions, `main` must also be declared as async with return type `Future<I32>`.

```uranite
public async function main() -> Future<I32>:
    return 0
```

The async runtime is automatically initialized when the program contains async functions.

### Async Functions with Parameters

Async functions accept parameters like regular functions.

```uranite
from uranite.io.console import puts

async function multiply( I64 first, I64 second ) -> Future<I64>:
    return first * second

public async function main() -> Future<I32>:
    I64 result = await multiply( 6, 7 )
    puts( result.toString() )
    return 0
```

Output:

```
42
```

### Chaining Async Calls

Multiple async calls can be chained by storing each result in a variable and passing it to the next call.

```uranite
from uranite.io.console import puts

async function fetchValue() -> Future<I64>:
    return 42

async function doubleAsync( I64 value ) -> Future<I64>:
    return value * 2

async function pipeline( I64 input ) -> Future<I64>:
    I64 fetched = await fetchValue()
    I64 doubled = await doubleAsync( fetched )
    return doubled + input

public async function main() -> Future<I32>:
    I64 result = await pipeline( 10 )
    puts( result.toString() )
    return 0
```

Output:

```
94
```

The pipeline fetches 42, doubles it to 84, then adds the input 10 to produce 94.

### Nested Await

An `await` expression can be used directly as an argument to another async function call.

```uranite
from uranite.io.console import puts

async function getValue() -> Future<I64>:
    return 100

async function addAsync( I64 first, I64 second ) -> Future<I64>:
    return first + second

public async function main() -> Future<I32>:
    I64 nested = await addAsync( await getValue(), 50 )
    puts( nested.toString() )
    return 0
```

Output:

```
150
```

The inner `await getValue()` resolves first, producing 100. That value is passed to `addAsync` along with 50, producing 150.

### Async Methods on Classes

Classes can declare async methods. The `async` keyword appears before `function` in the method declaration.

```uranite
from uranite.io.console import puts

class AsyncCounter:

    public I64 base

    public function AsyncCounter( self, I64 base ) -> Void:
        self.base = base

    public async function compute( self, I64 offset ) -> Future<I64>:
        return self.base + offset

public async function main() -> Future<I32>:
    AsyncCounter counter = new AsyncCounter( 100 )
    I64 result = await counter.compute( 50 )
    puts( result.toString() )
    return 0
```

Output:

```
150
```

### Async with Different Return Types

Async functions can return any type wrapped in `Future<T>`.

```uranite
from uranite.io.console import puts

async function fetchString() -> Future<String>:
    return "hello async"

async function checkThreshold( I64 value ) -> Future<Boolean>:
    if value > 50:
        return True
    return False

public async function main() -> Future<I32>:
    String message = await fetchString()
    puts( message )

    Boolean isLarge = await checkThreshold( 100 )
    Boolean isSmall = await checkThreshold( 10 )
    puts( isLarge.toString() )
    puts( isSmall.toString() )
    return 0
```

Output:

```
hello async
True
False
```

### Calling Sync Functions from Async

Async functions can call regular synchronous functions without `await`.

```uranite
from uranite.io.console import puts

function syncHelper( I64 value ) -> I64:
    return value * 3

async function asyncCompute( I64 input ) -> Future<I64>:
    I64 result = syncHelper( input )
    return result + 1

public async function main() -> Future<I32>:
    I64 output = await asyncCompute( 10 )
    puts( output.toString() )
    return 0
```

Output:

```
31
```

The sync function `syncHelper` is called normally inside the async function. Only calls to other async functions require `await`.

### Async Error Handling

Async functions can raise exceptions, and callers can catch them with `try`/`except`.

```uranite
from uranite.io.console import puts

async function asyncThrow() -> Future<I64>:
    raise new Exception( "async error" )
    return 0

public async function main() -> Future<I32>:
    try:
        puts( "before await" )
        I64 result = await asyncThrow()
        puts( "should not print" )
    except Exception:
        puts( "caught async exception" )
    puts( "done" )
    return 0
```

Output:

```
before await
caught async exception
done
```

Exceptions raised inside async functions propagate through `await` to the caller's `try`/`except` block.

---

## Generator Types

### Declaring Generator Functions

A generator function uses `yield` to produce values and declares its return type as `Generator<T>`.

```uranite
function countdown( I64 start ) -> Generator<I64>:
    I64 current = start
    while current > 0:
        yield current
        current = current - 1
```

Each `yield` produces one value and suspends the function. The next time the generator is advanced, execution resumes after the `yield` statement.

### Iterating Over Generators

Generators are consumed through `for-in` loops. The loop advances the generator on each iteration, receiving the yielded value.

```uranite
from uranite.io.console import puts

function countdown( I64 start ) -> Generator<I64>:
    I64 current = start
    while current > 0:
        yield current
        current = current - 1

public function main() -> I32:
    for I64 value in countdown( 5 ):
        puts( value.toString() )
    return 0
```

Output:

```
5
4
3
2
1
```

The `for-in` loop calls the generator function, then iterates through each yielded value until the generator is exhausted.

### Generator with Parameters

Generator functions accept parameters like regular functions.

```uranite
from uranite.io.console import puts

function rangeGen( I64 start, I64 end ) -> Generator<I64>:
    I64 current = start
    while current < end:
        yield current
        current = current + 1

public function main() -> I32:
    for I64 value in rangeGen( 1, 6 ):
        puts( value.toString() )
    return 0
```

Output:

```
1
2
3
4
5
```

### Multiple Sequential Yields

A generator can contain multiple `yield` statements in sequence. Each yield produces one value before execution advances to the next statement.

```uranite
from uranite.io.console import puts

function multiYield() -> Generator<I64>:
    yield 10
    yield 20
    yield 30

public function main() -> I32:
    for I64 value in multiYield():
        puts( value.toString() )
    return 0
```

Output:

```
10
20
30
```

### Infinite Generators

Generators can run indefinitely using `while True`. Use `break` in the consuming loop to stop iteration.

```uranite
from uranite.io.console import puts

function naturalNumbers() -> Generator<I64>:
    I64 current = 1
    while True:
        yield current
        current = current + 1

public function main() -> I32:
    I64 count = 0
    for I64 value in naturalNumbers():
        if count >= 5:
            break
        puts( value.toString() )
        count = count + 1
    return 0
```

Output:

```
1
2
3
4
5
```

The generator produces values indefinitely, but the consumer breaks out after 5 values.

### Conditional Yield

A generator can use conditional logic to decide which values to yield.

```uranite
from uranite.io.console import puts

function evenNumbers( I64 limit ) -> Generator<I64>:
    I64 current = 0
    while current < limit:
        if current % 2 == 0:
            yield current
        current = current + 1

public function main() -> I32:
    for I64 value in evenNumbers( 10 ):
        puts( value.toString() )
    return 0
```

Output:

```
0
2
4
6
8
```

Only even numbers are yielded. Odd numbers are skipped without producing a value.

### Generator with Expressions in Yield

The `yield` keyword accepts any expression, not just variables.

```uranite
from uranite.io.console import puts

function squares( I64 limit ) -> Generator<I64>:
    I64 current = 1
    while current <= limit:
        yield current * current
        current = current + 1

public function main() -> I32:
    I64 total = 0
    for I64 value in squares( 5 ):
        total = total + value
    puts( total.toString() )
    return 0
```

Output:

```
55
```

Each `yield current * current` computes the square before yielding. The loop accumulates 1 + 4 + 9 + 16 + 25 = 55.

### String Generators

Generators can yield any type, including `String`.

```uranite
from uranite.io.console import puts

function greetings() -> Generator<String>:
    yield "hello"
    yield "world"
    yield "uranite"

public function main() -> I32:
    for String word in greetings():
        puts( word )
    return 0
```

Output:

```
hello
world
uranite
```

### Multiple Generators

Multiple generator functions can be used in the same program. Each generator maintains its own independent state.

```uranite
from uranite.io.console import puts

function evens( I64 limit ) -> Generator<I64>:
    I64 current = 0
    while current < limit:
        yield current
        current = current + 2

function odds( I64 limit ) -> Generator<I64>:
    I64 current = 1
    while current < limit:
        yield current
        current = current + 2

public function main() -> I32:
    puts( "Evens:" )
    for I64 value in evens( 10 ):
        puts( value.toString() )
    puts( "Odds:" )
    for I64 value in odds( 10 ):
        puts( value.toString() )
    return 0
```

Output:

```
Evens:
0
2
4
6
8
Odds:
1
3
5
7
9
```

---

## Comparison

| Aspect | `Future<T>` | `Generator<T>` |
|---|---|---|
| Purpose | Asynchronous computation producing one value | Lazy sequence producing many values |
| Declaration | `async function name() -> Future<T>:` | `function name() -> Generator<T>:` |
| Production | `return value` | `yield value` |
| Consumption | `await expression` | `for T value in generator():` |
| Main signature | `public async function main() -> Future<I32>:` | `public function main() -> I32:` |
| Iteration | Not iterable | Directly iterable in `for-in` loops |
| Termination | Function returns | All yields exhausted or function returns |

---

## Method Reference

### Future Syntax

| Syntax | Description |
|---|---|
| `async function name() -> Future<T>:` | Declares an async function returning `Future<T>` |
| `async function name( params ) -> Future<T>:` | Async function with parameters |
| `public async function name() -> Future<T>:` | Public async function |
| `public async function main() -> Future<I32>:` | Async main entry point |
| `T value = await asyncCall()` | Extracts the inner value from a `Future<T>` |
| `await asyncCall( await otherCall() )` | Nested await as argument |

### Generator Syntax

| Syntax | Description |
|---|---|
| `function name() -> Generator<T>:` | Declares a generator function |
| `function name( params ) -> Generator<T>:` | Generator with parameters |
| `yield value` | Produces a value and suspends execution |
| `yield expression` | Yields the result of an expression |
| `for T value in generatorCall():` | Iterates over all yielded values |

---

## Examples

### Async Computation Pipeline

```uranite
from uranite.io.console import puts

async function stepOne() -> Future<I64>:
    return 10

async function stepTwo( I64 input ) -> Future<I64>:
    return input + 20

async function stepThree( I64 input ) -> Future<I64>:
    return input * 2

public async function main() -> Future<I32>:
    I64 first = await stepOne()
    I64 second = await stepTwo( first )
    I64 third = await stepThree( second )
    puts( first.toString() )
    puts( second.toString() )
    puts( third.toString() )
    return 0
```

Output:

```
10
30
60
```

Three async steps execute in sequence. Step one produces 10, step two adds 20 to get 30, step three doubles to get 60.

### Fibonacci Generator

```uranite
from uranite.io.console import puts

function fibonacci() -> Generator<I64>:
    I64 current = 0
    I64 next = 1
    while True:
        yield current
        I64 temp = current
        current = next
        next = temp + next

public function main() -> I32:
    I64 count = 0
    for I64 value in fibonacci():
        if count >= 10:
            break
        puts( value.toString() )
        count = count + 1
    return 0
```

Output:

```
0
1
1
2
3
5
8
13
21
34
```

An infinite Fibonacci generator. The consumer breaks after collecting 10 values. Local variables `current`, `next`, and `temp` maintain their state across yields.

### Async Class Integration

```uranite
from uranite.io.console import puts

class AsyncCounter:

    public I64 base

    public function AsyncCounter( self, I64 base ) -> Void:
        self.base = base

    public async function compute( self, I64 offset ) -> Future<I64>:
        return self.base + offset

public async function main() -> Future<I32>:
    AsyncCounter counter = new AsyncCounter( 100 )
    I64 first = await counter.compute( 10 )
    I64 second = await counter.compute( 50 )
    puts( first.toString() )
    puts( second.toString() )
    return 0
```

Output:

```
110
150
```

A class with an async method. Each call to `compute` adds the offset to the base value asynchronously.

### Number Sequence Generators

```uranite
from uranite.io.console import puts

function squares( I64 limit ) -> Generator<I64>:
    I64 current = 1
    while current <= limit:
        yield current * current
        current = current + 1

function countdown( I64 start ) -> Generator<I64>:
    I64 current = start
    while current > 0:
        yield current
        current = current - 1

public function main() -> I32:
    puts( "Squares:" )
    for I64 value in squares( 5 ):
        puts( value.toString() )
    puts( "Countdown:" )
    for I64 value in countdown( 3 ):
        puts( value.toString() )
    return 0
```

Output:

```
Squares:
1
4
9
16
25
Countdown:
3
2
1
```
