# String Formatting

Uranite provides string formatting through the `format` method on String values. There is no interpolation syntax (no f-strings, no template literals) — all formatting is done via explicit method calls. The `format` method uses Python-style curly-brace placeholders (`{0}`, `{name}`, `{}`) with format specifiers for width, precision, alignment, fill character, and type conversions. At compilation level, `format` is a builtin descriptor method on the `String` class — the compiler intercepts `String.format(args)` calls and expands them inline into optimized LLVM IR rather than dispatching to a runtime function. Two codegen paths exist: a fast path using `snprintf` for simple placeholders, and a slow path using piece-by-piece `memcpy` assembly for complex cases (binary format, center alignment, named placeholders).

---

## Table of Contents

- [Syntax](#syntax)
- [Placeholder Syntax](#placeholder-syntax)
  - [Positional Placeholders](#positional-placeholders)
  - [Named Placeholders](#named-placeholders)
  - [Format Specifiers](#format-specifiers)
- [toString Method](#tostring-method)
  - [OOP Wrapper toString](#oop-wrapper-tostring)
  - [Object toString Stub](#object-tostring-stub)
  - [coerceToString Helper](#coercetostring-helper)
- [Builtin Descriptor Implementation](#builtin-descriptor-implementation)
  - [FormatPlaceholder Struct](#formatplaceholder-struct)
  - [parseFormatString](#parseformatstring)
  - [Fast Path — snprintf](#fast-path--snprintf)
  - [Slow Path — Piece Assembly](#slow-path--piece-assembly)
  - [Binary Format Specifier](#binary-format-specifier)
  - [Width and Alignment Padding](#width-and-alignment-padding)
- [Memory Model](#memory-model)
- [Qualname Constants](#qualname-constants)
- [Examples](#examples)

---

## Syntax

String formatting is a method call on a format string:

```
String result = "Hello, {}!".format(name)
String coords = "({}, {})".format(x, y)
String report = "{0} has {1} items".format(user, count)
String padded = "{:>10}".format(value)
```

The format string is the `self` (receiver) value. Arguments are passed as positional or keyword parameters to `format`.

No string interpolation exists — these are **not** valid:

```
# NOT valid Uranite syntax:
# f"Hello, {name}!"
# "Hello, ${name}!"
# `Hello, ${name}!`
```

---

## Placeholder Syntax

### Positional Placeholders

| Syntax | Meaning |
|---|---|
| `{}` | Auto-indexed: first `{}` is arg 0, second is arg 1, etc. |
| `{0}` | Explicit index: arg 0 |
| `{1}` | Explicit index: arg 1 |
| `{{` | Escaped literal `{` |
| `}}` | Escaped literal `}` |

Auto-indexing and explicit indexing can be mixed — each `{}` without an index takes the next auto-index value.

### Named Placeholders

```
String result = "Name: {name}, Age: {age}".format(name="Alice", age=30)
```

Named placeholders reference keyword arguments. If a named placeholder does not match any keyword argument, the first positional argument is used as fallback.

### Format Specifiers

Format specifiers follow a colon inside the placeholder: `{index:spec}` or `{:spec}`.

Full specifier syntax: `{[index]:[fill][align][width][.precision][type]}`

| Component | Values | Default | Description |
|---|---|---|---|
| `fill` | Any character | Space (`' '`) | Padding character |
| `align` | `<`, `>`, `^` | `>` (right) | Left, right, or center alignment |
| `width` | Integer | None | Minimum field width |
| `.precision` | `.` + integer | None | Decimal places for floats |
| `type` | `d`, `f`, `e`, `x`, `X`, `o`, `b`, `s`, `B`, `O` | Auto-detected | Conversion type |

**Type specifiers**:

| Type | Description | snprintf Format |
|---|---|---|
| `d` | Decimal integer | `%ld` |
| `f` | Fixed-point float | `%f` or `%.Nf` |
| `e` | Scientific notation | `%e` |
| `x` | Lowercase hexadecimal | `%lx` |
| `X` | Uppercase hexadecimal | `%lX` |
| `o` | Octal | `%lo` |
| `b` | Binary | Custom loop (no printf equivalent) |
| `s` | String | `%s` |
| `B` | Boolean (auto-detected for `i1`) | `%s` ("True"/"False") |
| `O` | Object (auto-detected for struct pointers) | `%s` (via `coerceToString`) |

When no type specifier is given, the type is auto-detected from the LLVM type of the argument:

```cpp
if( argType == llvm::PointerType::getUnqual( arg->getContext() ) ) {
    typeChar = 's';
}
else if( argType->isPointerTy() ) {
    typeChar = 'O';
}
else if( argType->isDoubleTy() || argType->isFloatTy() ) {
    typeChar = 'f';
}
else if( argType->isIntegerTy( 1 ) ) {
    typeChar = 'B';
}
else {
    typeChar = 'd';
}
```

---

## toString Method

Every type in Uranite has a `toString` method, either from the `Stringable` interface, an OOP wrapper builtin, or the base `Object.toString` stub. This is the primary way to convert non-string values to strings outside of `format`.

### OOP Wrapper toString

For numeric OOP wrapper types (`I64`, `F64`, `Float`, `Int`, etc.), `toString` is handled inline at codegen level. At `src/uranite/ir/mir/codegen.cpp:3949-3976`:

```cpp
if( methodName ==
    semantic::qualname::classes::object::methods::ToString ) {
    llvm::Value* selfValue = loadSelf();
    if( selfValue != nullptr ) {
        llvm::Type* i64Type =
            llvm::Type::getInt64Ty( this->llvmContext );
        llvm::Function* snprintfFunction =
            this->getOrCreateSnprintf();
        llvm::Function* mallocFunction =
            this->getOrCreateMalloc();
        llvm::Value* bufSize =
            llvm::ConstantInt::get( i64Type, 48 );
        llvm::Value* bufPtr = this->irBuilder.CreateCall(
            mallocFunction, { bufSize }, "wrap.str.buf" );
        if( isFloatWrapper ) {
            llvm::Value* fmtStr =
                this->irBuilder.CreateGlobalStringPtr(
                    "%g", "wrap.str.fmt" );
            // FPExt if float32 -> double
            this->irBuilder.CreateCall( snprintfFunction,
                { bufPtr, bufSize, fmtStr, selfValue } );
        }
        else {
            // SExt or ZExt to i64 based on signedness
            llvm::Value* fmtStr =
                this->irBuilder.CreateGlobalStringPtr(
                    "%ld", "wrap.str.fmt" );
            this->irBuilder.CreateCall( snprintfFunction,
                { bufPtr, bufSize, fmtStr, printVal } );
        }
        setResult( bufPtr );
    }
    return;
}
```

| Wrapper Category | Format | Buffer Size | Extension |
|---|---|---|---|
| Float wrappers (`Float`, `Double`, `F32`, `F64`) | `"%g"` | 48 bytes | `FPExt` if `float` to `double` |
| Signed integer wrappers (`Int`, `I64`, `I32`, etc.) | `"%ld"` | 48 bytes | `SExt` to i64 |
| Unsigned integer wrappers (`U8`, `U16`, `U32`, `U64`, `Byte`) | `"%ld"` | 48 bytes | `ZExt` to i64 |

### Object toString Stub

For the base `Object` type, a stub `toString` is generated at codegen initialization (codegen.cpp:360-376). The stub is an identity function — it returns the self pointer unchanged, assuming the object pointer is already a string pointer:

```cpp
std::string objectToStringQualified =
    semantic::qualname::classes::object::Qualified + "." +
    semantic::qualname::classes::object::methods::ToString;
// Returns self pointer as-is
stubBuilder.CreateRet( toStringFunction->getArg( 0 ) );
```

For bare `toString` calls on pointer values at codegen.cpp:3048-3053, the codegen short-circuits: if the self value is a pointer, it returns the pointer directly (assumes it is already a string).

### coerceToString Helper

The `coerceToString` function in `src/uranite/descriptor/descriptor.cpp:321-359` provides comprehensive value-to-string conversion for format/print contexts:

```cpp
static llvm::Value* coerceToString(
    llvm::IRBuilder<>& builder,
    llvm::LLVMContext& context,
    llvm::Value* arg ) {
    // Pointer (already string) -> pass through
    // Boolean i1 -> Select("True", "False")
    // Struct pointer -> try _UR_ClassName_toString()
    //   fallback: "<ClassName object at 0xADDRESS>"
    // Other -> pass through
}
```

| Input Type | Conversion |
|---|---|
| Opaque pointer (`ptr`) | Pass through (assumed string) |
| Boolean (`i1`) | `CreateSelect` between "True" and "False" global strings |
| Struct pointer | Try calling `_UR_ClassName_toString` if it exists. Fallback: format as `"<ClassName object at 0x%lx>"` using `_UR_meta_ClassName` for display name |
| Other | Pass through |

The struct pointer fallback extracts the class name from either the `_UR_meta_ClassName` global variable or the LLVM struct type name, then formats the object representation with `snprintf`.

---

## Builtin Descriptor Implementation

`String.format` is registered as a builtin descriptor method at `src/uranite/descriptor/descriptor.cpp:1945-2279`. It is not a real Uranite function — the compiler intercepts calls and generates LLVM IR inline.

### FormatPlaceholder Struct

At `descriptor.cpp:43-52`:

```cpp
struct FormatPlaceholder {
    size_t startPos;
    size_t endPos;
    int argIndex;
    std::string name;
    char type;
    int width;
    int precision;
    char fill;
    char align;
};
```

### parseFormatString

At `descriptor.cpp:80-178`, the `parseFormatString` function parses Python-style format strings. It scans for `{...}` delimiters, handles `{{`/`}}` escaping, and extracts:

- Auto-indexed vs explicit-indexed vs named placeholders
- Format specifier components (fill, align, width, precision, type)

The parser uses a linear scan with an auto-index counter that increments for each `{}` without an explicit index.

### Fast Path — snprintf

When the format string has **no** binary specifiers (`b`), **no** center alignment (`^`), and **no** named placeholders, the codegen takes the fast path (descriptor.cpp:1994-2063):

1. **Build snprintf format string**: `buildSnprintfFormat` converts `"{} has {} items"` to `"%s has %ld items"` by replacing each placeholder with the appropriate printf specifier based on argument LLVM types.
2. **Estimate buffer size**: `estimateBufferSize` calculates base buffer = format string length + 32 bytes per numeric placeholder + 256 bytes per string placeholder + width if specified.
3. **Dynamic sizing for string args**: If any argument is a string pointer, compute actual size at runtime by summing `strlen` of each string argument + format overhead.
4. **Single snprintf call**: `malloc(bufSize)`, build format string global, call `snprintf(buffer, size, fmt, args...)`.

Type coercion before snprintf:
- Struct pointers: `coerceToString()` (try `_UR_ClassName_toString`, fallback repr)
- Boolean `i1`: `Select("True", "False")`
- Float `f32`: `FPExt` to `double`
- Narrow integers: `SExt` to `i64`

### Slow Path — Piece Assembly

When the format string contains binary format (`b`), center alignment (`^`), or named placeholders, the codegen falls through to the slow path (descriptor.cpp:2065-2277). This builds the result string by assembling pieces:

1. **Iterate format string and placeholders**: Walk through the format string, alternating between literal segments and placeholder expansions.
2. **Per-placeholder conversion**: Each placeholder argument is converted to a string piece with its computed length:
   - Pointer (string): pass through, `strlen` for length
   - Struct pointer: `coerceToString()` conversion
   - Boolean: Select "True"/"False"
   - Numeric: `snprintf` into 48-byte buffer with appropriate format
3. **Literal segments**: Extracted with `{{`/`}}` unescaping, stored as global string pointers.
4. **Final assembly**: Sum all piece lengths, `malloc(total + 1)`, `memcpy` each piece at its offset, null-terminate.

### Binary Format Specifier

The `b` type specifier (descriptor.cpp:2097-2143) generates a loop that converts an integer to its binary string representation:

1. Sign-extend value to i64.
2. Allocate 65-byte buffer (64 binary digits + null).
3. Special case: if value is 0, write "0" directly.
4. Loop: extract low bit (`AND 1`), convert to ASCII ('0'/'1'), write to buffer from right to left (index 63 down to 0), right-shift value.
5. Compute start position from final index, calculate length.
6. Select between zero-case buffer and loop-result buffer via PHI.

This is the only format specifier that cannot be expressed as a printf format, requiring custom LLVM IR generation.

### Width and Alignment Padding

When a width specifier is present (descriptor.cpp:2192-2230):

1. Compare piece length against target width.
2. If padding needed: `malloc(width + 1)`, `memset` with fill character.
3. Calculate destination offset based on alignment:
   - `>` (right-align, default): piece placed at `padAmount` offset
   - `<` (left-align): piece placed at offset 0
   - `^` (center-align): piece placed at `padAmount / 2` offset
4. `memcpy` piece into padded buffer, null-terminate.
5. PHI merge between padded and unpadded paths.

---

## Memory Model

Every `format` call allocates a new heap buffer via `malloc`. Buffer sizing depends on the codegen path:

**Fast path**: Single allocation. Size = estimated from format string length + per-placeholder overhead (32 bytes numeric, 256 bytes string), or dynamically computed with `strlen` for string arguments.

**Slow path**: Multiple allocations — one per numeric argument conversion (48 bytes each), one per binary conversion (65 bytes), one per padding operation (`width + 1` bytes), plus the final result buffer (`totalLength + 1`). Each intermediate piece allocation becomes garbage after the final assembly.

`toString` on OOP wrappers allocates a 48-byte buffer per call.

---

## Qualname Constants

```cpp
namespace string {
    namespace methods {
        static constexpr const char* Format = "format";
        static constexpr const char* ToString = "toString";
    };
};
namespace object {
    namespace methods {
        static constexpr const char* ToString = "toString";
        static constexpr const char* HashCode = "hashCode";
    };
};
namespace stringable {
    static inline const std::string Qualified =
        "uranite.operators.stringable.Stringable";
    static constexpr const char* Name = "Stringable";
    namespace methods {
        static constexpr const char* ToString = "toString";
    };
};
```

---

## Examples

### Basic Formatting

```
package examples

from uranite.io.console import puts

public function main() -> I32:
    String name = "Alice"
    I64 age = 30
    String message = "Name: {}, Age: {}".format(name, age)
    puts(message)
    return 0
```

**Compilation trace**:
1. **Semantic analysis**: `"Name: {}, Age: {}".format(name, age)` — `format` is a builtin method on `String`. Arguments: `String`, `I64`.
2. **Codegen (fast path)**: Format string has no binary/center/named placeholders. `buildSnprintfFormat` produces `"Name: %s, Age: %ld"`. Dynamic buffer sizing because of string arg. Single `snprintf` call.

### Explicit Indexing

```
package examples

from uranite.io.console import puts

public function main() -> I32:
    String result = "{1} before {0}".format("world", "hello")
    puts(result)
    return 0
```

Output: `"hello before world"`. Explicit indices reverse argument order.

### Format Specifiers

```
package examples

from uranite.io.console import puts

public function main() -> I32:
    F64 pi = 3.14159265358979
    puts("Pi: {:.2f}".format(pi))
    puts("Pi: {:.6f}".format(pi))

    I64 value = 255
    puts("Hex: {:x}".format(value))
    puts("Hex: {:X}".format(value))
    puts("Oct: {:o}".format(value))
    puts("Bin: {:b}".format(value))

    return 0
```

**Compilation trace**:
1. `{:.2f}` — precision=2, type=`f`. Fast path: `snprintf(buf, size, "%.2f", pi)`.
2. `{:x}` — type=`x`. Fast path: `snprintf(buf, size, "%lx", value)`.
3. `{:b}` — type=`b`. Slow path: custom binary conversion loop. Extracts bits right-to-left, writes ASCII '0'/'1' into 65-byte buffer.

### Width and Alignment

```
package examples

from uranite.io.console import puts

public function main() -> I32:
    puts("{:>10}".format("right"))
    puts("{:<10}".format("left"))
    puts("{:^10}".format("center"))
    puts("{:*>10}".format("star"))

    return 0
```

**Compilation trace**:
1. `{:>10}` — right-align, width 10, fill space. Piece = "right" (5 chars). Padding needed: `memset(buf, ' ', 10)`, `memcpy` piece at offset 5.
2. `{:<10}` — left-align. `memcpy` piece at offset 0.
3. `{:^10}` — center-align. Slow path (center triggers piece assembly). `memcpy` piece at offset `(10-6)/2 = 2`.
4. `{:*>10}` — fill=`*`, right-align. `memset(buf, '*', 10)`, overwrite rightmost chars.

### Named Placeholders

```
package examples

from uranite.io.console import puts

public function main() -> I32:
    String result = "Hello, {name}! You are {age}.".format(
        name="Bob", age=25)
    puts(result)
    return 0
```

**Compilation trace**: Named placeholders trigger slow path. Each `{name}` is matched against keyword arguments by name. If no match, first positional arg used as fallback. Piece-by-piece assembly into final buffer.

### toString for Type Conversion

```
package examples

from uranite.io.console import puts

public function main() -> I32:
    I64 count = 42
    F64 ratio = 3.14

    String countStr = count.toString()
    String ratioStr = ratio.toString()

    puts(countStr)
    puts(ratioStr)

    return 0
```

**Compilation trace**:
1. `count.toString()` — `I64` is integer OOP wrapper. Codegen: `malloc(48)`, `snprintf(buf, 48, "%ld", count)`.
2. `ratio.toString()` — `F64` is float OOP wrapper. Codegen: `malloc(48)`, `snprintf(buf, 48, "%g", ratio)`.

### Concatenation vs Formatting

```
package examples

from uranite.io.console import puts

public function main() -> I32:
    String name = "Alice"
    I64 score = 95

    String viaConcat = "Player " + name + " scored " + score.toString() + " points"

    String viaFormat = "Player {} scored {} points".format(name, score)

    puts(viaConcat)
    puts(viaFormat)

    return 0
```

Concatenation creates 4 intermediate heap allocations (one per `+`). Formatting creates 1 allocation (single `snprintf` call). For multi-value string construction, `format` is more efficient.
