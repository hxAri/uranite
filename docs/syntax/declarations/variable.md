# Variable Declarations

A variable declaration introduces a named binding with an explicit type and an initializer expression. Every variable must be initialized at the point of declaration — uninitialized variables are rejected by the compiler. Variables are mutable by default and can be reassigned after initialization.

---

## Table of Contents

- [Variable Declarations](#variable-declarations)
  - [Table of Contents](#table-of-contents)
  - [Syntax](#syntax)
  - [Primitive Type Variables](#primitive-type-variables)
  - [String Variables](#string-variables)
  - [Class-Typed Variables](#class-typed-variables)
  - [Generic Type Variables](#generic-type-variables)
  - [Nullable Variables](#nullable-variables)
  - [Computed Initializers](#computed-initializers)
  - [Local Constant Variables](#local-constant-variables)
  - [Initialization Requirement](#initialization-requirement)
  - [Ownership at Declaration](#ownership-at-declaration)

---

## Syntax

```
Type name = expression
```

The type appears first, followed by the variable name, the `=` operator, and the initializer expression. The type must be explicit — Uranite does not support type inference for local variables.

---

## Primitive Type Variables

Integer, floating-point, boolean, and character types are declared with their type name and an initializer:

```uranite
I64 count = 10
F64 ratio = 3.14
Boolean active = True
Char letter = 'A'
```

Primitive values are copied on assignment. Reassigning a primitive variable does not affect other variables that previously held the same value.

---

## String Variables

Strings are declared with the `String` type:

```uranite
String greeting = "hello"
String empty = ""
```

---

## Class-Typed Variables

Variables holding class instances use the class name as the type. Class instances are created with the `new` keyword:

```uranite
from uranite.collection.array-list import ArrayList

ArrayList<I64> scores = new ArrayList<>()
scores.add( 100 )
scores.add( 200 )
```

Class-typed variables hold ownership of their value. Assigning a class-typed variable to another transfers ownership — the source variable becomes invalid after the transfer.

---

## Generic Type Variables

Variables with generic types include type parameters in angle brackets:

```uranite
from uranite.collection.array-list import ArrayList
from uranite.collection.hash-map import HashMap

ArrayList<String> names = new ArrayList<>()
HashMap<String, I64> ages = new HashMap<>()
```

---

## Nullable Variables

Nullable variables can hold either a value of their declared type or `None`. The `?` prefix on the type marks the variable as nullable:

```uranite
?String maybeName = "hello"
?I64 maybeCount = None

if maybeName is not None:
    puts( "has name" )
```

Nullable variables must use explicit `is None` or `is not None` checks before accessing the value.

---

## Computed Initializers

The initializer expression can be any expression that produces a value compatible with the declared type:

```uranite
I64 first = 10
I64 second = 20
I64 total = first + second
```

Function calls, method calls, and complex expressions are valid initializers:

```uranite
from uranite.io.console import puts

String label = "score"
I64 score = 100
String message = "{}: {}".format( label, score.toString() )
puts( message )
```

---

## Local Constant Variables

The `const` keyword before the type creates an immutable local binding that cannot be reassigned after initialization:

```uranite
const I64 limit = 100
const String prefix = "uranite"
```

Attempting to reassign a `const` variable produces a compiler error:

```
cannot assign to immutable variable "limit"
hint: declare with "mut" to make it mutable
```

For top-level constants that exist outside functions, see [Constants](constants.md).

---

## Initialization Requirement

Every variable must have an initializer. Declaring a variable without assigning a value is rejected by the compiler:

```
use of uninitialized variable "count"
```

This prevents the use of uninitialized memory and ensures every variable has a defined value from its first use.

---

## Ownership at Declaration

When a variable is initialized with a class-typed value, it becomes the owner of that value. Ownership follows these rules:

- **Primitive types** (`I64`, `F64`, `Boolean`, `Char`) are copied. Both the source and target hold independent values.
- **Class-typed values** are moved. The source loses ownership and becomes invalid.
- **String values** follow class-typed semantics — assignment transfers ownership.

```uranite
from uranite.collection.array-list import ArrayList

ArrayList<I64> original = new ArrayList<>()
original.add( 1 )
ArrayList<I64> transferred = original
```

After the last line, `original` is no longer valid and cannot be used.
