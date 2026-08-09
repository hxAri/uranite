# Pointer and Reference Types

---

## Table of Contents

- [Pointer and Reference Types](#pointer-and-reference-types)
  - [Table of Contents](#table-of-contents)
  - [Overview](#overview)
  - [Reference Types](#reference-types)
    - [Immutable References](#immutable-references)
    - [Mutable References](#mutable-references)
    - [The Address-Of Operator](#the-address-of-operator)
    - [Mutable Address-Of](#mutable-address-of)
  - [Reference Parameters](#reference-parameters)
    - [Immutable Reference Parameters](#immutable-reference-parameters)
    - [Mutable Reference Parameters](#mutable-reference-parameters)
    - [Multiple Reference Parameters](#multiple-reference-parameters)
  - [String References](#string-references)
  - [Borrow Checker](#borrow-checker)
    - [Return-of-Local Prevention](#return-of-local-prevention)
  - [Pointer Types](#pointer-types)
  - [Method Reference](#method-reference)
    - [Reference Syntax](#reference-syntax)
    - [Address-Of Syntax](#address-of-syntax)
  - [Examples](#examples)
    - [Reference Passing](#reference-passing)
    - [Mixed Reference Modes](#mixed-reference-modes)
    - [Reference with String Types](#reference-with-string-types)
    - [Multiple Value References](#multiple-value-references)

---

## Overview

References provide a way to pass values to functions by reference rather than by value. A reference is a borrowed view of an existing value. Uranite supports two kinds of references:

- `&T` — an immutable reference to a value of type `T`
- `&mut T` — a mutable reference to a value of type `T`

References are created with the `&` operator (for immutable) or `&mut` (for mutable). References are primarily used as function parameter types, allowing functions to receive access to the caller's values without copying.

The borrow checker prevents returning references to local variables, catching dangling reference bugs at compile time.

---

## Reference Types

### Immutable References

An immutable reference `&T` provides read-only access to a value. It is created with the `&` operator.

```uranite
&T
```

For example, `&I64` is an immutable reference to an `I64` value.

### Mutable References

A mutable reference `&mut T` allows modification of the referenced value. It is created with the `&mut` operator.

```uranite
&mut T
```

For example, `&mut I64` is a mutable reference to an `I64` value.

### The Address-Of Operator

The `&` operator creates an immutable reference to a variable.

```uranite
I64 value = 42
&value
```

The expression `&value` produces a value of type `&I64`.

### Mutable Address-Of

The `&mut` operator creates a mutable reference to a variable.

```uranite
I64 value = 42
&mut value
```

The expression `&mut value` produces a value of type `&mut I64`.

---

## Reference Parameters

### Immutable Reference Parameters

Functions can accept immutable references as parameters, receiving access to the caller's value.

```uranite
from uranite.io.console import puts

public function printRef( &I64 value ) -> Void:
    puts( "ref received" )

public function main() -> I32:
    I64 number = 42
    printRef( &number )
    return 0
```

Output:

```
ref received
```

The function receives a reference to `number` without copying the value.

### Mutable Reference Parameters

Functions can accept mutable references, indicating they may modify the referenced value.

```uranite
from uranite.io.console import puts

public function processMut( &mut I64 value ) -> Void:
    puts( "mut ref received" )

public function main() -> I32:
    I64 number = 42
    processMut( &mut number )
    return 0
```

Output:

```
mut ref received
```

### Multiple Reference Parameters

Functions can accept multiple reference parameters of different mutability modes.

```uranite
from uranite.io.console import puts

public function compare( &I64 first, &I64 second ) -> Void:
    puts( "compared" )

public function main() -> I32:
    I64 alpha = 10
    I64 beta = 20
    compare( &alpha, &beta )
    return 0
```

Output:

```
compared
```

---

## String References

References work with `String` types.

```uranite
from uranite.io.console import puts

public function getStringRef( &String value ) -> Void:
    puts( "string ref received" )

public function main() -> I32:
    String text = "hello"
    getStringRef( &text )
    return 0
```

Output:

```
string ref received
```

---

## Borrow Checker

### Return-of-Local Prevention

The borrow checker prevents returning references to local variables. A local variable is destroyed when the function returns, so a reference to it would be dangling.

```uranite
public function dangling() -> &I64:
    I64 value = 42
    return &value
```

Error:

```
cannot return reference to local variable "value"
hint: the variable will be dropped when the function returns
```

This is a compile-time error — the program cannot be built with a dangling reference.

---

## Pointer Types

Pointer types (`*T`, `*mut T`) exist in the type system for low-level memory operations. Pointer type annotations are used in function parameter declarations for interacting with raw memory through the `Memory<T>` type and `unsafe` blocks.

Pointers are a low-level mechanism primarily used in standard library implementations and system-level code. For most programming tasks, use reference types instead.

---

## Method Reference

### Reference Syntax

| Syntax | Description |
|---|---|
| `&T` | Immutable reference to type `T` |
| `&mut T` | Mutable reference to type `T` |
| `function name( &T param ) -> Type:` | Function with immutable reference parameter |
| `function name( &mut T param ) -> Type:` | Function with mutable reference parameter |

### Address-Of Syntax

| Syntax | Description |
|---|---|
| `&variable` | Creates an immutable reference |
| `&mut variable` | Creates a mutable reference |

---

## Examples

### Reference Passing

```uranite
from uranite.io.console import puts

public function receiveRef( &I64 value ) -> Void:
    puts( "received" )

public function main() -> I32:
    I64 alpha = 10
    I64 beta = 20
    I64 gamma = 30
    receiveRef( &alpha )
    receiveRef( &beta )
    receiveRef( &gamma )
    return 0
```

Output:

```
received
received
received
```

### Mixed Reference Modes

```uranite
from uranite.io.console import puts

public function readOnly( &I64 value ) -> Void:
    puts( "read-only" )

public function readWrite( &mut I64 value ) -> Void:
    puts( "read-write" )

public function main() -> I32:
    I64 alpha = 10
    I64 beta = 20
    readOnly( &alpha )
    readWrite( &mut beta )
    return 0
```

Output:

```
read-only
read-write
```

### Reference with String Types

```uranite
from uranite.io.console import puts

public function processString( &String text ) -> Void:
    puts( "string processed" )

public function processNumber( &I64 number ) -> Void:
    puts( "number processed" )

public function main() -> I32:
    String name = "Alice"
    I64 age = 30
    processString( &name )
    processNumber( &age )
    return 0
```

Output:

```
string processed
number processed
```

### Multiple Value References

```uranite
from uranite.io.console import puts

public function receiveThree( &I64 first, &I64 second, &I64 third ) -> Void:
    puts( "three refs" )

public function receiveMixed( &I64 immutable, &mut I64 mutable ) -> Void:
    puts( "mixed refs" )

public function main() -> I32:
    I64 alpha = 10
    I64 beta = 20
    I64 gamma = 30
    I64 delta = 40
    receiveThree( &alpha, &beta, &gamma )
    receiveMixed( &alpha, &mut delta )
    return 0
```

Output:

```
three refs
mixed refs
```
