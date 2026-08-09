# Type Identity

---

## Table of Contents

- [Type Identity](#type-identity)
  - [Table of Contents](#table-of-contents)
  - [Overview](#overview)
  - [The is Keyword](#the-is-keyword)
    - [Checking for None](#checking-for-none)
    - [Checking is not None](#checking-is-not-none)
    - [Reference Identity](#reference-identity)
    - [Reference Non-Identity](#reference-non-identity)
  - [Identity vs Equality](#identity-vs-equality)
  - [Identity in Conditions](#identity-in-conditions)
    - [Optional Guarding](#optional-guarding)
    - [Branching on None](#branching-on-none)
  - [Method Reference](#method-reference)
    - [Identity Operators](#identity-operators)
    - [Usage Contexts](#usage-contexts)
  - [Examples](#examples)
    - [None Identity Checking](#none-identity-checking)
    - [Object Reference Identity](#object-reference-identity)
    - [Optional Config Selection](#optional-config-selection)
    - [Linked Node Chain](#linked-node-chain)

---

## Overview

Type identity determines whether two values refer to the same object in memory, or whether an optional value is `None`. Uranite provides the `is` keyword for identity checks, which is distinct from value equality (`==`).

The `is` keyword performs pointer comparison — it checks whether two variables point to the exact same object, not whether their contents are equal. For optional types, `is None` and `is not None` check whether the optional contains a value.

---

## The is Keyword

### Checking for None

The `is None` expression checks whether an optional value is absent.

```uranite
from uranite.io.console import puts

public function main() -> I32:
    ?String name = None
    if name is None:
        puts( "is none" )
    return 0
```

Output:

```
is none
```

The `is None` check returns `True` when the optional holds no value.

### Checking is not None

The `is not None` expression checks whether an optional value is present.

```uranite
from uranite.io.console import puts

public function main() -> I32:
    ?String name = "Alice"
    if name is not None:
        puts( name )
    return 0
```

Output:

```
Alice
```

When `is not None` returns `True`, the optional contains a value and can be used directly.

### Reference Identity

The `is` keyword checks whether two variables refer to the exact same object in memory.

```uranite
from uranite.io.console import puts

class Box:

    public I64 boxValue

    public function Box( self, I64 boxValue ) -> Void:
        self.boxValue = boxValue

public function main() -> I32:
    Box first = new Box( 42 )
    Box second = first
    Box third = new Box( 42 )

    Boolean sameRef = first is second
    puts( sameRef.toString() )

    Boolean diffRef = first is third
    puts( diffRef.toString() )
    return 0
```

Output:

```
True
False
```

`first` and `second` point to the same object (`second = first` copies the reference), so `is` returns `True`. `first` and `third` hold the same value `42` but are separate objects, so `is` returns `False`.

### Reference Non-Identity

The `is not` expression checks whether two variables refer to different objects.

```uranite
from uranite.io.console import puts

class Box:

    public I64 boxValue

    public function Box( self, I64 boxValue ) -> Void:
        self.boxValue = boxValue

public function main() -> I32:
    Box first = new Box( 10 )
    Box second = new Box( 10 )
    Box third = first

    Boolean notSame = first is not second
    puts( notSame.toString() )

    Boolean notThird = first is not third
    puts( notThird.toString() )
    return 0
```

Output:

```
True
False
```

`first is not second` is `True` because they are different objects. `first is not third` is `False` because they refer to the same object.

---

## Identity vs Equality

The `is` keyword and the `==` operator serve different purposes:

- `is` checks whether two variables refer to the **same object** in memory (pointer comparison)
- `==` checks whether two values have the **same content** (value comparison)

```uranite
from uranite.io.console import puts

class Box:

    public I64 boxValue

    public function Box( self, I64 boxValue ) -> Void:
        self.boxValue = boxValue

public function main() -> I32:
    Box first = new Box( 42 )
    Box second = new Box( 42 )

    Boolean sameIdentity = first is second
    puts( sameIdentity.toString() )
    return 0
```

Output:

```
False
```

Two `Box` objects with identical contents are **not** the same object. They occupy different memory locations, so `is` returns `False`.

For primitive values, `==` is the appropriate comparison:

```uranite
from uranite.io.console import puts

public function main() -> I32:
    I64 alpha = 42
    I64 beta = 42
    Boolean equal = alpha == beta
    puts( equal.toString() )
    return 0
```

Output:

```
True
```

---

## Identity in Conditions

### Optional Guarding

The `is not None` check is the standard pattern for guarding optional values before use.

```uranite
from uranite.io.console import puts

public function findItem( Boolean found ) -> ?String:
    if found:
        return "item"
    return None

public function main() -> I32:
    ?String result = findItem( True )
    if result is not None:
        puts( result )

    ?String missing = findItem( False )
    if missing is None:
        puts( "not found" )
    return 0
```

Output:

```
item
not found
```

### Branching on None

Identity checks combine with `if`/`else` for optional branching.

```uranite
from uranite.io.console import puts

public function main() -> I32:
    ?I64 empty = None
    ?I64 filled = 42

    if empty is None:
        puts( "empty is none" )

    if filled is not None:
        puts( "filled has value" )

    if empty is not None:
        puts( "should not print" )
    else:
        puts( "empty confirmed" )
    return 0
```

Output:

```
empty is none
filled has value
empty confirmed
```

---

## Method Reference

### Identity Operators

| Syntax | Description |
|---|---|
| `value is None` | Check if optional is absent |
| `value is not None` | Check if optional is present |
| `value1 is value2` | Check if both refer to same object |
| `value1 is not value2` | Check if they refer to different objects |

### Usage Contexts

| Context | Example |
|---|---|
| Condition | `if value is None:` |
| Else branch | `if value is not None:` ... `else:` |
| Variable assignment | `Boolean check = value is None` |
| Optional guard | `if result is not None:` then use `result` |
| Reference check | `if first is second:` |

---

## Examples

### None Identity Checking

```uranite
from uranite.io.console import puts

public function findUser( I64 userId ) -> ?String:
    if userId == 1:
        return "Alice"
    if userId == 2:
        return "Bob"
    return None

public function main() -> I32:
    ?String user1 = findUser( 1 )
    if user1 is not None:
        puts( user1 )
    else:
        puts( "user not found" )

    ?String user2 = findUser( 99 )
    if user2 is not None:
        puts( user2 )
    else:
        puts( "user not found" )

    ?String user3 = findUser( 3 )
    if user3 is None:
        puts( "confirmed missing" )
    return 0
```

Output:

```
Alice
user not found
confirmed missing
```

### Object Reference Identity

```uranite
from uranite.io.console import puts

class Token:

    public String label

    public function Token( self, String label ) -> Void:
        self.label = label

public function main() -> I32:
    Token original = new Token( "session-abc" )
    Token copy = original
    Token different = new Token( "session-abc" )

    Boolean isSame = original is copy
    puts( isSame.toString() )

    Boolean isDifferent = original is different
    puts( isDifferent.toString() )

    Boolean isNotDifferent = original is not different
    puts( isNotDifferent.toString() )
    return 0
```

Output:

```
True
False
True
```

### Optional Config Selection

```uranite
from uranite.io.console import puts

class Config:

    public String key
    public String configValue

    public function Config( self, String key, String configValue ) -> Void:
        self.key = key
        self.configValue = configValue

public function processConfig( ?Config primary, ?Config fallback ) -> String:
    if primary is not None:
        return primary.configValue
    if fallback is not None:
        return fallback.configValue
    return "default"

public function main() -> I32:
    Config primary = new Config( "theme", "dark" )
    String result1 = processConfig( primary, None )
    puts( result1 )

    Config fallback = new Config( "theme", "light" )
    String result2 = processConfig( None, fallback )
    puts( result2 )

    String result3 = processConfig( None, None )
    puts( result3 )
    return 0
```

Output:

```
dark
light
default
```

### Linked Node Chain

```uranite
from uranite.io.console import puts

class Node:

    public String data
    public ?Node next

    public function Node( self, String data ) -> Void:
        self.data = data
        self.next = None

    public function setNext( self, Node target ) -> Void:
        self.next = target

public function printChain( Node start ) -> Void:
    Node current = start
    puts( current.data )
    if current.next is not None:
        Node second = current.next
        puts( second.data )
        if second.next is not None:
            Node third = second.next
            puts( third.data )

public function main() -> I32:
    Node first = new Node( "alpha" )
    Node second = new Node( "beta" )
    Node third = new Node( "gamma" )

    first.setNext( second )
    second.setNext( third )

    printChain( first )

    if third.next is None:
        puts( "end of chain" )
    return 0
```

Output:

```
alpha
beta
gamma
end of chain
```
