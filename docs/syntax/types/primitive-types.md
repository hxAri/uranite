# Primitive Types

---

## Table of Contents

- [Primitive Types](#primitive-types)
  - [Table of Contents](#table-of-contents)
  - [Overview](#overview)
  - [Signed Integer Types](#signed-integer-types)
    - [I8](#i8)
    - [I16](#i16)
    - [I32](#i32)
    - [I64](#i64)
    - [Int](#int)
    - [Integer](#integer)
    - [Long](#long)
    - [Default Integer Inference](#default-integer-inference)
  - [Unsigned Integer Types](#unsigned-integer-types)
    - [U8](#u8)
    - [U16](#u16)
    - [U32](#u32)
    - [U64](#u64)
    - [UInt](#uint)
    - [Byte](#byte)
  - [Floating-Point Types](#floating-point-types)
    - [F32](#f32)
    - [F64](#f64)
    - [Float](#float)
    - [Double](#double)
    - [Default Float Inference](#default-float-inference)
    - [Special Float Values](#special-float-values)
  - [Boolean Type](#boolean-type)
    - [Boolean Values](#boolean-values)
    - [Boolean in Conditions](#boolean-in-conditions)
    - [Logical Operations](#logical-operations)
  - [Character Type](#character-type)
    - [Character Literals](#character-literals)
    - [Character Classification](#character-classification)
    - [Case Conversion](#case-conversion)
  - [String Type](#string-type)
    - [String Literals](#string-literals)
    - [String Methods](#string-methods)
    - [String Immutability](#string-immutability)
  - [Void and None](#void-and-none)
    - [Void](#void)
    - [None](#none)
    - [Void vs None](#void-vs-none)
  - [Everything is an Object](#everything-is-an-object)
    - [Method Calls on Literals](#method-calls-on-literals)
    - [Integer Methods](#integer-methods)
    - [Float Methods](#float-methods)
    - [Zero-Cost Object Model](#zero-cost-object-model)
  - [Type Conversions](#type-conversions)
    - [The as Keyword](#the-as-keyword)
    - [Numeric Widening](#numeric-widening)
    - [Numeric Narrowing](#numeric-narrowing)
    - [Conversion Methods](#conversion-methods)
  - [Type Compatibility](#type-compatibility)
    - [Assignment Rules](#assignment-rules)
    - [Arithmetic Mixing](#arithmetic-mixing)
    - [Comparison Compatibility](#comparison-compatibility)
  - [Practical Examples](#practical-examples)
    - [Working with Integers](#working-with-integers)
    - [Working with Floats](#working-with-floats)
    - [Working with Characters](#working-with-characters)
    - [Mixed-Type Program](#mixed-type-program)

---

## Overview

Uranite provides a rich set of primitive types that form the foundation of every program. Primitive types are value types: they are copied when assigned, passed by value to functions, and occupy fixed storage on the stack.

| Category | Types | Bit Widths |
|---|---|---|
| Signed integers | `I8`, `I16`, `I32`, `I64`, `Int`, `Integer`, `Long` | 8, 16, 32, 64 |
| Unsigned integers | `U8`, `U16`, `U32`, `U64`, `UInt`, `Byte` | 8, 16, 32, 64 |
| Floating-point | `F32`, `F64`, `Float`, `Double` | 32, 64 |
| Boolean | `Boolean` | 1 |
| Character | `Char` | 32 |
| String | `String` | variable |
| Void | `Void` | n/a |
| None | `None` | n/a |

Despite being primitives, every type in Uranite is an object. You can call methods directly on any primitive value, including literals. This "everything is an object" design carries zero runtime overhead — primitive method calls compile to native machine instructions with no heap allocation, no boxing, and no virtual dispatch.

---

## Signed Integer Types

Signed integers store whole numbers that can be positive, negative, or zero. Uranite provides four fixed-width signed integer types at 8, 16, 32, and 64 bits, plus three named types that map to specific widths.

### I8

An 8-bit signed integer. Stores values from **-128** to **127**.

```uranite
I8 temperature = -40
I8 exitCode = 0
I8 maxByte = 127
```

Use `I8` when memory is constrained and values fit within the 8-bit range, such as raw byte-level protocol fields or small counters.

### I16

A 16-bit signed integer. Stores values from **-32,768** to **32,767**.

```uranite
I16 portOffset = -1024
I16 elevation = 8848
I16 signalStrength = -72
```

Use `I16` for medium-range values like audio samples, sensor readings, or network protocol fields.

### I32

A 32-bit signed integer. Stores values from **-2,147,483,648** to **2,147,483,647**.

```uranite
I32 population = 1400000000
I32 fileDescriptor = 3
I32 errorCode = -1
```

`I32` is the standard choice for general-purpose integer work when 64-bit range is unnecessary. Functions returning exit codes or error codes typically use `I32`, including `main()`.

### I64

A 64-bit signed integer. Stores values from **-9,223,372,036,854,775,808** to **9,223,372,036,854,775,807**.

```uranite
I64 worldPopulation = 8000000000
I64 nanoseconds = 1625000000000000000
I64 fileSize = 4294967296
```

`I64` is the default integer type in Uranite. When you write an untyped integer literal, the compiler infers `I64`. This makes `I64` the workhorse type for most integer work — counters, indices, sizes, timestamps, and general arithmetic.

### Int

A 64-bit signed integer. `Int` is the natural name for the most commonly used integer type.

```uranite
Int count = 42
Int total = count + 10
```

`Int` and `I64` are the same type at every level — declaration, assignment, method calls, and storage. You can use them interchangeably in any context. Most Uranite code uses `Int` for readability unless a specific bit width is important.

### Integer

A 32-bit signed integer. `Integer` provides an explicit, readable name for 32-bit integer values.

```uranite
Integer statusCode = 200
Integer retryCount = 3
```

`Integer` and `I32` are the same type. Use whichever name reads better in context.

### Long

A 64-bit signed integer. `Long` provides an explicit name emphasizing the 64-bit width.

```uranite
Long bigNumber = 9223372036854775807
Long timestamp = 1625000000000
```

`Long` and `I64` are the same type. Some developers prefer `Long` when the 64-bit width is semantically important rather than incidental.

### Default Integer Inference

When you write a bare integer literal without a type annotation, Uranite infers `I64`:

```uranite
function example() -> Void:
    I64 explicit = 42
    Int alsoExplicit = 42
```

Both declarations produce the same result. The literal `42` is always `I64` unless assigned to a variable with a different integer type annotation, in which case the compiler checks that the value fits within the target type's range.

---

## Unsigned Integer Types

Unsigned integers store non-negative whole numbers only. They provide a larger positive range than their signed counterparts of the same bit width, at the cost of not representing negative values.

### U8

An 8-bit unsigned integer. Stores values from **0** to **255**.

```uranite
U8 redChannel = 255
U8 asciiCode = 65
U8 bitmask = 0xFF
```

`U8` is the natural type for raw byte data — pixel channels, ASCII values, binary protocol bytes, and byte-level I/O buffers.

### U16

A 16-bit unsigned integer. Stores values from **0** to **65,535**.

```uranite
U16 portNumber = 8080
U16 unicodePoint = 0x2764
U16 packetLength = 1500
```

### U32

A 32-bit unsigned integer. Stores values from **0** to **4,294,967,295**.

```uranite
U32 ipAddress = 0xC0A80001
U32 colorArgb = 0xFF00FF00
U32 filePermissions = 0o755
```

### U64

A 64-bit unsigned integer. Stores values from **0** to **18,446,744,073,709,551,615**.

```uranite
U64 hashValue = 0xDEADBEEFCAFEBABE
U64 memoryAddress = 0x7FFE00000000
U64 uniqueIdentifier = 18446744073709551615
```

### UInt

A 64-bit unsigned integer. `UInt` provides a readable name for the default unsigned integer type.

```uranite
UInt counter = 0
UInt bufferSize = 4096
```

`UInt` and `U64` are the same type. Use `UInt` for general-purpose unsigned values where the specific bit width is not the focus.

### Byte

An 8-bit unsigned integer. `Byte` provides a semantically clear name for byte-level data.

```uranite
Byte rawByte = 0xAB
Byte nullTerminator = 0
```

`Byte` and `U8` are the same type. `Byte` is preferred when working with raw binary data, byte buffers, or I/O operations, because the name communicates intent more clearly than `U8`.

---

## Floating-Point Types

Floating-point types store real numbers with fractional parts. Uranite provides two IEEE 754 floating-point types at 32-bit and 64-bit precision, plus two named types.

### F32

A 32-bit single-precision floating-point number. Provides approximately **7 decimal digits** of precision.

```uranite
F32 temperature = 98.6
F32 latitude = 37.7749
F32 probability = 0.95
```

Use `F32` when memory is constrained and 7-digit precision is sufficient, such as graphics coordinates, audio samples, or large arrays of measurements. `F32` requires an explicit type annotation — float literals default to `F64`.

### F64

A 64-bit double-precision floating-point number. Provides approximately **15 decimal digits** of precision.

```uranite
F64 pi = 3.141592653589793
F64 avogadro = 6.02214076e23
F64 planck = 6.62607015e-34
```

`F64` is the default floating-point type. When you write a bare float literal, the compiler infers `F64`. Use `F64` for scientific computing, financial calculations (with care), and any context where precision matters.

### Float

A 64-bit double-precision floating-point number. `Float` provides the natural, readable name for the default floating-point type.

```uranite
Float velocity = 299792458.0
Float gravity = 9.80665
```

`Float` and `F64` are the same type. Most Uranite code uses `Float` for readability unless a specific precision level is important.

### Double

A 64-bit double-precision floating-point number. `Double` provides an alternative name emphasizing double-precision.

```uranite
Double preciseResult = 1.7976931348623157e308
Double tinyValue = 2.2250738585072014e-308
```

`Double` and `F64` are the same type.

### Default Float Inference

When you write a bare float literal without a type annotation, Uranite infers `F64`:

```uranite
function example() -> Void:
    F64 explicit = 3.14
    Float alsoExplicit = 3.14
```

Both produce the same result. To store a value as `F32`, you must provide an explicit type annotation:

```uranite
F32 singlePrecision = 3.14
```

### Special Float Values

Floating-point arithmetic can produce special values defined by the IEEE 754 standard. Uranite handles these transparently:

**Not-a-Number (NaN)** results from undefined operations like dividing zero by zero. NaN is never equal to anything, including itself:

```uranite
F64 undefined = 0.0 / 0.0
Boolean isUndefined = undefined.isNaN()
```

**Infinity** results from dividing a non-zero number by zero or from overflow:

```uranite
F64 positiveInf = 1.0 / 0.0
F64 negativeInf = -1.0 / 0.0
Boolean isInf = positiveInf.isInfinite()
```

The `isFinite()` method returns `True` for normal numbers and `False` for NaN or infinity:

```uranite
F64 normal = 42.0
Boolean normalIsFinite = normal.isFinite()
```

---

## Boolean Type

### Boolean Values

`Boolean` is the truth value type. It has exactly two values: `True` and `False`. Note the capital letters — `true` and `false` are not valid in Uranite.

```uranite
Boolean isActive = True
Boolean isDeleted = False
```

`Boolean` is a `final` class, meaning it cannot be extended or subclassed.

### Boolean in Conditions

`Boolean` values drive all conditional logic — `if`, `elif`, `while`, and `match` guards:

```uranite
Boolean hasPermission = True
if hasPermission:
    processRequest()

Boolean keepRunning = True
while keepRunning:
    keepRunning = pollForShutdown()
```

Only `Boolean` values can appear as conditions. Uranite does not perform implicit truthiness conversion — integers, strings, and other types cannot be used directly as conditions. You must write explicit comparisons:

```uranite
I64 count = 0
if count > 0:
    processItems( count )

String name = ""
Boolean nameEmpty = name.isEmpty()
if nameEmpty == False:
    greet( name )
```

### Logical Operations

Uranite uses the keyword operators `and`, `or`, and `not` for Boolean logic. The symbolic operators `&&`, `||`, and `!` do not exist in Uranite.

```uranite
Boolean canAccess = isAuthenticated and hasPermission
Boolean shouldRetry = isTimeout or isTransient
Boolean isInvalid = not isValid
```

**Important:** `and` and `or` evaluate both sides unconditionally. Unlike some languages where logical operators short-circuit (skip the right side if the left side determines the result), Uranite always evaluates both operands. This matters when the right-hand side has side effects:

```uranite
Boolean result = checkFirst() and checkSecond()
```

In this example, `checkSecond()` is always called, even if `checkFirst()` returns `False`.

The `Boolean` class also provides method-based logical operations:

```uranite
Boolean active = True
Boolean inactive = active.negate()
Boolean conjunction = active.logicalAnd( inactive )
Boolean disjunction = active.logicalOr( inactive )
```

---

## Character Type

### Character Literals

`Char` represents a single Unicode character, stored as a 32-bit value. Character literals use single quotes:

```uranite
Char letter = 'A'
Char digit = '7'
Char newline = '\n'
Char tab = '\t'
Char backslash = '\\'
Char singleQuote = '\''
```

`Char` covers the full Unicode scalar value range from U+0000 to U+10FFFF, so it can represent characters from any writing system.

`Char` is a `final` class and cannot be subclassed.

### Character Classification

The `Char` class provides methods for testing what category a character belongs to:

```uranite
Char letter = 'A'
Boolean isLetter = letter.isAlpha()
Boolean isNumber = letter.isDigit()
Boolean isBoth = letter.isAlphanumeric()
Boolean isSpace = letter.isWhitespace()
```

`isAlpha()` returns `True` for letters `a`-`z` and `A`-`Z`. `isDigit()` returns `True` for digits `0`-`9`. `isAlphanumeric()` returns `True` if either `isAlpha()` or `isDigit()` would return `True`. `isWhitespace()` returns `True` for space, tab (`\t`), newline (`\n`), and carriage return (`\r`).

### Case Conversion

```uranite
Char lower = 'a'
Char upper = lower.toUpper()

Char upperZ = 'Z'
Char lowerZ = upperZ.toLower()
```

`toUpper()` converts lowercase ASCII letters to uppercase. `toLower()` converts uppercase ASCII letters to lowercase. Non-letter characters are returned unchanged by both methods.

---

## String Type

### String Literals

`String` represents an immutable sequence of characters. String literals use double quotes:

```uranite
String greeting = "hello, world"
String empty = ""
String withEscapes = "line one\nline two"
String withQuote = "she said \"hello\""
```

Multi-line strings use triple double quotes:

```uranite
String multiLine = """
    This is a multi-line string.
    Indentation is preserved relative to the closing quotes.
    """
```

### String Methods

The `String` class provides a comprehensive set of methods for querying and transforming string data:

**Length and emptiness:**

```uranite
String greeting = "hello"
I64 size = greeting.length()
Boolean empty = "".isEmpty()
```

**Searching:**

```uranite
String text = "hello world"
Boolean found = text.contains( "world" )
I64 position = text.indexOf( "ll" )
Boolean starts = text.startsWith( "hel" )
Boolean ends = text.endsWith( "llo" )
```

**Extracting:**

```uranite
String text = "hello"
Char fifth = text.charAt( 4 )
I64 code = text.charCodeAt( 0 )
String sub = "hello world".substring( 0, 5 )
```

**Transforming:**

```uranite
String upper = "hello".toUpper()
String lower = "HELLO".toLower()
String trimmed = "  hello  ".trim()
String replaced = "hello world".replace( "world", "uranite" )
```

**Concatenation:**

```uranite
String full = "hello" + " " + "world"
String combined = "hello".concat( " world" )
```

The `+` operator and `concat()` method both produce a new string by joining two strings together.

**Formatting:**

```uranite
I64 count = 42
String message = "found {} items".format( count )
```

The `format()` method uses bare `{}` placeholders. Each `{}` is replaced by the string representation of the corresponding argument, in order.

**Equality:**

```uranite
String strA = "hello"
String strB = "hello"
Boolean same = strA.equals( strB )
```

### String Immutability

Strings in Uranite are immutable. Every method that appears to modify a string actually returns a new string, leaving the original unchanged:

```uranite
String original = "hello"
String modified = original.toUpper()
```

After this code, `original` still holds `"hello"` and `modified` holds `"HELLO"`.

---

## Void and None

### Void

`Void` is the return type annotation for functions that produce no value. It is not a value you can store in a variable — it exists purely as a type-level marker:

```uranite
public function greet( String name ) -> Void:
    puts( "hello, " + name )
```

You cannot assign `Void` to a variable or pass it as an argument. A function declared with `-> Void` simply returns without producing a result.

### None

`None` is a literal value representing "no value present." It is used with optional types (`?T`) to indicate the absence of a value:

```uranite
?String maybeName = None
?I64 maybeCount = None
```

You can check whether an optional value is `None` using the `is` keyword:

```uranite
?String result = findUser( "alice" )
if result is None:
    puts( "user not found" )
```

### Void vs None

`Void` and `None` serve fundamentally different purposes:

- **`Void`** is a type. It appears only in function return type annotations (`-> Void`) and means "this function returns nothing."
- **`None`** is a value. It appears in expressions and means "no value is present." It can be assigned to any optional type (`?T`).

```uranite
public function doWork() -> Void:
    ?I64 result = computeIfPossible()
    if result is None:
        return
    processResult( result )
```

In this example, `Void` marks the function's return type, while `None` is a runtime value compared against the optional variable.

---

## Everything is an Object

Every primitive type in Uranite is an object with methods. There is no distinction between "primitive operations" and "method calls" — both compile to the same native machine instructions.

### Method Calls on Literals

You can call methods directly on literal values without storing them in a variable first:

```uranite
String text = 42.toString()
Boolean zero = 0.isZero()
Boolean alpha = 'A'.isAlpha()
Boolean empty = "".isEmpty()
```

This works because every literal is an instance of its type's wrapper class. The integer `42` is an `I64` object, the character `'A'` is a `Char` object, and so on.

Negative literals require parentheses for method calls, because the minus sign binds loosely:

```uranite
I64 absolute = (-15).abs()
```

Float literals also support direct method calls:

```uranite
F64 rounded = 3.14159.round()
```

### Integer Methods

All signed integer types (`I8`, `I16`, `I32`, `I64`, `Int`, `Integer`, `Long`) share these methods, inherited from the `Int` base class:

| Method | Return Type | Description |
|---|---|---|
| `getValue()` | `I64` | Returns the underlying numeric value |
| `toString()` | `String` | Returns the string representation |
| `abs()` | `Int` | Returns the absolute value |
| `negate()` | `Int` | Returns the value with sign inverted |
| `add( Int other )` | `Int` | Returns the sum |
| `subtract( Int other )` | `Int` | Returns the difference |
| `multiply( Int other )` | `Int` | Returns the product |
| `divide( Int other )` | `Int` | Returns the integer quotient |
| `modulo( Int other )` | `Int` | Returns the remainder |
| `equals( Int other )` | `Boolean` | Returns `True` if values are equal |
| `compareTo( Int other )` | `I32` | Returns -1, 0, or 1 |
| `min( Int other )` | `Int` | Returns the smaller value |
| `max( Int other )` | `Int` | Returns the larger value |
| `isZero()` | `Boolean` | Returns `True` if value is zero |
| `isPositive()` | `Boolean` | Returns `True` if value is positive |
| `isNegative()` | `Boolean` | Returns `True` if value is negative |
| `bitwiseAnd( Int other )` | `Int` | Bitwise AND |
| `bitwiseOr( Int other )` | `Int` | Bitwise OR |
| `bitwiseXor( Int other )` | `Int` | Bitwise XOR |
| `shiftLeft( Int amount )` | `Int` | Left bit shift |
| `shiftRight( Int amount )` | `Int` | Right bit shift |
| `toFloat()` | `F64` | Converts to floating-point |
| `toDouble()` | `F64` | Converts to `F64` |

Example using integer methods:

```uranite
I64 value = -42
I64 absolute = value.abs()
puts( absolute.toString() )
Boolean isNeg = value.isNegative()
puts( isNeg.toString() )
String asText = value.toString()
puts( asText )
I32 comparison = value.compareTo( 0 )
puts( comparison.toString() )
```

Bitwise operations example:

```uranite
I64 value = 1
I64 shifted = value.shiftLeft( 4 )
puts( shifted.toString() )
I64 masked = shifted.bitwiseAnd( 0x0F )
puts( masked.toString() )
I64 ored = value.bitwiseOr( 14 )
puts( ored.toString() )
```

### Float Methods

All floating-point types (`F32`, `F64`, `Float`, `Double`) share these methods, inherited from the `Float` base class:

| Method | Return Type | Description |
|---|---|---|
| `getValue()` | `F64` | Returns the underlying numeric value |
| `toString()` | `String` | Returns the string representation |
| `abs()` | `Float` | Returns the absolute value |
| `negate()` | `Float` | Returns the value with sign inverted |
| `add( Float other )` | `Float` | Returns the sum |
| `subtract( Float other )` | `Float` | Returns the difference |
| `multiply( Float other )` | `Float` | Returns the product |
| `divide( Float other )` | `Float` | Returns the quotient |
| `equals( Float other )` | `Boolean` | Returns `True` if values are equal |
| `compareTo( Float other )` | `I32` | Returns -1, 0, or 1 |
| `isNaN()` | `Boolean` | Returns `True` if value is Not-a-Number |
| `isInfinite()` | `Boolean` | Returns `True` if value is infinity |
| `isFinite()` | `Boolean` | Returns `True` if value is a normal number |
| `floor()` | `Float` | Rounds down to nearest integer value |
| `ceil()` | `Float` | Rounds up to nearest integer value |
| `round()` | `Float` | Rounds to nearest integer value |
| `sqrt()` | `Float` | Returns the square root |
| `power( Float exponent )` | `Float` | Returns self raised to a power |
| `toInt()` | `I64` | Converts to integer by truncation |
| `toFloat()` | `F64` | Returns the value as `F64` |
| `toDouble()` | `F64` | Returns the value as `F64` |

Example using float methods:

```uranite
F64 value = 3.14159
F64 rounded = value.round()
puts( rounded.toString() )
F64 floored = value.floor()
puts( floored.toString() )
I64 truncated = value.toInt()
puts( truncated.toString() )
F64 squareRoot = 144.0.sqrt()
puts( squareRoot.toString() )
```

Checking for special values:

```uranite
F64 nan = 0.0 / 0.0
Boolean isNan = nan.isNaN()
puts( isNan.toString() )
F64 inf = 1.0 / 0.0
Boolean isInf = inf.isInfinite()
puts( isInf.toString() )
F64 normal = 42.0
Boolean isFin = normal.isFinite()
puts( isFin.toString() )
```

### Zero-Cost Object Model

When you call a method on a primitive, Uranite does not create an object on the heap. There is no boxing, no wrapper allocation, and no virtual dispatch. The compiler recognizes primitive wrapper types and emits native machine instructions directly.

For example, `42.toString()` compiles to a direct integer-to-string formatting instruction — the same code a low-level systems language would produce. The object model exists at the language level for expressiveness and consistency, but it is completely erased at compile time.

This means you never need to choose between "efficient primitives" and "convenient objects." In Uranite, they are the same thing.

---

## Type Conversions

### The as Keyword

Uranite uses the `as` keyword for explicit type conversions between primitive types:

```uranite
I64 intValue = 42
F64 floatValue = intValue as F64
puts( floatValue.toString() )

F64 pi = 3.14
I64 truncated = pi as I64
puts( truncated.toString() )

Char letter = 'A'
I32 codePoint = letter as I32
puts( codePoint.toString() )

I32 code = 66
Char fromCode = code as Char
puts( fromCode.toString() )
```

The `as` keyword performs the conversion at compile time, emitting the appropriate machine instruction for the conversion (integer extend, float truncate, etc.).

### Numeric Widening

Widening conversions move a value to a larger type that can represent all values of the original type without loss:

```uranite
I8 small = 42
I16 medium = small as I16
I32 standard = medium as I32
I64 large = standard as I64

F32 singlePrecision = 3.14
F64 doublePrecision = singlePrecision as F64
```

Integer-to-float widening is also supported:

```uranite
I64 intValue = 42
F64 floatValue = intValue as F64
```

Widening conversions are always safe — no data is lost.

### Numeric Narrowing

Narrowing conversions move a value to a smaller type. These can lose data if the value exceeds the target type's range:

```uranite
I64 large = 300
I8 small = large as I8
```

In this example, the value 300 exceeds the `I8` range of -128 to 127. The result is truncated to the lower 8 bits, which produces a different value. Uranite performs the conversion without error — it is the programmer's responsibility to ensure the value fits.

Float-to-integer narrowing truncates the fractional part:

```uranite
F64 pi = 3.99999
I64 truncated = pi as I64
```

The result is `3`, not `4` — the fractional part is discarded, not rounded.

### Conversion Methods

Primitive objects also provide named conversion methods:

```uranite
I64 intValue = 42
F64 asFloat = intValue.toFloat()
F64 asDouble = intValue.toDouble()

F64 floatValue = 3.14
I64 asInt = floatValue.toInt()
```

These methods behave identically to the `as` keyword conversions.

---

## Type Compatibility

### Assignment Rules

Uranite enforces strict type safety for assignments. You cannot assign a value of one type to a variable of a different type without an explicit conversion:

```uranite
I64 count = 42
I32 smaller = count as I32

F64 ratio = 0.5
I64 truncated = ratio as I64
```

Without the `as` keyword, these assignments would produce a compilation error. This strictness prevents accidental data loss from implicit narrowing conversions.

There is one exception: assigning `None` to an optional type is always valid:

```uranite
?String name = None
?I64 count = None
```

### Arithmetic Mixing

Arithmetic operators (`+`, `-`, `*`, `/`, `%`) require both operands to be the same type. You cannot mix integer and float types in a single expression without explicit conversion:

```uranite
I64 count = 10
F64 rate = 1.5
F64 numerator = count as F64
F64 result = numerator * rate
```

Similarly, you cannot mix different integer widths directly:

```uranite
I32 small = 10
I64 large = 20
I64 widened = small as I64
I64 sum = widened + large
```

This design eliminates an entire class of subtle bugs caused by implicit promotion rules in other languages.

### Comparison Compatibility

Comparison operators (`==`, `!=`, `<`, `>`, `<=`, `>=`) follow the same rules as arithmetic — both operands must be the same type:

```uranite
I64 count = 42
I64 limit = 100
Boolean withinLimit = count < limit

F64 temperature = 98.6
F64 threshold = 100.0
Boolean isFever = temperature >= threshold
```

The `is` keyword is a separate operator used for identity checks, particularly for `None`:

```uranite
?String value = getOptionalValue()
if value is None:
    handleMissing()
```

---

## Practical Examples

### Working with Integers

```uranite
from uranite.io.console import puts

public function factorial( I64 number ) -> I64:
    if number <= 1:
        return 1
    return number * factorial( number - 1 )

public function fibonacci( I64 count ) -> I64:
    if count <= 1:
        return count
    I64 previous = 0
    I64 current = 1
    I64 index = 2
    while index <= count:
        I64 next = previous + current
        previous = current
        current = next
        index = index + 1
    return current

public function main() -> I32:
    I64 fact10 = factorial( 10 )
    puts( "10! = " + fact10.toString() )

    I64 fib20 = fibonacci( 20 )
    puts( "fib(20) = " + fib20.toString() )

    I64 value = -42
    I64 absolute = value.abs()
    puts( "abs(-42) = " + absolute.toString() )
    Boolean isNeg = value.isNegative()
    puts( "isNegative = " + isNeg.toString() )

    return 0
```

### Working with Floats

```uranite
from uranite.io.console import puts

public function celsiusToFahrenheit( F64 celsius ) -> F64:
    return celsius * 1.8 + 32.0

public function main() -> I32:
    F64 boiling = celsiusToFahrenheit( 100.0 )
    puts( boiling.toString() + "F" )

    F64 freezing = celsiusToFahrenheit( 0.0 )
    puts( freezing.toString() + "F" )

    F64 pi = 3.14159
    F64 floored = pi.floor()
    puts( "floor(pi) = " + floored.toString() )
    F64 ceiled = pi.ceil()
    puts( "ceil(pi) = " + ceiled.toString() )
    F64 rounded = pi.round()
    puts( "round(pi) = " + rounded.toString() )

    F64 nan = 0.0 / 0.0
    Boolean isNan = nan.isNaN()
    puts( "isNaN = " + isNan.toString() )
    Boolean isFin = nan.isFinite()
    puts( "isFinite = " + isFin.toString() )

    return 0
```

### Working with Characters

```uranite
from uranite.io.console import puts

public function classifyCharacter( Char character ) -> String:
    if character.isAlpha():
        Char upper = character.toUpper()
        Char original = character.getValue()
        if upper.getValue() == original.getValue():
            return "uppercase letter"
        return "lowercase letter"
    if character.isDigit():
        return "digit"
    if character.isWhitespace():
        return "whitespace"
    return "symbol"

public function main() -> I32:
    Char testChar = 'Z'
    String classification = classifyCharacter( testChar )
    puts( testChar.toString() + " is a " + classification )

    Char lower = 'a'
    Char upper = lower.toUpper()
    puts( lower.toString() + " -> " + upper.toString() )

    return 0
```

### Mixed-Type Program

```uranite
from uranite.io.console import puts

public function formatPercentage( I64 numerator, I64 denominator ) -> String:
    F64 numeratorFloat = numerator as F64
    F64 denominatorFloat = denominator as F64
    F64 ratio = numeratorFloat / denominatorFloat
    F64 percentage = ratio * 100.0
    F64 rounded = percentage.round()
    String text = rounded.toString()
    return text + "%"

public function formatTemperature( F64 celsius ) -> String:
    F64 fahrenheit = celsius * 1.8 + 32.0
    F64 rounded = fahrenheit.round()
    String celsiusText = celsius.toString()
    String fahrenheitText = rounded.toString()
    return celsiusText + "C = " + fahrenheitText + "F"

public function countDigits( String text ) -> I64:
    I64 digitCount = 0
    I64 index = 0
    while index < text.length():
        Char character = text.charAt( index )
        if character.isDigit():
            digitCount = digitCount + 1
        index = index + 1
    return digitCount

public function main() -> I32:
    puts( formatPercentage( 42, 100 ) )
    puts( formatPercentage( 1, 3 ) )

    puts( formatTemperature( 0.0 ) )
    puts( formatTemperature( 100.0 ) )
    puts( formatTemperature( 37.0 ) )

    String sample = "abc123def456"
    I64 digits = countDigits( sample )
    puts( "digits in '" + sample + "': " + digits.toString() )

    return 0
```

This program demonstrates explicit type conversions with `as`, method calls on primitives (`toString()`, `round()`, `isDigit()`, `charAt()`), string operations (`length()`, concatenation), and mixing integer and float types safely through explicit conversion. Every function stores method call results in intermediate variables before further use — this is the recommended pattern in Uranite.
