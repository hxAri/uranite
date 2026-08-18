# Generics

- [Table of Contents](#table-of-contents)

## Table of Contents

- [Generics](#generics)
  - [Table of Contents](#table-of-contents)
  - [Overview](#overview)
  - [Generic Classes](#generic-classes)
  - [Multiple Type Parameters](#multiple-type-parameters)
  - [Generic Functions](#generic-functions)
  - [Multi-Parameter Generic Functions](#multi-parameter-generic-functions)
  - [Generic Interfaces](#generic-interfaces)
  - [Nested Generics](#nested-generics)
  - [Default Type Parameters](#default-type-parameters)
  - [Type Parameter Constraints](#type-parameter-constraints)

## Overview

Generic type parameters are declared inside angle brackets after the name of a class, interface, or function. Each parameter is a placeholder name that can be used as a type throughout the declaration body. At each usage site, the caller supplies concrete types to substitute for the parameters.

The compiler generates specialized code for each unique combination of type arguments. This process ensures that generic code runs with the same performance as hand-written type-specific code.

## Generic Classes

A generic class declares one or more type parameters after the class name. The type parameter can be used for fields, method parameters, and return types.

```uranite
package testing

from uranite.io.console import puts

public class Wrapper<T>:
    public T value

    public function Wrapper( self, T value ) -> Void:
        self.value = value

    public function unwrap( self ) -> T:
        return self.value

public function main() -> I32:
    Wrapper<I64> intWrap = new Wrapper<I64>( 42 )
    puts( intWrap.unwrap() )
    Wrapper<String> strWrap = new Wrapper<String>( "hello" )
    puts( strWrap.unwrap() )
    return 0
```

Output:

```
42
hello
```

`Wrapper<I64>` and `Wrapper<String>` are two distinct types, each with fields and methods specialized for their respective type argument.

## Multiple Type Parameters

A class can accept multiple type parameters, separated by commas.

```uranite
package testing

from uranite.io.console import puts

public class KeyValue<K, V>:
    public K key
    public V val

    public function KeyValue( self, K key, V val ) -> Void:
        self.key = key
        self.val = val

public function main() -> I32:
    KeyValue<String, I64> entry = new KeyValue<String, I64>( "score", 100 )
    puts( entry.key )
    puts( entry.val )
    return 0
```

Output:

```
score
100
```

Each type parameter is independent. `K` and `V` can be substituted with the same or different concrete types.

## Generic Functions

Functions can declare their own type parameters after the function name. The caller provides type arguments at the call site.

```uranite
package testing

from uranite.io.console import puts

public function identity<T>( T value ) -> T:
    return value

public function main() -> I32:
    I64 num = identity<I64>( 99 )
    puts( num )
    String text = identity<String>( "world" )
    puts( text )
    return 0
```

Output:

```
99
world
```

The function `identity<T>` accepts a value of any type and returns it unchanged. Each call site specifies the concrete type.

## Multi-Parameter Generic Functions

Functions can accept multiple type parameters. Each parameter can be used independently in the parameter list and return type.

```uranite
package testing

from uranite.io.console import puts

public function first<A, B>( A left, B right ) -> A:
    return left

public function second<A, B>( A left, B right ) -> B:
    return right

public function main() -> I32:
    I64 num = first<I64, String>( 42, "hello" )
    puts( num )
    String text = second<I64, String>( 42, "hello" )
    puts( text )
    return 0
```

Output:

```
42
hello
```

`first<A, B>` returns the first argument and `second<A, B>` returns the second. Both functions accept two independent type parameters.

## Generic Interfaces

Interfaces can declare type parameters. Classes implementing a generic interface substitute concrete types or forward their own type parameters.

```uranite
package testing

from uranite.io.console import puts

public interface Stack<T>:
    public function push( self, T item ) -> Void;
    public function peek( self ) -> T;

public class SimpleStack<T> implements Stack<T>:
    public T top

    public function SimpleStack( self, T initial ) -> Void:
        self.top = initial

    public override function push( self, T item ) -> Void:
        self.top = item

    public override function peek( self ) -> T:
        return self.top

public function main() -> I32:
    SimpleStack<I64> stack = new SimpleStack<I64>( 10 )
    puts( stack.peek() )
    stack.push( 20 )
    puts( stack.peek() )
    return 0
```

Output:

```
10
20
```

`SimpleStack<T>` implements `Stack<T>` by forwarding its own type parameter. When instantiated as `SimpleStack<I64>`, it implements `Stack<I64>`.

## Nested Generics

Generic types can be used as type arguments for other generic types, creating nested generic structures.

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
    Box<Box<I64>> nested = new Box<Box<I64>>( new Box<I64>( 99 ) )
    Box<I64> inner = nested.get()
    puts( inner.get() )
    return 0
```

Output:

```
99
```

`Box<Box<I64>>` is a box that contains another box of integers. The inner box is retrieved with `get()` and then its own `get()` returns the integer value.

## Default Type Parameters

Type parameters can have default types using `=`. When the caller provides the type argument explicitly, the default is ignored.

```uranite
package testing

from uranite.io.console import puts

public class Container<T = I64>:
    public T stored

    public function Container( self, T stored ) -> Void:
        self.stored = stored

    public function get( self ) -> T:
        return self.stored

public function main() -> I32:
    Container<I64> box = new Container<I64>( 42 )
    puts( box.get() )
    return 0
```

Output:

```
42
```

The `Container` class declares a default type `I64` for its type parameter `T`. When instantiated with `Container<I64>`, the explicit type is used.

## Type Parameter Constraints

Type parameters can be constrained to require that the type argument implements one or more interfaces. Constraints are declared with a colon after the parameter name, and multiple constraints are separated by `+`.

```uranite
package testing

from uranite.io.console import puts

public interface Printable:
    public function display( self ) -> String;

public class Label implements Printable:
    public String text

    public function Label( self, String text ) -> Void:
        self.text = text

    public override function display( self ) -> String:
        return self.text

public function passThrough<T: Printable>( T item ) -> T:
    return item

public function main() -> I32:
    Label label = new Label( "hello" )
    Label result = passThrough<Label>( label )
    puts( result.text )
    return 0
```

Output:

```
hello
```

The constraint `T: Printable` means the type argument for `T` must implement the `Printable` interface. A constraint `T: Readable + Writable` would require implementing both interfaces. The compiler verifies the constraint at the call site when the concrete type is supplied.
