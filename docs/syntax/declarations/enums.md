# Enums

- [Table of Contents](#table-of-contents)

## Table of Contents

- [Enums](#enums)
  - [Table of Contents](#table-of-contents)
  - [Overview](#overview)
  - [Enum Declarations](#enum-declarations)
  - [Backed Enums](#backed-enums)
  - [Enum Comparison](#enum-comparison)
    - [Backed Enum Comparison](#backed-enum-comparison)
  - [Switch on Enums](#switch-on-enums)
  - [Match Expressions](#match-expressions)
    - [Wildcard Default](#wildcard-default)
  - [Enums as Parameters](#enums-as-parameters)

## Overview

An enum declaration creates a type with a finite set of named variants. Each variant is declared with the `unit` keyword inside the enum body. Enum values are compared with `==` and matched with `switch` statements. Enums can optionally be backed by a primitive type, giving each variant an explicit value.

## Enum Declarations

An enum declaration starts with a visibility modifier, the `enum` keyword, a name, a colon, and an indented body of `unit` declarations.

```uranite
package testing

from uranite.io.console import puts

public enum Direction:
    unit North
    unit South
    unit East
    unit West

public function main() -> I32:
    Direction dir = Direction.North
    puts( dir )
    return 0
```

Output:

```
0
```

Each variant is accessed as `EnumName.VariantName`. Variants are internally represented as integers. Printing an unbacked enum variant displays its ordinal position starting from 0.

## Backed Enums

A backed enum assigns explicit values of a primitive type to each variant. The backing type is specified with the `backed` keyword after the enum name. Each variant's value follows its name directly.

```uranite
package testing

from uranite.io.console import puts

public enum Status backed I64:
    unit Open 1
    unit Closed 2
    unit Pending 3

public function main() -> I32:
    Status state = Status.Closed
    puts( state )
    return 0
```

Output:

```
2
```

The backed type can be any integer type (`I8`, `I16`, `I32`, `I64`, `U8`, `U16`, `U32`, `U64`). Printing a backed enum variant displays its explicit value.

## Enum Comparison

Enum variants are compared using `==` to test for equality.

```uranite
package testing

from uranite.io.console import puts

public enum Priority:
    unit Low
    unit Medium
    unit High

public function main() -> I32:
    Priority level = Priority.High
    if level == Priority.High:
        puts( "urgent" )
    return 0
```

Output:

```
urgent
```

### Backed Enum Comparison

Backed enum variants can also be compared with `==`. The comparison checks the backing value.

```uranite
package testing

from uranite.io.console import puts

public enum Status backed I64:
    unit Open 1
    unit Closed 2
    unit Pending 3

public function main() -> I32:
    Status state = Status.Closed
    if state == Status.Closed:
        puts( "closed" )
    if state == Status.Open:
        puts( "open" )
    return 0
```

Output:

```
closed
```

## Switch on Enums

The `switch` statement provides matching over enum variants. Each `case` branch matches a specific variant.

```uranite
package testing

from uranite.io.console import puts

public enum Season:
    unit Spring
    unit Summer
    unit Autumn
    unit Winter

public function describe( Season season ) -> String:
    switch season:
        case Season.Spring:
            return "warm"
        case Season.Summer:
            return "hot"
        case Season.Autumn:
            return "cool"
        case Season.Winter:
            return "cold"
    return "unknown"

public function main() -> I32:
    puts( describe( Season.Summer ) )
    puts( describe( Season.Winter ) )
    return 0
```

Output:

```
hot
cold
```

Each `case` branch matches a fully-qualified variant name. The function returns from the matching branch directly.

## Match Expressions

The `match` expression provides inline pattern matching over enum variants. Unlike `switch`, `match` is an expression that produces a value. The syntax is `match subject in pattern => value, pattern => value`.

```uranite
package testing

from uranite.io.console import puts

public enum Color:
    unit Red
    unit Green
    unit Blue

public function main() -> I32:
    Color color = Color.Green
    String label = match color in Color.Red => "red", Color.Green => "green", Color.Blue => "blue"
    puts( label )
    return 0
```

Output:

```
green
```

Each arm maps a variant to a result value using `=>`. The result is assigned directly to a variable.

### Wildcard Default

Use `*` as a catch-all arm to handle any unmatched variants.

```uranite
package testing

from uranite.io.console import puts

public enum Direction:
    unit North
    unit South
    unit East
    unit West

public function main() -> I32:
    Direction dir = Direction.East
    String label = match dir in Direction.North => "up", Direction.South => "down", * => "other"
    puts( label )
    return 0
```

Output:

```
other
```

The `*` arm matches any variant not explicitly listed. It must be the last arm in the match expression.

## Enums as Parameters

Enums can be used as function parameter types. This enables functions that operate on specific enum values.

```uranite
package testing

from uranite.io.console import puts

public enum Color:
    unit Red
    unit Green
    unit Blue

public function isWarm( Color color ) -> Boolean:
    if color == Color.Red:
        return True
    return False

public function main() -> I32:
    puts( isWarm( Color.Red ) )
    puts( isWarm( Color.Blue ) )
    return 0
```

Output:

```
True
False
```
