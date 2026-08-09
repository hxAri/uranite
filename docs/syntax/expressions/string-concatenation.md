# String Concatenation

---

## Table of Contents

- [String Concatenation](#string-concatenation)
  - [Table of Contents](#table-of-contents)
  - [Overview](#overview)
  - [Basic Concatenation](#basic-concatenation)
    - [Literal Concatenation](#literal-concatenation)
    - [Variable Concatenation](#variable-concatenation)
  - [Concatenating Non-String Values](#concatenating-non-string-values)
    - [Integers](#integers)
    - [Floats](#floats)
    - [Booleans](#booleans)
  - [Multi-Step Concatenation](#multi-step-concatenation)
  - [Concatenation in Functions](#concatenation-in-functions)
    - [Return Values](#return-values)
    - [Building Strings in Loops](#building-strings-in-loops)
  - [Concatenation with Class Fields](#concatenation-with-class-fields)
  - [Left Operand Requirement](#left-operand-requirement)
  - [Empty Strings](#empty-strings)
  - [Method Reference](#method-reference)
  - [Examples](#examples)
    - [Literal and Variable Concatenation](#literal-and-variable-concatenation)
    - [String Builder Functions](#string-builder-functions)
    - [Object Description Building](#object-description-building)
    - [Path Construction](#path-construction)

---

## Overview

Uranite uses the `+` operator for string concatenation. When the left operand is a `String`, the `+` operator joins two strings into a new string. Each concatenation produces a new string value — neither operand is modified.

Non-string values must be converted to strings using `.toString()` before concatenation.

---

## Basic Concatenation

### Literal Concatenation

Two string literals can be joined with `+`.

```uranite
from uranite.io.console import puts

public function main() -> I32:
    String greeting = "Hello, " + "world!"
    puts( greeting )
    return 0
```

Output:

```
Hello, world!
```

### Variable Concatenation

String variables work the same way.

```uranite
from uranite.io.console import puts

public function main() -> I32:
    String first = "Uranite"
    String second = " is fast"
    String combined = first + second
    puts( combined )
    return 0
```

Output:

```
Uranite is fast
```

---

## Concatenating Non-String Values

To concatenate a non-string value with a string, call `.toString()` on the value first and store the result in an intermediate variable.

### Integers

```uranite
from uranite.io.console import puts

public function main() -> I32:
    I64 score = 100
    String scoreStr = score.toString()
    String message = "Score: " + scoreStr
    puts( message )
    return 0
```

Output:

```
Score: 100
```

### Floats

```uranite
from uranite.io.console import puts

public function main() -> I32:
    F64 temperature = 36.6
    String tempStr = temperature.toString()
    String reading = "Temp: " + tempStr
    puts( reading )
    return 0
```

Output:

```
Temp: 36.6
```

### Booleans

```uranite
from uranite.io.console import puts

public function main() -> I32:
    Boolean active = True
    String activeStr = active.toString()
    String status = "active: " + activeStr
    puts( status )
    return 0
```

Output:

```
active: True
```

---

## Multi-Step Concatenation

To join more than two strings, use intermediate variables for each step.

```uranite
from uranite.io.console import puts

public function main() -> I32:
    String part1 = "Hello"
    String part2 = ", "
    String part3 = "world"
    String part4 = "!"

    String step1 = part1 + part2
    String step2 = step1 + part3
    String result = step2 + part4
    puts( result )
    return 0
```

Output:

```
Hello, world!
```

Each `+` operation produces a new string. Concatenation is left-associative — `a + b + c` evaluates as `(a + b) + c`.

---

## Concatenation in Functions

### Return Values

Concatenation works in return statements.

```uranite
from uranite.io.console import puts

public function greet( String name ) -> String:
    return "Hello, " + name

public function buildPath( String base, String segment ) -> String:
    return base + "/" + segment

public function main() -> I32:
    String greeting = greet( "Alice" )
    puts( greeting )

    String path = buildPath( "home", "user" )
    puts( path )
    return 0
```

Output:

```
Hello, Alice
home/user
```

### Building Strings in Loops

Concatenation can be used in loops to build strings incrementally.

```uranite
from uranite.io.console import puts

public function repeatString( String text, I64 times ) -> String:
    String result = ""
    I64 count = 0
    while count < times:
        result = result + text
        count++
    return result

public function main() -> I32:
    String repeated = repeatString( "ab", 3 )
    puts( repeated )

    String single = repeatString( "x", 1 )
    puts( single )
    return 0
```

Output:

```
ababab
x
```

Each iteration creates a new string by appending `text` to the accumulated `result`.

---

## Concatenation with Class Fields

String fields on objects can be used in concatenation.

```uranite
from uranite.io.console import puts

class Person:

    public String fullName
    public I64 age

    public function Person( self, String fullName, I64 age ) -> Void:
        self.fullName = fullName
        self.age = age

    public function describe( self ) -> String:
        String ageStr = self.age.toString()
        String part1 = self.fullName + " (age "
        String result = part1 + ageStr
        String complete = result + ")"
        return complete

public function main() -> I32:
    Person alice = new Person( "Alice", 30 )
    String description = alice.describe()
    puts( description )

    Person bob = new Person( "Bob", 25 )
    String description2 = bob.describe()
    puts( description2 )
    return 0
```

Output:

```
Alice (age 30)
Bob (age 25)
```

---

## Left Operand Requirement

The `+` operator triggers string concatenation only when the **left** operand is a `String`. Placing a non-string value on the left side produces a compile-time error.

```uranite
I64 number = 42
String result = number + " items"
```

This produces:

```
Error: invalid operands to binary "+": "I64" and "String"
```

To fix, convert the left operand to a string first:

```uranite
I64 number = 42
String numberStr = number.toString()
String result = numberStr + " items"
```

---

## Empty Strings

Concatenation with an empty string produces the non-empty operand unchanged.

```uranite
from uranite.io.console import puts

public function main() -> I32:
    String empty = "" + "content"
    puts( empty )

    String leftEmpty = "content" + ""
    puts( leftEmpty )
    return 0
```

Output:

```
content
content
```

---

## Method Reference

| Expression | Result | Description |
|---|---|---|
| `"a" + "b"` | `"ab"` | Literal concatenation |
| `strVar + "b"` | New string | Variable + literal |
| `strVar + otherVar` | New string | Variable + variable |
| `"" + str` | Same as `str` | Empty left operand |
| `str + ""` | Same as `str` | Empty right operand |

---

## Examples

### Literal and Variable Concatenation

```uranite
from uranite.io.console import puts

public function main() -> I32:
    String greeting = "Hello, " + "world!"
    puts( greeting )

    String first = "Uranite"
    String second = " is fast"
    String combined = first + second
    puts( combined )

    String empty = "" + "content"
    puts( empty )

    I64 score = 100
    String scoreStr = score.toString()
    String message = "Score: " + scoreStr
    puts( message )

    F64 temperature = 36.6
    String tempStr = temperature.toString()
    String reading = "Temp: " + tempStr
    puts( reading )
    return 0
```

Output:

```
Hello, world!
Uranite is fast
content
Score: 100
Temp: 36.6
```

### String Builder Functions

```uranite
from uranite.io.console import puts

public function formatPair( String key, String val ) -> String:
    String colon = key + ": "
    String result = colon + val
    return result

public function repeatString( String text, I64 times ) -> String:
    String result = ""
    I64 count = 0
    while count < times:
        result = result + text
        count++
    return result

public function main() -> I32:
    String entry1 = formatPair( "name", "Alice" )
    puts( entry1 )

    String entry2 = formatPair( "role", "engineer" )
    puts( entry2 )

    String repeated = repeatString( "ab", 3 )
    puts( repeated )

    String single = repeatString( "x", 1 )
    puts( single )
    return 0
```

Output:

```
name: Alice
role: engineer
ababab
x
```

### Object Description Building

```uranite
from uranite.io.console import puts

class Person:

    public String fullName
    public I64 age

    public function Person( self, String fullName, I64 age ) -> Void:
        self.fullName = fullName
        self.age = age

    public function describe( self ) -> String:
        String ageStr = self.age.toString()
        String part1 = self.fullName + " (age "
        String result = part1 + ageStr
        String complete = result + ")"
        return complete

public function main() -> I32:
    Person alice = new Person( "Alice", 30 )
    String description = alice.describe()
    puts( description )

    Person bob = new Person( "Bob", 25 )
    String description2 = bob.describe()
    puts( description2 )
    return 0
```

Output:

```
Alice (age 30)
Bob (age 25)
```

### Path Construction

```uranite
from uranite.io.console import puts

public function buildPath( String base, String segment ) -> String:
    return base + "/" + segment

public function main() -> I32:
    String root = "/home"
    String userPath = buildPath( root, "alice" )
    puts( userPath )

    String docPath = buildPath( userPath, "documents" )
    puts( docPath )

    Boolean isAdmin = True
    String adminStr = isAdmin.toString()
    String adminLabel = "admin: " + adminStr
    puts( adminLabel )

    I64 fileCount = 42
    String countStr = fileCount.toString()
    String summary = docPath + " (" + countStr
    String fullSummary = summary + " files)"
    puts( fullSummary )
    return 0
```

Output:

```
/home/alice
/home/alice/documents
admin: True
/home/alice/documents (42 files)
```
