# Traits

- [Table of Contents](#table-of-contents)

## Table of Contents

- [Traits](#traits)
  - [Table of Contents](#table-of-contents)
  - [Overview](#overview)
  - [Trait Declarations](#trait-declarations)
  - [Mixing Traits into Classes](#mixing-traits-into-classes)
  - [Multiple Traits](#multiple-traits)
  - [Overriding Trait Methods](#overriding-trait-methods)
  - [Abstract Trait Methods](#abstract-trait-methods)
  - [Trait Fields](#trait-fields)
  - [Trait Methods Accessing Class Fields](#trait-methods-accessing-class-fields)
  - [Traits on Structs](#traits-on-structs)
  - [Traits with Interfaces](#traits-with-interfaces)
  - [Traits vs Interfaces](#traits-vs-interfaces)

## Overview

A trait is declared with the `trait` keyword and contains methods, properties, and fields. Classes and structs incorporate a trait's members using the `use` keyword inside their body. When a trait provides a method with a body, the class inherits that implementation. When a trait declares a method without a body (ending with a semicolon), the class must provide its own implementation.

Traits enable horizontal code reuse. They share behavior across unrelated types without requiring an inheritance relationship.

## Trait Declarations

A trait declaration starts with a visibility modifier, the `trait` keyword, a name, a colon, and an indented body of methods and fields.

```uranite
package testing

from uranite.io.console import puts

public trait Printable:
    public function display( self ) -> String:
        return "printable"

public class Document:
    public String title

    public function Document( self, String title ) -> Void:
        self.title = title

    use Printable

public function main() -> I32:
    Document doc = new Document( "report" )
    puts( doc.display() )
    return 0
```

Output:

```
printable
```

The `Printable` trait defines a `display` method with a default implementation. The `Document` class gains this method through `use Printable`.

## Mixing Traits into Classes

A class incorporates a trait with the `use` keyword followed by the trait name inside the class body. The trait's methods and fields become members of the class.

```uranite
package testing

from uranite.io.console import puts

public trait Loggable:
    public function logMessage( self ) -> String:
        return "logged"

public class Record:
    public String data

    public function Record( self, String data ) -> Void:
        self.data = data

    use Loggable

public function main() -> I32:
    Record rec = new Record( "test" )
    puts( rec.logMessage() )
    return 0
```

Output:

```
logged
```

After `use Loggable`, the `Record` class can call `logMessage` as if it were defined directly in the class.

## Multiple Traits

A class can mix in multiple traits by listing them after `use`, separated by commas.

```uranite
package testing

from uranite.io.console import puts

public trait Loggable:
    public function logMessage( self ) -> String:
        return "logged"

public trait Serializable:
    public function serialize( self ) -> String:
        return "serialized"

public class Record:
    public String data

    public function Record( self, String data ) -> Void:
        self.data = data

    use Loggable, Serializable

public function main() -> I32:
    Record rec = new Record( "test" )
    puts( rec.logMessage() )
    puts( rec.serialize() )
    return 0
```

Output:

```
logged
serialized
```

The class gains methods from all listed traits.

## Overriding Trait Methods

A class can override a trait method by declaring its own method with the same name. The class method takes precedence over the trait default.

```uranite
package testing

from uranite.io.console import puts

public trait Printable:
    public function display( self ) -> String:
        return "default"

public class Widget:
    public String label

    public function Widget( self, String label ) -> Void:
        self.label = label

    use Printable

    public function display( self ) -> String:
        return self.label

public function main() -> I32:
    Widget widget = new Widget( "button" )
    puts( widget.display() )
    return 0
```

Output:

```
button
```

The `Printable` trait provides `display` returning `"default"`, but `Widget` overrides it with its own implementation returning `self.label`.

## Abstract Trait Methods

A trait can declare methods without a body, ending with a semicolon instead of a colon and block. The class mixing in the trait must provide its own implementation.

```uranite
package testing

from uranite.io.console import puts

public trait Hashable:
    public function computeHash( self ) -> I64;

public class Token:
    public I64 code

    public function Token( self, I64 code ) -> Void:
        self.code = code

    use Hashable

    public function computeHash( self ) -> I64:
        return self.code * 31

public function main() -> I32:
    Token token = new Token( 42 )
    puts( token.computeHash() )
    return 0
```

Output:

```
1302
```

The `Hashable` trait requires a `computeHash` method. The `Token` class provides the implementation after mixing in the trait.

## Trait Fields

Traits can declare fields that are injected into the class. The class must initialize these fields in its constructor.

```uranite
package testing

from uranite.io.console import puts

public trait Timestamped:
    public I64 createdAt

public class Event:
    public String name

    public function Event( self, String name, I64 timestamp ) -> Void:
        self.name = name
        self.createdAt = timestamp

    use Timestamped

public function main() -> I32:
    Event event = new Event( "click", 1000 )
    puts( event.name )
    puts( event.createdAt )
    return 0
```

Output:

```
click
1000
```

The `Timestamped` trait declares a `createdAt` field. When `Event` uses the trait, `createdAt` becomes a field of `Event` and must be assigned in the constructor.

## Trait Methods Accessing Class Fields

A trait method can access fields defined on the class that uses it. The trait method is injected into the class, so `self` refers to the class instance.

```uranite
package testing

from uranite.io.console import puts

public trait Describable:
    public function describe( self ) -> String:
        return self.label

public class Item:
    public String label

    public function Item( self, String label ) -> Void:
        self.label = label

    use Describable

public function main() -> I32:
    Item item = new Item( "widget" )
    puts( item.describe() )
    return 0
```

Output:

```
widget
```

The `describe` method in the `Describable` trait accesses `self.label`, which is a field on `Item`. The field must exist on the class for the trait method to compile.

## Traits on Structs

Structs can also mix in traits using the `use` keyword, just like classes.

```uranite
package testing

from uranite.io.console import puts

public trait Printable:
    public function display( self ) -> String:
        return "printable"

public struct Point:
    public I64 coordX
    public I64 coordY

    public function Point( self, I64 coordX, I64 coordY ) -> Void:
        self.coordX = coordX
        self.coordY = coordY

    use Printable

public function main() -> I32:
    Point point = new Point( 10, 20 )
    puts( point.display() )
    return 0
```

Output:

```
printable
```

The `Point` struct gains the `display` method from `Printable` through `use`.

## Traits with Interfaces

A class can use traits and implement interfaces simultaneously. Trait methods provide reusable behavior while interface methods satisfy a polymorphic contract.

```uranite
package testing

from uranite.io.console import puts

public interface Comparable:
    public function compare( self, I64 other ) -> I64;

public trait Printable:
    public function display( self ) -> String:
        return "printable"

public class Score implements Comparable:
    public I64 value

    public function Score( self, I64 value ) -> Void:
        self.value = value

    use Printable

    public override function compare( self, I64 other ) -> I64:
        if self.value > other:
            return 1
        if self.value == other:
            return 0
        return -1

public function main() -> I32:
    Score score = new Score( 85 )
    puts( score.display() )
    puts( score.compare( 90 ) )
    return 0
```

Output:

```
printable
-1
```

`Score` implements `Comparable` for its interface contract and uses `Printable` for its default `display` method.

## Traits vs Interfaces

| Feature | Trait | Interface |
|---|---|---|
| Method signatures | Yes | Yes |
| Method bodies | Yes | No |
| Fields | Yes | No |
| Inheritance hierarchy | No | Yes (`extends`) |
| Polymorphic dispatch | No | Yes |
| Multiple mixing | Yes (`use A, B`) | Yes (`implements A, B`) |
| Used as parameter type | No | Yes |

Use traits when sharing concrete method implementations across unrelated types. Use interfaces when defining a contract that enables polymorphic dispatch through interface-typed parameters.
