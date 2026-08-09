# Struct Types

---

## Table of Contents

- [Struct Types](#struct-types)
  - [Table of Contents](#table-of-contents)
  - [Overview](#overview)
  - [Defining a Struct](#defining-a-struct)
    - [Fields](#fields)
    - [Constructors](#constructors)
    - [Methods](#methods)
  - [Creating Struct Instances](#creating-struct-instances)
  - [Accessing Fields](#accessing-fields)
  - [Calling Methods](#calling-methods)
  - [Visibility Modifiers](#visibility-modifiers)
  - [Constructor Overloading](#constructor-overloading)
  - [Mutable State](#mutable-state)
  - [Passing Structs to Functions](#passing-structs-to-functions)
  - [Returning Structs from Functions](#returning-structs-from-functions)
  - [Struct vs Class](#struct-vs-class)
  - [Method Reference](#method-reference)
    - [Instance Lifecycle](#instance-lifecycle)
    - [Member Visibility](#member-visibility)
  - [Examples](#examples)
    - [Color Value](#color-value)
    - [Dimension Calculator](#dimension-calculator)
    - [Configuration Record](#configuration-record)
    - [Counter with Overloaded Constructors](#counter-with-overloaded-constructors)

---

## Overview

Structs are lightweight compound types for grouping related data with associated methods. Unlike classes, structs do not support inheritance, interface implementation, or virtual dispatch. Structs are best suited for plain data containers, coordinate types, records, and other value-oriented groupings where polymorphism is not needed.

Structs support fields, constructors, instance methods, constructor overloading, and visibility modifiers.

---

## Defining a Struct

A struct definition starts with the `struct` keyword followed by the struct name and a colon. The struct body is indented.

```uranite
public struct Point:

    public I64 x
    public I64 y

    public function Point( self, I64 x, I64 y ) -> Void:
        self.x = x
        self.y = y

    public function sum( self ) -> I64:
        return self.x + self.y
```

### Fields

Fields declare the data stored in each instance. Each field specifies a visibility modifier, a type, and a name.

```uranite
public struct Address:

    public String street
    public String city
    public I64 zipCode
```

### Constructors

A constructor is a method with the same name as the struct. The first parameter is always `self`. Constructors return `Void`.

```uranite
public struct Rectangle:

    public I64 width
    public I64 height

    public function Rectangle( self, I64 width, I64 height ) -> Void:
        self.width = width
        self.height = height
```

### Methods

Instance methods take `self` as their first parameter and operate on the struct's fields.

```uranite
public struct Rectangle:

    public I64 width
    public I64 height

    public function Rectangle( self, I64 width, I64 height ) -> Void:
        self.width = width
        self.height = height

    public function area( self ) -> I64:
        return self.width * self.height

    public function perimeter( self ) -> I64:
        return 2 * (self.width + self.height)
```

---

## Creating Struct Instances

Create a struct instance using the `new` keyword followed by the struct name and constructor arguments.

```uranite
Point origin = new Point( 0, 0 )
Rectangle rect = new Rectangle( 10, 5 )
```

---

## Accessing Fields

Access fields using dot notation.

```uranite
Point point = new Point( 3, 7 )
I64 horizontal = point.x
I64 vertical = point.y
```

---

## Calling Methods

Call methods using dot notation on an instance.

```uranite
Rectangle rect = new Rectangle( 10, 5 )
I64 size = rect.area()
I64 border = rect.perimeter()
```

---

## Visibility Modifiers

Struct fields and methods support the same visibility modifiers as classes.

| Modifier | Access |
|---|---|
| `public` | Accessible from anywhere |
| `private` | Accessible only within the struct |
| `protect` | Accessible within the struct |

```uranite
public struct Account:

    private I64 balance

    public function Account( self, I64 balance ) -> Void:
        self.balance = balance

    public function getBalance( self ) -> I64:
        return self.balance
```

---

## Constructor Overloading

Structs support multiple constructors with different parameter lists.

```uranite
from uranite.io.console import puts

public struct Counter:

    public I64 count

    public function Counter( self ) -> Void:
        self.count = 0

    public function Counter( self, I64 initial ) -> Void:
        self.count = initial

    public function getCount( self ) -> I64:
        return self.count

public function main() -> I32:
    Counter defaultCounter = new Counter()
    puts( defaultCounter.getCount().toString() )

    Counter customCounter = new Counter( 50 )
    puts( customCounter.getCount().toString() )
    return 0
```

Output:

```
0
50
```

---

## Mutable State

Struct fields can be modified through methods, just like class fields.

```uranite
from uranite.io.console import puts

public struct Counter:

    public I64 count

    public function Counter( self ) -> Void:
        self.count = 0

    public function increment( self ) -> Void:
        self.count = self.count + 1

    public function decrement( self ) -> Void:
        self.count = self.count - 1

    public function getCount( self ) -> I64:
        return self.count

public function main() -> I32:
    Counter counter = new Counter()
    counter.increment()
    counter.increment()
    counter.increment()
    puts( counter.getCount().toString() )
    return 0
```

Output:

```
3
```

---

## Passing Structs to Functions

Structs can be passed as function parameters. The function receives the same instance.

```uranite
from uranite.io.console import puts

public struct Config:

    public String name
    public I64 version

    public function Config( self, String name, I64 version ) -> Void:
        self.name = name
        self.version = version

    public function getName( self ) -> String:
        return self.name

    public function getVersion( self ) -> I64:
        return self.version

public function printConfig( Config config ) -> Void:
    puts( config.getName() )
    puts( config.getVersion().toString() )

public function main() -> I32:
    Config config = new Config( "MyApp", 3 )
    printConfig( config )
    return 0
```

Output:

```
MyApp
3
```

---

## Returning Structs from Functions

Functions can create and return struct instances.

```uranite
from uranite.io.console import puts

public struct Coordinate:

    public I64 x
    public I64 y

    public function Coordinate( self, I64 x, I64 y ) -> Void:
        self.x = x
        self.y = y

public function createOrigin() -> Coordinate:
    return new Coordinate( 0, 0 )

public function main() -> I32:
    Coordinate origin = createOrigin()
    puts( origin.x.toString() )
    puts( origin.y.toString() )
    return 0
```

Output:

```
0
0
```

---

## Struct vs Class

| Feature | Struct | Class |
|---|---|---|
| Inheritance | No | Single parent via `extends` |
| Interface implementation | No | Yes via `implements` |
| Virtual dispatch | No | Yes |
| Abstract methods | No | Yes |
| Constructor overloading | Yes | Yes |
| Instance methods | Yes | Yes |
| Field access modifiers | Yes | Yes |
| Mutable fields | Yes | Yes |

Use structs for simple data groupings. Use classes when you need inheritance, interfaces, or polymorphism.

---

## Method Reference

### Instance Lifecycle

| Operation | Syntax | Description |
|---|---|---|
| Create | `new StructName( args )` | Allocates and initializes an instance |
| Field access | `instance.field` | Reads a field value |
| Field assign | `instance.field = value` | Writes a field value |
| Method call | `instance.method( args )` | Calls an instance method |

### Member Visibility

| Modifier | Access |
|---|---|
| `public` | Accessible from anywhere |
| `private` | Accessible only within the struct |
| `protect` | Accessible within the struct |

---

## Examples

### Color Value

```uranite
from uranite.io.console import puts

public struct Color:

    public I64 red
    public I64 green
    public I64 blue

    public function Color( self, I64 red, I64 green, I64 blue ) -> Void:
        self.red = red
        self.green = green
        self.blue = blue

    public function getRed( self ) -> I64:
        return self.red

    public function getGreen( self ) -> I64:
        return self.green

    public function getBlue( self ) -> I64:
        return self.blue

public function main() -> I32:
    Color color = new Color( 255, 128, 64 )
    puts( color.getRed().toString() )
    puts( color.getGreen().toString() )
    puts( color.getBlue().toString() )
    return 0
```

Output:

```
255
128
64
```

### Dimension Calculator

```uranite
from uranite.io.console import puts

public struct Dimension:

    public I64 width
    public I64 height

    public function Dimension( self, I64 width, I64 height ) -> Void:
        self.width = width
        self.height = height

    public function area( self ) -> I64:
        return self.width * self.height

    public function perimeter( self ) -> I64:
        return 2 * (self.width + self.height)

    public function isSquare( self ) -> Boolean:
        return self.width == self.height

public function main() -> I32:
    Dimension rect = new Dimension( 5, 3 )
    puts( rect.area().toString() )
    puts( rect.perimeter().toString() )
    puts( rect.isSquare().toString() )

    Dimension square = new Dimension( 4, 4 )
    puts( square.isSquare().toString() )
    return 0
```

Output:

```
15
16
False
True
```

### Configuration Record

```uranite
from uranite.io.console import puts

public struct Config:

    public String name
    public I64 version

    public function Config( self, String name, I64 version ) -> Void:
        self.name = name
        self.version = version

    public function getName( self ) -> String:
        return self.name

    public function getVersion( self ) -> I64:
        return self.version

public function printConfig( Config config ) -> Void:
    puts( config.getName() )
    puts( config.getVersion().toString() )

public function main() -> I32:
    Config appConfig = new Config( "Uranite", 1 )
    printConfig( appConfig )

    Config dbConfig = new Config( "Database", 5 )
    printConfig( dbConfig )
    return 0
```

Output:

```
Uranite
1
Database
5
```

### Counter with Overloaded Constructors

```uranite
from uranite.io.console import puts

public struct Counter:

    public I64 count

    public function Counter( self ) -> Void:
        self.count = 0

    public function Counter( self, I64 initial ) -> Void:
        self.count = initial

    public function increment( self ) -> Void:
        self.count = self.count + 1

    public function getCount( self ) -> I64:
        return self.count

public function main() -> I32:
    Counter first = new Counter()
    first.increment()
    first.increment()
    puts( first.getCount().toString() )

    Counter second = new Counter( 100 )
    second.increment()
    puts( second.getCount().toString() )
    return 0
```

Output:

```
2
101
```
