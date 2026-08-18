# Class Inheritance

- [Table of Contents](#table-of-contents)

## Table of Contents

- [Class Inheritance](#class-inheritance)
  - [Table of Contents](#table-of-contents)
  - [Overview](#overview)
  - [Extending a Class](#extending-a-class)
  - [Parent Constructor Calls](#parent-constructor-calls)
  - [Inherited Fields](#inherited-fields)
  - [Method Overriding](#method-overriding)
    - [Virtual Methods](#virtual-methods)
    - [Override Methods](#override-methods)
  - [Multi-Level Inheritance](#multi-level-inheritance)
  - [Adding Fields in Subclasses](#adding-fields-in-subclasses)

## Overview

Uranite supports single inheritance. A class can extend at most one base class using the `extends` keyword. The subclass inherits all fields and methods from the parent class. Constructors are not inherited and must be defined explicitly in each class. The parent constructor is invoked using `parent()`.

## Extending a Class

Use `extends` after the class name to inherit from a base class. The subclass gains access to all fields and methods of the parent.

```uranite
package testing

from uranite.io.console import puts

public class Animal:
    public String name
    public I64 age

    public function Animal( self, String name, I64 age ) -> Void:
        self.name = name
        self.age = age

public class Dog extends Animal:
    public String breed

    public function Dog( self, String name, I64 age, String breed ) -> Void:
        parent( name, age )
        self.breed = breed

public function main() -> I32:
    Dog dog = new Dog( "Rex", 5, "Husky" )
    puts( dog.name )
    puts( dog.age )
    puts( dog.breed )
    return 0
```

Output:

```
Rex
5
Husky
```

`Dog` extends `Animal` and adds its own `breed` field. The `Dog` constructor calls `parent( name, age )` to initialize the inherited fields.

## Parent Constructor Calls

Every subclass constructor must call `parent()` to invoke the parent constructor. Pass the required arguments for the parent constructor inside the parentheses.

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

public function main() -> I32:
    Circle circle = new Circle( 5.0 )
    puts( circle.kind )
    puts( circle.radius )
    return 0
```

Output:

```
circle
5
```

The `parent()` call in `Circle` passes the string `"circle"` to the `Shape` constructor, which assigns it to `self.kind`.

## Inherited Fields

Subclass instances have access to all fields declared on the parent class. Fields are accessed directly on the child instance.

```uranite
package testing

from uranite.io.console import puts

public class Vehicle:
    public String brand
    public I64 year

    public function Vehicle( self, String brand, I64 year ) -> Void:
        self.brand = brand
        self.year = year

public class Truck extends Vehicle:
    public I64 payload

    public function Truck( self, String brand, I64 year, I64 payload ) -> Void:
        parent( brand, year )
        self.payload = payload

public function main() -> I32:
    Truck truck = new Truck( "Ford", 2024, 5000 )
    puts( truck.brand )
    puts( truck.year )
    puts( truck.payload )
    return 0
```

Output:

```
Ford
2024
5000
```

## Method Overriding

### Virtual Methods

Mark a method as `virtual` on the base class to allow subclasses to override it. Virtual methods enable subclasses to provide their own implementation.

```uranite
package testing

from uranite.io.console import puts

public class Shape:
    public String kind

    public function Shape( self, String kind ) -> Void:
        self.kind = kind

    public virtual function area( self ) -> F64:
        return 0.0

public function main() -> I32:
    Shape shape = new Shape( "unknown" )
    puts( shape.area() )
    return 0
```

Output:

```
0
```

### Override Methods

Use the `override` keyword in the subclass to replace a `virtual` method from the parent class. The overriding method must match the parameter types and return type of the base method.

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

Calling `circle.area()` on the child instance invokes the overridden implementation.

## Multi-Level Inheritance

Inheritance can span multiple levels. Each class in the chain extends the one above it. The `parent()` call in each constructor invokes the immediate parent.

```uranite
package testing

from uranite.io.console import puts

public class Base:
    public String label

    public function Base( self, String label ) -> Void:
        self.label = label

public class Middle extends Base:
    public I64 level

    public function Middle( self, String label, I64 level ) -> Void:
        parent( label )
        self.level = level

public class Leaf extends Middle:
    public String detail

    public function Leaf( self, String label, I64 level, String detail ) -> Void:
        parent( label, level )
        self.detail = detail

public function main() -> I32:
    Leaf leaf = new Leaf( "root", 3, "extra" )
    puts( leaf.label )
    puts( leaf.level )
    puts( leaf.detail )
    return 0
```

Output:

```
root
3
extra
```

`Leaf` inherits from `Middle` which inherits from `Base`. The `Leaf` instance has access to fields from all three levels: `label` from `Base`, `level` from `Middle`, and `detail` from `Leaf`.

## Adding Fields in Subclasses

Each subclass can add its own fields alongside inherited fields. The subclass constructor initializes its own fields after calling `parent()` for the inherited ones.

```uranite
package testing

from uranite.io.console import puts

public class Person:
    public String name

    public function Person( self, String name ) -> Void:
        self.name = name

public class Employee extends Person:
    public String department
    public I64 employeeId

    public function Employee( self, String name, String department, I64 employeeId ) -> Void:
        parent( name )
        self.department = department
        self.employeeId = employeeId

public function main() -> I32:
    Employee employee = new Employee( "Alice", "Engineering", 1042 )
    puts( employee.name )
    puts( employee.department )
    puts( employee.employeeId )
    return 0
```

Output:

```
Alice
Engineering
1042
```
