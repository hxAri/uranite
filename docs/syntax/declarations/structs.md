# Structs

- [Table of Contents](#table-of-contents)

## Table of Contents

- [Structs](#structs)
  - [Table of Contents](#table-of-contents)
  - [Overview](#overview)
  - [Struct Declarations](#struct-declarations)
  - [Fields and Methods](#fields-and-methods)
  - [Constructors](#constructors)
    - [Constructor Overloading](#constructor-overloading)
  - [Visibility Modifiers](#visibility-modifiers)
  - [Direct Field Access](#direct-field-access)
  - [Structs vs Classes](#structs-vs-classes)

## Overview

Structs are lightweight composite types that group related fields and methods together. Unlike classes, structs do not support inheritance or interface implementation. They are suited for simple data containers, value-oriented types, and cases where the full machinery of class hierarchies is unnecessary.

Struct instances are created with the `new` keyword, just like classes. Structs support constructors, method definitions, properties, field modifiers (`readonly`, `final`, `static`), and constructor overloading. Structs do not support `extends` or `implements`.

## Struct Declarations

A struct declaration starts with a visibility modifier, the `struct` keyword, a name, a colon, and an indented body.

```uranite
package testing

from uranite.io.console import puts

public struct Vector2D:
    public F64 coordX
    public F64 coordY

    public function Vector2D( self, F64 coordX, F64 coordY ) -> Void:
        self.coordX = coordX
        self.coordY = coordY

    public function magnitude( self ) -> F64:
        return self.coordX * self.coordX + self.coordY * self.coordY

public function main() -> I32:
    Vector2D vec = new Vector2D( 3.0, 4.0 )
    puts( vec.magnitude() )
    return 0
```

Output:

```
25
```

The constructor follows the same pattern as class constructors — a function with the same name as the struct, taking `self` as the first parameter.

## Fields and Methods

Struct fields declare per-instance data. Methods define operations on the struct. Instance methods take `self` as their first parameter.

```uranite
package testing

from uranite.io.console import puts

public struct Color:
    public I64 red
    public I64 green
    public I64 blue

    public function Color( self, I64 red, I64 green, I64 blue ) -> Void:
        self.red = red
        self.green = green
        self.blue = blue

    public function isWhite( self ) -> Boolean:
        if self.red == 255 and self.green == 255 and self.blue == 255:
            return True
        return False

public function main() -> I32:
    Color white = new Color( 255, 255, 255 )
    Color dark = new Color( 0, 0, 0 )
    puts( white.isWhite() )
    puts( dark.isWhite() )
    return 0
```

Output:

```
True
False
```

Fields support the same modifiers as class fields: `public`, `private`, `protect`, `readonly`, `final`, and `static`.

## Constructors

A struct constructor initializes the fields of a new instance. It is declared as a function with the same name as the struct. The first parameter must be `self`.

```uranite
package testing

from uranite.io.console import puts

public struct Dimensions:
    public I64 width
    public I64 height

    public function Dimensions( self, I64 width, I64 height ) -> Void:
        self.width = width
        self.height = height

    public function area( self ) -> I64:
        return self.width * self.height

public function main() -> I32:
    Dimensions screen = new Dimensions( 1920, 1080 )
    puts( screen.area() )
    return 0
```

Output:

```
2073600
```

### Constructor Overloading

Structs can define multiple constructors with different parameter signatures. The compiler selects the correct constructor based on the number and types of arguments at the call site.

```uranite
package testing

from uranite.io.console import puts

public struct Rect:
    public I64 width
    public I64 height

    public function Rect( self, I64 width, I64 height ) -> Void:
        self.width = width
        self.height = height

    public function Rect( self, I64 side ) -> Void:
        self.width = side
        self.height = side

    public function area( self ) -> I64:
        return self.width * self.height

public function main() -> I32:
    Rect rect = new Rect( 10, 5 )
    puts( rect.area() )
    Rect square = new Rect( 7 )
    puts( square.area() )
    return 0
```

Output:

```
50
49
```

## Visibility Modifiers

Struct members support the same visibility modifiers as class members.

| Modifier | Effect |
|---|---|
| `public` | Accessible from any code |
| `private` | Accessible only within the struct |
| `protect` | Accessible within the struct |

```uranite
package testing

from uranite.io.console import puts

public struct Account:
    private I64 balance

    public function Account( self, I64 balance ) -> Void:
        self.balance = balance

    public function getBalance( self ) -> I64:
        return self.balance

    public function deposit( self, I64 amount ) -> Void:
        self.balance = self.balance + amount

public function main() -> I32:
    Account account = new Account( 1000 )
    account.deposit( 500 )
    puts( account.getBalance() )
    return 0
```

Output:

```
1500
```

## Direct Field Access

Struct fields can be accessed directly on the instance using dot notation, just like class fields.

```uranite
package testing

from uranite.io.console import puts

public struct Point:
    public I64 coordX
    public I64 coordY

    public function Point( self, I64 coordX, I64 coordY ) -> Void:
        self.coordX = coordX
        self.coordY = coordY

public function main() -> I32:
    Point point = new Point( 10, 20 )
    puts( point.coordX )
    puts( point.coordY )
    return 0
```

Output:

```
10
20
```

## Structs vs Classes

| Feature | Struct | Class |
|---|---|---|
| Fields and methods | Yes | Yes |
| Constructors | Yes | Yes |
| Constructor overloading | Yes | Yes |
| Visibility modifiers | Yes | Yes |
| Properties | Yes | Yes |
| Inheritance (`extends`) | No | Yes |
| Interface implementation (`implements`) | No | Yes |
| Abstract members | No | Yes |
| Virtual dispatch | No | Yes |
| `final` / `Readonly` class modifiers | No | Yes |

Use structs for simple data types that do not need inheritance or polymorphism. Use classes when you need type hierarchies, virtual dispatch, or interface contracts.
