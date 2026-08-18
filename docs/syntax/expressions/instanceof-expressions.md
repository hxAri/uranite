# Instanceof Expressions

The `instanceof` operator checks whether a value is an instance of a given type. It produces a `Boolean` result based on the value's declared type and the target type's position in the class hierarchy.

---

## Table of Contents

- [Instanceof Expressions](#instanceof-expressions)
  - [Table of Contents](#table-of-contents)
  - [Syntax](#syntax)
  - [Exact Type Match](#exact-type-match)
  - [Inheritance Check](#inheritance-check)
  - [Negative Result](#negative-result)
  - [Generic Types](#generic-types)
  - [Primitive Types](#primitive-types)
  - [Static Type Checking](#static-type-checking)
  - [Operator Precedence](#operator-precedence)
  - [Examples](#examples)
    - [Basic Instanceof with Inheritance](#basic-instanceof-with-inheritance)
    - [Deep Class Hierarchy](#deep-class-hierarchy)
    - [Generic Collection Check](#generic-collection-check)
    - [Conditional Branching by Type](#conditional-branching-by-type)

---

## Syntax

```
expression instanceof Type
```

The left side is a value expression. The right side is a type name. The result is a `Boolean` — `True` if the value's type matches the target type or inherits from it, `False` otherwise.

```uranite
Dog dog = new Dog()
if dog instanceof Animal:
    puts( "Dog is an Animal" )
```

---

## Exact Type Match

When the value's declared type matches the target type exactly, `instanceof` returns `True`:

```uranite
Dog dog = new Dog()
if dog instanceof Dog:
    puts( "match" )
```

---

## Inheritance Check

When the value's declared type extends the target type, `instanceof` walks the inheritance chain and returns `True` if the target is found at any level:

```uranite
Sedan sedan = new Sedan()
if sedan instanceof Car:
    puts( "Sedan extends Car" )
if sedan instanceof Vehicle:
    puts( "Sedan extends Vehicle" )
```

Both conditions evaluate to `True`. The check walks the full inheritance chain: `Sedan` extends `Car`, which extends `Vehicle`. At each level, the parent class is compared against the target type.

---

## Negative Result

When the value's type has no relationship to the target type, `instanceof` returns `False`:

```uranite
Cat cat = new Cat()
if cat instanceof Dog:
    puts( "never printed" )
else:
    puts( "Cat is not Dog" )
```

The `else` branch executes because `Cat` does not extend `Dog` and is not `Dog`.

---

## Generic Types

When checking generic types, `instanceof` compares the base type name. Generic type parameters are not part of the comparison:

```uranite
ArrayList<String> names = new ArrayList<String>()
if names instanceof ArrayList:
    puts( "is ArrayList" )
```

The check compares `ArrayList` against `ArrayList` — the `<String>` parameter is stripped before comparison.

---

## Primitive Types

`instanceof` works with primitive types as well:

```uranite
I64 count = 42
if count instanceof I64:
    puts( "is I64" )
```

---

## Static Type Checking

`instanceof` evaluates based on the declared type of the variable, not the runtime type of the value. When a subclass instance is passed to a function that accepts a base type parameter, `instanceof` sees the parameter's declared type:

```uranite
public function describe( Shape shape ) -> Void:
    if shape instanceof Circle:
        puts( "got circle" )
    if shape instanceof Shape:
        puts( "is a shape" )
```

Calling `describe( circle )` where `circle` is a `Circle` prints only `is a shape`. The parameter `shape` is declared as `Shape`, so `shape instanceof Circle` evaluates to `False` even though the actual value is a `Circle`.

For type-specific dispatch, use `instanceof` directly on the original variable before passing it to a base-typed function, or structure code to avoid needing runtime type identification.

---

## Operator Precedence

`instanceof` shares precedence level 7 with comparison operators:

| Precedence | Operators |
|---|---|
| 12 (highest) | `as` |
| 11 | `**` |
| 10 | `*`, `/`, `%` |
| 9 | `+`, `-` |
| 8 | `<<`, `>>` |
| 7 | `<`, `<=`, `>`, `>=`, `in`, `instanceof`, `is`, `subclassof` |
| 6 | `==`, `!=` |
| 5 | `&` |
| 4 | `^` |
| 3 | `\|` |
| 2 | `and` |
| 1 (lowest) | `or` |

This means `instanceof` binds at the same level as comparison operators:

- `x instanceof Dog and y instanceof Cat` parses as `(x instanceof Dog) and (y instanceof Cat)`
- Arithmetic binds tighter: `count + 1 instanceof I64` parses as `(count + 1) instanceof I64`

---

## Examples

### Basic Instanceof with Inheritance

```uranite
package testing

from uranite.io.console import puts

public class Animal:
    public String species

    public function Animal( self, String species ) -> Void:
        self.species = species

public class Dog extends Animal:
    public function Dog( self ) -> Void:
        parent( "Canine" )

public class Cat extends Animal:
    public function Cat( self ) -> Void:
        parent( "Feline" )

public function main() -> I32:
    Dog dog = new Dog()
    if dog instanceof Animal:
        puts( "Dog is Animal" )
    if dog instanceof Dog:
        puts( "Dog is Dog" )
    if dog instanceof Cat:
        puts( "wrong" )
    else:
        puts( "Dog is not Cat" )
    return 0
```

Output:

```
Dog is Animal
Dog is Dog
Dog is not Cat
```

A `Dog` instance matches both `Animal` (its parent class) and `Dog` (its own type). It does not match `Cat`, an unrelated sibling class, so the `else` branch executes for that check.

---

### Deep Class Hierarchy

```uranite
package testing

from uranite.io.console import puts

public class Vehicle:
    public String kind

    public function Vehicle( self, String kind ) -> Void:
        self.kind = kind

public class Car extends Vehicle:
    public function Car( self ) -> Void:
        parent( "car" )

public class Sedan extends Car:
    public function Sedan( self ) -> Void:
        parent()

public function main() -> I32:
    Sedan sedan = new Sedan()
    if sedan instanceof Vehicle:
        puts( "Sedan is Vehicle" )
    if sedan instanceof Car:
        puts( "Sedan is Car" )
    if sedan instanceof Sedan:
        puts( "Sedan is Sedan" )
    return 0
```

Output:

```
Sedan is Vehicle
Sedan is Car
Sedan is Sedan
```

`instanceof` walks the entire inheritance chain. `Sedan` extends `Car`, which extends `Vehicle`. All three checks succeed because `Vehicle` and `Car` are both ancestors of `Sedan` in the class hierarchy.

---

### Generic Collection Check

```uranite
package testing

from uranite.io.console import puts
from uranite.collection.array-list import ArrayList

public function main() -> I32:
    ArrayList<String> names = new ArrayList<String>()
    names.add( "Alice" )
    names.add( "Bob" )

    if names instanceof ArrayList:
        puts( "is ArrayList" )

    I64 count = 42
    if count instanceof I64:
        puts( "is I64" )
    return 0
```

Output:

```
is ArrayList
is I64
```

When checking a generic type like `ArrayList<String>`, the type parameter is not included in the comparison. The check compares the base type `ArrayList` against the target `ArrayList`. Primitive types also work with `instanceof` — checking `I64` against `I64` matches.

---

### Conditional Branching by Type

```uranite
package testing

from uranite.io.console import puts

public class Shape:
    public String label

    public function Shape( self, String label ) -> Void:
        self.label = label

public class Circle extends Shape:
    public F64 radius

    public function Circle( self, F64 radius ) -> Void:
        parent( "circle" )
        self.radius = radius

public class Rectangle extends Shape:
    public F64 width
    public F64 height

    public function Rectangle( self, F64 width, F64 height ) -> Void:
        parent( "rectangle" )
        self.width = width
        self.height = height

public function main() -> I32:
    Circle circle = new Circle( 5.0 )
    Rectangle rect = new Rectangle( 3.0, 4.0 )

    if circle instanceof Shape:
        puts( "Circle is Shape" )
    if circle instanceof Circle:
        puts( "Circle is Circle" )
    if rect instanceof Shape:
        puts( "Rectangle is Shape" )
    if rect instanceof Rectangle:
        puts( "Rectangle is Rectangle" )
    return 0
```

Output:

```
Circle is Shape
Circle is Circle
Rectangle is Shape
Rectangle is Rectangle
```

When `instanceof` is used directly on a variable with its concrete type, both the exact type and inherited types match. The checks work because each variable retains its declared concrete type — `circle` is declared as `Circle`, not `Shape`.
