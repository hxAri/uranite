# Abstract Classes

- [Table of Contents](#table-of-contents)

## Table of Contents

- [Abstract Classes](#abstract-classes)
  - [Table of Contents](#table-of-contents)
  - [Overview](#overview)
  - [Declaring Abstract Classes](#declaring-abstract-classes)
  - [Abstract Methods](#abstract-methods)
  - [Implementing Abstract Methods](#implementing-abstract-methods)
  - [Concrete Methods on Abstract Classes](#concrete-methods-on-abstract-classes)
  - [Multiple Abstract Methods](#multiple-abstract-methods)
  - [Abstract Inheritance Chains](#abstract-inheritance-chains)

## Overview

An abstract class is declared with the `abstract` keyword before `class`. It can contain both concrete methods (with a body) and abstract methods (without a body). Abstract methods declare only a signature, ending with a semicolon. Any class that extends an abstract class must override all abstract methods with concrete implementations, or itself be declared abstract.

Abstract classes can have fields, constructors, and concrete methods just like regular classes. They cannot be instantiated with `new` — only their concrete subclasses can. Attempting to instantiate an abstract class produces a compilation error.

## Declaring Abstract Classes

Place the `abstract` keyword before `class` in the declaration.

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

`Vehicle` defines a field `brand`, a constructor, and one abstract method `fuelType`. `Car` extends `Vehicle`, calls the parent constructor with `parent( brand )`, and provides a concrete implementation of `fuelType`.

## Abstract Methods

Abstract methods declare a signature without a body. They end with a semicolon instead of a colon and indented block. The `abstract` keyword precedes `function`.

```uranite
public abstract function fuelType( self ) -> String;
```

Abstract methods must be inside an abstract class. A non-abstract class cannot contain abstract methods.

## Implementing Abstract Methods

Subclasses implement abstract methods using the `override` keyword. The overriding method must match the abstract method's parameter types and return type.

```uranite
package testing

from uranite.io.console import puts

public abstract class Animal:
    public abstract function sound( self ) -> String;

public class Dog extends Animal:
    public function Dog( self ) -> Void:
        pass

    public override function sound( self ) -> String:
        return "woof"

public function main() -> I32:
    Dog dog = new Dog()
    puts( dog.sound() )
    return 0
```

Output:

```
woof
```

## Concrete Methods on Abstract Classes

Abstract classes can mix abstract and concrete methods. Concrete methods have a body and are inherited by subclasses. A concrete method can call abstract methods on `self` — the call dispatches to the overridden implementation provided by the concrete subclass.

```uranite
package testing

from uranite.io.console import puts

public abstract class Logger:
    public abstract function format( self, String message ) -> String;

    public function log( self, String message ) -> Void:
        String formatted = self.format( message )
        puts( formatted )

public class SimpleLogger extends Logger:
    public function SimpleLogger( self ) -> Void:
        pass

    public override function format( self, String message ) -> String:
        return "[LOG] " + message

public function main() -> I32:
    SimpleLogger logger = new SimpleLogger()
    logger.log( "server started" )
    return 0
```

Output:

```
[LOG] server started
```

`Logger` defines a concrete method `log` that calls the abstract method `format`. When `SimpleLogger` overrides `format`, calling `logger.log()` dispatches `self.format()` to `SimpleLogger.format()`.

## Multiple Abstract Methods

An abstract class can declare any number of abstract methods. Subclasses must implement all of them.

```uranite
package testing

from uranite.io.console import puts

public abstract class Renderer:
    public abstract function render( self ) -> String;

    public abstract function width( self ) -> I64;

public class HtmlRenderer extends Renderer:
    public function HtmlRenderer( self ) -> Void:
        pass

    public override function render( self ) -> String:
        return "<div>content</div>"

    public override function width( self ) -> I64:
        return 800

public function main() -> I32:
    HtmlRenderer renderer = new HtmlRenderer()
    puts( renderer.render() )
    puts( renderer.width() )
    return 0
```

Output:

```
<div>content</div>
800
```

## Abstract Inheritance Chains

An abstract class can extend another abstract class. The intermediate class does not need to implement the abstract methods — it can defer them to further subclasses. The final concrete class in the chain must implement all abstract methods from the entire hierarchy.

```uranite
package testing

from uranite.io.console import puts

public abstract class Base:
    public abstract function identity( self ) -> String;

public abstract class Middle extends Base:
    public String tag

    public function Middle( self, String tag ) -> Void:
        self.tag = tag

public class Concrete extends Middle:
    public function Concrete( self ) -> Void:
        parent( "concrete" )

    public override function identity( self ) -> String:
        return self.tag

public function main() -> I32:
    Concrete item = new Concrete()
    puts( item.identity() )
    return 0
```

Output:

```
concrete
```

`Base` declares the abstract method `identity`. `Middle` extends `Base` without implementing `identity` and adds a `tag` field. `Concrete` extends `Middle` and provides the implementation, completing the chain.
