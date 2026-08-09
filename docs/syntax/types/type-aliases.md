# Type Aliases

---

## Table of Contents

- [Type Aliases](#type-aliases)
  - [Table of Contents](#table-of-contents)
  - [Overview](#overview)
  - [Basic Type Aliases](#basic-type-aliases)
    - [Primitive Aliases](#primitive-aliases)
    - [Multiple Aliases](#multiple-aliases)
  - [Aliases in Functions](#aliases-in-functions)
    - [Parameter Types](#parameter-types)
    - [Return Types](#return-types)
  - [Aliases with Classes](#aliases-with-classes)
    - [Class Fields](#class-fields)
    - [Alias to Class Type](#alias-to-class-type)
  - [Generic Type Aliases](#generic-type-aliases)
    - [Parameterized Aliases](#parameterized-aliases)
    - [Concrete Generic Aliases](#concrete-generic-aliases)
  - [Aliases for Complex Types](#aliases-for-complex-types)
    - [Optional Type Aliases](#optional-type-aliases)
    - [Callable Type Aliases](#callable-type-aliases)
  - [Access Modifiers](#access-modifiers)
  - [Method Reference](#method-reference)
    - [Declaration Syntax](#declaration-syntax)
    - [Usage Contexts](#usage-contexts)
  - [Examples](#examples)
    - [Player Score Tracker](#player-score-tracker)
    - [Generic Alias Collections](#generic-alias-collections)
    - [Optional Alias with Nicknames](#optional-alias-with-nicknames)
    - [Callable Alias Dispatch](#callable-alias-dispatch)

---

## Overview

A type alias declares an alternative name for an existing type. The alias and the original type are fully interchangeable — they refer to the same type. Aliases introduce no runtime cost and create no new type identity. They exist purely as naming conveniences that improve readability.

Type aliases are declared with the `type` keyword followed by the alias name, an `=` sign, and the target type.

```uranite
type Integer = I64
```

After this declaration, `Integer` can be used anywhere `I64` is expected — in variable declarations, function parameters, return types, class fields, and generic arguments.

---

## Basic Type Aliases

### Primitive Aliases

Aliases can provide descriptive names for primitive types.

```uranite
from uranite.io.console import puts

type Integer = I64

public function main() -> I32:
    Integer value = 42
    puts( value.toString() )
    return 0
```

Output:

```
42
```

The variable `value` is declared with the alias `Integer`, which is the same type as `I64`. All `I64` operations work on `Integer` values.

### Multiple Aliases

Multiple aliases can be declared in the same module.

```uranite
from uranite.io.console import puts

type Text = String
type Number = I64
type Flag = Boolean

public function main() -> I32:
    Text name = "Alice"
    Number age = 30
    Flag active = True
    puts( name )
    puts( age.toString() )
    puts( active.toString() )
    return 0
```

Output:

```
Alice
30
True
```

Each alias is independent. `Text` is `String`, `Number` is `I64`, and `Flag` is `Boolean`.

---

## Aliases in Functions

### Parameter Types

Aliases can be used as function parameter types.

```uranite
from uranite.io.console import puts

type Number = I64

public function add( Number first, Number second ) -> Number:
    return first + second

public function main() -> I32:
    Number result = add( 10, 20 )
    puts( result.toString() )
    return 0
```

Output:

```
30
```

The function `add` accepts and returns `Number` values, which are `I64` underneath. Arithmetic operations work identically.

### Return Types

Aliases work as return types. The alias and the original type are interchangeable at call sites.

```uranite
from uranite.io.console import puts

type Label = String
type Count = I64

public function format( Label name, Count value ) -> String:
    return name + ": " + value.toString()

public function main() -> I32:
    Label name = "items"
    Count total = 42
    String output = format( name, total )
    puts( output )
    return 0
```

Output:

```
items: 42
```

The function returns `String` directly, even though its parameters use aliases `Label` and `Count`. A `Label` value passes where `String` is expected because they are the same type.

---

## Aliases with Classes

### Class Fields

Aliases can be used as class field types.

```uranite
from uranite.io.console import puts

type Score = I64
type Name = String

class Player:

    public Name playerName
    public Score playerScore

    public function Player( self, Name name, Score score ) -> Void:
        self.playerName = name
        self.playerScore = score

    public function display( self ) -> Void:
        puts( self.playerName + " scored " + self.playerScore.toString() )

public function main() -> I32:
    Player player = new Player( "Alice", 100 )
    player.display()
    return 0
```

Output:

```
Alice scored 100
```

Fields `playerName` and `playerScore` use alias types. Constructors and methods work with alias types in parameter positions.

### Alias to Class Type

An alias can name a class type, not just primitives. The alias works as a constructor target.

```uranite
from uranite.io.console import puts

class Animal:

    public String name

    public function Animal( self, String name ) -> Void:
        self.name = name

type Pet = Animal

public function main() -> I32:
    Pet dog = new Pet( "Rex" )
    puts( dog.name )
    return 0
```

Output:

```
Rex
```

`Pet` is an alias for `Animal`. Using `new Pet( "Rex" )` calls the `Animal` constructor. The resulting object has all `Animal` fields and methods.

---

## Generic Type Aliases

### Parameterized Aliases

A type alias can carry generic parameters, creating a shorter name for a parameterized type.

```uranite
from uranite.io.console import puts
from uranite.collection.array-list import ArrayList

type List<T> = ArrayList<T>

public function main() -> I32:
    List<String> names = new List<String>()
    names.add( "Alice" )
    names.add( "Bob" )
    puts( names.size().toString() )
    return 0
```

Output:

```
2
```

`List<T>` is a generic alias for `ArrayList<T>`. When used as `List<String>`, it resolves to `ArrayList<String>`. All `ArrayList` methods are available through the alias.

### Concrete Generic Aliases

An alias can fix generic parameters to specific types, creating a non-generic shorthand for a specific instantiation.

```uranite
from uranite.io.console import puts
from uranite.collection.array-list import ArrayList

type StringList = ArrayList<String>

public function main() -> I32:
    StringList names = new StringList()
    names.add( "Alice" )
    names.add( "Bob" )
    puts( names.size().toString() )
    return 0
```

Output:

```
2
```

`StringList` is a non-generic alias for `ArrayList<String>`. No type parameter is needed when using `StringList`.

---

## Aliases for Complex Types

### Optional Type Aliases

Aliases can target optional types using the `?` prefix syntax.

```uranite
from uranite.io.console import puts

type MaybeName = ?String

public function findName( Boolean found ) -> MaybeName:
    if found:
        return "Alice"
    return None

public function main() -> I32:
    MaybeName result = findName( True )
    if result is not None:
        puts( result )
    MaybeName empty = findName( False )
    if empty is None:
        puts( "not found" )
    return 0
```

Output:

```
Alice
not found
```

`MaybeName` is `?String`. All optional operations — `None` assignment, identity checks with `is` and `is not` — work through the alias.

### Callable Type Aliases

Aliases can name function types, making higher-order function signatures more readable.

```uranite
from uranite.io.console import puts

type Callback = Callable<Void, <String>>

public function greet( String name ) -> Void:
    puts( "Hello " + name )

public function invoke( Callback action, String value ) -> Void:
    action( value )

public function main() -> I32:
    invoke( greet, "World" )
    return 0
```

Output:

```
Hello World
```

`Callback` aliases `Callable<Void, <String>>` — a function that takes a `String` and returns `Void`. The function `greet` matches this signature and can be passed where `Callback` is expected.

---

## Access Modifiers

Type aliases support access modifiers that control visibility across modules.

```uranite
public type Message = String
```

A `public` type alias can be imported by other modules using standard import syntax. Without an access modifier, the alias is visible only within the declaring module.

| Modifier | Visibility |
|---|---|
| `public` | Importable by other modules |
| `protect` | Visible within the module and submodules |
| *(none)* | Visible only within the declaring module |

---

## Method Reference

### Declaration Syntax

| Syntax | Description |
|---|---|
| `type AliasName = TargetType` | Declare alias for a type |
| `type AliasName<T> = TargetType<T>` | Declare generic alias |
| `type AliasName = ?TargetType` | Declare alias for optional type |
| `type AliasName = Callable<R, <P1, P2>>` | Declare alias for callable type |
| `public type AliasName = TargetType` | Declare public alias |

### Usage Contexts

| Context | Example |
|---|---|
| Variable declaration | `AliasName value = expression` |
| Function parameter | `function name( AliasName param ) -> Type:` |
| Function return type | `function name() -> AliasName:` |
| Class field | `public AliasName fieldName` |
| Constructor call | `new AliasName( args )` |
| Generic argument | `List<AliasName>` |

---

## Examples

### Player Score Tracker

```uranite
from uranite.io.console import puts

type Score = I64
type Name = String
type Active = Boolean

class Player:

    public Name playerName
    public Score playerScore
    public Active isActive

    public function Player( self, Name name, Score score, Active active ) -> Void:
        self.playerName = name
        self.playerScore = score
        self.isActive = active

public function displayPlayer( Player player ) -> Void:
    puts( player.playerName )
    puts( player.playerScore.toString() )
    puts( player.isActive.toString() )

public function addScore( Score current, Score bonus ) -> Score:
    return current + bonus

public function main() -> I32:
    Player alice = new Player( "Alice", 100, True )
    Player bob = new Player( "Bob", 85, False )

    displayPlayer( alice )
    displayPlayer( bob )

    Score combined = addScore( alice.playerScore, bob.playerScore )
    puts( combined.toString() )
    return 0
```

Output:

```
Alice
100
True
Bob
85
False
185
```

### Generic Alias Collections

```uranite
from uranite.io.console import puts
from uranite.collection.array-list import ArrayList

type List<T> = ArrayList<T>
type Text = String
type Count = I64

public function countNames( List<Text> items ) -> Count:
    return items.size()

public function countNumbers( List<Count> items ) -> Count:
    return items.size()

public function main() -> I32:
    List<Text> fruits = new List<Text>()
    fruits.add( "apple" )
    fruits.add( "banana" )
    fruits.add( "cherry" )

    Count fruitCount = countNames( fruits )
    puts( fruitCount.toString() )

    List<Count> numbers = new List<Count>()
    numbers.add( 10 )
    numbers.add( 20 )
    numbers.add( 30 )

    Count numberCount = countNumbers( numbers )
    puts( numberCount.toString() )
    return 0
```

Output:

```
3
3
```

### Optional Alias with Nicknames

```uranite
from uranite.io.console import puts

type MaybeName = ?String
type Age = I64

class Person:

    public MaybeName nickname
    public String fullName
    public Age personAge

    public function Person( self, String full, MaybeName nick, Age age ) -> Void:
        self.fullName = full
        self.nickname = nick
        self.personAge = age

    public function displayName( self ) -> String:
        if self.nickname is not None:
            return self.nickname
        return self.fullName

public function main() -> I32:
    Person alice = new Person( "Alice Johnson", "Ali", 28 )
    Person bob = new Person( "Robert Smith", None, 35 )

    puts( alice.displayName() )
    puts( bob.displayName() )

    puts( alice.personAge.toString() )
    puts( bob.personAge.toString() )
    return 0
```

Output:

```
Ali
Robert Smith
28
35
```

### Callable Alias Dispatch

```uranite
from uranite.io.console import puts

type Callback = Callable<String, <I64>>
type Predicate = Callable<Boolean, <I64>>
type Number = I64

public function formatPlain( Number value ) -> String:
    return value.toString()

public function formatDoubled( Number value ) -> String:
    Number doubled = value * 2
    return doubled.toString()

public function isPositive( Number value ) -> Boolean:
    if value > 0:
        return True
    return False

public function applyFormat( Callback formatter, Number value ) -> String:
    return formatter( value )

public function checkValue( Predicate checker, Number value ) -> Boolean:
    return checker( value )

public function main() -> I32:
    String result1 = applyFormat( formatPlain, 50 )
    String result2 = applyFormat( formatDoubled, 15 )
    puts( result1 )
    puts( result2 )

    Boolean positive = checkValue( isPositive, 10 )
    Boolean negative = checkValue( isPositive, -5 )
    puts( positive.toString() )
    puts( negative.toString() )
    return 0
```

Output:

```
50
30
True
False
```
