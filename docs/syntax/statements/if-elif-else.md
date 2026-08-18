# If / Elif / Else

- [Table of Contents](#table-of-contents)

## Table of Contents

- [If / Elif / Else](#if--elif--else)
  - [Table of Contents](#table-of-contents)
  - [Overview](#overview)
  - [Basic Conditional](#basic-conditional)
  - [Standalone If](#standalone-if)
  - [If and Else](#if-and-else)
  - [Elif Chains](#elif-chains)
  - [Nested Conditionals](#nested-conditionals)
  - [Logical Operators in Conditions](#logical-operators-in-conditions)
  - [Negation with Not](#negation-with-not)
  - [String Equality Conditions](#string-equality-conditions)
  - [None Checks](#none-checks)
  - [Multi-Statement Blocks](#multi-statement-blocks)

## Overview

The `if` statement evaluates a Boolean condition and executes a block when the condition is `True`. Optional `elif` clauses provide additional conditions tested in order when the preceding conditions are `False`. An optional `else` clause provides a fallback block that executes when no conditions match.

Every condition must be an explicit Boolean expression. Uranite does not support implicit truthiness, so values like integers or strings cannot be used directly as conditions.

Each branch body is an indented block following a colon. There is no limit to the number of `elif` clauses in a chain.

## Basic Conditional

An `if` statement with `elif` and `else` selects exactly one branch based on the first condition that evaluates to `True`.

```uranite
package testing

from uranite.io.console import puts

public function main() -> I32:
    I64 score = 85
    if score >= 90:
        puts( "excellent" )
    elif score >= 70:
        puts( "good" )
    else:
        puts( "needs work" )
    return 0
```

The condition `score >= 90` is `False`, so the next condition `score >= 70` is tested. That evaluates to `True`, so the second branch executes and prints `good`.

## Standalone If

An `if` statement can appear without `elif` or `else`. When the condition is `False`, execution skips the indented block and continues with the next statement.

```uranite
package testing

from uranite.io.console import puts

public function main() -> I32:
    I64 value = 10
    if value > 5:
        puts( "big" )
    puts( "done" )
    return 0
```

Both `big` and `done` are printed. The `puts( "done" )` call is not part of the `if` block because it is at the same indentation level as the `if` statement itself.

## If and Else

An `if` with `else` and no `elif` creates a two-way branch. Exactly one of the two blocks always executes.

```uranite
package testing

from uranite.io.console import puts

public function main() -> I32:
    I64 value = 3
    if value > 5:
        puts( "big" )
    else:
        puts( "small" )
    return 0
```

Since `value > 5` is `False`, the `else` branch executes and prints `small`.

## Elif Chains

Multiple `elif` clauses test conditions in order. The first `True` condition selects its branch, and all remaining branches are skipped. If no condition matches, the `else` branch executes.

```uranite
package testing

from uranite.io.console import puts

public function classify( I64 value ) -> String:
    if value < 0:
        return "negative"
    elif value == 0:
        return "zero"
    elif value < 10:
        return "small"
    elif value < 100:
        return "medium"
    else:
        return "large"

public function main() -> I32:
    puts( classify( -5 ) )
    puts( classify( 0 ) )
    puts( classify( 7 ) )
    puts( classify( 50 ) )
    puts( classify( 200 ) )
    return 0
```

This prints `negative`, `zero`, `small`, `medium`, and `large` on separate lines. Each `elif` is tested only when all preceding conditions were `False`.

## Nested Conditionals

An `if` statement can appear inside the body of another `if`, `elif`, or `else` block. Each nested level adds one indentation step.

```uranite
package testing

from uranite.io.console import puts

public function main() -> I32:
    I64 age = 25
    Boolean member = True
    if age >= 18:
        if member == True:
            puts( "adult member" )
        else:
            puts( "adult non-member" )
    else:
        puts( "minor" )
    return 0
```

The outer `if` checks `age >= 18`. When that is `True`, the inner `if` checks `member == True` and prints `adult member`.

## Logical Operators in Conditions

The `and` and `or` operators combine Boolean conditions. Both sides of `and` and `or` are always evaluated. Uranite does not short-circuit logical operators.

```uranite
package testing

from uranite.io.console import puts

public function main() -> I32:
    I64 age = 25
    I64 score = 80
    if age >= 18 and score >= 70:
        puts( "qualified" )
    else:
        puts( "not qualified" )

    I64 vip = 0
    if vip == 1 or score >= 90:
        puts( "priority" )
    else:
        puts( "standard" )
    return 0
```

The first condition combines two comparisons with `and`. Both `age >= 18` and `score >= 70` are `True`, so `qualified` is printed. The second condition uses `or`. Neither `vip == 1` nor `score >= 90` is `True`, so `standard` is printed.

## Negation with Not

The `not` operator inverts a Boolean value. It produces `True` when the operand is `False`, and `False` when the operand is `True`.

```uranite
package testing

from uranite.io.console import puts

public function main() -> I32:
    Boolean active = False
    if not active:
        puts( "inactive" )
    else:
        puts( "active" )
    return 0
```

Since `active` is `False`, `not active` evaluates to `True` and the first branch prints `inactive`.

## String Equality Conditions

String comparisons use `==` and `!=`. The comparison checks string content, not identity.

```uranite
package testing

from uranite.io.console import puts

public function main() -> I32:
    String status = "active"
    if status == "active":
        puts( "running" )
    elif status == "paused":
        puts( "waiting" )
    else:
        puts( "stopped" )
    return 0
```

The string `status` equals `"active"`, so the first branch prints `running`.

## None Checks

The `is` operator tests identity, and `is not` tests non-identity. These operators are used with `None` to check whether an optional value is present.

```uranite
package testing

from uranite.io.console import puts

public function main() -> I32:
    ?String name = None
    if name is None:
        puts( "no name" )
    else:
        puts( "has name" )

    ?String title = "hello"
    if title is not None:
        puts( "has title" )
    else:
        puts( "no title" )
    return 0
```

The `?String` type declares an optional string that can hold either a `String` value or `None`. The `is None` check prints `no name`, and the `is not None` check prints `has title`.

## Multi-Statement Blocks

Each branch body can contain multiple statements. All statements at the same indentation level belong to the same block.

```uranite
package testing

from uranite.io.console import puts

public function main() -> I32:
    I64 value = 42
    if value > 0:
        String label = "positive"
        puts( label )
        I64 doubled = value * 2
        puts( "doubled" )
    else:
        puts( "non-positive" )
    return 0
```

When `value > 0` is `True`, both `puts` calls in the `if` block execute. The variable `label` and `doubled` are local to the `if` block and are not accessible outside it.
