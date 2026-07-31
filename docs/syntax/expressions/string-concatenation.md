# String Concatenation

Uranite overloads the `+` operator for string concatenation. When the semantic analyzer detects that the left operand of a `Plus` binary expression is a `String` type, it accepts the expression and returns `String` as the result type — bypassing the normal numeric-only arithmetic path. At MIR level, string `+` lowers to a `CallFunction` targeting `concat` (the `qualname::classes::string::methods::Concat` constant), not an arithmetic instruction. At LLVM codegen, the `concat` call is intercepted and expanded inline into a `strlen` + `strlen` + `malloc` + `strcpy` + `strcat` sequence that heap-allocates a new buffer holding the joined result. Non-string right operands (integers, floats, booleans, generic parameters) are automatically converted to their string representation via `snprintf` formatting before concatenation.

---

## Table of Contents

- [Operator Syntax](#operator-syntax)
- [Semantic Analysis](#semantic-analysis)
- [String Detection in MIR Lowering](#string-detection-in-mir-lowering)
- [MIR Lowering — CallFunction concat](#mir-lowering--callfunction-concat)
- [MIR Codegen — Inline Expansion](#mir-codegen--inline-expansion)
  - [Method-Style Concat (Self + Other)](#method-style-concat-self--other)
  - [Bare Concat (Left + Right)](#bare-concat-left--right)
  - [Type Coercion — ensureString](#type-coercion--ensurestring)
- [Memory Model](#memory-model)
- [String Representation](#string-representation)
- [Qualname Constants](#qualname-constants)
- [Examples](#examples)

---

## Operator Syntax

String concatenation uses the same `+` operator as arithmetic addition:

```
String greeting = "Hello, " + "world!"
String message = "count: " + total.toString()
```

No dedicated concatenation operator exists. The `+` token type (`token::Type::Plus`) is shared between numeric addition and string concatenation — disambiguation happens at semantic analysis based on operand types.

---

## Semantic Analysis

At `src/uranite/semantic/analyzer.cpp:1238-1239`, after the numeric arithmetic path fails (neither operand is numeric), the analyzer checks for string concatenation:

```cpp
if( expression.operation == token::Type::Plus &&
    ( leftSideType->kind == Type::Kind::String ||
      leftSideType->qualified == qualname::String ) ) {
    return leftSideType;
}
```

Resolution order within `analyzeBinaryExpression` for `Plus`:
1. **Both operands numeric**: standard arithmetic path — promote to `F64` if either is float, else `I64`.
2. **Left operand is String**: string concatenation — result type is `String`. Right operand type is unconstrained at semantic level.
3. **Left operand is class/struct**: operator interface dispatch via `ArithmeticMappings` (checks `Addable` interface).
4. **None of the above**: error "invalid operands to binary \"+\"".

Key detail: only the **left** operand is checked for `String` type. `42 + "hello"` does not trigger string concatenation — it falls through to the arithmetic/interface path and produces an error. String concatenation requires the left operand to be a `String`.

---

## String Detection in MIR Lowering

Before lowering a binary `Plus` operation, the MIR lowering pass performs multi-layered string detection at `src/uranite/ir/mir/lowering.cpp:3226-3253`:

```cpp
auto isStringType = []( const semantic::TypeSharedPointer& type ) -> bool {
    return type != nullptr &&
        ( type->kind == semantic::Type::Kind::String ||
          ( type->kind == semantic::Type::Kind::Class &&
            type->name == semantic::qualname::classes::string::Name ) );
};
auto isStringHIRNode = [&]( const hir::HIRNodeSharedPointer& node ) -> bool {
    if( node == nullptr ) { return false; }
    if( node->nodeKind == hir::HIRNodeKind::StringLiteral ) { return true; }
    if( isStringType( node->resolvedType ) ) { return true; }
    return false;
};
auto isStringMIRVariable = [&]( MIRVariableIdentifier variable ) -> bool {
    std::unordered_map<MIRVariableIdentifier, MIRVariableDescriptor>::iterator
        descriptorIterator =
            this->currentFunction->variableDescriptorTable.find( variable );
    if( descriptorIterator !=
        this->currentFunction->variableDescriptorTable.end() ) {
        return isStringType(
            descriptorIterator->second.variableType );
    }
    return false;
};
bool isStringOperation = isStringType( hirBinaryOp.resolvedType );
bool hasStringOperands =
    isStringType( hirBinaryOp.leftOperand->resolvedType ) ||
    isStringType( hirBinaryOp.rightOperand->resolvedType );
bool hasStringHIRNodes =
    isStringHIRNode( hirBinaryOp.leftOperand ) ||
    isStringHIRNode( hirBinaryOp.rightOperand );
bool hasStringMIRVariables =
    isStringMIRVariable( leftVariable ) ||
    isStringMIRVariable( rightVariable );
bool isStringContext = isStringOperation || hasStringOperands ||
    hasStringHIRNodes || hasStringMIRVariables;
```

Four independent checks are OR-combined into `isStringContext`:

| Check | What It Examines | Catches |
|---|---|---|
| `isStringOperation` | Result type from semantic analysis | Normal case: `String + anything` |
| `hasStringOperands` | Resolved types of left/right HIR nodes | Operand-level type info |
| `hasStringHIRNodes` | HIR node kind (`StringLiteral`) or resolved type | String literals without explicit type annotation |
| `hasStringMIRVariables` | Variable descriptor table after MIR lowering | Variables whose MIR descriptor carries string type |

This multi-layered detection ensures string concatenation is correctly identified even when type information propagation is incomplete through generic contexts or type inference boundaries.

---

## MIR Lowering — CallFunction concat

When `isStringContext` is true and the operator is `Plus`, the lowering emits a `CallFunction` instruction instead of an arithmetic instruction. At `src/uranite/ir/mir/lowering.cpp:3254-3269`:

```cpp
if( isStringContext &&
    hirBinaryOp.operatorKind == token::Type::Plus ) {
    semantic::TypeSharedPointer concatResultType =
        hirBinaryOp.resolvedType;
    if( concatResultType == nullptr ) {
        concatResultType = std::make_shared<semantic::Type>(
            semantic::Type::Kind::String,
            semantic::qualname::classes::string::Name );
    }
    MIRInstruction concatInstruction(
        MIRInstructionKind::CallFunction );
    concatInstruction.calledFunctionQualifiedName =
        semantic::qualname::classes::string::methods::Concat;
    concatInstruction.sourceOperands.push_back( leftVariable );
    concatInstruction.sourceOperands.push_back( rightVariable );
    concatInstruction.operandType = concatResultType;
    concatInstruction.sourceLocation =
        hirBinaryOp.sourceLocation;
    MIRVariableIdentifier resultVariable =
        this->currentFunction->allocateVariable(
            "_concat", concatResultType, false );
    concatInstruction.destinationVariable = resultVariable;
    return this->emitInstruction( concatInstruction );
}
```

The emitted MIR instruction:
- Kind: `CallFunction`
- Target: `"concat"` (`qualname::classes::string::methods::Concat`)
- Source operands: `[leftVariable, rightVariable]`
- Destination: `_concat` variable with `String` type

This `CallFunction("concat")` is **not** a real function call — no `concat` function exists in the program. The MIR codegen intercepts this specific callee name and expands it inline.

---

## MIR Codegen — Inline Expansion

The codegen has two paths that intercept `concat` calls, both producing identical LLVM IR. Which path fires depends on how the call was structured.

### Method-Style Concat (Self + Other)

At `src/uranite/ir/mir/codegen.cpp:3241-3338`, inside the string method dispatch block (when the call is recognized as a method on a `String` self value):

The codegen performs:
1. **Right operand type coercion** — converts non-string right operand to string representation.
2. **Length computation** — `strlen(left)` + `strlen(right)`.
3. **Buffer allocation** — `malloc(totalLength + 1)` for null terminator.
4. **Copy and append** — `strcpy(buffer, left)` then `strcat(buffer, right)`.

### Bare Concat (Left + Right)

At `src/uranite/ir/mir/codegen.cpp:4125-4206`, when the call is recognized as a bare/object-level `concat` call with exactly 2 operands:

```cpp
if( isBareOrObjectCall &&
    bareMethodName ==
        semantic::qualname::classes::string::methods::Concat &&
    instruction.sourceOperands.size() == 2 ) {
    llvm::Value* leftValue = this->loadVariableValue(
        instruction.sourceOperands[0] );
    llvm::Value* rightValue = this->loadVariableValue(
        instruction.sourceOperands[1] );
    // ... ensureString conversion ...
    llvm::Value* lenA = this->irBuilder.CreateCall(
        strlenFunction, { leftStr }, "len.a" );
    llvm::Value* lenB = this->irBuilder.CreateCall(
        strlenFunction, { rightStr }, "len.b" );
    llvm::Value* totalLen = this->irBuilder.CreateAdd(
        lenA, lenB, "total.len" );
    llvm::Value* allocSize = this->irBuilder.CreateAdd(
        totalLen,
        llvm::ConstantInt::get( i64Type, 1 ),
        "alloc.size" );
    llvm::Value* buffer = this->irBuilder.CreateCall(
        mallocFunction, { allocSize }, "str.buf" );
    this->irBuilder.CreateCall(
        strcpyFunction, { buffer, leftStr } );
    this->irBuilder.CreateCall(
        strcatFunction, { buffer, rightStr } );
    this->setVariableValue(
        instruction.destinationVariable, buffer );
    return;
}
```

Both paths produce the same LLVM IR pattern:

```llvm
%len.a = call i64 @strlen(ptr %left)
%len.b = call i64 @strlen(ptr %right)
%total.len = add i64 %len.a, %len.b
%alloc.size = add i64 %total.len, 1
%str.buf = call ptr @malloc(i64 %alloc.size)
call ptr @strcpy(ptr %str.buf, ptr %left)
call ptr @strcat(ptr %str.buf, ptr %right)
```

### Type Coercion — ensureString

Before concatenation, non-string operands are converted to string representation. The `ensureString` lambda (codegen.cpp:4136-4192) handles all LLVM value types:

| LLVM Type | Conversion | Buffer Size | Format |
|---|---|---|---|
| Pointer (string) | Pass through | — | — |
| Pointer (generic param) | Runtime branch: page-threshold heuristic (4096) distinguishes real pointers from `inttoptr`-encoded integers | 24 bytes | `"%ld"` for integer path |
| Double / Float | `snprintf` with `"%.6g"` format | 48 bytes | `"%.6g"` |
| Integer `i1` (boolean) | `CreateSelect` between `"True"` and `"False"` global strings | — | — |
| Integer `iN` (wider) | Sign-extend to i64, `snprintf` with `"%ld"` | 24 bytes | `"%ld"` |

**Generic parameter heuristic**: When the operand is a generic type parameter (`Type::Kind::GenericParameter`), the LLVM value is a pointer that might be either a real string pointer or an integer stored via `inttoptr`. The codegen emits a runtime branch:

```cpp
llvm::Value* ptrAsInt = this->irBuilder.CreatePtrToInt(
    value, i64Type, "es.ptoi" );
llvm::Value* threshold =
    llvm::ConstantInt::get( i64Type, 4096 );
llvm::Value* isRealPtr = this->irBuilder.CreateICmpUGE(
    ptrAsInt, threshold, "es.isptr" );
```

Pointers with numeric value >= 4096 are treated as real string pointers (passed through). Values < 4096 are treated as encoded integers and formatted via `snprintf("%ld")`. The two paths merge at a PHI node.

---

## Memory Model

Every string concatenation **allocates a new heap buffer** via `malloc`. The result is a fresh null-terminated C string. Neither operand is modified — concatenation is non-mutating.

Allocation formula: `malloc(strlen(left) + strlen(right) + 1)`

The `+1` accounts for the null terminator byte. No capacity padding, no growth factor — the buffer is exactly sized for the concatenated result.

**Performance implications**:
- Each `+` operation performs 2 `strlen` calls (O(n) each), 1 `malloc`, 1 `strcpy`, and 1 `strcat`.
- Chained concatenation `a + b + c + d` creates intermediate buffers for each `+` — three allocations, three intermediate strings, of which two become garbage immediately.
- No string builder optimization or temporary elision is performed.

---

## String Representation

Strings in Uranite are null-terminated UTF-8 byte sequences stored as raw `ptr` (opaque pointer) values in LLVM IR:

- **String literals**: emitted as LLVM global string pointers via `CreateGlobalStringPtr`, stored in the `.rodata` section.
- **String variables**: `ConstantString` MIR instruction wraps the literal value, codegen emits `CreateGlobalStringPtr`.
- **Concatenation results**: heap-allocated via `malloc`, mutable (owned by the caller).
- **Type kind**: `Type::Kind::String` in the semantic type system.
- **LLVM type**: `ptr` (opaque pointer to i8 sequence).

The `HIRStringLiteral` node (hir.hpp:545-557) carries the raw `stringValue` string. MIR lowers it to `ConstantString` instruction (mir/lowering.cpp:2278-2286). Codegen expands to `CreateGlobalStringPtr` (mir/codegen.cpp:1891-1898).

---

## Qualname Constants

All string-related method names are centralized in `src/uranite/semantic/qualnames.hpp:245-268`:

```cpp
namespace string {
    static inline const std::string Qualified =
        "uranite.language.string.String";
    static constexpr const char* Package =
        "uranite.language.string";
    static constexpr const char* Name = "String";
    namespace methods {
        static constexpr const char* Length = "length";
        static constexpr const char* Contains = "contains";
        static constexpr const char* Equals = "equals";
        static constexpr const char* ToString = "toString";
        static constexpr const char* CharCodeAt = "charCodeAt";
        static constexpr const char* Substring = "substring";
        static constexpr const char* IsEmpty = "isEmpty";
        static constexpr const char* Concat = "concat";
        static constexpr const char* StartsWith = "startsWith";
        static constexpr const char* EndsWith = "endsWith";
        static constexpr const char* IndexOf = "indexOf";
        static constexpr const char* CharAt = "charAt";
        static constexpr const char* ToUpper = "toUpper";
        static constexpr const char* ToLower = "toLower";
        static constexpr const char* Trim = "trim";
        static constexpr const char* Replace = "replace";
        static constexpr const char* Split = "split";
        static constexpr const char* Format = "format";
    };
};
```

The `Concat` constant (`"concat"`) is the key used by both MIR lowering (to emit the `CallFunction`) and MIR codegen (to intercept and inline-expand it).

---

## Runtime Functions

The codegen uses five C library functions for string concatenation, declared lazily via `getOrCreate*` helpers:

| Helper | C Function | Signature | Purpose |
|---|---|---|---|
| `getOrCreateStrlen` | `strlen` | `i64 (ptr)` | Compute string length |
| `getOrCreateMalloc` | `malloc` | `ptr (i64)` | Allocate result buffer |
| `getOrCreateStrcpy` | `strcpy` | `ptr (ptr, ptr)` | Copy left operand to buffer |
| `getOrCreateStrcat` | `strcat` | `ptr (ptr, ptr)` | Append right operand to buffer |
| `getOrCreateSnprintf` | `snprintf` | `i32 (ptr, i64, ptr, ...)` | Format non-string operands |

Each helper checks `llvmModule->getFunction(name)` first, creating the declaration only if absent. Declared at `src/uranite/ir/mir/codegen.cpp:7121-7129` (strcat example).

---

## Examples

### Basic String Concatenation

```
package examples

from uranite.io.console import puts

public function main() -> I32:
    String greeting = "Hello, " + "world!"
    puts(greeting)
    return 0
```

**Compilation trace**:
1. **Semantic analysis**: `"Hello, " + "world!"` — left operand is `String`, `Plus` operator, matches string concatenation path. Result: `String`.
2. **MIR lowering**: emits `CallFunction("concat", [_const_str_0, _const_str_1]) -> _concat`.
3. **LLVM codegen**: `strlen` both strings, `malloc(14)` (7 + 6 + 1), `strcpy` + `strcat` into buffer.

### Concatenation with Non-String Types

```
package examples

from uranite.io.console import puts

public function main() -> I32:
    I64 count = 42
    F64 ratio = 3.14
    Boolean flag = true

    String message = "count=" + count.toString()
    puts(message)

    return 0
```

**Compilation trace**:
1. **Semantic analysis**: `"count=" + count.toString()` — `toString()` returns `String`, so both operands are `String`. Standard string concat path.
2. If `toString()` were omitted and the right operand were raw `I64`, the codegen `ensureString` would convert it: sign-extend to i64, `snprintf("%ld", value)` into 24-byte malloc buffer, then concatenate.

### Chained Concatenation

```
package examples

from uranite.io.console import puts

public function main() -> I32:
    String first = "Hello"
    String second = ", "
    String third = "world"
    String fourth = "!"

    String result = first + second + third + fourth
    puts(result)

    return 0
```

**Compilation trace**:
1. **Parsing**: left-associative at precedence 9. Parses as `((first + second) + third) + fourth`.
2. **MIR lowering**: three `CallFunction("concat")` instructions:
   - `_concat_0 = concat(first, second)` — "Hello, "
   - `_concat_1 = concat(_concat_0, third)` — "Hello, world"
   - `_concat_2 = concat(_concat_1, fourth)` — "Hello, world!"
3. **LLVM codegen**: three separate `malloc` + `strcpy` + `strcat` sequences. Intermediate buffers `_concat_0` and `_concat_1` become garbage after the next concatenation. No intermediate buffer reuse.

### Generic Parameter Concatenation

```
package examples

from uranite.io.console import puts

public function printLabeled<T>(String label, T value) -> Void:
    String output = label + ": " + value.toString()
    puts(output)

public function main() -> I32:
    printLabeled<I64>("count", 42)
    printLabeled<String>("name", "Alice")
    return 0
```

When `T` is a generic type parameter, the codegen emits the page-threshold heuristic branch for the `value` operand — at runtime, pointers >= 4096 are treated as string pointers (the `String` case), while values < 4096 are formatted as integers via `snprintf("%ld")`. This handles monomorphized generic functions where the concrete type may be either a pointer-based type (String, Object) or an integer packed into a pointer via `inttoptr`.

### String Equality (Related)

String `==` and `!=` follow a similar interception pattern. When `isStringContext` is true:

```cpp
if( isStringContext &&
    ( hirBinaryOp.operatorKind == token::Type::Equal ||
      hirBinaryOp.operatorKind == token::Type::NotEqual ) ) {
    // Emit CallFunction("equals") + optional LogicalNot
}
```

At codegen, `equals` on strings is intercepted and expanded to `strcmp()` with `ICmpEQ(result, 0)`. See [Comparison Operators](comparison-operators.md) for full details.

### Error Case — Non-String Left Operand

```
package examples

public function main() -> I32:
    I64 number = 42
    String result = number + " items"
    return 0
```

**Diagnostic output**:
```
error: invalid operands to binary "+": "I64" and "String"
```

String concatenation requires the **left** operand to be `String`. Reversing the operand order or calling `number.toString() + " items"` resolves this.
