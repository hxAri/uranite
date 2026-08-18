# Pass

- [Table of Contents](#table-of-contents)

## Table of Contents

- [Pass](#pass)
  - [Table of Contents](#table-of-contents)
  - [Overview](#overview)
  - [Pass in Functions](#pass-in-functions)
  - [Ellipsis as Pass](#ellipsis-as-pass)
  - [Pass in Conditional Branches](#pass-in-conditional-branches)
  - [Pass in Loops](#pass-in-loops)
  - [Pass in Class Methods](#pass-in-class-methods)
  - [Pass as Intentional No-Op](#pass-as-intentional-no-op)

## Overview

The `pass` statement is a placeholder that performs no operation. It exists so that syntactically required blocks can remain empty without causing a parse error.

Uranite uses indentation-based blocks, so every colon-terminated header (`if`, `while`, `for`, `function`, `class`) must be followed by at least one indented statement. When a block is intentionally empty, `pass` fills that requirement.

The ellipsis `...` is an alternative syntax for `pass`. Both are interchangeable.

## Pass in Functions

A function body must contain at least one statement. When a function is not yet implemented, `pass` provides a valid empty body.

```uranite
package testing

from uranite.io.console import puts

public function placeholder() -> Void:
    pass

public function main() -> I32:
    placeholder()
    puts( "done" )
    return 0
```

The function `placeholder` does nothing and returns immediately. The caller continues to the next statement and prints `done`.

## Ellipsis as Pass

The `...` token serves as an alternative to `pass`. It has identical behavior and can be used anywhere `pass` is valid.

```uranite
package testing

from uranite.io.console import puts

public function stub() -> Void:
    ...

public function main() -> I32:
    stub()
    puts( "done" )
    return 0
```

The function `stub` uses `...` instead of `pass`. Both produce the same result: the function body is empty and execution falls through.

## Pass in Conditional Branches

When one branch of a conditional should do nothing while another performs work, `pass` fills the empty branch.

```uranite
package testing

from uranite.io.console import puts

public function main() -> I32:
    I64 value = 5
    if value > 10:
        pass
    else:
        puts( "small" )
    return 0
```

The `if` branch does nothing because `value` is not greater than `10`. The `else` branch executes and prints `small`. Without `pass`, the empty `if` block would be a syntax error.

## Pass in Loops

A loop body can contain only `pass` when the loop structure is needed but no work is required inside it.

```uranite
package testing

from uranite.io.console import puts

public function main() -> I32:
    for I64 index in 0..3:
        pass
    puts( "loop done" )
    return 0
```

The loop iterates three times but performs no work on each iteration. After the loop completes, `loop done` is printed.

## Pass in Class Methods

Class constructors and methods that have no implementation yet can use `pass` as their body.

```uranite
package testing

from uranite.io.console import puts

public class Marker:

    public function Marker( self ) -> Void:
        pass

    public function action( self ) -> Void:
        pass

public function main() -> I32:
    Marker marker = new Marker()
    marker.action()
    puts( "created" )
    return 0
```

Both the constructor and the `action` method do nothing. The object is constructed, the method is called, and execution continues to print `created`.

## Pass as Intentional No-Op

When a condition deliberately requires no action, `pass` communicates that the empty branch is intentional rather than an oversight.

```uranite
package testing

from uranite.io.console import puts

public function main() -> I32:
    I64 count = 0
    while count < 5:
        count = count + 1
        if count == 3:
            pass
        else:
            puts( count )
    return 0
```

When `count` equals `3`, the `pass` branch executes and nothing is printed. All other values are printed, producing `1`, `2`, `4`, `5` on separate lines.
