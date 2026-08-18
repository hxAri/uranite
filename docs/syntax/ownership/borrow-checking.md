# Borrow Checking

- [Table of Contents](#table-of-contents)

## Table of Contents

- [Borrow Checking](#borrow-checking)
  - [Table of Contents](#table-of-contents)
  - [Overview](#overview)
  - [Ownership Tracking](#ownership-tracking)
  - [Use After Move](#use-after-move)
  - [Use After Drop](#use-after-drop)
  - [Double Free Detection](#double-free-detection)
  - [Move After Move](#move-after-move)
  - [Move After Drop](#move-after-drop)
  - [Branch State Merging](#branch-state-merging)

## Overview

The borrow checker is a compile-time analysis pass that enforces ownership rules. It tracks every variable through five possible states: owned, borrowed, mutable-borrowed, moved, and dropped. Violations produce compile-time errors, preventing memory safety bugs before the program runs.

The borrow checker verifies the following invariants:

- A moved value cannot be used
- A dropped value cannot be used
- A dropped value cannot be deleted again
- A moved value cannot be moved again
- A dropped value cannot be moved
- Ownership state is merged conservatively across branches

## Ownership Tracking

When a variable is declared, the borrow checker registers it as owned. The variable remains in the owned state until it is explicitly moved or deleted.

```uranite
package testing

from uranite.io.console import puts

public class Buffer:
    public I64 size

    public function Buffer( self, I64 size ) -> Void:
        self.size = size

public function main() -> I32:
    Buffer first = new Buffer( 64 )
    Buffer second = move first
    puts( second.size )
    delete second
    return 0
```

The variable `first` starts as owned. After `move first`, its state changes to moved. The variable `second` becomes the new owner. After `delete second`, its state changes to dropped. The program prints `64`.

## Use After Move

Accessing a variable after its ownership has been transferred with `move` is a compile-time error. The borrow checker detects this and reports the violation.

The following code is rejected by the compiler:

```
package testing

from uranite.io.console import puts

public class Box:
    public I64 value

    public function Box( self, I64 value ) -> Void:
        self.value = value

public function consume( Box box ) -> Void:
    puts( box.value )
    delete box

public function main() -> I32:
    Box box = new Box( 10 )
    consume( move box )
    puts( box.value )
    return 0
```

The compiler produces:

```
Error: use of moved value "box"
```

After `consume( move box )`, the variable `box` is in the moved state. The subsequent `puts( box.value )` attempts to use a moved value, which the borrow checker rejects.

## Use After Drop

Accessing a variable after it has been freed with `delete` is a compile-time error. The borrow checker tracks the dropped state and prevents any subsequent access.

The following code is rejected by the compiler:

```
package testing

from uranite.io.console import puts

public class Token:
    public I64 code

    public function Token( self, I64 code ) -> Void:
        self.code = code

public function main() -> I32:
    Token token = new Token( 99 )
    delete token
    puts( token.code )
    return 0
```

The compiler produces:

```
Error: use of dropped value "token"
```

After `delete token`, the variable `token` is in the dropped state. Any attempt to access its fields is rejected.

## Double Free Detection

Deleting a variable that has already been freed is a compile-time error. The borrow checker prevents double-free bugs by tracking the dropped state.

The following code is rejected by the compiler:

```
package testing

public class Item:
    public function Item( self ) -> Void:
        pass

public function main() -> I32:
    Item item = new Item()
    delete item
    delete item
    return 0
```

The compiler produces:

```
Error: double free: "item" was already deleted
```

After the first `delete item`, the variable enters the dropped state. The second `delete` is rejected as a double free.

## Move After Move

Moving a variable that has already been moved is a compile-time error. The borrow checker ensures each value is transferred exactly once.

The following code is rejected by the compiler:

```
package testing

from uranite.io.console import puts

public class Packet:
    public function Packet( self ) -> Void:
        pass

public function take( Packet packet ) -> Void:
    puts( "taken" )
    delete packet

public function main() -> I32:
    Packet packet = new Packet()
    take( move packet )
    take( move packet )
    return 0
```

The compiler produces:

```
Error: use of moved value "packet"
  hint: value was previously moved
```

After the first `take( move packet )`, the variable is in the moved state. The second move attempt is rejected with a hint indicating the prior transfer.

## Move After Drop

Moving a variable that has already been deleted is a compile-time error. The borrow checker prevents transferring ownership of freed memory.

The following code is rejected by the compiler:

```
package testing

public class Packet:
    public function Packet( self ) -> Void:
        pass

public function main() -> I32:
    Packet packet = new Packet()
    delete packet
    Packet other = move packet
    return 0
```

The compiler produces:

```
Error: cannot move dropped value "packet"
  hint: value was previously deleted
```

After `delete packet`, the variable is in the dropped state. The subsequent `move` is rejected because the memory no longer exists.

## Branch State Merging

The borrow checker tracks ownership across conditional branches. If any branch moves or drops a variable, the merged state after the branch reflects the most restrictive outcome. This prevents use-after-move bugs in code paths where a variable might or might not have been transferred.

```uranite
package testing

from uranite.io.console import puts

public class Token:
    public String name

    public function Token( self, String name ) -> Void:
        self.name = name

public function main() -> I32:
    I64 mode = 1
    Token token = new Token( "session" )
    if mode == 1:
        puts( token.name )
    else:
        puts( "skipped" )
    puts( token.name )
    delete token
    return 0
```

The variable `token` is used in the `if` branch but not moved or deleted in either branch. After the conditional, `token` remains in the owned state and can be used. The program prints `session`, `session`.
