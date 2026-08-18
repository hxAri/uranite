# Yield Expressions

The `yield` keyword produces a value from a generator function without ending execution. Each `yield` suspends the function, delivers a value to the caller, and resumes from the same point on the next iteration. Generator functions return `Generator<T>` and are consumed with `for-in` loops.

---

## Table of Contents

- [Yield Expressions](#yield-expressions)
  - [Table of Contents](#table-of-contents)
  - [Syntax](#syntax)
  - [Generator Functions](#generator-functions)
    - [Declaring Generators](#declaring-generators)
    - [Return Type Requirement](#return-type-requirement)
  - [Yield](#yield)
    - [Producing Values](#producing-values)
    - [Yield Inside Loops](#yield-inside-loops)
    - [Conditional Yield](#conditional-yield)
    - [Multiple Yield Points](#multiple-yield-points)
  - [Consuming Generators](#consuming-generators)
  - [Examples](#examples)
    - [Countdown Generator](#countdown-generator)
    - [Computed Sequence](#computed-sequence)
    - [Filtered Generator](#filtered-generator)
    - [Fibonacci Sequence](#fibonacci-sequence)

---

## Syntax

```
yield expression
```

The `yield` keyword produces a value of type `T` from a function that returns `Generator<T>`. Execution pauses at the `yield` point and resumes when the next value is requested.

---

## Generator Functions

### Declaring Generators

A generator function contains one or more `yield` statements and declares `Generator<T>` as its return type:

```uranite
public function numbers() -> Generator<I64>:
    yield 1
    yield 2
    yield 3
```

Calling `numbers()` does not immediately execute the body. Instead, it returns a `Generator<I64>` that produces values one at a time as they are requested.

---

### Return Type Requirement

Functions containing `yield` must declare their return type as `Generator<T>`. Using `yield` in a function with a non-generator return type produces a compiler error:

```
function "name" contains "yield" but does not declare return type "Generator<T>"
```

The type parameter `T` must match the type of the yielded values. If the function yields `I64` values, the return type must be `Generator<I64>`.

---

## Yield

### Producing Values

Each `yield` statement produces one value and suspends execution:

```uranite
public function greetings() -> Generator<String>:
    yield "hello"
    yield "world"
```

The first iteration produces `"hello"`. The second produces `"world"`. After all yield points are exhausted, the generator ends.

---

### Yield Inside Loops

`yield` inside a loop produces a value on each iteration:

```uranite
public function squares( I64 count ) -> Generator<I64>:
    for I64 index in 0..count:
        I64 square = index * index
        yield square
```

Each loop iteration computes a square and yields it. The generator produces `count` values total, one per loop iteration.

---

### Conditional Yield

`yield` inside a conditional produces values only when the condition is met:

```uranite
public function evenNumbers( I64 limit ) -> Generator<I64>:
    for I64 index in 0..limit:
        I64 remainder = index % 2
        if remainder == 0:
            yield index
```

Only even numbers are yielded. Odd numbers skip the `yield` and continue to the next iteration.

---

### Multiple Yield Points

A generator function can have multiple `yield` statements at different points in the control flow:

```uranite
public function sequence() -> Generator<I64>:
    yield 10
    yield 20
    for I64 index in 0..3:
        yield index
```

This produces `10`, `20`, `0`, `1`, `2` — first the two explicit yields, then three from the loop.

---

## Consuming Generators

Generators are consumed with `for-in` loops. The loop variable receives each yielded value:

```uranite
for I64 number in squares( 5 ):
    puts( number.toString() )
```

Each iteration of the `for-in` loop requests the next value from the generator. The loop ends when the generator has no more values to produce.

---

## Examples

### Countdown Generator

```uranite
package testing

from uranite.io.console import puts

public function countdown( I64 start ) -> Generator<I64>:
    for I64 index in 0..start:
        I64 remaining = start - index
        yield remaining

public function main() -> I32:
    for I64 number in countdown( 5 ):
        puts( number.toString() )
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

The generator computes `start - index` on each iteration, producing values from `5` down to `1`. The `for-in` loop in `main` consumes each yielded value and prints it.

---

### Computed Sequence

```uranite
package testing

from uranite.io.console import puts

public function squares( I64 count ) -> Generator<I64>:
    for I64 index in 0..count:
        I64 square = index * index
        yield square

public function main() -> I32:
    for I64 number in squares( 5 ):
        puts( number.toString() )
    return 0
```

Output:

```
0
1
4
9
16
```

Each yielded value is computed from the loop variable. The generator squares each index (`0*0`, `1*1`, `2*2`, `3*3`, `4*4`) and yields the result. Values are produced lazily — each square is computed only when requested by the `for-in` loop.

---

### Filtered Generator

```uranite
package testing

from uranite.io.console import puts

public function evenNumbers( I64 limit ) -> Generator<I64>:
    for I64 index in 0..limit:
        I64 remainder = index % 2
        if remainder == 0:
            yield index

public function main() -> I32:
    for I64 number in evenNumbers( 10 ):
        puts( number.toString() )
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

The generator filters its output — only values where `index % 2 == 0` are yielded. Odd values are skipped entirely, and the `for-in` loop only sees even numbers.

---

### Fibonacci Sequence

```uranite
package testing

from uranite.io.console import puts

public function fibonacci( I64 count ) -> Generator<I64>:
    I64 previous = 0
    I64 current = 1
    for I64 index in 0..count:
        yield current
        I64 next = previous + current
        previous = current
        current = next

public function main() -> I32:
    for I64 number in fibonacci( 8 ):
        puts( number.toString() )
    return 0
```

Output:

```
1
1
2
3
5
8
13
21
```

The generator maintains state between yields. Variables `previous` and `current` track the Fibonacci sequence. Each `yield current` produces the next Fibonacci number, then the state variables are updated for the next iteration. The generator preserves its local state across suspensions.
