# Generic Types

---

## Table of Contents

- [Generic Types](#generic-types)
  - [Table of Contents](#table-of-contents)
  - [Overview](#overview)
  - [Generic Classes](#generic-classes)
    - [Single Type Parameter](#single-type-parameter)
    - [Multiple Type Parameters](#multiple-type-parameters)
    - [Instantiating Generic Classes](#instantiating-generic-classes)
  - [Generic Functions](#generic-functions)
    - [Declaring Generic Functions](#declaring-generic-functions)
    - [Calling Generic Functions](#calling-generic-functions)
  - [Generic Interfaces](#generic-interfaces)
  - [Generic Classes with Interfaces](#generic-classes-with-interfaces)
  - [Returning Generic Types](#returning-generic-types)
  - [Method Reference](#method-reference)
    - [Declaration Syntax](#declaration-syntax)
    - [Usage Syntax](#usage-syntax)
  - [Examples](#examples)
    - [Container with Swap](#container-with-swap)
    - [Named Value](#named-value)
    - [Key-Value Entry](#key-value-entry)
    - [Generic Factory Function](#generic-factory-function)

---

## Overview

Generic types allow classes, functions, and interfaces to work with any type without duplicating code. A generic definition declares one or more type parameters in angle brackets. At each usage site, concrete types are provided for the parameters. The compiler generates specialized code for each distinct combination of type arguments.

Generics enable writing reusable, type-safe abstractions. A single `Box<T>` class works with integers, strings, or any other type — each instantiation is fully type-checked.

---

## Generic Classes

### Single Type Parameter

Declare a type parameter in angle brackets after the class name. The parameter can be used as a field type, method parameter type, or return type throughout the class body.

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

Separate multiple type parameters with commas.

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

### Instantiating Generic Classes

When creating an instance, specify concrete types in angle brackets.

```uranite
Box<I64> numbers = new Box<I64>( 100 )
Box<String> text = new Box<String>( "world" )
Pair<String, I64> record = new Pair<String, I64>( "age", 25 )
```

Each combination of type arguments produces a distinct type. `Box<I64>` and `Box<String>` are separate types.

---

## Generic Functions

### Declaring Generic Functions

Functions can declare type parameters in angle brackets after the function name.

```uranite
public function identity<T>( T value ) -> T:
    return value
```

The type parameter `T` can be used for parameter types and the return type.

### Calling Generic Functions

Specify the concrete type when calling a generic function.

```uranite
from uranite.io.console import puts

public function identity<T>( T value ) -> T:
    return value

public function main() -> I32:
    I64 number = identity<I64>( 42 )
    puts( number.toString() )

    String text = identity<String>( "hello" )
    puts( text )
    return 0
```

Output:

```
42
hello
```

---

## Generic Interfaces

Interfaces can declare type parameters. Implementing classes specify concrete types for the parameters.

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
    CelsiusToFahrenheit temp = new CelsiusToFahrenheit( 100 )
    puts( temp.convert().toString() )
    return 0
```

Output:

```
212
```

---

## Generic Classes with Interfaces

A generic class can implement interfaces, combining generics with polymorphism.

```uranite
from uranite.io.console import puts

public interface Printable:

    public function display( self ) -> String;

class NamedValue<T> implements Printable:

    public String label
    public T value

    public function NamedValue( self, String label, T value ) -> Void:
        self.label = label
        self.value = value

    public function display( self ) -> String:
        return self.label

    public function getValue( self ) -> T:
        return self.value

public function main() -> I32:
    NamedValue<I64> score = new NamedValue<I64>( "Score", 100 )
    puts( score.display() )
    puts( score.getValue().toString() )

    NamedValue<String> greeting = new NamedValue<String>( "Hello", "World" )
    puts( greeting.display() )
    puts( greeting.getValue() )
    return 0
```

Output:

```
Score
100
Hello
World
```

---

## Returning Generic Types

Functions can return generic class instances.

```uranite
from uranite.io.console import puts

class Box<T>:

    public T value

    public function Box( self, T value ) -> Void:
        self.value = value

    public function getValue( self ) -> T:
        return self.value

public function makeBox<T>( T value ) -> Box<T>:
    return new Box<T>( value )

public function main() -> I32:
    Box<I64> box = makeBox<I64>( 99 )
    puts( box.getValue().toString() )
    return 0
```

Output:

```
99
```

---

## Method Reference

### Declaration Syntax

| Syntax | Description |
|---|---|
| `class Name<T>:` | Declares a generic class with one type parameter |
| `class Name<A, B>:` | Declares a generic class with multiple type parameters |
| `function name<T>( T value ) -> T:` | Declares a generic function |
| `interface Name<T>:` | Declares a generic interface |

### Usage Syntax

| Syntax | Description |
|---|---|
| `Box<I64>` | Instantiates a generic class with a concrete type |
| `new Box<I64>( value )` | Creates an instance of a generic class |
| `identity<String>( text )` | Calls a generic function with a concrete type |
| `implements Converter<I64>` | Implements a generic interface with a concrete type |

---

## Examples

### Container with Swap

```uranite
from uranite.io.console import puts

class Container<T>:

    public T stored

    public function Container( self, T value ) -> Void:
        self.stored = value

    public function get( self ) -> T:
        return self.stored

    public function set( self, T value ) -> Void:
        self.stored = value

public function swap<T>( Container<T> first, Container<T> second ) -> Void:
    T temp = first.get()
    first.set( second.get() )
    second.set( temp )

public function main() -> I32:
    Container<I64> alpha = new Container<I64>( 10 )
    Container<I64> beta = new Container<I64>( 20 )

    puts( alpha.get().toString() )
    puts( beta.get().toString() )

    swap<I64>( alpha, beta )

    puts( alpha.get().toString() )
    puts( beta.get().toString() )
    return 0
```

Output:

```
10
20
20
10
```

### Named Value

```uranite
from uranite.io.console import puts

public interface Printable:

    public function display( self ) -> String;

class NamedValue<T> implements Printable:

    public String label
    public T value

    public function NamedValue( self, String label, T value ) -> Void:
        self.label = label
        self.value = value

    public function display( self ) -> String:
        return self.label

    public function getValue( self ) -> T:
        return self.value

public function printItem( Printable item ) -> Void:
    puts( item.display() )

public function main() -> I32:
    NamedValue<I64> score = new NamedValue<I64>( "Score", 100 )
    printItem( score )
    puts( score.getValue().toString() )

    NamedValue<String> message = new NamedValue<String>( "Greeting", "Hello" )
    printItem( message )
    puts( message.getValue() )
    return 0
```

Output:

```
Score
100
Greeting
Hello
```

### Key-Value Entry

```uranite
from uranite.io.console import puts

class Entry<K, V>:

    public K key
    public V value

    public function Entry( self, K key, V value ) -> Void:
        self.key = key
        self.value = value

    public function getKey( self ) -> K:
        return self.key

    public function getValue( self ) -> V:
        return self.value

public function main() -> I32:
    Entry<String, I64> age = new Entry<String, I64>( "Alice", 30 )
    puts( age.getKey() )
    puts( age.getValue().toString() )

    Entry<I64, String> lookup = new Entry<I64, String>( 404, "Not Found" )
    puts( lookup.getKey().toString() )
    puts( lookup.getValue() )
    return 0
```

Output:

```
Alice
30
404
Not Found
```

### Generic Factory Function

```uranite
from uranite.io.console import puts

class Box<T>:

    public T value

    public function Box( self, T value ) -> Void:
        self.value = value

    public function getValue( self ) -> T:
        return self.value

public function createBox<T>( T value ) -> Box<T>:
    return new Box<T>( value )

public function main() -> I32:
    Box<I64> intBox = createBox<I64>( 42 )
    puts( intBox.getValue().toString() )

    Box<String> strBox = createBox<String>( "world" )
    puts( strBox.getValue() )
    return 0
```

Output:

```
42
world
```
