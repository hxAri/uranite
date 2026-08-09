# Interface Types

---

## Table of Contents

- [Interface Types](#interface-types)
  - [Table of Contents](#table-of-contents)
  - [Overview](#overview)
  - [Defining an Interface](#defining-an-interface)
  - [Implementing an Interface](#implementing-an-interface)
  - [Multiple Method Contracts](#multiple-method-contracts)
  - [Multiple Interfaces](#multiple-interfaces)
  - [Interface Inheritance](#interface-inheritance)
  - [Generic Interfaces](#generic-interfaces)
  - [Polymorphic Dispatch](#polymorphic-dispatch)
  - [Interface-Typed Parameters](#interface-typed-parameters)
  - [Method Reference](#method-reference)
    - [Declaration Syntax](#declaration-syntax)
    - [Implementation Rules](#implementation-rules)
  - [Examples](#examples)
    - [Shape Hierarchy](#shape-hierarchy)
    - [Validator Pattern](#validator-pattern)
    - [Temperature Converter](#temperature-converter)
    - [Describable Items](#describable-items)

---

## Overview

Interfaces define contracts that classes must fulfill. An interface declares method signatures without providing implementations. Classes adopt interfaces using the `implements` keyword and must provide concrete implementations for every declared method.

Interfaces enable polymorphism — a function that accepts an interface-typed parameter can work with any class that implements that interface, dispatching to the correct method implementation at runtime.

Interfaces support inheritance via `extends`, generic type parameters, and multiple interface implementation on a single class.

---

## Defining an Interface

An interface is declared with the `interface` keyword. Method signatures end with a semicolon instead of a body.

```uranite
public interface Greetable:

    public function greet( self ) -> String;
```

Each method in an interface takes `self` as its first parameter, just like class instance methods. The semicolon after the method signature indicates there is no body — implementing classes provide the body.

---

## Implementing an Interface

A class uses the `implements` keyword to adopt an interface. The class must define every method declared by the interface with a matching signature.

```uranite
from uranite.io.console import puts

public interface Greetable:

    public function greet( self ) -> String;

class Person implements Greetable:

    public String name

    public function Person( self, String name ) -> Void:
        self.name = name

    public function greet( self ) -> String:
        return self.name

public function main() -> I32:
    Person person = new Person( "Alice" )
    puts( person.greet() )
    return 0
```

Output:

```
Alice
```

---

## Multiple Method Contracts

Interfaces can declare any number of methods.

```uranite
public interface Validator:

    public function isValid( self ) -> Boolean;

    public function errorMessage( self ) -> String;
```

Implementing classes must provide all declared methods.

```uranite
class AgeValidator implements Validator:

    public I64 age

    public function AgeValidator( self, I64 age ) -> Void:
        self.age = age

    public function isValid( self ) -> Boolean:
        if self.age >= 0:
            if self.age <= 150:
                return True
        return False

    public function errorMessage( self ) -> String:
        return "Invalid age"
```

---

## Multiple Interfaces

A class can implement multiple interfaces by separating them with commas.

```uranite
public interface Printable:

    public function display( self ) -> String;

public interface Measurable:

    public function measure( self ) -> I64;

class Widget implements Printable, Measurable:

    public String label
    public I64 weight

    public function Widget( self, String label, I64 weight ) -> Void:
        self.label = label
        self.weight = weight

    public function display( self ) -> String:
        return self.label

    public function measure( self ) -> I64:
        return self.weight
```

The class must implement every method from every interface it adopts.

---

## Interface Inheritance

Interfaces can extend other interfaces using the `extends` keyword. An interface can extend multiple parent interfaces, separated by commas.

```uranite
from uranite.io.console import puts

public interface Sizeable:

    public function size( self ) -> I64;

public interface Nameable:

    public function label( self ) -> String;

public interface Describable extends Sizeable, Nameable:

    public function describe( self ) -> String;

class Item implements Describable:

    public String itemName
    public I64 itemSize

    public function Item( self, String itemName, I64 itemSize ) -> Void:
        self.itemName = itemName
        self.itemSize = itemSize

    public function size( self ) -> I64:
        return self.itemSize

    public function label( self ) -> String:
        return self.itemName

    public function describe( self ) -> String:
        return self.itemName

public function main() -> I32:
    Item item = new Item( "Widget", 42 )
    puts( item.label() )
    puts( item.size().toString() )
    puts( item.describe() )
    return 0
```

Output:

```
Widget
42
Widget
```

A class implementing `Describable` must also implement methods from `Sizeable` and `Nameable`, since `Describable` extends both.

---

## Generic Interfaces

Interfaces can accept type parameters, making them work with any type.

```uranite
from uranite.io.console import puts

public interface Converter<T>:

    public function convert( self ) -> T;

class Temperature implements Converter<I64>:

    public I64 celsius

    public function Temperature( self, I64 celsius ) -> Void:
        self.celsius = celsius

    public function convert( self ) -> I64:
        return self.celsius * 9 / 5 + 32

public function main() -> I32:
    Temperature temp = new Temperature( 100 )
    I64 fahrenheit = temp.convert()
    puts( fahrenheit.toString() )
    return 0
```

Output:

```
212
```

When implementing a generic interface, the class specifies concrete types for the type parameters.

---

## Polymorphic Dispatch

When a function parameter is typed as an interface, any class implementing that interface can be passed as an argument. The correct method implementation is called at runtime based on the actual object type.

```uranite
from uranite.io.console import puts

public interface Shape:

    public function area( self ) -> I64;

    public function name( self ) -> String;

class Circle implements Shape:

    public I64 radius

    public function Circle( self, I64 radius ) -> Void:
        self.radius = radius

    public function area( self ) -> I64:
        return 3 * self.radius * self.radius

    public function name( self ) -> String:
        return "Circle"

class Square implements Shape:

    public I64 side

    public function Square( self, I64 side ) -> Void:
        self.side = side

    public function area( self ) -> I64:
        return self.side * self.side

    public function name( self ) -> String:
        return "Square"

public function printShape( Shape shape ) -> Void:
    puts( shape.name() )
    puts( shape.area().toString() )

public function main() -> I32:
    Circle circle = new Circle( 5 )
    printShape( circle )

    Square square = new Square( 4 )
    printShape( square )
    return 0
```

Output:

```
Circle
75
Square
16
```

The `printShape` function accepts any `Shape` implementation. When called with a `Circle`, it dispatches to `Circle.name()` and `Circle.area()`. When called with a `Square`, it dispatches to the `Square` implementations.

---

## Interface-Typed Parameters

Functions can accept interface-typed parameters to operate on any implementing class.

```uranite
public interface Validator:

    public function isValid( self ) -> Boolean;

    public function errorMessage( self ) -> String;

public function validate( Validator validator ) -> Void:
    if validator.isValid():
        puts( "Valid" )
    else:
        puts( validator.errorMessage() )
```

Any class implementing `Validator` can be passed to `validate`.

---

## Method Reference

### Declaration Syntax

| Syntax | Description |
|---|---|
| `public interface Name:` | Declares an interface |
| `public function method( self ) -> Type;` | Declares a method signature |
| `extends InterfaceA, InterfaceB` | Inherits from parent interfaces |
| `implements InterfaceA, InterfaceB` | Class adopts interfaces |

### Implementation Rules

| Rule | Description |
|---|---|
| All methods required | A class must implement every method from every adopted interface |
| Signature match | Parameter types and return type must match exactly |
| Inherited methods | Extending an interface inherits all parent methods |
| Multiple implementation | A class can implement any number of interfaces |

---

## Examples

### Shape Hierarchy

```uranite
from uranite.io.console import puts

public interface Shape:

    public function area( self ) -> I64;

    public function name( self ) -> String;

class Rectangle implements Shape:

    public I64 width
    public I64 height

    public function Rectangle( self, I64 width, I64 height ) -> Void:
        self.width = width
        self.height = height

    public function area( self ) -> I64:
        return self.width * self.height

    public function name( self ) -> String:
        return "Rectangle"

class Triangle implements Shape:

    public I64 base
    public I64 height

    public function Triangle( self, I64 base, I64 height ) -> Void:
        self.base = base
        self.height = height

    public function area( self ) -> I64:
        return self.base * self.height / 2

    public function name( self ) -> String:
        return "Triangle"

public function printShape( Shape shape ) -> Void:
    puts( shape.name() )
    puts( shape.area().toString() )

public function main() -> I32:
    Rectangle rect = new Rectangle( 6, 4 )
    printShape( rect )

    Triangle tri = new Triangle( 10, 5 )
    printShape( tri )
    return 0
```

Output:

```
Rectangle
24
Triangle
25
```

### Validator Pattern

```uranite
from uranite.io.console import puts

public interface Validator:

    public function isValid( self ) -> Boolean;

    public function errorMessage( self ) -> String;

class RangeValidator implements Validator:

    public I64 value
    public I64 minimum
    public I64 maximum

    public function RangeValidator( self, I64 value, I64 minimum, I64 maximum ) -> Void:
        self.value = value
        self.minimum = minimum
        self.maximum = maximum

    public function isValid( self ) -> Boolean:
        if self.value >= self.minimum:
            if self.value <= self.maximum:
                return True
        return False

    public function errorMessage( self ) -> String:
        return "Value out of range"

public function validate( Validator validator ) -> Void:
    if validator.isValid():
        puts( "Valid" )
    else:
        puts( validator.errorMessage() )

public function main() -> I32:
    RangeValidator valid = new RangeValidator( 50, 0, 100 )
    validate( valid )

    RangeValidator invalid = new RangeValidator( 200, 0, 100 )
    validate( invalid )
    return 0
```

Output:

```
Valid
Value out of range
```

### Temperature Converter

```uranite
from uranite.io.console import puts

public interface Converter<T>:

    public function convert( self ) -> T;

class CelsiusToFahrenheit implements Converter<I64>:

    public I64 celsius

    public function CelsiusToFahrenheit( self, I64 celsius ) -> Void:
        self.celsius = celsius

    public function convert( self ) -> I64:
        return self.celsius * 9 / 5 + 32

public function main() -> I32:
    CelsiusToFahrenheit boiling = new CelsiusToFahrenheit( 100 )
    puts( boiling.convert().toString() )

    CelsiusToFahrenheit freezing = new CelsiusToFahrenheit( 0 )
    puts( freezing.convert().toString() )

    CelsiusToFahrenheit body = new CelsiusToFahrenheit( 37 )
    puts( body.convert().toString() )
    return 0
```

Output:

```
212
32
98
```

### Describable Items

```uranite
from uranite.io.console import puts

public interface Sizeable:

    public function size( self ) -> I64;

public interface Nameable:

    public function label( self ) -> String;

public interface Describable extends Sizeable, Nameable:

    public function describe( self ) -> String;

class Product implements Describable:

    public String productName
    public I64 productWeight

    public function Product( self, String productName, I64 productWeight ) -> Void:
        self.productName = productName
        self.productWeight = productWeight

    public function size( self ) -> I64:
        return self.productWeight

    public function label( self ) -> String:
        return self.productName

    public function describe( self ) -> String:
        return self.productName

public function main() -> I32:
    Product laptop = new Product( "Laptop", 2500 )
    puts( laptop.label() )
    puts( laptop.size().toString() )
    puts( laptop.describe() )

    Product phone = new Product( "Phone", 180 )
    puts( phone.label() )
    puts( phone.size().toString() )
    return 0
```

Output:

```
Laptop
2500
Laptop
Phone
180
```
