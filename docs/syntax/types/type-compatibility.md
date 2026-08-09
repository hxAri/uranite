# Type Compatibility

---

## Table of Contents

- [Type Compatibility](#type-compatibility)
  - [Table of Contents](#table-of-contents)
  - [Overview](#overview)
  - [Exact Type Match](#exact-type-match)
  - [Numeric Type Conversions](#numeric-type-conversions)
    - [Integer Widening](#integer-widening)
    - [Integer to Float](#integer-to-float)
    - [Float Widening](#float-widening)
    - [Float to Integer](#float-to-integer)
  - [Class Inheritance](#class-inheritance)
    - [Child to Parent](#child-to-parent)
    - [Multi-Level Inheritance](#multi-level-inheritance)
    - [Parent to Child](#parent-to-child)
  - [Interface Conformance](#interface-conformance)
    - [Class to Interface](#class-to-interface)
  - [Object Universality](#object-universality)
  - [Optional Compatibility](#optional-compatibility)
    - [Concrete to Optional](#concrete-to-optional)
    - [None Assignment](#none-assignment)
  - [Union Compatibility](#union-compatibility)
    - [Member Type to Union](#member-type-to-union)
  - [Comparison Compatibility](#comparison-compatibility)
    - [Same-Type Comparisons](#same-type-comparisons)
  - [Return Type Checking](#return-type-checking)
  - [Incompatible Types](#incompatible-types)
  - [Method Reference](#method-reference)
    - [Compatibility Rules Summary](#compatibility-rules-summary)
    - [Error Messages](#error-messages)
  - [Examples](#examples)
    - [Numeric Type Conversion](#numeric-type-conversion)
    - [Class Hierarchy Assignability](#class-hierarchy-assignability)
    - [Interface Conformance Dispatch](#interface-conformance-dispatch)
    - [Optional Union and Object Compatibility](#optional-union-and-object-compatibility)

---

## Overview

Uranite enforces type compatibility at compile time. Every assignment, function argument, and return value is checked to ensure the source type is compatible with the target type. When a type mismatch is detected, compilation fails with a descriptive error message.

Type compatibility is directional — whether type `A` can be assigned to a variable of type `B` depends on specific rules. Some conversions are symmetric (integer types can convert in both directions), while others are one-directional (a child class can be assigned to a parent class variable, but not the reverse).

---

## Exact Type Match

When the source and target types are identical, the assignment is always valid.

```uranite
from uranite.io.console import puts

public function main() -> I32:
    I64 alpha = 42
    I64 beta = alpha
    puts( beta.toString() )

    String first = "hello"
    String second = first
    puts( second )
    return 0
```

Output:

```
42
hello
```

Assigning a value to a variable of the same type requires no conversion.

---

## Numeric Type Conversions

Uranite allows implicit conversion between numeric types. These conversions happen automatically at assignment and function call sites.

### Integer Widening

Smaller integer types can be assigned to larger integer types.

```uranite
from uranite.io.console import puts

public function main() -> I32:
    I32 small = 10
    I64 big = small
    puts( big.toString() )
    return 0
```

Output:

```
10
```

An `I32` value is widened to `I64` without loss of information.

### Integer to Float

Integer values can be assigned to floating-point variables.

```uranite
from uranite.io.console import puts

public function main() -> I32:
    I64 integer = 42
    F64 floating = integer
    puts( floating.toString() )
    return 0
```

Output:

```
42
```

The integer value `42` is converted to floating-point representation.

### Float Widening

`F32` values can be assigned to `F64` variables.

```uranite
from uranite.io.console import puts

public function main() -> I32:
    F32 small = 2.5
    F64 big = small
    puts( big.toString() )
    return 0
```

Output:

```
2.5
```

### Float to Integer

Floating-point values can be assigned to integer variables. The fractional part is truncated.

```uranite
from uranite.io.console import puts

public function main() -> I32:
    F64 precise = 9.7
    I64 truncated = precise
    puts( truncated.toString() )
    return 0
```

Output:

```
9
```

The value `9.7` is truncated to `9`. The fractional part `.7` is discarded.

---

## Class Inheritance

### Child to Parent

A child class value can be assigned to a parent class variable. This is because a child class contains all the fields and methods of the parent.

```uranite
from uranite.io.console import puts

class Animal:

    public String name

    public function Animal( self, String name ) -> Void:
        self.name = name

class Dog extends Animal:

    public function Dog( self, String name ) -> Void:
        parent( name )

public function acceptAnimal( Animal animal ) -> Void:
    puts( animal.name )

public function main() -> I32:
    Dog dog = new Dog( "Rex" )
    acceptAnimal( dog )

    Animal animal = dog
    puts( animal.name )
    return 0
```

Output:

```
Rex
Rex
```

A `Dog` is assignable to `Animal` because `Dog` extends `Animal`. This works both for variable assignment and function parameter passing.

### Multi-Level Inheritance

Assignability follows the full inheritance chain. A class at any depth can be assigned to any ancestor.

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

public function acceptBase( Base value ) -> Void:
    puts( "base accepted" )

public function main() -> I32:
    Bottom bottom = new Bottom()
    acceptBase( bottom )

    Base asBase = bottom
    puts( "assigned" )
    return 0
```

Output:

```
base accepted
assigned
```

`Bottom` extends `Middle`, which extends `Base`. A `Bottom` value is assignable to `Base` even though the inheritance is two levels deep.

### Parent to Child

A parent class value **cannot** be assigned to a child class variable. This is rejected at compile time.

```uranite
class Animal:

    public function Animal( self ) -> Void:
        pass

class Dog extends Animal:

    public function Dog( self ) -> Void:
        parent()

public function main() -> I32:
    Animal animal = new Animal()
    Dog dog = animal
    return 0
```

Error:

```
cannot assign value of type "Animal" to variable of type "Dog"
```

An `Animal` is not a `Dog` — the parent type lacks the child-specific fields and methods.

---

## Interface Conformance

### Class to Interface

A class that implements an interface can be assigned to a variable of that interface type or passed to a function expecting that interface.

```uranite
from uranite.io.console import puts

interface Greetable:

    public function greet( self ) -> String;

class Person implements Greetable:

    public String name

    public function Person( self, String name ) -> Void:
        self.name = name

    public function greet( self ) -> String:
        return "Hello " + self.name

public function sayHello( Greetable target ) -> Void:
    puts( target.greet() )

public function main() -> I32:
    Person person = new Person( "Alice" )
    sayHello( person )
    return 0
```

Output:

```
Hello Alice
```

`Person` implements `Greetable`, so a `Person` value can be passed where `Greetable` is expected. The interface method `greet` is dispatched to the `Person` implementation.

---

## Object Universality

`Object` is the root of the type hierarchy. Any value can be assigned to an `Object` variable or passed to a function expecting `Object`.

```uranite
from uranite.io.console import puts

public function acceptObject( Object value ) -> Void:
    puts( "object accepted" )

public function main() -> I32:
    acceptObject( 42 )
    acceptObject( "hello" )
    acceptObject( True )
    return 0
```

Output:

```
object accepted
object accepted
object accepted
```

Integers, strings, booleans, and all other types are assignable to `Object`.

---

## Optional Compatibility

### Concrete to Optional

A concrete value can be assigned to an optional variable of the same type. The value is automatically wrapped in the optional representation.

```uranite
from uranite.io.console import puts

public function main() -> I32:
    String concrete = "hello"
    ?String optional = concrete
    if optional is not None:
        puts( optional )
    return 0
```

Output:

```
hello
```

A `String` value is assignable to `?String`. The concrete value is promoted to the optional representation.

### None Assignment

`None` can be assigned to any optional type.

```uranite
from uranite.io.console import puts

public function main() -> I32:
    ?String empty = None
    ?I64 noNumber = None
    if empty is None:
        puts( "string is none" )
    if noNumber is None:
        puts( "number is none" )
    return 0
```

Output:

```
string is none
number is none
```

---

## Union Compatibility

### Member Type to Union

A value of any member type can be passed to a union-typed parameter. The compiler boxes the value into the union representation.

```uranite
from uranite.io.console import puts

public function acceptMixed( I64 | String value ) -> Void:
    puts( "accepted" )

public function main() -> I32:
    I64 number = 42
    String text = "hello"
    acceptMixed( number )
    acceptMixed( text )
    return 0
```

Output:

```
accepted
accepted
```

Both `I64` and `String` values are assignable to the `I64 | String` union parameter. A type outside the union members would be rejected.

---

## Comparison Compatibility

### Same-Type Comparisons

Values of the same type can be compared using `==`, `!=`, `<`, `>`, `<=`, and `>=`.

```uranite
from uranite.io.console import puts

public function main() -> I32:
    I64 alpha = 10
    I64 beta = 20
    Boolean equal = alpha == beta
    Boolean less = alpha < beta
    Boolean greater = alpha > beta
    puts( equal.toString() )
    puts( less.toString() )
    puts( greater.toString() )

    String first = "abc"
    String second = "def"
    Boolean strEqual = first == second
    puts( strEqual.toString() )
    return 0
```

Output:

```
False
True
False
False
```

Numeric comparisons produce the expected ordering results. String equality comparison checks whether the string contents match.

---

## Return Type Checking

The compiler verifies that every `return` statement produces a value compatible with the declared return type.

```uranite
public function getName() -> String:
    return "Alice"
```

This is valid because `"Alice"` is a `String` matching the declared return type.

A mismatched return type produces an error:

```uranite
public function getName() -> String:
    I64 value = 42
    return value
```

Error:

```
return type mismatch: expected "String" but found "I64"
```

The compiler rejects returning an `I64` from a function declared to return `String`.

---

## Incompatible Types

Types that have no compatibility relationship are rejected at compile time.

```uranite
public function main() -> I32:
    String text = 42
    return 0
```

Error:

```
cannot assign value of type "I64" to variable of type "String"
```

```uranite
public function main() -> I32:
    Boolean flag = "hello"
    return 0
```

Error:

```
cannot assign value of type "String" to variable of type "Boolean"
```

Incompatible assignments produce clear error messages naming both the source and target types.

---

## Method Reference

### Compatibility Rules Summary

| Source Type | Target Type | Compatible | Notes |
|---|---|---|---|
| `I32` | `I64` | Yes | Integer widening |
| `I64` | `F64` | Yes | Integer to float |
| `F32` | `F64` | Yes | Float widening |
| `F64` | `I64` | Yes | Truncates fractional part |
| Child class | Parent class | Yes | Inheritance chain |
| Parent class | Child class | No | Compile-time error |
| Implementing class | Interface | Yes | Interface conformance |
| Any type | `Object` | Yes | Universal root type |
| `T` | `?T` | Yes | Concrete to optional |
| `None` | `?T` | Yes | None to any optional |
| Union member | Union parameter | Yes | Member containment |
| `I64` | `String` | No | Incompatible types |
| `String` | `Boolean` | No | Incompatible types |

### Error Messages

| Error | Meaning |
|---|---|
| `cannot assign value of type "X" to variable of type "Y"` | Assignment type mismatch |
| `return type mismatch: expected "X" but found "Y"` | Return value does not match function signature |

---

## Examples

### Numeric Type Conversion

```uranite
from uranite.io.console import puts

public function acceptI64( I64 value ) -> Void:
    puts( value.toString() )

public function acceptF64( F64 value ) -> Void:
    puts( value.toString() )

public function main() -> I32:
    I32 smallInt = 10
    I64 bigInt = smallInt
    acceptI64( bigInt )

    I64 integer = 42
    F64 floating = integer
    acceptF64( floating )

    F32 smallFloat = 2.5
    F64 bigFloat = smallFloat
    acceptF64( bigFloat )

    F64 precise = 9.7
    I64 truncated = precise
    puts( truncated.toString() )
    return 0
```

Output:

```
10
42
2.5
9
```

### Class Hierarchy Assignability

```uranite
from uranite.io.console import puts

class Shape:

    public String shapeName

    public function Shape( self, String name ) -> Void:
        self.shapeName = name

class Circle extends Shape:

    public I64 radius

    public function Circle( self, I64 radius ) -> Void:
        parent( "circle" )
        self.radius = radius

class Square extends Shape:

    public I64 side

    public function Square( self, I64 side ) -> Void:
        parent( "square" )
        self.side = side

public function describeShape( Shape shape ) -> Void:
    puts( shape.shapeName )

public function main() -> I32:
    Circle circle = new Circle( 10 )
    Square square = new Square( 5 )

    describeShape( circle )
    describeShape( square )

    Shape asShape = circle
    puts( asShape.shapeName )
    return 0
```

Output:

```
circle
square
circle
```

### Interface Conformance Dispatch

```uranite
from uranite.io.console import puts

interface Describable:

    public function describe( self ) -> String;

class Product implements Describable:

    public String productName
    public I64 price

    public function Product( self, String name, I64 price ) -> Void:
        self.productName = name
        self.price = price

    public function describe( self ) -> String:
        return self.productName + " costs " + self.price.toString()

public function showDescription( Describable item ) -> Void:
    puts( item.describe() )

public function main() -> I32:
    Product laptop = new Product( "Laptop", 999 )
    Product phone = new Product( "Phone", 699 )

    showDescription( laptop )
    showDescription( phone )
    return 0
```

Output:

```
Laptop costs 999
Phone costs 699
```

### Optional Union and Object Compatibility

```uranite
from uranite.io.console import puts

public function processOptional( ?String value ) -> Void:
    if value is not None:
        puts( value )
    else:
        puts( "none" )

public function acceptUnion( I64 | String value ) -> Void:
    puts( "union accepted" )

public function acceptObject( Object value ) -> Void:
    puts( "object accepted" )

public function main() -> I32:
    String concrete = "hello"
    ?String optional = concrete
    processOptional( optional )
    processOptional( None )

    I64 number = 42
    String text = "world"
    acceptUnion( number )
    acceptUnion( text )

    acceptObject( 100 )
    acceptObject( "test" )
    acceptObject( True )
    return 0
```

Output:

```
hello
none
union accepted
union accepted
object accepted
object accepted
object accepted
```
