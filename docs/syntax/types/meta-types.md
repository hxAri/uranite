# Meta Types and instanceof

---

## Table of Contents

- [Meta Types and instanceof](#meta-types-and-instanceof)
  - [Table of Contents](#table-of-contents)
  - [Overview](#overview)
  - [The instanceof Operator](#the-instanceof-operator)
    - [Same-Type Check](#same-type-check)
    - [Parent Class Check](#parent-class-check)
    - [Negative Check](#negative-check)
    - [Primitive Type Check](#primitive-type-check)
    - [instanceof in Conditions](#instanceof-in-conditions)
  - [Meta Types](#meta-types)
    - [Declaring Meta Variables](#declaring-meta-variables)
  - [Method Reference](#method-reference)
    - [instanceof Syntax](#instanceof-syntax)
    - [Meta Type Syntax](#meta-type-syntax)
  - [Examples](#examples)
    - [Class Hierarchy Type Checking](#class-hierarchy-type-checking)
    - [Primitive Type Verification](#primitive-type-verification)
    - [Type Guard Pattern](#type-guard-pattern)
    - [Multi-Level Inheritance Check](#multi-level-inheritance-check)

---

## Overview

The `instanceof` operator checks whether a value belongs to a specific type, producing a `Boolean` result. It performs compile-time type checking against the declared type of the value and the target type, including class inheritance chains.

`Meta<T>` represents a type itself as a value. When a type name appears in expression context, it is wrapped in `Meta<T>`, allowing types to be stored in variables.

---

## The instanceof Operator

### Same-Type Check

The `instanceof` operator returns `True` when the value's type matches the target type exactly.

```uranite
from uranite.io.console import puts

public function main() -> I32:
    String text = "hello"
    Boolean isString = text instanceof String
    puts( isString.toString() )

    I64 number = 42
    Boolean isI64 = number instanceof I64
    puts( isI64.toString() )
    return 0
```

Output:

```
True
True
```

### Parent Class Check

The `instanceof` operator returns `True` when the value's type is a subclass of the target type.

```uranite
from uranite.io.console import puts

class Animal:

    public function Animal( self ) -> Void:
        pass

class Dog extends Animal:

    public function Dog( self ) -> Void:
        parent()

public function main() -> I32:
    Dog dog = new Dog()
    Boolean isDog = dog instanceof Dog
    Boolean isAnimal = dog instanceof Animal
    puts( isDog.toString() )
    puts( isAnimal.toString() )
    return 0
```

Output:

```
True
True
```

A `Dog` is both a `Dog` and an `Animal` because `Dog` extends `Animal`.

### Negative Check

The `instanceof` operator returns `False` when the types do not match.

```uranite
from uranite.io.console import puts

class Animal:

    public function Animal( self ) -> Void:
        pass

class Dog extends Animal:

    public function Dog( self ) -> Void:
        parent()

public function main() -> I32:
    Animal animal = new Animal()
    Boolean isDog = animal instanceof Dog
    puts( isDog.toString() )

    String text = "hello"
    Boolean isI64 = text instanceof I64
    puts( isI64.toString() )
    return 0
```

Output:

```
False
False
```

An `Animal` is not a `Dog` (the parent is not an instance of the child). A `String` is not an `I64`.

### Primitive Type Check

The `instanceof` operator works with primitive types.

```uranite
from uranite.io.console import puts

public function main() -> I32:
    I64 number = 42
    String text = "hello"
    Boolean flag = True

    puts( (number instanceof I64).toString() )
    puts( (text instanceof String).toString() )
    puts( (flag instanceof Boolean).toString() )
    return 0
```

Output:

```
True
True
True
```

### instanceof in Conditions

The `instanceof` result can be used directly in `if` conditions.

```uranite
from uranite.io.console import puts

class Animal:

    public function Animal( self ) -> Void:
        pass

class Dog extends Animal:

    public function Dog( self ) -> Void:
        parent()

public function main() -> I32:
    Dog dog = new Dog()
    if dog instanceof Animal:
        puts( "dog is an animal" )
    Animal animal = new Animal()
    if animal instanceof Dog:
        puts( "should not print" )
    else:
        puts( "animal is not a dog" )
    return 0
```

Output:

```
dog is an animal
animal is not a dog
```

---

## Meta Types

### Declaring Meta Variables

`Meta<T>` wraps a type as a value. A bare type name in expression context becomes a `Meta<T>` value.

```uranite
from uranite.io.console import puts

public function main() -> I32:
    Meta<String> stringType = String
    Meta<I64> intType = I64
    puts( "types stored" )
    return 0
```

Output:

```
types stored
```

---

## Method Reference

### instanceof Syntax

| Syntax | Description |
|---|---|
| `value instanceof Type` | Check if value is of the specified type |
| `value instanceof ParentType` | Check if value's type extends the target |
| `if value instanceof Type:` | Use instanceof in condition |

### Meta Type Syntax

| Syntax | Description |
|---|---|
| `Meta<T>` | A type value wrapping type `T` |
| `Meta<String> variable = String` | Store a type as a value |

---

## Examples

### Class Hierarchy Type Checking

```uranite
from uranite.io.console import puts

class Animal:

    public function Animal( self ) -> Void:
        pass

class Dog extends Animal:

    public function Dog( self ) -> Void:
        parent()

public function main() -> I32:
    Dog dog = new Dog()
    Animal animal = new Animal()

    Boolean dogIsDog = dog instanceof Dog
    Boolean dogIsAnimal = dog instanceof Animal
    Boolean animalIsAnimal = animal instanceof Animal
    Boolean animalIsDog = animal instanceof Dog

    puts( dogIsDog.toString() )
    puts( dogIsAnimal.toString() )
    puts( animalIsAnimal.toString() )
    puts( animalIsDog.toString() )
    return 0
```

Output:

```
True
True
True
False
```

### Primitive Type Verification

```uranite
from uranite.io.console import puts

public function main() -> I32:
    String text = "hello"
    I64 number = 42

    Boolean textIsString = text instanceof String
    Boolean numberIsI64 = number instanceof I64
    Boolean textIsI64 = text instanceof I64

    puts( textIsString.toString() )
    puts( numberIsI64.toString() )
    puts( textIsI64.toString() )
    return 0
```

Output:

```
True
True
False
```

### Type Guard Pattern

```uranite
from uranite.io.console import puts

class Vehicle:

    public function Vehicle( self ) -> Void:
        pass

class Car extends Vehicle:

    public function Car( self ) -> Void:
        parent()

class Truck extends Vehicle:

    public function Truck( self ) -> Void:
        parent()

public function main() -> I32:
    Car car = new Car()
    Truck truck = new Truck()

    if car instanceof Vehicle:
        puts( "car is vehicle" )
    if truck instanceof Vehicle:
        puts( "truck is vehicle" )
    if car instanceof Truck:
        puts( "should not print" )
    else:
        puts( "car is not truck" )
    return 0
```

Output:

```
car is vehicle
truck is vehicle
car is not truck
```

### Multi-Level Inheritance Check

```uranite
from uranite.io.console import puts

class Base:

    public function Base( self ) -> Void:
        pass

class Middle extends Base:

    public function Middle( self ) -> Void:
        parent()

class Bottom extends Middle:

    public function Bottom( self ) -> Void:
        parent()

public function main() -> I32:
    Bottom bottom = new Bottom()
    Boolean bottomIsBottom = bottom instanceof Bottom
    Boolean bottomIsMiddle = bottom instanceof Middle
    Boolean bottomIsBase = bottom instanceof Base
    puts( bottomIsBottom.toString() )
    puts( bottomIsMiddle.toString() )
    puts( bottomIsBase.toString() )

    Middle middle = new Middle()
    Boolean middleIsBottom = middle instanceof Bottom
    Boolean middleIsMiddle = middle instanceof Middle
    Boolean middleIsBase = middle instanceof Base
    puts( middleIsBottom.toString() )
    puts( middleIsMiddle.toString() )
    puts( middleIsBase.toString() )
    return 0
```

Output:

```
True
True
True
False
True
True
```
