# String Type

Uranite's `String` type represents an immutable sequence of bytes stored as a null-terminated C-style string. It maps to LLVM's opaque pointer type (`ptr`) — a raw pointer to a heap- or data-segment-resident byte array. String literals use double-quote delimiters (`"hello"`), support 8 escape sequences plus hex escapes, and compile to `CreateGlobalStringPtr` constants embedded in the module's read-only data section. All mutation methods (concatenation, case conversion, trimming, replacement) allocate new heap buffers via `malloc` — the original string is never modified.

This document covers the complete `String` type specification, LLVM representation, lexer processing with escape sequences, the full compilation pipeline from source to LLVM IR, the smart string comparison mechanism with pointer-threshold dispatch, all 18 builtin method codegen implementations with their C library foundations, heterogeneous concatenation with type coercion, memory management semantics, and the OOP wrapper class.

---

## Table of Contents

- [Type Identity](#type-identity)
- [LLVM Representation](#llvm-representation)
  - [Type Mapping](#type-mapping)
  - [Constant Emission](#constant-emission)
  - [Module-Level Constants](#module-level-constants)
  - [Memory Layout](#memory-layout)
- [String Literals](#string-literals)
  - [Basic Syntax](#basic-syntax)
  - [Escape Sequences](#escape-sequences)
  - [Hex Escapes](#hex-escapes)
  - [Multi-Character Sequences](#multi-character-sequences)
  - [Delimiter Rule](#delimiter-rule)
- [Compilation Pipeline](#compilation-pipeline)
  - [Lexer Stage](#lexer-stage)
  - [Parser Stage](#parser-stage)
  - [HIR Stage](#hir-stage)
  - [MIR Stage](#mir-stage)
  - [Codegen Stage](#codegen-stage)
- [String Comparison](#string-comparison)
  - [The Pointer-Threshold Heuristic](#the-pointer-threshold-heuristic)
  - [Equality Dispatch](#equality-dispatch)
  - [Inequality Dispatch](#inequality-dispatch)
  - [When the Heuristic Applies](#when-the-heuristic-applies)
- [Builtin Methods](#builtin-methods)
  - [Query Methods](#query-methods)
  - [Search Methods](#search-methods)
  - [Access Methods](#access-methods)
  - [Transformation Methods](#transformation-methods)
  - [Concatenation and Type Coercion](#concatenation-and-type-coercion)
- [Memory Management](#memory-management)
  - [Allocation Strategy](#allocation-strategy)
  - [Ownership Semantics](#ownership-semantics)
- [Assignability Rules](#assignability-rules)
- [The OOP Wrapper](#the-oop-wrapper)
  - [String Class](#string-class)
  - [Stdlib-Level Methods](#stdlib-level-methods)
  - [Builtin Method Interception](#builtin-method-interception)
- [Examples](#examples)
  - [String Declarations](#string-declarations)
  - [Escape Sequences in Practice](#escape-sequences-in-practice)
  - [String Operations](#string-operations)
  - [String Comparisons](#string-comparisons)
  - [Search and Access](#search-and-access)
  - [Practical Usage](#practical-usage)

---

## Type Identity

| Property | Value |
|---|---|
| Type Name | `str` |
| Primitive Kind | `Type::Kind::String` |
| Qualified Name (primitive) | `uranite.builtin.str` |
| Qualified Name (OOP wrapper) | `uranite.language.string.String` |
| LLVM Type | `ptr` (opaque pointer) |
| Literal Delimiter | Double quotes (`"..."`) |

The `String` type is classified by `isPrimitive()` (via `Kind::String`) and is part of the `oopWrapperQualified` set. It is not in `integerOopQualified` or `floatOopQualified` — it is not considered a numeric type. The `isIntegral()` and `isFloatingPoint()` predicates return `false` for `Kind::String`.

---

## LLVM Representation

### Type Mapping

The `String` type maps to `llvm::PointerType::getUnqual(context)` — an opaque pointer. This mapping applies to both the primitive kind and the OOP wrapper:

| Source Type | Condition | LLVM Type |
|---|---|---|
| `Type::Kind::String` | Always | `ptr` |
| `Type::Kind::Class` | `className == "String"` | `ptr` |

Strings are pointer-sized values. On a 64-bit target, a `String` occupies 8 bytes in registers and on the stack — the pointer to the underlying null-terminated byte array.

### Constant Emission

String constants are emitted by `generateConstantString()`:

```
CreateGlobalStringPtr(instruction.stringConstantValue, "str")
→ setVariableValue(destination, constValue)
```

`CreateGlobalStringPtr` creates a global constant array in the LLVM module's read-only data section, containing the string bytes plus a null terminator, and returns a pointer (`ptr`) to the first byte. Each unique string literal becomes one global constant — LLVM's constant merging pass may deduplicate identical strings across the module.

### Module-Level Constants

Top-level `const String` declarations are also emitted via `CreateGlobalStringPtr`:

```
case MIRModuleConstant::String:
    constantValue = CreateGlobalStringPtr(constant.stringValue, "const.str")
```

These produce module-scoped global string pointers, accessible from any function in the compilation unit.

### Memory Layout

| Property | Value |
|---|---|
| Pointer Size | 8 bytes (64-bit target) |
| Underlying Data | Null-terminated byte array |
| Encoding | Raw bytes (typically UTF-8 by convention) |
| Mutability | Literals are read-only; heap-allocated strings are writable but treated as immutable by the type system |

String literals reside in the `.rodata` section — writing to them is undefined behavior at the hardware level. Strings produced by mutation methods (concatenation, case conversion, trimming) are heap-allocated via `malloc` and are technically writable, but the language semantics treat all strings as immutable.

---

## String Literals

### Basic Syntax

String literals are enclosed in double quotes. They may contain zero or more characters:

```uranite
String greeting = "Hello, world!"
String empty = ""
String single = "A"
String multiWord = "Uranite programming language"
```

Single quotes delimit characters, not strings. `'A'` is a `Char`, `"A"` is a `String`.

### Escape Sequences

The lexer's `readString()` function recognizes 8 escape sequences — the same set as character literals plus the double-quote escape:

| Escape | Character | Code Point | Description |
|---|---|---|---|
| `\"` | `"` | U+0022 | Double quote (literal) |
| `\'` | `'` | U+0027 | Single quote |
| `\\` | `\` | U+005C | Backslash |
| `\0` | NUL | U+0000 | Null character |
| `\n` | LF | U+000A | Line feed (newline) |
| `\r` | CR | U+000D | Carriage return |
| `\t` | TAB | U+0009 | Horizontal tab |
| `\xHH` | varies | U+00HH | Hex byte (2 hex digits) |

Any unrecognized escape character after `\` produces a diagnostic warning and is taken literally — `\q` produces the character `q` with a compiler warning: `unknown escape sequence "\q"`.

### Hex Escapes

The `\x` escape reads up to 2 hexadecimal digits and interprets them as a byte value:

```uranite
String nullByte = "\x00"
String bell = "\x07"
String maxByte = "\xFF"
String mixed = "prefix\x41suffix"
```

The hex value is parsed via `std::stoi(hexSequence, nullptr, 16)` and cast to `char`. This limits `\x` escapes to the range 0x00–0xFF (0–255) per byte.

### Multi-Character Sequences

Unlike character literals (which hold exactly one character), string literals hold zero or more characters and may contain multiple escape sequences:

```uranite
String path = "C:\\Users\\hxari\\Documents"
String twoLines = "line one\nline two"
String tabbed = "col1\tcol2\tcol3"
String quoted = "She said \"hello\""
```

### Delimiter Rule

Double quotes delimit strings; single quotes delimit characters. The lexer dispatches on the opening delimiter:

- `"` triggers `readString()` — reads characters until closing `"`, building a multi-byte string value.
- `'` triggers `readChar()` — reads exactly one character (or one escape sequence), then closing `'`.

Newlines are not permitted inside string literals. A `\n` inside a string causes the lexer to emit an `unterminated string literal` error. Use the `\n` escape sequence for embedded newlines.

An unterminated string literal (reaching end-of-file before closing `"`) produces an `Error` token with an `unterminated string literal` diagnostic.

---

## Compilation Pipeline

### Lexer Stage

The `readString()` method processes string literals:

1. Advance past the opening `"` and capture the quote character.
2. Loop until end-of-input or the matching closing `"`:
   - If the current character is the closing quote, advance past it and return a `LiteralString` token with the accumulated string value.
   - If the current character is `\n`, emit an `unterminated string literal` error and return an `Error` token.
   - If the current character is `\`, advance and enter escape sequence handling (switch on the escape character).
   - Otherwise, append the current character directly to the string value.
3. If end-of-input is reached without a closing quote, emit an `unterminated string literal` error.

The token stores the processed string value (post-escape-processing), not the raw source text. The escape sequence `\n` in source becomes a literal newline byte (0x0A) in the token value.

### Parser Stage

The parser encounters a `LiteralString` token and creates a `StringLiteralExpression` AST node:

```
StringLiteralExpression(value = token.value, source)
```

The string value is stored as a C++ `std::string` in the AST node.

### HIR Stage

HIR lowering creates an `HIRStringLiteral` node:

```
HIRStringLiteral {
    stringValue: std::string   — the processed string content
}
```

### MIR Stage

MIR lowering emits a `ConstantString` instruction:

```
MIRInstruction(MIRInstructionKind::ConstantString)
    .stringConstantValue = stringLiteral.stringValue
    .destinationVariable = allocated variable
```

### Codegen Stage

The `generateConstantString()` function emits the LLVM global string:

```
CreateGlobalStringPtr(stringConstantValue, "str")
→ setVariableValue(destination, constValue)
```

**Complete pipeline example for `"Hello"`:**

```
Source:  "Hello"
Lexer:   LiteralString token, value = "Hello"
Parser:  StringLiteralExpression(value = "Hello")
HIR:     HIRStringLiteral(stringValue = "Hello")
MIR:     ConstantString(stringConstantValue = "Hello")
LLVM IR: @str = private unnamed_addr constant [6 x i8] c"Hello\00"
         ptr @str
```

**Pipeline example with escape sequences for `"A\tB\n"`:**

```
Source:  "A\tB\n"
Lexer:   LiteralString token, value = "A<TAB>B<LF>"  (3 bytes: 0x41 0x09 0x42 0x0A)
Parser:  StringLiteralExpression(value = "A\tB\n")
HIR:     HIRStringLiteral(stringValue = "A\tB\n")
MIR:     ConstantString(stringConstantValue = "A\tB\n")
LLVM IR: @str = private unnamed_addr constant [5 x i8] c"A\09B\0A\00"
         ptr @str
```

---

## String Comparison

String comparison in Uranite uses a sophisticated dual-path dispatch mechanism. Because strings are stored as opaque pointers (`ptr`) and LLVM represents them as `i64` values in many contexts, the codegen must distinguish between pointer values (which point to string data) and small integer values (which are just numbers). This is solved with a page-threshold heuristic.

### The Pointer-Threshold Heuristic

The codegen uses a threshold of 4096 (one memory page) to distinguish real pointers from small integer values:

```
pageThreshold = ConstantInt::get(i64Type, 4096)
leftAbove  = ICmpUGE(leftOperand, pageThreshold)
rightAbove = ICmpUGE(rightOperand, pageThreshold)
bothAbove  = And(leftAbove, rightAbove)
```

If both operands have values >= 4096, they are treated as string pointers and compared via `strcmp`. Otherwise, they fall back to integer comparison (`ICmpEQ`/`ICmpNE`).

This heuristic works because:
- Valid heap and data-segment pointers always reside above the first page boundary (addresses below 4096 are typically unmapped guard pages on modern operating systems).
- Small integer constants (0, 1, 255, etc.) never exceed 4096, so they correctly take the integer comparison path.

### Equality Dispatch

For `CompareEqual` when a string comparison is detected:

```
              ┌─────────────────────────┐
              │  both operands >= 4096? │
              └────────┬────────────────┘
                  yes  │            no
              ┌────────▼───────┐  ┌─────▼──────┐
              │ strcmp(l, r)   │  │ ICmpEQ(l,r) │
              │ result == 0?  │  │             │
              └────────┬──────┘  └──────┬──────┘
                       │                │
              ┌────────▼────────────────▼──────┐
              │  PHI: merge both i1 results    │
              └────────────────────────────────┘
```

The `strcmp` path converts both `i64` operands to pointers via `IntToPtr`, calls `strcmp`, and compares the result to zero. The fallback path does a direct `ICmpEQ` on the raw `i64` values. A PHI node at the merge block selects the correct result.

### Inequality Dispatch

For `CompareNotEqual`, the same dual-path structure applies:

- `strcmp` path: `ICmpNE(strcmp(left, right), 0)` — true if strings differ.
- Fallback path: `ICmpNE(left, right)` — direct integer inequality.
- PHI node merges both branches.

### When the Heuristic Applies

The string comparison path activates only when **all** of these conditions hold:

1. The instruction is `CompareEqual` or `CompareNotEqual` (not `<`, `>`, `<=`, `>=`).
2. The current function context is available.
3. At least one operand has a variable descriptor with type `Kind::String`, `Kind::GenericParameter`, or `Kind::Class` with name `"String"`.
4. Neither operand is a small constant (value between -1 and 255) — prevents false activation on enum comparisons or byte checks.
5. Both operands are `i64` at the LLVM level.

Relational comparisons (`<`, `>`, `<=`, `>=`) on strings use standard integer comparison on the pointer values — they compare memory addresses, not lexicographic order. Lexicographic ordering requires explicit method calls.

---

## Builtin Methods

The codegen intercepts 18 method names on the `String` OOP wrapper and emits inline LLVM IR instead of dispatching to stdlib function calls. All string methods that produce new strings allocate fresh heap buffers — the original string is never modified.

### Query Methods

| Method | Signature | LLVM Implementation | Returns |
|---|---|---|---|
| `length()` | `() -> I64` | `strlen(self)` | Byte count (excluding null terminator) |
| `isEmpty()` | `() -> Boolean` | `ICmpEQ(strlen(self), 0)` | `True` if length is zero |

`length()` calls `strlen` which counts bytes, not Unicode code points. For ASCII strings, byte count equals character count. For multi-byte UTF-8 sequences, `length()` returns the byte count, which may exceed the visible character count.

### Search Methods

| Method | Signature | LLVM Implementation | Returns |
|---|---|---|---|
| `contains(substr)` | `(String) -> Boolean` | `ICmpNE(strstr(self, substr), null)` | `True` if substring found |
| `indexOf(substr)` | `(String) -> I64` | `strstr` + `PtrDiff`, or `-1` if null | Byte offset of first occurrence, or -1 |
| `startsWith(prefix)` | `(String) -> Boolean` | `ICmpEQ(strncmp(self, prefix, strlen(prefix)), 0)` | `True` if prefix matches |
| `endsWith(suffix)` | `(String) -> Boolean` | GEP to tail + `ICmpEQ(strcmp(tail, suffix), 0)` with length guard | `True` if suffix matches |

**`indexOf` implementation detail:**

```
found = strstr(self, substr)
isNull = ICmpEQ(found, null)
offset = PtrDiff(found, self)
result = Select(isNull, -1, offset)
```

Returns -1 when the substring is not found. Returns a byte offset (not a character index) when found.

**`endsWith` implementation detail:**

```
selfLen   = strlen(self)
suffixLen = strlen(suffix)
offset    = Sub(selfLen, suffixLen)
tailPtr   = GEP(i8, self, offset)
cmpResult = strcmp(tailPtr, suffix)
isMatch   = ICmpEQ(cmpResult, 0)
lenOk     = ICmpSGE(selfLen, suffixLen)
result    = And(lenOk, isMatch)
```

The length guard (`lenOk`) ensures that the suffix is not longer than the string itself — without it, the GEP would compute a negative offset and read garbage memory.

### Access Methods

| Method | Signature | LLVM Implementation | Returns |
|---|---|---|---|
| `charAt(index)` | `(I64) -> I64` | `ZExt(Load(GEP(i8, self, index)), i64)` | Byte value at position as I64 |
| `charCodeAt(index)` | `(I64) -> I64` | `ZExt(Load(GEP(i8, self, index)), i64)` | Byte value at position as I64 |
| `getValue()` | `() -> String` | Identity return of pointer | The raw string pointer |
| `toString()` | `() -> String` | Identity return of pointer | The string itself |
| `equals(other)` | `(String) -> Boolean` | `ICmpEQ(strcmp(self, other), 0)` | `True` if contents match |

`charAt` and `charCodeAt` are identical in implementation — both load a single byte (`i8`) at the given index via GEP, then zero-extend to `i64`. If the index argument is a floating-point value, it is first converted via `FPToSI` to `i64`. No bounds checking is performed — an out-of-range index reads past the string buffer.

### Transformation Methods

| Method | Signature | LLVM Implementation | Returns |
|---|---|---|---|
| `toUpper()` | `() -> String` | malloc + memcpy + byte-wise loop (a-z check, subtract 32) | New uppercase string |
| `toLower()` | `() -> String` | malloc + memcpy + byte-wise loop (A-Z check, add 32) | New lowercase string |
| `trim()` | `() -> String` | Dual-scan loop (leading + trailing whitespace) + malloc + memcpy | New trimmed string |
| `replace(old, new)` | `(String, String) -> String` | `strstr` loop + `memcpy` reconstruction | New string with all occurrences replaced |

**`toUpper` codegen structure:**

1. Compute length via `strlen(self)`.
2. Allocate `length + 1` bytes via `malloc`.
3. Copy the original string via `memcpy` (including null terminator).
4. Loop over each byte in the buffer:
   - Load the byte at the current index.
   - Check if it is in the range `'a'` (0x61) to `'z'` (0x7A).
   - If yes, subtract 32 to produce the uppercase equivalent.
   - Store the result (original or converted) back.
5. Return the new buffer pointer.

`toLower` follows the same structure but checks for `'A'` (0x41) to `'Z'` (0x5A) and adds 32.

Both methods perform ASCII-only case conversion. Bytes outside the A-Z/a-z range pass through unchanged. Multi-byte UTF-8 characters are not recognized as letters.

**`trim` codegen structure:**

1. Compute length via `strlen(self)`.
2. Initialize `start = 0` and `end = length`.
3. Forward scan: while `start < length` and the byte at `start` is whitespace (space, tab, newline, or carriage return), increment `start`.
4. Backward scan: while `end > start` and the byte at `end - 1` is whitespace, decrement `end`.
5. Compute `trimmedLength = end - start`.
6. Allocate `trimmedLength + 1` bytes via `malloc`.
7. Copy `trimmedLength` bytes from `self + start` via `memcpy`.
8. Write null terminator at position `trimmedLength`.
9. Return the new buffer pointer.

Whitespace characters recognized: space (0x20), tab (0x09), newline (0x0A), carriage return (0x0D).

**`replace` codegen structure:**

1. Compute `selfLen`, `oldLen`, `newLen` via `strlen`.
2. Allocate worst-case buffer: `selfLen * (newLen + 1) + 1` bytes.
3. Initialize source pointer to `self` and destination pointer to the buffer.
4. Loop:
   - Call `strstr(source, old)` to find the next occurrence.
   - If found: copy prefix bytes (from source to found position) via `memcpy`, then copy `new` replacement via `memcpy`, advance source past `old`, advance destination past prefix and replacement.
   - If not found: copy remaining source bytes (including null terminator) via `memcpy`, exit loop.
5. Return the buffer pointer.

This performs a global replace — all non-overlapping occurrences of `old` are substituted with `new`.

### Concatenation and Type Coercion

The `concat(other)` method is the most complex string operation. It concatenates `self` with `other`, but `other` may be any type — the codegen performs automatic type coercion to convert non-string operands to their string representation:

| Operand Type | Coercion Strategy | Format |
|---|---|---|
| `ptr` (String) | Used directly | N/A |
| Integer (non-i1) | `snprintf(buf, 24, "%ld", value)` | Decimal signed long |
| Boolean (i1) | `Select(value, "True", "False")` | Literal boolean name |
| Float/Double | `snprintf(buf, 48, "%.6g", value)` | 6 significant digits |
| Generic parameter (ptr) | Runtime branch: pointer-vs-integer dispatch | See below |

**Integer coercion:** integers narrower than 64 bits are sign-extended to `i64` before formatting. The `snprintf` call writes the decimal representation into a 24-byte malloc'd buffer.

**Float coercion:** `float` values are first extended to `double` via `FPExt`. The `snprintf` call writes the `%.6g` representation into a 48-byte malloc'd buffer. The `%g` format suppresses trailing zeros.

**Boolean coercion:** a `Select` instruction chooses between the global string constants `"True"` and `"False"` based on the `i1` value. No heap allocation needed — both strings are in the data section.

**Generic parameter coercion:** when the other operand is a generic type parameter (`Kind::GenericParameter`), the codegen emits a runtime branch:

```
ptrAsInt  = PtrToInt(other, i64)
threshold = 4096
isRealPtr = ICmpUGE(ptrAsInt, threshold)
branch(isRealPtr → strBlock, intBlock)

strBlock:   use other directly as string pointer
intBlock:   snprintf(buf, 24, "%ld", ptrAsInt)   — treat as integer

mergeBlock: PHI selects the string pointer
```

This uses the same page-threshold heuristic as string comparison — values >= 4096 are treated as string pointers, values below are treated as integers and formatted via `snprintf`.

**Final concatenation assembly:**

After coercing `other` to a string pointer (`otherStr`):

```
lenA     = strlen(self)
lenB     = strlen(otherStr)
totalLen = Add(lenA, lenB)
allocSize = Add(totalLen, 1)
buf      = malloc(allocSize)
strcpy(buf, self)
strcat(buf, otherStr)
→ setVariableValue(destination, buf)
```

The result is a newly heap-allocated string containing the concatenation. The caller takes ownership.

---

## Memory Management

### Allocation Strategy

String operations fall into two categories based on memory allocation:

| Category | Methods | Allocation |
|---|---|---|
| Read-only queries | `length`, `isEmpty`, `contains`, `indexOf`, `startsWith`, `endsWith`, `charAt`, `charCodeAt`, `equals`, `getValue`, `toString` | None — operate on the existing string pointer |
| Mutating transforms | `concat`, `toUpper`, `toLower`, `trim`, `replace` | `malloc` — allocate a new heap buffer for the result |

**C library functions used by string codegen:**

| Function | Purpose | Obtained via |
|---|---|---|
| `strlen` | Compute byte length | `getOrCreateStrlen()` |
| `strcmp` | Full-string equality | `getOrCreateStrcmp()` |
| `strncmp` | Prefix comparison | `getOrCreateStrncmp()` |
| `strstr` | Substring search | `getOrCreateStrstr()` |
| `strcpy` | Copy string into buffer | `getOrCreateStrcpy()` |
| `strcat` | Append string to buffer | `getOrCreateStrcat()` |
| `malloc` | Heap allocation | `getOrCreateMalloc()` |
| `memcpy` | Raw byte copy | `getOrCreateMemcpy()` |
| `snprintf` | Formatted type-to-string conversion | `getOrCreateSnprintf()` |

Each `getOrCreate*` method lazily declares the C function prototype in the LLVM module on first use. Subsequent calls retrieve the cached declaration.

### Ownership Semantics

String literals are statically allocated in the `.rodata` section — they exist for the lifetime of the program and do not need deallocation.

Strings produced by mutation methods are heap-allocated via `malloc`. Ownership follows Uranite's move semantics:

- The returned pointer is owned by the caller.
- When the owning variable goes out of scope, the borrow checker tracks it.
- No garbage collector exists — the runtime relies on ownership tracking for memory safety.
- Intermediate strings produced during chained operations (e.g., `text.trim().toUpper()`) allocate separate buffers. The intermediate buffer from `trim()` is not automatically freed when `toUpper()` allocates its own buffer.

---

## Assignability Rules

The `String` type has strict assignability constraints:

- `String` is assignable to `String` — trivial identity via `target->equals(source)`.
- `String` is assignable to `Object` — all types are assignable to `Object` (the universal supertype).
- `Object` is assignable to `String` — `Object` is assignable to any class type.
- `String` is **not** assignable to any numeric type (`Integer`, `Float`, `Bool`, `Char`) — no implicit string-to-number conversion exists.
- No numeric type is implicitly assignable to `String` — explicit conversion via `.toString()` or concatenation is required.
- `String` is classified by `isPrimitive()` via `Kind::String`.
- `String` is part of `oopWrapperQualified` — the OOP wrapper is recognized.
- `String` is not numeric — `isIntegral()` and `isFloatingPoint()` return `false`.
- `isComparable()` permits comparing `String` with `String`.

The `isAssignable` function does not have a special case for String — it relies on the general identity match (`target->equals(source)`) and the Object supertype rule. String is not part of the integer cross-assignability block or the float cross-assignability block.

---

## The OOP Wrapper

### String Class

The `String` class in `stdlibs/language/string.urn`:

```
class String
    protect String value
```

It is a non-final class with a `protect` field typed as `String`. At the LLVM level, the wrapper's `value` field is a `ptr` — the same representation as the primitive.

**Constructor:** `String(self, String value)` — wraps a string value.

### Stdlib-Level Methods

The OOP wrapper declares methods that delegate to the protected `value` field. Some are intercepted by the codegen at compile time (see below); others execute as normal Uranite function calls:

| Method | Signature | Stdlib Implementation |
|---|---|---|
| `getValue()` | `() -> String` | `return self.value` |
| `toString()` | `() -> String` | `return self.value` |
| `length()` | `() -> I64` | `return self.value as I64` (intercepted) |
| `isEmpty()` | `() -> Boolean` | `return self.length() == 0` |
| `concat(other)` | `(String) -> String` | `return new String(self.value + other.value)` |
| `equals(other)` | `(String) -> Boolean` | `return self.value == other.value` |
| `startsWith(prefix)` | `(String) -> Boolean` | `return self.value == prefix` (intercepted) |
| `endsWith(suffix)` | `(String) -> Boolean` | `return self.value == suffix` (intercepted) |
| `contains(substr)` | `(String) -> Boolean` | `return self.value == substr` (intercepted) |
| `toUpper()` | `() -> String` | `return new String(self.value)` (intercepted) |
| `toLower()` | `() -> String` | `return new String(self.value)` (intercepted) |
| `trim()` | `() -> String` | `return new String(self.value)` (intercepted) |
| `replace(old, new)` | `(String, String) -> String` | `return new String(self.value)` (intercepted) |
| `split(delim)` | `(String) -> String` | `return self.value` |
| `hash()` | `() -> I64` | `return self.value as I64` |
| `format(args)` | `(I64[]) -> String` | Pure Uranite byte-level implementation |

The `format` method is notable — it is implemented entirely in Uranite using raw syscall imports (`ptrToString`, `readByteAt`, `stringLen`, `stringToPtr`, `writeByteAt`, `alloc`). It scans the template string for `{}` placeholder pairs and substitutes positional arguments from the `args` array. This is a zero-C-runtime implementation.

### Builtin Method Interception

The codegen intercepts 18 method names when called on a value identified as a string wrapper. The interception check matches the method name against constants defined in `semantic::qualname::classes::string::methods`:

| Qualname Constant | Method Name | Intercepted? |
|---|---|---|
| `Length` | `"length"` | Yes |
| `IsEmpty` | `"isEmpty"` | Yes |
| `Contains` | `"contains"` | Yes |
| `Equals` | `"equals"` | Yes |
| `Concat` | `"concat"` | Yes |
| `StartsWith` | `"startsWith"` | Yes |
| `EndsWith` | `"endsWith"` | Yes |
| `IndexOf` | `"indexOf"` | Yes |
| `CharAt` | `"charAt"` | Yes |
| `CharCodeAt` | `"charCodeAt"` | Yes |
| `ToUpper` | `"toUpper"` | Yes |
| `ToLower` | `"toLower"` | Yes |
| `Trim` | `"trim"` | Yes |
| `Replace` | `"replace"` | Yes |
| `ToString` | `"toString"` | Yes |
| `Split` | `"split"` | No — dispatched to stdlib |
| `Format` | `"format"` | No — dispatched to stdlib |
| `Substring` | `"substring"` | Registered but not intercepted |

When interception matches, the codegen emits inline LLVM IR for the operation instead of generating a function call. This eliminates call overhead for the most common string operations. Methods not intercepted (`split`, `format`, `hash`) fall through to normal method dispatch and execute the stdlib Uranite implementation.

---

## Examples

### String Declarations

```uranite
String greeting = "Hello, world!"
String empty = ""
String name = "Uranite"
String path = "/home/user/project"
String version = "1.0.0"
```

### Escape Sequences in Practice

```uranite
from uranite.io.console import puts

String newlines = "Line one\nLine two\nLine three"
puts( newlines )

String tabs = "Name\tAge\tCity"
puts( tabs )

String quoted = "She said \"hello\" to everyone"
puts( quoted )

String backslashes = "C:\\Users\\hxari\\Documents"
puts( backslashes )

String hexEscape = "\x48\x65\x6C\x6C\x6F"
puts( hexEscape )

String nullTerminated = "before\0after"
puts( nullTerminated )
```

### String Operations

```uranite
from uranite.io.console import puts

String text = "  Hello, World!  "
String trimmed = text.trim()
puts( trimmed )

String upper = trimmed.toUpper()
puts( upper )

String lower = trimmed.toLower()
puts( lower )

String replaced = trimmed.replace( "World", "Uranite" )
puts( replaced )

String greeting = "Hello"
String name = "Uranite"
String message = greeting.concat( ", " ).concat( name ).concat( "!" )
puts( message )
```

### String Comparisons

```uranite
from uranite.io.console import puts

String first = "alpha"
String second = "alpha"
String third = "beta"

Boolean same = first == second
puts( same.toString() )

Boolean different = first == third
puts( different.toString() )

Boolean notEqual = first != third
puts( notEqual.toString() )

String greeting = "Hello, World!"
Boolean match = greeting.equals( "Hello, World!" )
puts( match.toString() )
```

### Search and Access

```uranite
from uranite.io.console import puts

String text = "The quick brown fox jumps over the lazy dog"

I64 length = text.length()
Boolean empty = text.isEmpty()

Boolean hasQuick = text.contains( "quick" )
puts( hasQuick.toString() )

Boolean startsWithThe = text.startsWith( "The" )
puts( startsWithThe.toString() )

Boolean endsWithDog = text.endsWith( "dog" )
puts( endsWithDog.toString() )

I64 foxPosition = text.indexOf( "fox" )

I64 firstByte = text.charAt( 0 )
I64 codeByte = text.charCodeAt( 4 )
```

### Practical Usage

```uranite
from uranite.io.console import puts

public function isBlank( String text ) -> Boolean:
    return text.trim().isEmpty()

public function repeat( String text, I64 count ) -> String:
    String result = ""
    I64 iteration = 0
    while iteration < count:
        result = result.concat( text )
        iteration = iteration + 1
    return result

public function countOccurrences( String haystack, String needle ) -> I64:
    I64 count = 0
    I64 searchStart = 0
    I64 needleLength = needle.length()
    I64 haystackLength = haystack.length()
    while searchStart < haystackLength:
        I64 position = haystack.indexOf( needle )
        if position == -1:
            return count
        count = count + 1
        searchStart = position + needleLength
    return count

public function padLeft( String text, I64 targetLength, String padding ) -> String:
    String result = text
    while result.length() < targetLength:
        result = padding.concat( result )
    return result

public function reverseWords( String sentence ) -> String:
    String trimmed = sentence.trim()
    String result = ""
    String current = ""
    I64 index = 0
    I64 length = trimmed.length()
    while index < length:
        I64 charCode = trimmed.charAt( index )
        if charCode == 32:
            if current.isEmpty() == false:
                if result.isEmpty():
                    result = current
                else:
                    result = current.concat( " " ).concat( result )
                current = ""
        else:
            String charString = ""
            current = current.concat( charString )
        index = index + 1
    if current.isEmpty() == false:
        if result.isEmpty():
            result = current
        else:
            result = current.concat( " " ).concat( result )
    return result

public function main() -> I32:
    Boolean blankCheck = isBlank( "   " )
    puts( blankCheck.toString() )

    String repeated = repeat( "ab", 3 )
    puts( repeated )

    String padded = padLeft( "42", 6, "0" )
    puts( padded )

    String greeting = "Hello"
    String upper = greeting.toUpper()
    puts( upper )

    String withSpaces = "  trim me  "
    String trimmed = withSpaces.trim()
    puts( trimmed )

    String replaced = greeting.replace( "l", "r" )
    puts( replaced )

    Boolean starts = greeting.startsWith( "Hel" )
    puts( starts.toString() )

    Boolean ends = greeting.endsWith( "llo" )
    puts( ends.toString() )

    return 0
```

This example demonstrates whitespace detection via `trim().isEmpty()`, string repetition through concatenation loops, occurrence counting with `indexOf`, left-padding with `concat`, word reversal by byte-level scanning, and chained transformation methods — all compiled to inline `strlen`/`strcmp`/`strstr`/`malloc`/`memcpy` calls with zero function dispatch overhead for intercepted methods.
