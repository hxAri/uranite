# Switch / Case

- [Table of Contents](#table-of-contents)

## Table of Contents

- [Switch / Case](#switch--case)
  - [Table of Contents](#table-of-contents)
  - [Overview](#overview)
  - [Basic Switch](#basic-switch)
  - [Default Branch](#default-branch)
  - [Multi-Statement Case Bodies](#multi-statement-case-bodies)
  - [Inline Case Bodies](#inline-case-bodies)
  - [Switch in a Function](#switch-in-a-function)
  - [Nested Switch](#nested-switch)
  - [Enum Switch](#enum-switch)

## Overview

The `switch` statement selects a branch based on the value of an expression. Each `case` clause specifies a value to compare against. When the expression matches a case value, that branch executes. Only the first matching branch runs, with no fallthrough to subsequent cases.

The `case *:` clause serves as the default branch. It executes when no other case matches.

Each case body can be a single statement on the same line after the colon, or an indented block of multiple statements on the following lines.

## Basic Switch

A switch statement evaluates an expression and compares it against each case value in order. The first match selects the branch to execute.

```uranite
package testing

from uranite.io.console import puts

public function main() -> I32:
    I64 value = 1
    switch value:
        case 0:
            puts( "zero" )
        case 1:
            puts( "one" )
        case 2:
            puts( "two" )
    return 0
```

The value `1` matches `case 1:`, so the branch prints `one`. Cases `0` and `2` are skipped.

## Default Branch

The `case *:` clause acts as a default branch. It executes when no preceding case matches the switch expression.

```uranite
package testing

from uranite.io.console import puts

public function main() -> I32:
    I64 value = 5
    switch value:
        case 0:
            puts( "zero" )
        case 1:
            puts( "one" )
        case *:
            puts( "other" )
    return 0
```

The value `5` does not match `case 0:` or `case 1:`, so the default branch executes and prints `other`.

## Multi-Statement Case Bodies

Each case body can contain multiple statements. All statements at the same indentation level belong to that case branch.

```uranite
package testing

from uranite.io.console import puts

public function main() -> I32:
    I64 level = 2
    switch level:
        case 0:
            puts( "beginner" )
            puts( "welcome" )
        case 1:
            puts( "intermediate" )
        case 2:
            puts( "advanced" )
            puts( "congratulations" )
        case *:
            puts( "unknown" )
    return 0
```

The value `2` matches `case 2:`, and both `puts` calls in that branch execute, printing `advanced` and `congratulations` on separate lines.

## Inline Case Bodies

A case body can be written on the same line as the case clause, after the colon. This is useful for short single-statement branches.

```uranite
package testing

from uranite.io.console import puts

public function main() -> I32:
    I64 value = 0
    switch value:
        case 0: puts( "zero" )
        case 1: puts( "one" )
        case *: puts( "other" )
    return 0
```

Each case has a single `puts` call on the same line. The value `0` matches `case 0:` and prints `zero`.

## Switch in a Function

A switch statement inside a function can `return` from each branch, providing a clean way to map input values to output values.

```uranite
package testing

from uranite.io.console import puts

public function describe( I64 code ) -> String:
    switch code:
        case 0:
            return "zero"
        case 1:
            return "one"
        case 2:
            return "two"
        case *:
            return "other"

public function main() -> I32:
    puts( describe( 0 ) )
    puts( describe( 1 ) )
    puts( describe( 2 ) )
    puts( describe( 5 ) )
    return 0
```

This prints `zero`, `one`, `two`, and `other` on separate lines. Each case branch returns a string directly, so the function always produces a result.

## Nested Switch

A switch statement can appear inside the body of another switch case. The inner switch evaluates independently within its enclosing branch.

```uranite
package testing

from uranite.io.console import puts

public function main() -> I32:
    I64 category = 1
    I64 subtype = 0
    switch category:
        case 0:
            puts( "type a" )
        case 1:
            switch subtype:
                case 0:
                    puts( "type b first" )
                case 1:
                    puts( "type b second" )
                case *:
                    puts( "type b other" )
        case *:
            puts( "unknown" )
    return 0
```

The outer switch matches `category = 1` to `case 1:`. Inside that branch, the inner switch matches `subtype = 0` to `case 0:` and prints `type b first`.

## Enum Switch

Switch statements work with unbacked enum values. Each case specifies a fully qualified enum variant.

```uranite
package testing

from uranite.io.console import puts

public enum Direction:

    unit North
    unit South
    unit East
    unit West

public function main() -> I32:
    Direction heading = Direction.East
    switch heading:
        case Direction.North:
            puts( "north" )
        case Direction.South:
            puts( "south" )
        case Direction.East:
            puts( "east" )
        case Direction.West:
            puts( "west" )
    return 0
```

The variable `heading` holds `Direction.East`, which matches `case Direction.East:` and prints `east`. Each case uses the enum type name followed by the variant name.
