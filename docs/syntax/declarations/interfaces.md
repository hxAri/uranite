# Interfaces

- [Table of Contents](#table-of-contents)

## Table of Contents

- [Interfaces](#interfaces)
  - [Table of Contents](#table-of-contents)
  - [Overview](#overview)
  - [Interface Declarations](#interface-declarations)
  - [Implementing Interfaces](#implementing-interfaces)
  - [Multiple Interface Implementation](#multiple-interface-implementation)
  - [Multiple Methods](#multiple-methods)
  - [Interface Inheritance](#interface-inheritance)
  - [Generic Interfaces](#generic-interfaces)
  - [Polymorphic Dispatch](#polymorphic-dispatch)
  - [Extends with Implements](#extends-with-implements)

## Overview

An interface declaration creates a named contract consisting of method signatures. Interfaces contain no fields and no method bodies. Every method signature ends with a semicolon. Classes that implement an interface must provide concrete implementations for all declared methods using the `override` keyword.

Interfaces enable polymorphism: a function that accepts an interface type can operate on any class that implements it. The correct method is dispatched at runtime based on the actual type of the object.

## Interface Declarations

An interface declaration starts with a visibility modifier, the `interface` keyword, a name, a colon, and an indented body of method signatures.

```uranite
package testing

from uranite.io.console import puts

public interface Describable:
    public function describe( self ) -> String;

public class Item implements Describable:
    public String label

    public function Item( self, String label ) -> Void:
        self.label = label

    public override function describe( self ) -> String:
        return self.label

public function main() -> I32:
    Item item = new Item( "widget" )
    puts( item.describe() )
    return 0
```

Output:

```
widget
```

Interface methods are declared with their full signature followed by a semicolon instead of a colon and body. The implementing class must use `override` on each method it provides.

## Implementing Interfaces

A class implements an interface using the `implements` keyword after the class name. The class must provide an `override` implementation for every method in the interface.

```uranite
package testing

from uranite.io.console import puts

public interface Logger:
    public function log( self, String message ) -> Void;

public class ConsoleLogger implements Logger:
    public function ConsoleLogger( self ) -> Void:
        pass

    public override function log( self, String message ) -> Void:
        puts( message )

public function main() -> I32:
    ConsoleLogger logger = new ConsoleLogger()
    logger.log( "application started" )
    return 0
```

Output:

```
application started
```

If a class does not implement all methods declared in its interface, the compiler produces an error.

## Multiple Interface Implementation

A class can implement multiple interfaces by listing them after `implements`, separated by commas. The class must provide implementations for all methods from every interface.

```uranite
package testing

from uranite.io.console import puts

public interface Readable:
    public function read( self ) -> String;

public interface Writable:
    public function write( self, String data ) -> Void;

public class FileStream implements Readable, Writable:
    public String content

    public function FileStream( self ) -> Void:
        self.content = ""

    public override function read( self ) -> String:
        return self.content

    public override function write( self, String data ) -> Void:
        self.content = data

public function main() -> I32:
    FileStream stream = new FileStream()
    stream.write( "hello world" )
    puts( stream.read() )
    return 0
```

Output:

```
hello world
```

Both `read` and `write` are called directly on the concrete `FileStream` type. The class satisfies the contracts of both `Readable` and `Writable`.

## Multiple Methods

An interface can declare any number of method signatures. The implementing class must provide `override` implementations for all of them.

```uranite
package testing

from uranite.io.console import puts

public interface Validator:
    public function validate( self, String input ) -> Boolean;
    public function errorMessage( self ) -> String;

public class LengthValidator implements Validator:
    public I64 minLength

    public function LengthValidator( self, I64 minLength ) -> Void:
        self.minLength = minLength

    public override function validate( self, String input ) -> Boolean:
        if input.length() >= self.minLength:
            return True
        return False

    public override function errorMessage( self ) -> String:
        return "too short"

public function main() -> I32:
    LengthValidator checker = new LengthValidator( 5 )
    puts( checker.validate( "hello" ) )
    puts( checker.validate( "hi" ) )
    puts( checker.errorMessage() )
    return 0
```

Output:

```
True
False
too short
```

The `Validator` interface declares two methods. `LengthValidator` provides both.

## Interface Inheritance

An interface can extend one or more other interfaces using the `extends` keyword. The extending interface inherits all method requirements from its parent interfaces and can add new ones. Any class implementing the child interface must implement all methods from the entire hierarchy.

```uranite
package testing

from uranite.io.console import puts

public interface Shape:
    public function area( self ) -> I64;

public interface Resizable extends Shape:
    public function resize( self, I64 factor ) -> Void;

public class Square implements Resizable:
    public I64 side

    public function Square( self, I64 side ) -> Void:
        self.side = side

    public override function area( self ) -> I64:
        return self.side * self.side

    public override function resize( self, I64 factor ) -> Void:
        self.side = self.side * factor

public function main() -> I32:
    Square sq = new Square( 5 )
    puts( sq.area() )
    sq.resize( 2 )
    puts( sq.area() )
    return 0
```

Output:

```
25
100
```

`Square` implements `Resizable`, which extends `Shape`. Therefore `Square` must implement both `area` (from `Shape`) and `resize` (from `Resizable`).

## Generic Interfaces

Interfaces can accept type parameters using angle bracket syntax. Implementing classes substitute concrete types for the parameters.

```uranite
package testing

from uranite.io.console import puts

public interface Container<T>:
    public function get( self ) -> T;
    public function set( self, T value ) -> Void;

public class Holder<T> implements Container<T>:
    public T stored

    public function Holder( self, T stored ) -> Void:
        self.stored = stored

    public override function get( self ) -> T:
        return self.stored

    public override function set( self, T value ) -> Void:
        self.stored = value

public function main() -> I32:
    Holder<I64> holder = new Holder<I64>( 10 )
    puts( holder.get() )
    holder.set( 99 )
    puts( holder.get() )
    return 0
```

Output:

```
10
99
```

`Container<T>` declares generic method signatures. `Holder<T>` implements `Container<T>` and forwards its own type parameter. At usage, `Holder<I64>` concretely substitutes `I64` for `T`.

## Polymorphic Dispatch

A function that accepts an interface type can receive any object whose class implements that interface. The correct method implementation is dispatched at runtime.

```uranite
package testing

from uranite.io.console import puts

public interface Greetable:
    public function greet( self ) -> String;

public class English implements Greetable:
    public function English( self ) -> Void:
        pass

    public override function greet( self ) -> String:
        return "hello"

public class Spanish implements Greetable:
    public function Spanish( self ) -> Void:
        pass

    public override function greet( self ) -> String:
        return "hola"

public function sayHello( Greetable speaker ) -> Void:
    puts( speaker.greet() )

public function main() -> I32:
    English eng = new English()
    Spanish spa = new Spanish()
    sayHello( eng )
    sayHello( spa )
    return 0
```

Output:

```
hello
hola
```

The `sayHello` function accepts any `Greetable`. When called with an `English` instance, it dispatches to `English.greet()`. When called with a `Spanish` instance, it dispatches to `Spanish.greet()`.

## Extends with Implements

A class can extend a base class and implement interfaces simultaneously. The base class is listed after `extends`, and the interface is listed after `implements`.

```uranite
package testing

from uranite.io.console import puts

public interface Logger:
    public function log( self, String message ) -> Void;

public class Animal extends Object implements Logger:
    public String name

    public function Animal( self, String name ) -> Void:
        self.name = name

    public override function log( self, String message ) -> Void:
        puts( message )

public function main() -> I32:
    Animal cat = new Animal( "Whiskers" )
    cat.log( "meow" )
    puts( cat.name )
    return 0
```

Output:

```
meow
Whiskers
```

`Animal` extends `Object` and implements `Logger`. It must provide the `log` method from `Logger`. The `extends` clause appears before `implements` in the class declaration.
