# Optional Types

---

## Table of Contents

- [Optional Types](#optional-types)
  - [Table of Contents](#table-of-contents)
  - [Overview](#overview)
  - [Declaring Optional Variables](#declaring-optional-variables)
    - [Optional String](#optional-string)
    - [Optional Integer](#optional-integer)
    - [Optional Boolean](#optional-boolean)
    - [Assigning Values to Optionals](#assigning-values-to-optionals)
  - [None Checks](#none-checks)
    - [Identity Check with is](#identity-check-with-is)
    - [Negated Identity with is not](#negated-identity-with-is-not)
    - [Equality Check with ==](#equality-check-with-)
    - [Inequality Check with !=](#inequality-check-with-)
  - [Optional Function Parameters](#optional-function-parameters)
  - [Optional Return Types](#optional-return-types)
  - [Unwrapping Optionals](#unwrapping-optionals)
  - [Optional Fields in Classes](#optional-fields-in-classes)
  - [Common Patterns](#common-patterns)
    - [Default Value Pattern](#default-value-pattern)
    - [First Present Selection](#first-present-selection)
    - [Presence Check](#presence-check)
    - [Optional Forwarding](#optional-forwarding)
  - [Method Reference](#method-reference)
    - [Declaration Syntax](#declaration-syntax)
    - [None Check Syntax](#none-check-syntax)
  - [Examples](#examples)
    - [User Lookup System](#user-lookup-system)
    - [Configuration with Defaults](#configuration-with-defaults)
    - [Optional Class Fields](#optional-class-fields)
    - [Multi-Optional Decision Logic](#multi-optional-decision-logic)

---

## Overview

An optional type represents a value that may or may not be present. An optional is either a valid value of its inner type or `None`. Optional types are declared with the `?` prefix before the type name.

`?String` means "a String or None". `?I64` means "an I64 or None". Each combination of `?` and a type creates a distinct type in the type system.

Optional types use `is None` and `is not None` for presence checks. Values of type `T` can be assigned directly to `?T` variables, and `None` can be assigned to any optional type.

---

## Declaring Optional Variables

### Optional String

Use `?String` to declare a variable that can hold a `String` or `None`.

```uranite
?String maybeName = "Alice"
?String noName = None
```

### Optional Integer

Use `?I64` to declare a variable that can hold an `I64` or `None`.

```uranite
?I64 maybeCount = 42
?I64 noCount = None
```

### Optional Boolean

Use `?Boolean` to declare a variable that can hold a `Boolean` or `None`.

```uranite
?Boolean maybeFlag = True
?Boolean noFlag = None
```

### Assigning Values to Optionals

A concrete value of type `T` can be assigned directly to a variable of type `?T`. No wrapping is needed.

```uranite
?String name = "Alice"
?I64 count = 100
?Boolean flag = False
```

Assigning `None` sets the optional to the absent state.

```uranite
?String name = None
?I64 count = None
```

---

## None Checks

### Identity Check with is

The `is` keyword checks whether an optional value is `None`.

```uranite
from uranite.io.console import puts

public function main() -> I32:
    ?String name = None
    if name is None:
        puts( "absent" )
    return 0
```

Output:

```
absent
```

### Negated Identity with is not

Use `is not None` to check that an optional has a value.

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

### Equality Check with ==

The `==` operator can compare an optional with `None`.

```uranite
from uranite.io.console import puts

public function main() -> I32:
    ?String name = "hello"
    if name == None:
        puts( "none" )
    else:
        puts( "not none" )
    return 0
```

Output:

```
not none
```

### Inequality Check with !=

The `!=` operator checks that an optional is not `None`.

```uranite
from uranite.io.console import puts

public function main() -> I32:
    ?String name = "hello"
    if name != None:
        puts( "has value" )
    return 0
```

Output:

```
has value
```

---

## Optional Function Parameters

Functions can accept optional parameters, allowing callers to pass either a value or `None`.

```uranite
from uranite.io.console import puts

public function greet( String name, ?String title ) -> Void:
    if title is not None:
        puts( title )
        puts( name )
    else:
        puts( name )

public function main() -> I32:
    greet( "Alice", "Dr." )
    greet( "Bob", None )
    return 0
```

Output:

```
Dr.
Alice
Bob
```

When `title` is not `None`, both the title and name are printed. When `title` is `None`, only the name is printed.

---

## Optional Return Types

Functions can return `?T` to indicate they may or may not produce a value.

```uranite
from uranite.io.console import puts

public function findUser( String name ) -> ?String:
    if name == "admin":
        return "Administrator"
    return None

public function main() -> I32:
    ?String found = findUser( "admin" )
    ?String missing = findUser( "guest" )
    if found is not None:
        puts( found )
    if missing is None:
        puts( "not found" )
    return 0
```

Output:

```
Administrator
not found
```

The function returns a `String` value for known users and `None` for unknown users. The caller checks the result before using it.

---

## Unwrapping Optionals

An optional value can be assigned to a non-optional variable of the same type. This implicitly unwraps the optional.

```uranite
from uranite.io.console import puts

public function main() -> I32:
    ?I64 maybeValue = 42
    I64 concrete = maybeValue
    puts( concrete.toString() )
    return 0
```

Output:

```
42
```

If the optional is `None` at runtime when unwrapped this way, the behavior is undefined. Always check for `None` before unwrapping.

```uranite
from uranite.io.console import puts

public function safeUnwrap( ?I64 value ) -> I64:
    if value is None:
        return 0
    I64 unwrapped = value
    return unwrapped

public function main() -> I32:
    puts( safeUnwrap( 42 ).toString() )
    puts( safeUnwrap( None ).toString() )
    return 0
```

Output:

```
42
0
```

---

## Optional Fields in Classes

Class fields can be declared as optional types, allowing objects to have nullable properties.

```uranite
from uranite.io.console import puts

class UserProfile:

    public String name
    public ?String email

    public function UserProfile( self, String name, ?String email ) -> Void:
        self.name = name
        self.email = email

    public function hasEmail( self ) -> Boolean:
        if self.email is not None:
            return True
        return False

public function main() -> I32:
    UserProfile withEmail = new UserProfile( "Alice", "alice@example.com" )
    UserProfile noEmail = new UserProfile( "Bob", None )
    puts( withEmail.name )
    if withEmail.hasEmail() == True:
        puts( withEmail.email )
    puts( noEmail.name )
    if noEmail.hasEmail() == False:
        puts( "no email" )
    return 0
```

Output:

```
Alice
alice@example.com
Bob
no email
```

---

## Common Patterns

### Default Value Pattern

Return a fallback value when an optional is `None`.

```uranite
from uranite.io.console import puts

public function withDefault( ?String value, String fallback ) -> String:
    if value is not None:
        return value
    return fallback

public function main() -> I32:
    puts( withDefault( "present", "default" ) )
    puts( withDefault( None, "default" ) )
    return 0
```

Output:

```
present
default
```

### First Present Selection

Return the first non-None value from a series of optionals.

```uranite
from uranite.io.console import puts

public function firstPresent( ?String alpha, ?String beta ) -> ?String:
    if alpha is not None:
        return alpha
    if beta is not None:
        return beta
    return None

public function main() -> I32:
    ?String result = firstPresent( None, "backup" )
    if result is not None:
        puts( result )
    ?String noResult = firstPresent( None, None )
    if noResult is None:
        puts( "all None" )
    return 0
```

Output:

```
backup
all None
```

### Presence Check

Check whether multiple optional values are all present or all absent.

```uranite
from uranite.io.console import puts

public function allPresent( ?String first, ?String second ) -> Boolean:
    if first is None:
        return False
    if second is None:
        return False
    return True

public function main() -> I32:
    Boolean both = allPresent( "a", "b" )
    Boolean oneMissing = allPresent( "a", None )
    Boolean noneMissing = allPresent( None, None )
    puts( both.toString() )
    puts( oneMissing.toString() )
    puts( noneMissing.toString() )
    return 0
```

Output:

```
True
False
False
```

### Optional Forwarding

Pass optional values through multiple function layers.

```uranite
from uranite.io.console import puts

public function processName( ?String name ) -> String:
    if name is None:
        return "anonymous"
    return name

public function formatGreeting( ?String name ) -> String:
    String resolved = processName( name )
    return resolved

public function main() -> I32:
    puts( formatGreeting( "Alice" ) )
    puts( formatGreeting( None ) )
    return 0
```

Output:

```
Alice
anonymous
```

---

## Method Reference

### Declaration Syntax

| Syntax | Description |
|---|---|
| `?String name` | Optional String variable |
| `?I64 count` | Optional integer variable |
| `?Boolean flag` | Optional Boolean variable |
| `?String name = "value"` | Optional initialized with a value |
| `?String name = None` | Optional initialized as absent |
| `function name() -> ?T:` | Function returning an optional |
| `function name( ?T param ) -> Void:` | Function accepting an optional parameter |

### None Check Syntax

| Syntax | Description |
|---|---|
| `value is None` | True when the optional is absent |
| `value is not None` | True when the optional has a value |
| `value == None` | Equality check against None |
| `value != None` | Inequality check against None |

---

## Examples

### User Lookup System

```uranite
from uranite.io.console import puts

public function findUser( String name ) -> ?String:
    if name == "admin":
        return "Administrator"
    if name == "root":
        return "Superuser"
    return None

public function describeUser( String name ) -> Void:
    ?String role = findUser( name )
    if role is not None:
        puts( name )
        puts( role )
    else:
        puts( name )
        puts( "unknown" )

public function main() -> I32:
    describeUser( "admin" )
    describeUser( "root" )
    describeUser( "guest" )
    return 0
```

Output:

```
admin
Administrator
root
Superuser
guest
unknown
```

### Configuration with Defaults

```uranite
from uranite.io.console import puts

public function resolveHost( ?String host ) -> String:
    if host is not None:
        return host
    return "localhost"

public function resolvePort( ?I64 port ) -> I64:
    if port is not None:
        I64 resolved = port
        return resolved
    return 8080

public function main() -> I32:
    String host1 = resolveHost( "example.com" )
    String host2 = resolveHost( None )
    I64 port1 = resolvePort( 3000 )
    I64 port2 = resolvePort( None )
    puts( host1 )
    puts( host2 )
    puts( port1.toString() )
    puts( port2.toString() )
    return 0
```

Output:

```
example.com
localhost
3000
8080
```

### Optional Class Fields

```uranite
from uranite.io.console import puts

class Contact:

    public String name
    public ?String phone
    public ?String email

    public function Contact( self, String name, ?String phone, ?String email ) -> Void:
        self.name = name
        self.phone = phone
        self.email = email

    public function contactMethodCount( self ) -> I64:
        I64 count = 0
        if self.phone is not None:
            count = count + 1
        if self.email is not None:
            count = count + 1
        return count

public function main() -> I32:
    Contact full = new Contact( "Alice", "555-1234", "alice@example.com" )
    Contact partial = new Contact( "Bob", "555-5678", None )
    Contact minimal = new Contact( "Charlie", None, None )
    puts( full.contactMethodCount().toString() )
    puts( partial.contactMethodCount().toString() )
    puts( minimal.contactMethodCount().toString() )
    return 0
```

Output:

```
2
1
0
```

### Multi-Optional Decision Logic

```uranite
from uranite.io.console import puts

public function allPresent( ?String first, ?String second ) -> Boolean:
    if first is None:
        return False
    if second is None:
        return False
    return True

public function withDefault( ?String value, String fallback ) -> String:
    if value is not None:
        return value
    return fallback

public function firstPresent( ?String alpha, ?String beta, ?String gamma ) -> ?String:
    if alpha is not None:
        return alpha
    if beta is not None:
        return beta
    if gamma is not None:
        return gamma
    return None

public function main() -> I32:
    ?String result = firstPresent( None, None, "fallback" )
    puts( withDefault( result, "none" ) )

    ?String noResult = firstPresent( None, None, None )
    puts( withDefault( noResult, "none" ) )

    Boolean both = allPresent( "a", "b" )
    Boolean missing = allPresent( "a", None )
    puts( both.toString() )
    puts( missing.toString() )
    return 0
```

Output:

```
fallback
none
True
False
```
