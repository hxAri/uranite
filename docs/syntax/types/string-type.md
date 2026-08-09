# String Type

---

## Table of Contents

- [String Type](#string-type)
  - [Table of Contents](#table-of-contents)
  - [Overview](#overview)
  - [String Literals](#string-literals)
    - [Basic Syntax](#basic-syntax)
    - [Escape Sequences](#escape-sequences)
    - [Hex Escapes](#hex-escapes)
    - [Strings vs Characters](#strings-vs-characters)
  - [String Length and Emptiness](#string-length-and-emptiness)
    - [length](#length)
    - [isEmpty](#isempty)
  - [String Concatenation](#string-concatenation)
    - [The Plus Operator](#the-plus-operator)
    - [The concat Method](#the-concat-method)
    - [Converting Other Types to String](#converting-other-types-to-string)
    - [Building Strings in Loops](#building-strings-in-loops)
  - [String Comparison](#string-comparison)
    - [Equality and Inequality Operators](#equality-and-inequality-operators)
    - [The equals Method](#the-equals-method)
  - [String Searching](#string-searching)
    - [contains](#contains)
    - [startsWith](#startswith)
    - [endsWith](#endswith)
    - [indexOf](#indexof)
  - [Byte Access](#byte-access)
    - [charAt](#charat)
    - [charCodeAt](#charcodeat)
    - [Byte Values and Characters](#byte-values-and-characters)
  - [Substrings](#substrings)
    - [substring](#substring)
  - [String Transformation](#string-transformation)
    - [toUpper](#toupper)
    - [toLower](#tolower)
    - [trim](#trim)
    - [replace](#replace)
    - [Transformation Produces New Strings](#transformation-produces-new-strings)
  - [String Constants](#string-constants)
  - [Optional Strings](#optional-strings)
  - [String Methods](#string-methods)
    - [Complete Method Reference](#complete-method-reference)
    - [Method Chaining Limitation](#method-chaining-limitation)
  - [Practical Examples](#practical-examples)
    - [String Repetition](#string-repetition)
    - [Left Padding](#left-padding)
    - [Greeting Builder](#greeting-builder)
    - [Word Counter](#word-counter)
    - [String Sanitizer](#string-sanitizer)

---

## Overview

`String` represents an immutable sequence of bytes. Strings are one of the most frequently used types in Uranite — they hold text, messages, file paths, formatted output, and any other sequence of characters.

| Property | Value |
|---|---|
| Type name | `String` |
| Literal delimiter | Double quotes (`"..."`) |
| Mutability | Immutable — all transformations produce new strings |
| Empty string | `""` (zero-length, valid) |
| Subclassable | Yes |

Strings in Uranite are immutable. Every operation that appears to modify a string — concatenation, case conversion, trimming, replacement — actually creates and returns a new string. The original string is never changed.

---

## String Literals

### Basic Syntax

String literals are enclosed in double quotes. They may contain zero or more characters:

```uranite
String greeting = "Hello, world!"
String empty = ""
String single = "A"
String multiWord = "Uranite programming language"
String path = "/home/user/project"
```

An empty string `""` is a valid string with a length of zero. It is not the same as `None` — an empty string is a real `String` value that exists in memory.

### Escape Sequences

Certain characters cannot be typed directly inside a string literal because they would conflict with the delimiter or represent invisible control characters. Use escape sequences — a backslash followed by a specific character — to include them:

| Escape | Character | Description |
|---|---|---|
| `\"` | `"` | Double quote (the delimiter itself) |
| `\'` | `'` | Single quote |
| `\\` | `\` | Backslash |
| `\0` | NUL | Null character (code point 0) |
| `\n` | LF | Line feed (newline) |
| `\r` | CR | Carriage return |
| `\t` | TAB | Horizontal tab |

```uranite
from uranite.io.console import puts

public function main() -> I32:
    String newlines = "Line one\nLine two"
    puts( newlines )
    String tabs = "Name\tAge"
    puts( tabs )
    String quoted = "She said \"hello\""
    puts( quoted )
    String backslash = "C:\\Users\\docs"
    puts( backslash )
    return 0
```

This outputs:

```
Line one
Line two
Name	Age
She said "hello"
C:\Users\docs
```

The `\"` escape is necessary when you need a literal double quote inside a string. Without it, the double quote would be interpreted as the end of the string. The `\\` escape produces a single backslash — since backslash is the escape character, you must double it to include a literal backslash.

### Hex Escapes

The `\x` escape specifies a byte by its hexadecimal value, using exactly two hex digits:

```uranite
from uranite.io.console import puts

public function main() -> I32:
    String hexHello = "\x48\x65\x6C\x6C\x6F"
    puts( hexHello )
    return 0
```

This outputs `Hello` — each hex escape produces the byte corresponding to the ASCII code point: `\x48` is `H` (72), `\x65` is `e` (101), `\x6C` is `l` (108), `\x6F` is `o` (111).

The hex range is 0x00 through 0xFF (0 through 255), covering the full ASCII set and Latin-1 supplement. Hex escapes can be mixed freely with regular characters in the same string.

### Strings vs Characters

Single quotes produce a `Char` value. Double quotes produce a `String` value. These are distinct types with no implicit conversion between them:

```uranite
Char letter = 'A'
String text = "A"
```

`letter` is a single character value. `text` is a string containing one character. To convert a `Char` to a `String`, use `toString()`:

```uranite
Char letter = 'A'
String asString = letter.toString()
```

---

## String Length and Emptiness

### length

`length()` returns the number of bytes in the string as an `I64` value:

```uranite
from uranite.io.console import puts

public function main() -> I32:
    String text = "Hello, Uranite!"
    I64 len = text.length()
    puts( "length: " + len.toString() )
    String empty = ""
    I64 emptyLen = empty.length()
    puts( "empty length: " + emptyLen.toString() )
    return 0
```

This outputs:

```
length: 15
empty length: 0
```

For ASCII strings, the byte count equals the character count. Each ASCII character occupies exactly one byte.

### isEmpty

`isEmpty()` returns `True` if the string has zero length, and `False` otherwise:

```uranite
from uranite.io.console import puts

public function main() -> I32:
    String text = "Hello"
    Boolean textEmpty = text.isEmpty()
    puts( "Hello isEmpty: " + textEmpty.toString() )
    String blank = ""
    Boolean blankEmpty = blank.isEmpty()
    puts( "empty isEmpty: " + blankEmpty.toString() )
    return 0
```

This outputs:

```
Hello isEmpty: False
empty isEmpty: True
```

Note that a string containing only whitespace (like `" "`) is not empty — it has a length greater than zero. Use `trim()` followed by `isEmpty()` to check for whitespace-only strings (store each result in a variable — see [Method Chaining Limitation](#method-chaining-limitation)).

---

## String Concatenation

### The Plus Operator

The `+` operator joins two strings together, producing a new string:

```uranite
from uranite.io.console import puts

public function main() -> I32:
    String hello = "Hello"
    String world = "World"
    String combined = hello + ", " + world + "!"
    puts( combined )
    return 0
```

This outputs `Hello, World!`. Each `+` operation creates a new string containing the bytes of both operands.

### The concat Method

The `concat()` method provides the same functionality as `+`:

```uranite
from uranite.io.console import puts

public function main() -> I32:
    String hello = "Hello"
    String space = hello.concat( " " )
    String full = space.concat( "World" )
    puts( full )
    return 0
```

This outputs `Hello World`. Note that each `concat()` call is stored in a separate variable — do not chain `concat()` calls together (see [Method Chaining Limitation](#method-chaining-limitation)).

### Converting Other Types to String

To include non-string values in a concatenated expression, convert them to strings first using `toString()`:

```uranite
from uranite.io.console import puts

public function main() -> I32:
    I64 value = 42
    String withNum = "answer: " + value.toString()
    puts( withNum )
    F64 pi = 3.14159
    String withFloat = "pi = " + pi.toString()
    puts( withFloat )
    Boolean flag = True
    String withBool = "active: " + flag.toString()
    puts( withBool )
    Char letter = 'X'
    String withChar = "letter: " + letter.toString()
    puts( withChar )
    return 0
```

This outputs:

```
answer: 42
pi = 3.14159
active: True
letter: X
```

Every type in Uranite has a `toString()` method. Integers produce their decimal representation. Floats produce their decimal representation. Booleans produce `"True"` or `"False"`. Characters produce a single-character string.

### Building Strings in Loops

Concatenation inside loops builds strings incrementally. Each iteration creates a new string:

```uranite
from uranite.io.console import puts

public function repeatString( String text, I64 count ) -> String:
    String result = ""
    I64 iteration = 0
    while iteration < count:
        result = result + text
        iteration = iteration + 1
    return result

public function main() -> I32:
    String repeated = repeatString( "ab", 3 )
    puts( repeated )
    return 0
```

This outputs `ababab`. The loop concatenates `text` onto `result` in each iteration, reassigning `result` to the new combined string.

---

## String Comparison

### Equality and Inequality Operators

The `==` operator compares the content of two strings. It returns `True` if both strings contain exactly the same bytes:

```uranite
from uranite.io.console import puts

public function main() -> I32:
    String first = "alpha"
    String second = "alpha"
    Boolean same = first == second
    puts( "alpha == alpha: " + same.toString() )
    String third = "beta"
    Boolean different = first == third
    puts( "alpha == beta: " + different.toString() )
    Boolean neq = first != third
    puts( "alpha != beta: " + neq.toString() )
    return 0
```

This outputs:

```
alpha == alpha: True
alpha == beta: False
alpha != beta: True
```

String comparison is case-sensitive — `"Hello"` and `"hello"` are not equal. To perform case-insensitive comparison, convert both strings to the same case before comparing:

```uranite
String first = "Hello"
String second = "hello"
String firstLower = first.toLower()
String secondLower = second.toLower()
Boolean match = firstLower == secondLower
```

### The equals Method

`equals()` provides the same content comparison as `==`:

```uranite
from uranite.io.console import puts

public function main() -> I32:
    String greeting = "Hello"
    Boolean match = greeting.equals( "Hello" )
    puts( "equals Hello: " + match.toString() )
    Boolean noMatch = greeting.equals( "World" )
    puts( "equals World: " + noMatch.toString() )
    return 0
```

This outputs:

```
equals Hello: True
equals World: False
```

Use whichever form reads more naturally in context. The `==` operator is typically clearer for simple comparisons, while `equals()` may read better in method chains or complex expressions.

---

## String Searching

### contains

`contains()` returns `True` if the string includes the given substring anywhere within it:

```uranite
from uranite.io.console import puts

public function main() -> I32:
    String text = "The quick brown fox"
    Boolean hasQuick = text.contains( "quick" )
    puts( "contains quick: " + hasQuick.toString() )
    Boolean hasCat = text.contains( "cat" )
    puts( "contains cat: " + hasCat.toString() )
    return 0
```

This outputs:

```
contains quick: True
contains cat: False
```

The search is case-sensitive. To search case-insensitively, convert both the string and the search term to the same case first.

### startsWith

`startsWith()` returns `True` if the string begins with the given prefix:

```uranite
from uranite.io.console import puts

public function main() -> I32:
    String text = "Hello, World!"
    Boolean startsHello = text.startsWith( "Hello" )
    puts( "starts Hello: " + startsHello.toString() )
    Boolean startsWorld = text.startsWith( "World" )
    puts( "starts World: " + startsWorld.toString() )
    return 0
```

This outputs:

```
starts Hello: True
starts World: False
```

An empty prefix `""` matches any string — `startsWith( "" )` always returns `True`.

### endsWith

`endsWith()` returns `True` if the string ends with the given suffix:

```uranite
from uranite.io.console import puts

public function main() -> I32:
    String text = "Hello, World!"
    Boolean endsExclaim = text.endsWith( "!" )
    puts( "ends !: " + endsExclaim.toString() )
    Boolean endsHello = text.endsWith( "Hello" )
    puts( "ends Hello: " + endsHello.toString() )
    return 0
```

This outputs:

```
ends !: True
ends Hello: False
```

### indexOf

`indexOf()` returns the byte position of the first occurrence of a substring. If the substring is not found, it returns `-1`:

```uranite
from uranite.io.console import puts

public function main() -> I32:
    String text = "The quick brown fox"
    I64 foxPos = text.indexOf( "fox" )
    puts( "indexOf fox: " + foxPos.toString() )
    I64 catPos = text.indexOf( "cat" )
    puts( "indexOf cat: " + catPos.toString() )
    return 0
```

This outputs:

```
indexOf fox: 16
indexOf cat: -1
```

The returned position is a zero-based byte offset. For ASCII strings, the byte offset equals the character position. The value `-1` is the standard sentinel indicating "not found" — always check for this before using the result as an index.

---

## Byte Access

### charAt

`charAt()` returns the byte value at the given zero-based position as an `I64`:

```uranite
from uranite.io.console import puts

public function main() -> I32:
    String text = "Hello"
    I64 firstByte = text.charAt( 0 )
    puts( "charAt 0: " + firstByte.toString() )
    I64 lastByte = text.charAt( 4 )
    puts( "charAt 4: " + lastByte.toString() )
    return 0
```

This outputs:

```
charAt 0: 72
charAt 4: 111
```

The value 72 is the ASCII code point for `H`. The value 111 is the ASCII code point for `o`. The method returns raw byte values, not `Char` values.

### charCodeAt

`charCodeAt()` is identical to `charAt()` — it returns the byte value at the given position as an `I64`:

```uranite
from uranite.io.console import puts

public function main() -> I32:
    String text = "Hello"
    I64 code = text.charCodeAt( 1 )
    puts( "charCodeAt 1: " + code.toString() )
    return 0
```

This outputs `charCodeAt 1: 101`, which is the ASCII code point for `e`.

### Byte Values and Characters

Both `charAt()` and `charCodeAt()` return numeric byte values, not characters. To interpret these values, refer to the ASCII table:

| Byte Value | Character |
|---|---|
| 32 | Space |
| 48–57 | `0` through `9` |
| 65–90 | `A` through `Z` |
| 97–122 | `a` through `z` |

To compare a byte value against a specific character, use the character's ASCII code point:

```uranite
I64 byteValue = text.charAt( 0 )
if byteValue == 32:
    puts( "space found" )
```

The value 32 is the ASCII code for the space character. No bounds checking is performed on the index — passing an index beyond the string length reads invalid memory. Always verify that the index is within the valid range (0 to `length() - 1`) before calling these methods.

---

## Substrings

### substring

`substring()` extracts a portion of a string given a start position and an end position. Both positions are zero-based byte offsets. The returned string includes the character at the start position and excludes the character at the end position:

```uranite
from uranite.io.console import puts

public function main() -> I32:
    String text = "Hello, World!"
    String sub1 = text.substring( 0, 5 )
    puts( "0-5: " + sub1 )
    String sub2 = text.substring( 7, 12 )
    puts( "7-12: " + sub2 )
    String sub3 = text.substring( 0, 1 )
    puts( "0-1: " + sub3 )
    return 0
```

This outputs:

```
0-5: Hello
7-12: World
0-1: H
```

The first argument is the inclusive start index. The second argument is the exclusive end index. `substring( 0, 5 )` extracts bytes at positions 0, 1, 2, 3, and 4 — five characters total.

To extract from a known position to the end of the string, pass `length()` as the end index:

```uranite
String text = "Hello, World!"
I64 len = text.length()
String tail = text.substring( 7, len )
```

No bounds checking is performed — passing indices beyond the string length reads invalid memory. Always verify that both indices are within the valid range before calling this method.

---

## String Transformation

All transformation methods produce new strings. The original string is never modified.

### toUpper

`toUpper()` returns a new string with all lowercase ASCII letters converted to uppercase. Characters outside the `a`–`z` range are unchanged:

```uranite
from uranite.io.console import puts

public function main() -> I32:
    String text = "Hello, World!"
    String upper = text.toUpper()
    puts( upper )
    return 0
```

This outputs `HELLO, WORLD!`. The comma, space, and exclamation mark pass through unchanged because they are not letters.

### toLower

`toLower()` returns a new string with all uppercase ASCII letters converted to lowercase. Characters outside the `A`–`Z` range are unchanged:

```uranite
from uranite.io.console import puts

public function main() -> I32:
    String text = "Hello, World!"
    String lower = text.toLower()
    puts( lower )
    return 0
```

This outputs `hello, world!`.

### trim

`trim()` returns a new string with leading and trailing whitespace removed. Whitespace characters are: space, tab (`\t`), newline (`\n`), and carriage return (`\r`):

```uranite
from uranite.io.console import puts

public function main() -> I32:
    String padded = "  hello  "
    String trimmed = padded.trim()
    puts( "[" + trimmed + "]" )
    return 0
```

This outputs `[hello]`. The spaces at the beginning and end are removed. Whitespace in the middle of the string is not affected.

### replace

`replace()` returns a new string with all non-overlapping occurrences of the search string replaced by the replacement string:

```uranite
from uranite.io.console import puts

public function main() -> I32:
    String text = "Hello, World!"
    String replaced = text.replace( "World", "Uranite" )
    puts( replaced )
    String multi = "aabaa"
    String multiReplaced = multi.replace( "a", "x" )
    puts( multiReplaced )
    return 0
```

This outputs:

```
Hello, Uranite!
xxbxx
```

The replacement is global — every occurrence of the search string is replaced, not just the first one. In the second example, all four `a` characters are replaced with `x`.

If the search string is not found, the original string is returned unchanged.

### Transformation Produces New Strings

Every transformation method allocates and returns a new string. The original is untouched:

```uranite
from uranite.io.console import puts

public function main() -> I32:
    String original = "  Hello  "
    String trimmed = original.trim()
    String upper = trimmed.toUpper()
    puts( "original: [" + original + "]" )
    puts( "trimmed: [" + trimmed + "]" )
    puts( "upper: [" + upper + "]" )
    return 0
```

This outputs:

```
original: [  Hello  ]
trimmed: [Hello]
upper: [HELLO]
```

All three strings exist independently. Modifying (or discarding) `trimmed` or `upper` has no effect on `original`.

---

## String Constants

Top-level `const` declarations create compile-time string constants accessible from any function in the module:

```uranite
from uranite.io.console import puts

const String GREETING = "Hello"
const String SEPARATOR = ", "

public function main() -> I32:
    String message = GREETING + SEPARATOR + "World!"
    puts( message )
    return 0
```

This outputs `Hello, World!`. String constants are embedded in the program at compile time and exist for the lifetime of the program.

---

## Optional Strings

Optional strings use the `?String` type to represent a value that may or may not be present. Use `is None` to check whether an optional string has a value:

```uranite
from uranite.io.console import puts

public function findGreeting( String name ) -> ?String:
    if name == "Alice":
        return "Hello, Alice!"
    return None

public function main() -> I32:
    ?String result = findGreeting( "Alice" )
    if result is None:
        puts( "not found" )
    else:
        puts( "found" )
    ?String missing = findGreeting( "Bob" )
    if missing is None:
        puts( "Bob not found" )
    return 0
```

This outputs:

```
found
Bob not found
```

`None` represents the absence of a value. It is distinct from an empty string `""` — an empty string is a valid `String` value, while `None` means no string exists at all.

---

## String Methods

### Complete Method Reference

| Method | Return Type | Description |
|---|---|---|
| `length()` | `I64` | Returns the byte count of the string |
| `isEmpty()` | `Boolean` | Returns `True` if the string has zero length |
| `contains( String substring )` | `Boolean` | Returns `True` if the substring is found |
| `startsWith( String prefix )` | `Boolean` | Returns `True` if the string begins with the prefix |
| `endsWith( String suffix )` | `Boolean` | Returns `True` if the string ends with the suffix |
| `indexOf( String substring )` | `I64` | Returns byte offset of first occurrence, or `-1` |
| `charAt( I64 index )` | `I64` | Returns byte value at position |
| `charCodeAt( I64 index )` | `I64` | Returns byte value at position (same as `charAt`) |
| `substring( I64 start, I64 end )` | `String` | Returns new string from start (inclusive) to end (exclusive) |
| `toUpper()` | `String` | Returns new string with lowercase letters uppercased |
| `toLower()` | `String` | Returns new string with uppercase letters lowercased |
| `trim()` | `String` | Returns new string with leading/trailing whitespace removed |
| `replace( String search, String replacement )` | `String` | Returns new string with all occurrences replaced |
| `concat( String other )` | `String` | Returns new string combining both strings |
| `equals( String other )` | `Boolean` | Returns `True` if contents match |
| `getValue()` | `String` | Returns the string itself |
| `toString()` | `String` | Returns the string itself |
| `hash()` | `I64` | Returns a hash value |

### Method Chaining Limitation

Method chaining — calling a method on the result of another method call — produces incorrect results in Uranite. Always store intermediate results in separate variables:

Do **not** write:

```
String result = text.trim().toUpper()
```

Instead, write:

```uranite
String trimmed = text.trim()
String result = trimmed.toUpper()
```

This applies to all method calls on all types, not just strings. Each method call result must be stored in its own variable before calling the next method.

---

## Practical Examples

### String Repetition

Repeat a string a specified number of times by concatenating it in a loop:

```uranite
from uranite.io.console import puts

public function repeatString( String text, I64 count ) -> String:
    String result = ""
    I64 iteration = 0
    while iteration < count:
        result = result + text
        iteration = iteration + 1
    return result

public function main() -> I32:
    String stars = repeatString( "*", 5 )
    puts( stars )
    String pattern = repeatString( "ab", 3 )
    puts( pattern )
    String none = repeatString( "x", 0 )
    puts( "[" + none + "]" )
    return 0
```

This outputs:

```
*****
ababab
[]
```

When `count` is zero, the loop body never executes and the function returns the empty initial string.

### Left Padding

Pad a string to a target length by prepending a padding string:

```uranite
from uranite.io.console import puts

public function padLeft( String text, I64 targetLength, String padding ) -> String:
    String result = text
    I64 currentLength = result.length()
    while currentLength < targetLength:
        result = padding + result
        currentLength = result.length()
    return result

public function main() -> I32:
    String padded = padLeft( "42", 6, "0" )
    puts( padded )
    String already = padLeft( "hello", 3, " " )
    puts( already )
    return 0
```

This outputs:

```
000042
hello
```

When the string is already at or beyond the target length, the loop condition is immediately false and the original string is returned unchanged.

### Greeting Builder

Build a formatted greeting from components:

```uranite
from uranite.io.console import puts

public function buildGreeting( String name, I64 age, String city ) -> String:
    String greeting = "Hello, " + name + "!"
    String ageInfo = " Age: " + age.toString()
    String cityInfo = " From: " + city
    return greeting + ageInfo + cityInfo

public function main() -> I32:
    String message = buildGreeting( "Alice", 30, "Jakarta" )
    puts( message )
    String message2 = buildGreeting( "Bob", 25, "Tokyo" )
    puts( message2 )
    return 0
```

This outputs:

```
Hello, Alice! Age: 30 From: Jakarta
Hello, Bob! Age: 25 From: Tokyo
```

### Word Counter

Count words in a string by scanning for space-separated tokens:

```uranite
from uranite.io.console import puts

public function countWords( String text ) -> I64:
    String trimmed = text.trim()
    Boolean isEmpty = trimmed.isEmpty()
    if isEmpty:
        return 0
    I64 wordCount = 1
    I64 index = 0
    I64 length = trimmed.length()
    Boolean inSpace = False
    while index < length:
        I64 byteValue = trimmed.charAt( index )
        if byteValue == 32:
            if inSpace == False:
                wordCount = wordCount + 1
                inSpace = True
        else:
            inSpace = False
        index = index + 1
    return wordCount

public function main() -> I32:
    I64 count1 = countWords( "Hello World" )
    puts( "words: " + count1.toString() )
    I64 count2 = countWords( "  one  two  three  " )
    puts( "words: " + count2.toString() )
    I64 count3 = countWords( "" )
    puts( "words: " + count3.toString() )
    I64 count4 = countWords( "single" )
    puts( "words: " + count4.toString() )
    return 0
```

This outputs:

```
words: 2
words: 3
words: 0
words: 1
```

The function trims leading and trailing whitespace first, then scans through the string byte by byte. Each transition from non-space to space increments the word count. Consecutive spaces are handled by the `inSpace` flag — multiple adjacent spaces count as one word boundary.

### String Sanitizer

Clean up user input by trimming whitespace, converting to lowercase, and replacing unwanted characters:

```uranite
from uranite.io.console import puts

public function sanitize( String input ) -> String:
    String trimmed = input.trim()
    String lowered = trimmed.toLower()
    String noTabs = lowered.replace( "\t", " " )
    return noTabs

public function formatLabel( String label ) -> String:
    String clean = sanitize( label )
    Boolean empty = clean.isEmpty()
    if empty:
        return "(empty)"
    String upper = clean.toUpper()
    return "[" + upper + "]"

public function main() -> I32:
    String label1 = formatLabel( "  Hello World  " )
    puts( label1 )
    String label2 = formatLabel( "" )
    puts( label2 )
    String label3 = formatLabel( "\tTabbed Input\t" )
    puts( label3 )
    return 0
```

This outputs:

```
[HELLO WORLD]
(empty)
[TABBED INPUT]
```

Each transformation step stores its result in a dedicated variable. The `sanitize` function chains three transformations — trim, lowercase, tab replacement — each producing a new string. The `formatLabel` function then wraps the result in brackets after converting to uppercase, or returns a placeholder for empty input.
