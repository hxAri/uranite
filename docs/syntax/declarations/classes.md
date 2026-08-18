# Classes

- [Table of Contents](#table-of-contents)

## Table of Contents

- [Classes](#classes)
  - [Table of Contents](#table-of-contents)
  - [Overview](#overview)
  - [Class Declarations](#class-declarations)
  - [Fields](#fields)
    - [Field Modifiers](#field-modifiers)
    - [Field Default Values](#field-default-values)
  - [Constructors](#constructors)
    - [Constructor Overloading](#constructor-overloading)
  - [Methods](#methods)
    - [Static Methods](#static-methods)
  - [Properties](#properties)
  - [Inheritance](#inheritance)
    - [Virtual and Override](#virtual-and-override)
  - [Abstract Classes](#abstract-classes)
  - [Final Classes](#final-classes)
  - [Readonly Classes](#readonly-classes)
  - [Interface Implementation](#interface-implementation)
  - [Generic Classes](#generic-classes)

## Overview

Classes are the primary mechanism for defining custom types in Uranite. A class groups fields and methods into a single named type. Instances are created with the `new` keyword, which allocates memory and invokes the constructor. Uranite classes support single inheritance, interface implementation, generic type parameters, visibility modifiers, and constructor overloading.

Uranite uses ownership-based memory management with no garbage collector. When an instance goes out of scope and has no remaining references, its memory is released.

## Class Declarations

A class declaration starts with a visibility modifier, the `class` keyword, a name, optional generic parameters, optional inheritance and interface clauses, a colon, and an indented body.

```uranite
package testing

from uranite.io.console import puts

public class Animal:
    public String name
    public I64 age

    public function Animal( self, String name, I64 age ) -> Void:
        self.name = name
        self.age = age

    public function speak( self ) -> String:
        return self.name

public function main() -> I32:
    Animal dog = new Animal( "Rex", 5 )
    puts( dog.speak() )
    puts( dog.age )
    return 0
```

Output:

```
Rex
5
```

The `self` parameter in methods and constructors refers to the current instance. It must be the first parameter of every instance method and constructor.

## Fields

Fields declare per-instance data stored in every instance of the class. Each field has a visibility modifier, a type, and a name.

```uranite
public class Coordinate:
    public F64 latitude
    public F64 longitude
    private String label
```

### Field Modifiers

| Modifier | Effect |
|---|---|
| `public` | Accessible from any code |
| `private` | Accessible only within the class |
| `protect` | Accessible within the class and subclasses |
| `readonly` | Can be assigned once in the constructor, then immutable |
| `final` | Cannot be overridden in subclasses |
| `static` | Shared across all instances, accessed via the class name |

### Field Default Values

Fields can have default values assigned at declaration. If the constructor does not explicitly assign the field, the default value is used.

```uranite
package testing

from uranite.io.console import puts

public class Settings:
    public I64 maxRetries = 3
    public String protocol = "https"

    public function Settings( self ) -> Void:
        pass

public function main() -> I32:
    Settings settings = new Settings()
    puts( settings.maxRetries )
    puts( settings.protocol )
    return 0
```

Output:

```
3
https
```

## Constructors

A constructor is a method with the same name as the class. It initializes the fields of a new instance. The first parameter must be `self`.

```uranite
package testing

from uranite.io.console import puts

public class Point:
    public I64 pointX
    public I64 pointY

    public function Point( self, I64 pointX, I64 pointY ) -> Void:
        self.pointX = pointX
        self.pointY = pointY

public function main() -> I32:
    Point point = new Point( 10, 20 )
    puts( point.pointX )
    puts( point.pointY )
    return 0
```

Output:

```
10
20
```

### Constructor Overloading

A class can define multiple constructors with different parameter signatures. The compiler selects the correct constructor based on the arguments provided at the call site.

```uranite
package testing

from uranite.io.console import puts

public class Rectangle:
    public I64 width
    public I64 height

    public function Rectangle( self, I64 width, I64 height ) -> Void:
        self.width = width
        self.height = height

    public function Rectangle( self, I64 side ) -> Void:
        self.width = side
        self.height = side

    public function area( self ) -> I64:
        return self.width * self.height

public function main() -> I32:
    Rectangle rect = new Rectangle( 10, 5 )
    puts( rect.area() )
    Rectangle square = new Rectangle( 7 )
    puts( square.area() )
    return 0
```

Output:

```
50
49
```

## Methods

Methods are functions declared inside a class body. Instance methods take `self` as their first parameter.

```uranite
package testing

from uranite.io.console import puts

public class Calculator:
    public I64 value

    public function Calculator( self, I64 value ) -> Void:
        self.value = value

    public function add( self, I64 amount ) -> I64:
        return self.value + amount

    public function multiply( self, I64 factor ) -> I64:
        return self.value * factor

public function main() -> I32:
    Calculator calc = new Calculator( 10 )
    puts( calc.add( 5 ) )
    puts( calc.multiply( 3 ) )
    return 0
```

Output:

```
15
30
```

Methods support modifiers including `virtual`, `override`, `abstract`, `final`, `static`, and `async`.

### Static Methods

Static methods belong to the class rather than any instance. They do not take `self` as a parameter and are called on the class name directly.

```uranite
package testing

from uranite.io.console import puts

public class MathHelper:
    public static function add( I64 first, I64 second ) -> I64:
        return first + second

    public static function multiply( I64 first, I64 second ) -> I64:
        return first * second

public function main() -> I32:
    puts( MathHelper.add( 10, 20 ) )
    puts( MathHelper.multiply( 5, 6 ) )
    return 0
```

Output:

```
30
30
```

## Properties

Properties are methods accessed without parentheses, providing field-like syntax while executing method logic. Declare a property with the `property` keyword instead of `function`.

```uranite
package testing

from uranite.io.console import puts

public class Temperature:
    private F64 celsius

    public function Temperature( self, F64 celsius ) -> Void:
        self.celsius = celsius

    public property getCelsius( self ) -> F64:
        return self.celsius

public function main() -> I32:
    Temperature temp = new Temperature( 100.0 )
    puts( temp.getCelsius )
    return 0
```

Output:

```
100
```

Properties are invoked without parentheses at the call site (`temp.getCelsius` instead of `temp.getCelsius()`).

## Inheritance

A class can extend one base class using the `extends` keyword. The subclass inherits all fields and methods from the parent class. Use `parent()` in the constructor to call the parent constructor.

```uranite
package testing

from uranite.io.console import puts

public class Shape:
    public String kind

    public function Shape( self, String kind ) -> Void:
        self.kind = kind

public class Circle extends Shape:
    public F64 radius

    public function Circle( self, F64 radius ) -> Void:
        parent( "circle" )
        self.radius = radius

    public function area( self ) -> F64:
        return 3.14159 * self.radius * self.radius

public function main() -> I32:
    Circle circle = new Circle( 5.0 )
    puts( circle.kind )
    puts( circle.area() )
    return 0
```

Output:

```
circle
78.5397
```

Uranite supports single inheritance only. A class can extend at most one base class but can implement any number of interfaces.

### Virtual and Override

Use the `virtual` modifier on base class methods and `override` on subclass methods for polymorphic dispatch. When a method is declared `virtual`, subclasses can provide their own implementation with `override`.

```uranite
package testing

from uranite.io.console import puts

public class Shape:
    public String kind

    public function Shape( self, String kind ) -> Void:
        self.kind = kind

    public virtual function area( self ) -> F64:
        return 0.0

public class Circle extends Shape:
    public F64 radius

    public function Circle( self, F64 radius ) -> Void:
        parent( "circle" )
        self.radius = radius

    public override function area( self ) -> F64:
        return 3.14159 * self.radius * self.radius

public function main() -> I32:
    Circle circle = new Circle( 5.0 )
    puts( circle.kind )
    puts( circle.area() )
    return 0
```

Output:

```
circle
78.5397
```

## Abstract Classes

An abstract class cannot be instantiated directly. It serves as a base for subclasses that must implement its abstract methods. Declare abstract classes with the `abstract` keyword before `class`, and abstract methods with the `abstract` keyword before `function`. Abstract methods end with a semicolon instead of a colon and body.

```uranite
package testing

from uranite.io.console import puts

public abstract class Vehicle:
    public String brand

    public function Vehicle( self, String brand ) -> Void:
        self.brand = brand

    public abstract function fuelType( self ) -> String;

public class Car extends Vehicle:
    public function Car( self, String brand ) -> Void:
        parent( brand )

    public override function fuelType( self ) -> String:
        return "gasoline"

public function main() -> I32:
    Car car = new Car( "Toyota" )
    puts( car.brand )
    puts( car.fuelType() )
    return 0
```

Output:

```
Toyota
gasoline
```

## Final Classes

A final class cannot be extended. Use the `final` keyword before `class` to prevent inheritance. Attempting to extend a final class produces a compilation error.

```uranite
package testing

from uranite.io.console import puts

public final class Config:
    public String host
    public I64 port

    public function Config( self, String host, I64 port ) -> Void:
        self.host = host
        self.port = port

public function main() -> I32:
    Config config = new Config( "localhost", 8080 )
    puts( config.host )
    puts( config.port )
    return 0
```

Output:

```
localhost
8080
```

## Readonly Classes

A Readonly class marks all its fields as immutable after construction. Fields can be assigned in the constructor but cannot be modified afterward.

```uranite
package testing

from uranite.io.console import puts

public Readonly class Dimensions:
    public I64 width
    public I64 height

    public function Dimensions( self, I64 width, I64 height ) -> Void:
        self.width = width
        self.height = height

public function main() -> I32:
    Dimensions dim = new Dimensions( 1920, 1080 )
    puts( dim.width )
    puts( dim.height )
    return 0
```

Output:

```
1920
1080
```

## Interface Implementation

A class can implement one or more interfaces using the `implements` keyword. The class must provide concrete implementations for all methods declared in the interface.

```uranite
package testing

from uranite.io.console import puts

public interface Printable:
    public function display( self ) -> String;

public class Book implements Printable:
    public String title

    public function Book( self, String title ) -> Void:
        self.title = title

    public override function display( self ) -> String:
        return self.title

public function main() -> I32:
    Book book = new Book( "Uranite Guide" )
    puts( book.display() )
    return 0
```

Output:

```
Uranite Guide
```

A class can implement multiple interfaces by listing them after `implements` separated by commas. A class can also extend a base class and implement interfaces simultaneously.

## Generic Classes

Classes can accept type parameters using angle bracket syntax. Type parameters are substituted with concrete types at each usage site.

```uranite
package testing

from uranite.io.console import puts

public class Box<T>:
    public T value

    public function Box( self, T value ) -> Void:
        self.value = value

    public function get( self ) -> T:
        return self.value

public function main() -> I32:
    Box<I64> intBox = new Box<I64>( 42 )
    puts( intBox.get() )
    Box<String> strBox = new Box<String>( "hello" )
    puts( strBox.get() )
    return 0
```

Output:

```
42
hello
```
