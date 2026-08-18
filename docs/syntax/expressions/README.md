# Expressions

Expressions are constructs that evaluate to a value. Every expression in Uranite produces a typed result that can be assigned to a variable, passed as an argument, returned from a function, or composed with other expressions. This section covers all expression forms available in the language.

---

## Table of Contents

- [Expressions](#expressions)
  - [Table of Contents](#table-of-contents)
  - [Overview](#overview)
  - [Expression Categories](#expression-categories)
    - [Value Expressions](#value-expressions)
    - [Call and Access Expressions](#call-and-access-expressions)
    - [Type Expressions](#type-expressions)
    - [Operators](#operators)
    - [Comprehensions](#comprehensions)
    - [Control Flow Expressions](#control-flow-expressions)
  - [Expression Example](#expression-example)
  - [Subpages](#subpages)

---

## Overview

Uranite distinguishes between expressions (which produce values) and statements (which perform actions). Some constructs serve as both — an assignment is a statement, but the right-hand side is always an expression. Function calls can appear as standalone statements or as part of larger expressions.

All expressions have a static type determined at compile time by the semantic analyzer. The compiler rejects expressions where types are incompatible.

---

## Expression Categories

### Value Expressions

Literal values, variable references, and object construction:

- **Literals** — integer, float, string, char, boolean, and `None` values
- **Variable references** — reading a previously declared variable
- **Constructor expressions** — creating new objects with `new`
- **Lambda expressions** — anonymous function values
- **Range expressions** — producing integer sequences with `..` and `...`

### Call and Access Expressions

Invoking functions and accessing members:

- **Call expressions** — invoking functions with positional, variadic, and keyword arguments
- **Method call expressions** — invoking methods on an object through dot notation
- **Member access expressions** — reading fields and properties with dot notation
- **Index expressions** — accessing elements by position with bracket notation
- **Self expressions** — referencing the current instance with `self`
- **Parent expressions** — delegating to parent class constructors with `parent()`

### Type Expressions

Checking and converting types:

- **Instanceof expressions** — testing whether a value is an instance of a type
- **Subclassof expressions** — testing whether a type inherits from another type
- **Type casting** — converting values between compatible types with `as`

### Operators

Arithmetic, comparison, logical, bitwise, and unary operators that combine or transform values. See the [Operators](operators/README.md) section for full documentation.

### Comprehensions

Compact syntax for building collections from iteration. See the [Comprehensions](comprehensions/README.md) section for list, map, and set comprehension forms.

### Control Flow Expressions

Expressions that evaluate conditionally:

- **Match expressions** — pattern matching that produces a value
- **Await expressions** — suspending execution until an async result is available

---

## Expression Example

```uranite
public class Greeter:
    public String name

    public function Greeter( self, String name ) -> Void:
        self.name = name

    public function greet( self ) -> String:
        return "Hello"

public function add( I64 left, I64 right ) -> I64:
    return left + right

public function main() -> I32:
    I64 result = add( 3, 4 )
    Greeter greeter = new Greeter( "Uranite" )
    String greeting = greeter.greet()
    puts( greeting )
    return 0
```

This example uses several expression forms: function call (`add( 3, 4 )`), constructor (`new Greeter( "Uranite" )`), method call (`greeter.greet()`), and literal values (`3`, `4`, `"Uranite"`, `"Hello"`, `0`).

---

## Subpages

| Page | Description |
|---|---|
| [Call Expressions](call-expressions.md) | Function invocation with positional, variadic, and keyword arguments |
| [Method Call Expressions](method-call-expressions.md) | Invoking methods on objects through dot notation |
| [Constructor Expressions](constructor-expressions.md) | Creating new objects with the `new` keyword |
| [Member Access Expressions](member-access-expressions.md) | Reading fields and properties with dot notation |
| [Index Expressions](index-expressions.md) | Accessing elements by position with bracket notation |
| [Range Expressions](range-expressions.md) | Producing integer sequences with `..` and `...` operators |
| [Instanceof Expressions](instanceof-expressions.md) | Runtime type checking with `instanceof` |
| [Subclassof Expressions](subclassof-expressions.md) | Type-level inheritance checking with `subclassof` |
| [Self Expressions](self-expressions.md) | Referencing the current instance with `self` |
| [Parent Expressions](parent-expressions.md) | Delegating to parent class constructors with `parent()` |
| [Lambda Expressions](lambda-expressions.md) | Anonymous functions and closures |
| [Match Expressions](match-expressions.md) | Pattern matching that produces a value |
| [Await Expressions](await-expressions.md) | Suspending execution for async results |
| [Operator Precedence](operator-precedence.md) | Complete precedence table for all operators |
| [Operators](operators/README.md) | Arithmetic, bitwise, comparison, logical, and unary operators |
| [Comprehensions](comprehensions/README.md) | List, map, and set comprehension syntax |
