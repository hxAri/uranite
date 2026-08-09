# Enum Types

---

## Table of Contents

- [Enum Types](#enum-types)
  - [Table of Contents](#table-of-contents)
  - [Overview](#overview)
  - [Defining an Enum](#defining-an-enum)
    - [Basic Enum](#basic-enum)
    - [Backed Enum](#backed-enum)
  - [Using Enum Variants](#using-enum-variants)
  - [Variant Name and Value](#variant-name-and-value)
    - [The name Property](#the-name-property)
    - [The value Property](#the-value-property)
  - [Backed Types](#backed-types)
    - [Integer Backing](#integer-backing)
    - [String Backing](#string-backing)
  - [Comparing Enums](#comparing-enums)
  - [Switch Statements](#switch-statements)
  - [Enums as Function Parameters](#enums-as-function-parameters)
  - [Returning Enums from Functions](#returning-enums-from-functions)
  - [Method Reference](#method-reference)
    - [Built-in Properties](#built-in-properties)
    - [Variant Syntax](#variant-syntax)
  - [Examples](#examples)
    - [Traffic Light](#traffic-light)
    - [HTTP Status Codes](#http-status-codes)
    - [Compass Navigation](#compass-navigation)
    - [Log Level Filtering](#log-level-filtering)

---

## Overview

Enums define a type with a fixed set of named variants. Each variant is declared with the `unit` keyword. Enums are useful for representing states, categories, options, or any value drawn from a known set of alternatives.

Enums can optionally be backed by a type, where each variant carries an associated value. The two built-in properties `.name` and `.value` provide access to the variant's name as a string and its backing value respectively.

---

## Defining an Enum

### Basic Enum

A basic enum declares a set of named variants. Each variant is introduced with the `unit` keyword.

```uranite
public enum Direction:

    unit North
    unit South
    unit East
    unit West
```

### Backed Enum

A backed enum associates each variant with a value of a specific type. The backing type is declared after the `backed` keyword.

```uranite
public enum HttpStatus backed I64:

    unit Ok 200
    unit NotFound 404
    unit ServerError 500
```

Each variant specifies its backing value after the variant name.

---

## Using Enum Variants

Access variants using `EnumName.VariantName` syntax.

```uranite
Direction heading = Direction.North
HttpStatus status = HttpStatus.Ok
```

Variants are constants of the enum type. They can be assigned to variables, passed to functions, returned from functions, and compared with other values of the same enum type.

---

## Variant Name and Value

### The name Property

Every enum variant has a `.name` property that returns the variant's name as a `String`.

```uranite
from uranite.io.console import puts

public enum Color:

    unit Red
    unit Green
    unit Blue

public function main() -> I32:
    Color favorite = Color.Green
    puts( favorite.name )
    return 0
```

Output:

```
Green
```

### The value Property

Backed enums have a `.value` property that returns the associated backing value.

```uranite
from uranite.io.console import puts

public enum HttpStatus backed I64:

    unit Ok 200
    unit NotFound 404
    unit ServerError 500

public function main() -> I32:
    HttpStatus status = HttpStatus.NotFound
    puts( status.name )
    puts( status.value.toString() )
    return 0
```

Output:

```
NotFound
404
```

---

## Backed Types

### Integer Backing

Use `backed I64` to associate integer values with variants.

```uranite
public enum Priority backed I64:

    unit Low 1
    unit Medium 2
    unit High 3
    unit Critical 4
```

Access the backing value with `.value`:

```uranite
Priority task = Priority.High
I64 level = task.value
```

### String Backing

Use `backed String` to associate string values with variants.

```uranite
from uranite.io.console import puts

public enum Color backed String:

    unit Red "red"
    unit Green "green"
    unit Blue "blue"

public function main() -> I32:
    Color favorite = Color.Green
    puts( favorite.name )
    puts( favorite.value )
    return 0
```

Output:

```
Green
green
```

---

## Comparing Enums

Compare enum values using the `==` equality operator.

```uranite
from uranite.io.console import puts

public enum Status:

    unit Active
    unit Inactive

public function main() -> I32:
    Status current = Status.Active

    if current == Status.Active:
        puts( "System is active" )
    return 0
```

Output:

```
System is active
```

Backed enums also support equality comparison.

```uranite
Priority task = Priority.High

if task == Priority.High:
    puts( "high priority" )
```

---

## Switch Statements

Unbacked enums work with `switch` statements for branching on variant values.

```uranite
from uranite.io.console import puts

public enum Status:

    unit Active
    unit Inactive
    unit Pending

public function main() -> I32:
    Status current = Status.Inactive

    switch current:
        case Status.Active:
            puts( "active" )
        case Status.Inactive:
            puts( "inactive" )
        case Status.Pending:
            puts( "pending" )
    return 0
```

Output:

```
inactive
```

Each `case` branch specifies an enum variant. When the switch value matches a variant, the corresponding branch executes.

---

## Enums as Function Parameters

Enums can be passed as function parameters.

```uranite
from uranite.io.console import puts

public enum Direction:

    unit North
    unit South
    unit East
    unit West

public function isVertical( Direction direction ) -> Boolean:
    if direction == Direction.North:
        return True
    if direction == Direction.South:
        return True
    return False

public function main() -> I32:
    puts( isVertical( Direction.North ).toString() )
    puts( isVertical( Direction.East ).toString() )
    return 0
```

Output:

```
True
False
```

---

## Returning Enums from Functions

Functions can return enum values.

```uranite
from uranite.io.console import puts

public enum Season:

    unit Spring
    unit Summer
    unit Autumn
    unit Winter

public function describeSeason( Season season ) -> String:
    switch season:
        case Season.Spring:
            return "Warm and blooming"
        case Season.Summer:
            return "Hot and sunny"
        case Season.Autumn:
            return "Cool and colorful"
        case Season.Winter:
            return "Cold and snowy"
    return "Unknown"

public function main() -> I32:
    puts( describeSeason( Season.Summer ) )
    puts( describeSeason( Season.Winter ) )
    return 0
```

Output:

```
Hot and sunny
Cold and snowy
```

---

## Method Reference

### Built-in Properties

| Property | Type | Description |
|---|---|---|
| `.name` | `String` | The variant's name as a string |
| `.value` | Backing type | The variant's backing value (backed enums only) |

### Variant Syntax

| Syntax | Description |
|---|---|
| `unit VariantName` | Declares a variant in a basic enum |
| `unit VariantName value` | Declares a variant with a backing value |
| `EnumName.VariantName` | Accesses a variant |
| `backed Type` | Specifies the backing type for the enum |

---

## Examples

### Traffic Light

```uranite
from uranite.io.console import puts

public enum TrafficLight:

    unit Red
    unit Yellow
    unit Green

public function shouldStop( TrafficLight light ) -> Boolean:
    if light == TrafficLight.Red:
        return True
    if light == TrafficLight.Yellow:
        return True
    return False

public function main() -> I32:
    TrafficLight light = TrafficLight.Green
    puts( light.name )
    puts( shouldStop( light ).toString() )

    TrafficLight stop = TrafficLight.Red
    puts( stop.name )
    puts( shouldStop( stop ).toString() )
    return 0
```

Output:

```
Green
False
Red
True
```

### HTTP Status Codes

```uranite
from uranite.io.console import puts

public enum HttpStatus backed I64:

    unit Ok 200
    unit Created 201
    unit BadRequest 400
    unit NotFound 404
    unit ServerError 500

public function isSuccess( HttpStatus status ) -> Boolean:
    if status == HttpStatus.Ok:
        return True
    if status == HttpStatus.Created:
        return True
    return False

public function main() -> I32:
    HttpStatus response = HttpStatus.Ok
    puts( response.name )
    puts( response.value.toString() )
    puts( isSuccess( response ).toString() )

    HttpStatus error = HttpStatus.NotFound
    puts( error.name )
    puts( error.value.toString() )
    puts( isSuccess( error ).toString() )
    return 0
```

Output:

```
Ok
200
True
NotFound
404
False
```

### Compass Navigation

```uranite
from uranite.io.console import puts

public enum Direction:

    unit North
    unit South
    unit East
    unit West

public function opposite( Direction direction ) -> Direction:
    if direction == Direction.North:
        return Direction.South
    if direction == Direction.South:
        return Direction.North
    if direction == Direction.East:
        return Direction.West
    return Direction.East

public function main() -> I32:
    Direction heading = Direction.North
    Direction reversed = opposite( heading )
    puts( heading.name )
    puts( reversed.name )

    Direction lateral = Direction.East
    Direction lateralReversed = opposite( lateral )
    puts( lateral.name )
    puts( lateralReversed.name )
    return 0
```

Output:

```
North
South
East
West
```

### Log Level Filtering

```uranite
from uranite.io.console import puts

public enum LogLevel backed I64:

    unit Debug 0
    unit Info 1
    unit Warning 2
    unit Error 3

public function shouldLog( LogLevel messageLevel, LogLevel minimumLevel ) -> Boolean:
    if messageLevel.value >= minimumLevel.value:
        return True
    return False

public function main() -> I32:
    LogLevel minimum = LogLevel.Warning

    puts( shouldLog( LogLevel.Debug, minimum ).toString() )
    puts( shouldLog( LogLevel.Info, minimum ).toString() )
    puts( shouldLog( LogLevel.Warning, minimum ).toString() )
    puts( shouldLog( LogLevel.Error, minimum ).toString() )
    return 0
```

Output:

```
False
False
True
True
```
