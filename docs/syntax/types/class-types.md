# Class Types

---

## Table of Contents

- [Class Types](#class-types)
  - [Table of Contents](#table-of-contents)
  - [Overview](#overview)
  - [Defining a Class](#defining-a-class)
    - [Fields](#fields)
    - [Constructors](#constructors)
    - [Methods](#methods)
    - [Static Methods](#static-methods)
  - [Creating Objects](#creating-objects)
  - [Accessing Fields](#accessing-fields)
  - [Calling Methods](#calling-methods)
  - [Visibility Modifiers](#visibility-modifiers)
    - [Public](#public)
    - [Private](#private)
    - [Protect](#protect)
  - [Constructor Overloading](#constructor-overloading)
  - [Mutable State](#mutable-state)
  - [Inheritance](#inheritance)
    - [Extending a Class](#extending-a-class)
    - [Parent Constructor](#parent-constructor)
    - [Accessing Inherited Fields](#accessing-inherited-fields)
    - [Method Override](#method-override)
  - [Abstract Classes](#abstract-classes)
  - [Final Classes](#final-classes)
  - [Readonly Classes](#readonly-classes)
  - [Interfaces](#interfaces)
    - [Defining an Interface](#defining-an-interface)
    - [Implementing an Interface](#implementing-an-interface)
    - [Multiple Interfaces](#multiple-interfaces)
  - [Generic Classes](#generic-classes)
    - [Single Type Parameter](#single-type-parameter)
    - [Multiple Type Parameters](#multiple-type-parameters)
  - [Self-Referential Classes](#self-referential-classes)
  - [Passing Objects to Functions](#passing-objects-to-functions)
  - [Returning Objects from Functions](#returning-objects-from-functions)
  - [Method Reference](#method-reference)
    - [Object Lifecycle](#object-lifecycle)
    - [Class Modifiers](#class-modifiers)
    - [Member Visibility](#member-visibility)
  - [Examples](#examples)
    - [Bank Account](#bank-account)
    - [Linked List Node](#linked-list-node)
    - [Shape Hierarchy](#shape-hierarchy)
    - [Generic Container](#generic-container)

---

## Overview

Classes are the primary mechanism for defining custom types in Uranite. A class bundles data (fields) and behavior (methods) into a single unit. Classes support single inheritance, multiple interface implementation, generic type parameters, constructor overloading, visibility modifiers, and several class-level modifiers including `abstract`, `final`, and `Readonly`.

Every class instance is heap-allocated. Variables holding class instances are references to the underlying object.

---

## Defining a Class

A class definition starts with the `class` keyword followed by the class name and a colon. The class body is indented.

```uranite
class Point:

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
class Person:

    public String name
    public I64 age
    private String identifier
```

### Constructors

A constructor is a method with the same name as the class. The first parameter is always `self`, which refers to the instance being constructed. Constructors return `Void`.

```uranite
class Rectangle:

    public I64 width
    public I64 height

    public function Rectangle( self, I64 width, I64 height ) -> Void:
        self.width = width
        self.height = height
```

### Methods

Methods are functions defined inside the class body. Instance methods take `self` as their first parameter.

```uranite
class Calculator:

    public I64 result

    public function Calculator( self ) -> Void:
        self.result = 0

    public function add( self, I64 value ) -> Void:
        self.result = self.result + value

    public function getResult( self ) -> I64:
        return self.result
```

### Static Methods

Static methods belong to the class itself rather than to any instance. They do not take `self` as a parameter. Call them using `ClassName.methodName()`.

```uranite
class MathHelper:

    public static function add( I64 first, I64 second ) -> I64:
        return first + second

    public static function multiply( I64 first, I64 second ) -> I64:
        return first * second
```

Usage:

```uranite
I64 sum = MathHelper.add( 10, 20 )
I64 product = MathHelper.multiply( 5, 6 )
```

---

## Creating Objects

Create an instance using the `new` keyword followed by the class name and constructor arguments.

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
Point point = new Point( 3, 7 )
I64 total = point.sum()
```

For static methods, use the class name directly.

```uranite
I64 result = MathHelper.add( 10, 20 )
```

---

## Visibility Modifiers

Every field and method in a class must have a visibility modifier.

### Public

Accessible from anywhere.

```uranite
class Data:

    public I64 value
```

### Private

Accessible only within the class itself.

```uranite
class Secret:

    private String code

    public function Secret( self, String code ) -> Void:
        self.code = code

    public function getCode( self ) -> String:
        return self.code
```

### Protect

Accessible within the class and its subclasses.

```uranite
class Base:

    protect I64 internalValue

    public function Base( self, I64 value ) -> Void:
        self.internalValue = value
```

---

## Constructor Overloading

A class can have multiple constructors with different parameter lists. The correct constructor is selected based on the arguments provided.

```uranite
from uranite.io.console import puts

class Counter:

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

    Counter customCounter = new Counter( 100 )
    puts( customCounter.getCount().toString() )
    return 0
```

Output:

```
0
100
```

---

## Mutable State

Fields can be modified through methods. Assign new values using `self.field = value`.

```uranite
from uranite.io.console import puts

class Counter:

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
    counter.decrement()
    puts( counter.getCount().toString() )
    return 0
```

Output:

```
2
```

---

## Inheritance

### Extending a Class

A class can inherit from one parent class using the `extends` keyword. The child class receives all fields from the parent.

```uranite
class Animal:

    public String name
    public I64 age

    public function Animal( self, String name, I64 age ) -> Void:
        self.name = name
        self.age = age

class Dog extends Animal:

    public String breed

    public function Dog( self, String name, I64 age, String breed ) -> Void:
        parent( name, age )
        self.breed = breed
```

### Parent Constructor

Call the parent constructor using `parent( arguments )` inside the child constructor. This initializes the inherited fields.

```uranite
class Vehicle:

    public String brand
    public I64 year

    public function Vehicle( self, String brand, I64 year ) -> Void:
        self.brand = brand
        self.year = year

class Car extends Vehicle:

    public I64 doors

    public function Car( self, String brand, I64 year, I64 doors ) -> Void:
        parent( brand, year )
        self.doors = doors
```

### Accessing Inherited Fields

Child classes have full access to inherited fields. Define methods on the child class to work with inherited fields.

```uranite
from uranite.io.console import puts

class Vehicle:

    public String brand
    public I64 year

    public function Vehicle( self, String brand, I64 year ) -> Void:
        self.brand = brand
        self.year = year

class Car extends Vehicle:

    public I64 doors

    public function Car( self, String brand, I64 year, I64 doors ) -> Void:
        parent( brand, year )
        self.doors = doors

    public function describe( self ) -> String:
        return self.brand

    public function getDoors( self ) -> I64:
        return self.doors

    public function getYear( self ) -> I64:
        return self.year

public function main() -> I32:
    Car car = new Car( "Toyota", 2024, 4 )
    puts( car.describe() )
    puts( car.getDoors().toString() )
    puts( car.getYear().toString() )
    return 0
```

Output:

```
Toyota
4
2024
```

Inherited fields can also be accessed directly via dot notation on the child instance.

```uranite
puts( car.brand )
puts( car.year.toString() )
```

### Method Override

A child class can override methods from the parent by defining a method with the same name and signature. The child version replaces the parent version for instances of the child class.

```uranite
class Shape:

    public function name( self ) -> String:
        return "Shape"

class Circle extends Shape:

    public I64 radius

    public function Circle( self, I64 radius ) -> Void:
        self.radius = radius

    public function name( self ) -> String:
        return "Circle"
```

---

## Abstract Classes

An abstract class cannot be instantiated directly. It serves as a base for subclasses. Abstract methods declare a signature with a semicolon instead of a body, requiring subclasses to provide an implementation.

```uranite
abstract class Shape:

    public function area( self ) -> I64;

    public function name( self ) -> String;
```

Subclasses must implement all abstract methods.

```uranite
from uranite.io.console import puts

abstract class Shape:

    public function area( self ) -> I64;

    public function name( self ) -> String;

class Square extends Shape:

    public I64 side

    public function Square( self, I64 side ) -> Void:
        self.side = side

    public function area( self ) -> I64:
        return self.side * self.side

    public function name( self ) -> String:
        return "Square"

public function main() -> I32:
    Square square = new Square( 5 )
    puts( square.name() )
    puts( square.area().toString() )
    return 0
```

Output:

```
Square
25
```

---

## Final Classes

A final class cannot be extended. Use `final` to prevent inheritance.

```uranite
final class Constant:

    public I64 value

    public function Constant( self, I64 value ) -> Void:
        self.value = value

    public function getValue( self ) -> I64:
        return self.value
```

Attempting to extend a final class produces a compile-time error.

---

## Readonly Classes

A `Readonly` class creates immutable instances. Fields are set once in the constructor and cannot be modified afterward.

```uranite
from uranite.io.console import puts

Readonly class Config:

    public String name
    public I64 version

    public function Config( self, String name, I64 version ) -> Void:
        self.name = name
        self.version = version

    public function describe( self ) -> String:
        return self.name

public function main() -> I32:
    Config config = new Config( "MyApp", 3 )
    puts( config.name )
    puts( config.version.toString() )
    puts( config.describe() )
    return 0
```

Output:

```
MyApp
3
MyApp
```

---

## Interfaces

### Defining an Interface

An interface declares method signatures without implementations. Methods end with a semicolon instead of a body.

```uranite
public interface Drawable:

    public function draw( self ) -> String;
```

### Implementing an Interface

A class uses the `implements` keyword to adopt an interface. The class must provide implementations for all declared methods.

```uranite
from uranite.io.console import puts

public interface Drawable:

    public function draw( self ) -> String;

class Circle implements Drawable:

    public I64 radius

    public function Circle( self, I64 radius ) -> Void:
        self.radius = radius

    public function draw( self ) -> String:
        return "Drawing circle"

    public function getRadius( self ) -> I64:
        return self.radius

public function main() -> I32:
    Circle circle = new Circle( 10 )
    puts( circle.draw() )
    puts( circle.getRadius().toString() )
    return 0
```

Output:

```
Drawing circle
10
```

### Multiple Interfaces

A class can implement multiple interfaces by separating them with commas.

```uranite
from uranite.io.console import puts

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

public function main() -> I32:
    Widget widget = new Widget( "Button", 50 )
    puts( widget.display() )
    puts( widget.measure().toString() )
    return 0
```

Output:

```
Button
50
```

---

## Generic Classes

### Single Type Parameter

Classes can accept type parameters, making them work with any type.

```uranite
from uranite.io.console import puts

class Box<T>:

    public T value

    public function Box( self, T value ) -> Void:
        self.value = value

    public function getValue( self ) -> T:
        return self.value

public function main() -> I32:
    Box<I64> intBox = new Box<I64>( 42 )
    puts( intBox.getValue().toString() )

    Box<String> strBox = new Box<String>( "hello" )
    puts( strBox.getValue() )
    return 0
```

Output:

```
42
hello
```

### Multiple Type Parameters

Classes can have multiple type parameters separated by commas.

```uranite
from uranite.io.console import puts

class Pair<A, B>:

    public A first
    public B second

    public function Pair( self, A first, B second ) -> Void:
        self.first = first
        self.second = second

    public function getFirst( self ) -> A:
        return self.first

    public function getSecond( self ) -> B:
        return self.second

public function main() -> I32:
    Pair<I64, String> entry = new Pair<I64, String>( 1, "one" )
    puts( entry.getFirst().toString() )
    puts( entry.getSecond() )
    return 0
```

Output:

```
1
one
```

---

## Self-Referential Classes

A class can reference its own type in field declarations using the optional type prefix `?` for nullable references.

```uranite
from uranite.io.console import puts

class Node:

    public I64 value
    public ?Node next

    public function Node( self, I64 value ) -> Void:
        self.value = value
        self.next = None

    public function getValue( self ) -> I64:
        return self.value

    public function setNext( self, Node next ) -> Void:
        self.next = next

    public function hasNext( self ) -> Boolean:
        return self.next is not None

public function main() -> I32:
    Node first = new Node( 10 )
    Node second = new Node( 20 )
    first.setNext( second )
    puts( first.getValue().toString() )
    puts( first.hasNext().toString() )
    return 0
```

Output:

```
10
True
```

Use `?ClassName` to declare a field that can hold either an instance or `None`. Check for `None` using `is None` or `is not None`.

---

## Passing Objects to Functions

Objects are passed to functions by reference. The function receives the same instance.

```uranite
from uranite.io.console import puts

class Logger:

    public String prefix

    public function Logger( self, String prefix ) -> Void:
        self.prefix = prefix

    public function log( self, String message ) -> Void:
        puts( self.prefix )
        puts( message )

public function printWithLogger( Logger logger, String message ) -> Void:
    logger.log( message )

public function main() -> I32:
    Logger logger = new Logger( "[INFO]" )
    printWithLogger( logger, "System started" )
    return 0
```

Output:

```
[INFO]
System started
```

---

## Returning Objects from Functions

Functions can create and return class instances.

```uranite
class Point:

    public I64 x
    public I64 y

    public function Point( self, I64 x, I64 y ) -> Void:
        self.x = x
        self.y = y

public function createOrigin() -> Point:
    return new Point( 0, 0 )
```

---

## Method Reference

### Object Lifecycle

| Operation | Syntax | Description |
|---|---|---|
| Create | `new ClassName( args )` | Allocates and initializes an instance |
| Field access | `instance.field` | Reads a field value |
| Field assign | `instance.field = value` | Writes a field value |
| Method call | `instance.method( args )` | Calls an instance method |
| Static call | `ClassName.method( args )` | Calls a static method |
| Identity check | `instance is None` | Checks if reference is None |
| Type check | `instance is ClassName` | Checks instance type |

### Class Modifiers

| Modifier | Effect |
|---|---|
| `abstract` | Cannot be instantiated; may contain abstract methods |
| `final` | Cannot be extended by other classes |
| `Readonly` | Fields become immutable after construction |

### Member Visibility

| Modifier | Access |
|---|---|
| `public` | Accessible from anywhere |
| `private` | Accessible only within the defining class |
| `protect` | Accessible within the defining class and its subclasses |

---

## Examples

### Bank Account

```uranite
from uranite.io.console import puts

class BankAccount:

    private String owner
    private I64 balance

    public function BankAccount( self, String owner, I64 initialBalance ) -> Void:
        self.owner = owner
        self.balance = initialBalance

    public function deposit( self, I64 amount ) -> Void:
        self.balance = self.balance + amount

    public function withdraw( self, I64 amount ) -> Boolean:
        if self.balance >= amount:
            self.balance = self.balance - amount
            return True
        return False

    public function getBalance( self ) -> I64:
        return self.balance

    public function getOwner( self ) -> String:
        return self.owner

public function main() -> I32:
    BankAccount account = new BankAccount( "Alice", 1000 )
    account.deposit( 500 )
    puts( account.getBalance().toString() )

    Boolean success = account.withdraw( 200 )
    puts( success.toString() )
    puts( account.getBalance().toString() )
    return 0
```

Output:

```
1500
True
1300
```

### Linked List Node

```uranite
from uranite.io.console import puts

class ListNode:

    public I64 value
    public ?ListNode next

    public function ListNode( self, I64 value ) -> Void:
        self.value = value
        self.next = None

    public function getValue( self ) -> I64:
        return self.value

    public function setNext( self, ListNode node ) -> Void:
        self.next = node

    public function hasNext( self ) -> Boolean:
        return self.next is not None

public function main() -> I32:
    ListNode first = new ListNode( 10 )
    ListNode second = new ListNode( 20 )
    ListNode third = new ListNode( 30 )

    first.setNext( second )
    second.setNext( third )

    puts( first.getValue().toString() )
    puts( first.hasNext().toString() )
    puts( second.getValue().toString() )
    puts( third.getValue().toString() )
    puts( third.hasNext().toString() )
    return 0
```

Output:

```
10
True
20
30
False
```

### Shape Hierarchy

```uranite
from uranite.io.console import puts

abstract class Shape:

    public function area( self ) -> I64;

    public function name( self ) -> String;

class Rectangle extends Shape:

    public I64 width
    public I64 height

    public function Rectangle( self, I64 width, I64 height ) -> Void:
        self.width = width
        self.height = height

    public function area( self ) -> I64:
        return self.width * self.height

    public function name( self ) -> String:
        return "Rectangle"

class Square extends Shape:

    public I64 side

    public function Square( self, I64 side ) -> Void:
        self.side = side

    public function area( self ) -> I64:
        return self.side * self.side

    public function name( self ) -> String:
        return "Square"

public function main() -> I32:
    Rectangle rect = new Rectangle( 6, 4 )
    puts( rect.name() )
    puts( rect.area().toString() )

    Square square = new Square( 5 )
    puts( square.name() )
    puts( square.area().toString() )
    return 0
```

Output:

```
Rectangle
24
Square
25
```

### Generic Container

```uranite
from uranite.io.console import puts

class Container<T>:

    private T stored
    private Boolean occupied

    public function Container( self, T value ) -> Void:
        self.stored = value
        self.occupied = True

    public function get( self ) -> T:
        return self.stored

    public function set( self, T value ) -> Void:
        self.stored = value

    public function isOccupied( self ) -> Boolean:
        return self.occupied

public function main() -> I32:
    Container<I64> numbers = new Container<I64>( 42 )
    puts( numbers.get().toString() )
    puts( numbers.isOccupied().toString() )

    numbers.set( 100 )
    puts( numbers.get().toString() )

    Container<String> text = new Container<String>( "hello" )
    puts( text.get() )
    return 0
```

Output:

```
42
True
100
hello
```
