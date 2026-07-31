# Future and Generator Types

Future and Generator are wrapper types that represent deferred computation. `Future<T>` wraps a value that will be available after asynchronous execution completes. `Generator<T>` wraps a lazy sequence of values produced one at a time via `yield`. Both are first-class types in the Uranite type system with dedicated syntax (`async`/`await` for futures, `yield` for generators), distinct compilation pipelines, and specialized LLVM code generation strategies.

---

## Future Types

### Syntax

Async functions must declare their return type as `Future<T>`:

```uranite
async function fetchValue() -> Future<I64>:
    return 42

async function multiply(I64 a, I64 b) -> Future<I64>:
    return a * b
```

The `await` keyword extracts the inner value from a future. Only valid inside `async` functions:

```uranite
async function pipeline(I64 x) -> Future<I64>:
    I64 doubled = await multiply(x, 2)
    I64 base = await fetchValue()
    return doubled + base
```

Async methods work on classes:

```uranite
class DataProcessor:

    public async function load(self, I64 id) -> Future<I64>:
        return id * 10

    public async function transform(self, I64 value) -> Future<I64>:
        I64 loaded = await self.load(value)
        return loaded + 1
```

Async main:

```uranite
public async function main() -> Future<I32>:
    I64 val = await fetchValue()
    return 0
```

### Type System Representation

**Defined in**: `src/uranite/semantic/typeref.hpp:1077-1091`

```cpp
struct FutureType : Type {
    TypeSharedPointer innerType;

    FutureType( TypeSharedPointer inner )
        : Type( Type::Kind::Future,
                fmt::format( "Future<{}>", inner->name ) ),
          innerType( std::move( inner ) ) {}
};
```

| Field | Type | Description |
|---|---|---|
| `innerType` | `TypeSharedPointer` | Type of value produced when future completes |
| `name` | `string` (computed) | Formatted as `"Future<T>"` |
| `kind` | `Type::Kind` (inherited) | Always `Kind::Future` |

Factory method:

```cpp
TypeSharedPointer Registry::makeFuture( TypeSharedPointer inner ) {
    return std::make_shared<FutureType>( std::move( inner ) );
}
```

### AST Representation

**AwaitExpression** (`src/uranite/ast/node.hpp:1175-1192`):

```cpp
struct AwaitExpression : Expression {
    ExpressionSharedPointer operand;

    AwaitExpression(
        ExpressionSharedPointer operand,
        const lookup::SourceSharedPointer& source
    ) : Expression( Node::Kind::AwaitExpression, source ),
        operand( std::move( operand ) ) {}
};
```

The `FunctionDeclaration` node carries `isAsync` flag (`node.hpp:1070`). Lambda expressions also carry `isAsync` (`node.hpp:1543`).

### Parsing

**Async function declaration** (`parser.cpp:742-787`):

1. Match `async` keyword before `function`
2. Set `isAsync = true` on the `FunctionDeclaration` node
3. Parse function body normally

**Async lambda** (`parser.cpp:2215-2223`):

1. Match `async` keyword followed by `lambda` or `function`
2. Set `isAsync = true` on the lambda expression

**Await expression** (`parser.cpp:2225-2228`):

1. Match `await` keyword
2. Parse operand expression
3. Return `AwaitExpression(operand)`

**Grammar**:
```
AsyncFunction  ::= "async" FunctionDeclaration
AsyncLambda    ::= "async" ("lambda" | "function") LambdaBody
AwaitExpr      ::= "await" Expression
```

### Semantic Analysis

**Async function validation** (`analyzer.cpp:879-882`, `2828-2829`):

During function analysis, the compiler enforces that async functions declare `Future<T>` return types:

1. If `isAsync` is true and return type is not `Kind::Future`:
   - Emit error: `"async function \"X\" must declare return type as \"Future<T>\""`
2. If return type is `Future<T>`, the function type in the registry wraps the full `Future<T>` as return type

**Await expression analysis** (`analyzer.cpp:2106-2123`):

1. Check `isInsideAsyncFunction` flag. If false, emit error: `"\"await\" can only be used inside an async function"`
2. Analyze operand expression to get operand type
3. If operand type is `Kind::Future`:
   - Extract `innerType` from `FutureType` — this becomes the expression result type
   - `await fetchValue()` where `fetchValue` returns `Future<I64>` produces type `I64`
4. If operand type is not `Future`:
   - Emit error: `"\"await\" requires a \"Future<T>\" operand, got \"X\""`

**Special-case type resolution** (`analyzer.cpp:4796-4798`):

When resolving `Future<T>` as a generic type node, the analyzer special-cases it rather than going through general monomorphization:

```cpp
if( genericTypeNode.name == qualname::classes::future::Name
    && genericTypeNode.typeArguments.size() == 1 ) {
    TypeSharedPointer innerType = this->resolveType(
        genericTypeNode.typeArguments[0] );
    return innerType ? this->typeRegistry.makeFuture( innerType )
                     : this->typeRegistry.getError();
}
```

This bypasses the full monomorphization pipeline (no cache key, no substitution map) since `Future` is a built-in wrapper, not a user-defined generic class.

### Generic Substitution

When `Future<T>` appears inside a generic type being monomorphized, the substitution lambda recurses into the inner type:

**Defined in**: `analyzer.cpp:4857-4858`

```cpp
if( targetType->kind == Type::Kind::Future ) {
    return this->typeRegistry.makeFuture(
        substituteType(
            std::static_pointer_cast<FutureType>( targetType )->innerType,
            substitutionMap ) );
}
```

Similarly in `substituteGenericParameters` (`analyzer.cpp:5376-5377`).

### HIR Representation

**HIRAwait** (`hir.hpp:855-867`):

```cpp
struct HIRAwait : HIRNode {
    HIRNodeSharedPointer awaitedExpression;

    HIRAwait(
        HIRNodeSharedPointer awaitedExpression,
        semantic::TypeSharedPointer resolvedType,
        const lookup::SourceSharedPointer& sourceLocation
    ) : HIRNode( HIRNodeKind::Await, resolvedType, sourceLocation ),
        awaitedExpression( std::move( awaitedExpression ) ) {}
};
```

HIR lowering sets `isAsyncFunction` on `HIRFunctionDefinition` (`lowering.cpp:260`).

### Code Generation — Async Functions

Async functions use a **spawner + wrapper** pattern. The original function becomes a spawner that stores arguments to globals and spawns a task. A separate wrapper function contains the actual body.

**Defined in**: `src/uranite/ir/mir/codegen.cpp:7648-7879`

**Architecture**:

```
async function multiply(I64 a, I64 b) -> Future<I64>
    │
    ├── multiply()           [spawner — stores args, spawns task, returns task ID]
    │     ├── store a → global "multiply.arg.a"
    │     ├── store b → global "multiply.arg.b"
    │     └── call uraniteSpawnTask(multiply.async, null) → return task ID
    │
    └── multiply.async(ptr task)   [wrapper — actual body]
          ├── load a ← global "multiply.arg.a"
          ├── load b ← global "multiply.arg.b"
          ├── __uranite_push_frame(...)
          ├── [function body instructions]
          ├── call uraniteTaskComplete(task, result)
          ├── __uranite_pop_frame()
          └── ret void
```

**Spawner function** (`codegen.cpp:7819-7876`):

1. Create entry block in original LLVM function
2. For each parameter: store argument value to a global variable (`{funcName}.arg.{paramName}`)
3. Call `runtimeSpawn` or `uraniteSpawnTask` with wrapper function pointer
4. Return task ID (i64 value)

**Wrapper function** (`codegen.cpp:7688-7812`):

1. Create internal `{funcName}.async` function taking `ptr task` parameter
2. Load each parameter from globals into local allocas
3. Emit `__uranite_push_frame` for shadow stack
4. Generate all basic blocks from the MIR function body
5. On return: call `runtimeComplete` or `uraniteTaskComplete(task, retValue)` with result converted to i64
6. Set up catch block for async exceptions: calls `uraniteTaskError(task, exception)`
7. Emit `__uranite_pop_frame` and `ret void`

**Return value handling in async wrapper** (`codegen.cpp:5519-5543`):

When a return instruction is reached inside an async wrapper:

1. Load return value
2. Convert to i64: integer cast, `PtrToInt` for pointers, `FPToSI` for floats
3. Call `runtimeComplete(value)` or `uraniteTaskComplete(task, value)`
4. Pop frame, return void

**Scheduler initialization** (`codegen.cpp:1018-1028`):

If any function in the program is async, main calls `uraniteSchedulerInit()` at startup before executing its body.

**Async runtime auto-import** (`compiler/driver.cpp:977-981`):

The compiler driver automatically imports `stdlibs/async/runtime.urn` when async functions are detected, providing the runtime scheduler implementation.

### LLVM Type Mapping

```cpp
case semantic::Type::Kind::Future:
    return llvm::Type::getInt64Ty( this->llvmContext );
```

`Future<T>` maps to `i64` — the task ID returned by the spawner. The actual value is communicated through the runtime completion mechanism, not through the return value directly.

---

## Generator Types

### Syntax

Generator functions use `yield` to produce values lazily and must declare `Generator<T>` return type:

```uranite
function fibonacci() -> Generator<I64>:
    I64 current = 0
    I64 next = 1
    while true:
        yield current
        I64 temp = current
        current = next
        next = temp + next

function countdown(I64 start) -> Generator<I64>:
    I64 index = start
    while index > 0:
        yield index
        index = index - 1
```

Generators are iterable — they work directly in `for-in` loops:

```uranite
for I64 value in fibonacci():
    if value > 100:
        break
    puts(value)
```

### Type System Representation

**Defined in**: `src/uranite/semantic/typeref.hpp:1099-1113`

```cpp
struct GeneratorType : Type {
    TypeSharedPointer yieldType;

    GeneratorType( TypeSharedPointer yieldType )
        : Type( Type::Kind::Generator,
                fmt::format( "Generator<{}>", yieldType->name ) ),
          yieldType( std::move( yieldType ) ) {}
};
```

| Field | Type | Description |
|---|---|---|
| `yieldType` | `TypeSharedPointer` | Type of values produced by each `yield` |
| `name` | `string` (computed) | Formatted as `"Generator<T>"` |
| `kind` | `Type::Kind` (inherited) | Always `Kind::Generator` |

Factory method:

```cpp
TypeSharedPointer Registry::makeGenerator( TypeSharedPointer yieldType ) {
    return std::make_shared<GeneratorType>( std::move( yieldType ) );
}
```

### AST Representation

**YieldExpression** (`src/uranite/ast/node.hpp:1922-1939`):

```cpp
struct YieldExpression : Expression {
    ExpressionSharedPointer value;

    YieldExpression(
        ExpressionSharedPointer value,
        const lookup::SourceSharedPointer& source
    ) : Expression( Node::Kind::YieldExpression, source ),
        value( std::move( value ) ) {}
};
```

The `FunctionDeclaration` node carries `isGenerator` flag (`node.hpp:1079`).

### Parsing

**Yield expression** (`parser.cpp:2360-2363`):

1. Match `yield` keyword
2. Parse value expression
3. Return `YieldExpression(value)`

**Grammar**:
```
YieldExpr  ::= "yield" Expression
```

The `isGenerator` flag is not parsed directly — it is inferred during semantic analysis based on the presence of `yield` in the function body or a `Generator<T>` return type.

### Semantic Analysis

**Generator detection** (`analyzer.cpp:2752-2767`):

The analyzer scans the function body for `yield` expressions using a recursive `statementContainsYield` helper (`analyzer.cpp:30-46`):

1. Walk all statements in the function body
2. If any `YieldExpression` is found, set `hasYieldExpression = true`
3. Check if declared return type is `Generator<T>` (`Kind::Generator`)
4. If `yield` present but return type is not `Generator<T>`:
   - Emit error: `"function \"X\" contains \"yield\" but does not declare return type \"Generator<T>\"; use \"-> Generator<T>\" instead of \"-> T\""`
5. If return type is `Generator<T>`:
   - Set `declaration.isGenerator = true`

**Return type auto-wrapping** (`analyzer.cpp:883-884`):

During pre-registration, if `isGenerator` is true but the return type is not already `Generator<T>`, the return type is automatically wrapped:

```cpp
if( functionDeclaration.isGenerator
    && returnType->kind != Type::Kind::Generator ) {
    returnType = this->typeRegistry.makeGenerator( returnType );
}
```

**Yield expression analysis** (`analyzer.cpp:2443-2446`):

Simple — analyze the yielded value expression and return its type:

```cpp
case ast::Node::Kind::YieldExpression: {
    YieldExpression& yieldExpression = ...;
    expressionType = yieldExpression.value
        ? this->analyzeExpression( yieldExpression.value )
        : this->typeRegistry.getVoid();
    break;
}
```

**Generator iteration type inference** (`analyzer.cpp:2603-2604`, `2164-2165`):

When a `Generator<T>` appears as iterable in a `for-in` loop or comprehension, the loop variable type is extracted from `yieldType`:

```cpp
else if( iterableType->kind == Type::Kind::Generator ) {
    variableType = std::static_pointer_cast<GeneratorType>(
        iterableType )->yieldType;
}
```

**Iterable validation** (`analyzer.cpp:2580-2591`):

`Generator<T>` is explicitly allowed as iterable — the check skips generators alongside arrays, strings, and types implementing `Iterator`/`Iterable` interfaces.

**Special-case type resolution** (`analyzer.cpp:4792-4794`):

Like `Future`, `Generator<T>` bypasses general monomorphization:

```cpp
if( genericTypeNode.name == qualname::classes::generator::Name
    && genericTypeNode.typeArguments.size() == 1 ) {
    TypeSharedPointer innerType = this->resolveType(
        genericTypeNode.typeArguments[0] );
    return innerType ? this->typeRegistry.makeGenerator( innerType )
                     : this->typeRegistry.getError();
}
```

### Generic Substitution

When `Generator<T>` appears inside a generic type being monomorphized:

```cpp
if( targetType->kind == Type::Kind::Generator ) {
    return this->typeRegistry.makeGenerator(
        substituteType(
            std::static_pointer_cast<GeneratorType>( targetType )->yieldType,
            substitutionMap ) );
}
```

### HIR Representation

**HIRYield** (`hir.hpp:870-882`):

```cpp
struct HIRYield : HIRNode {
    HIRNodeSharedPointer yieldedExpression;

    HIRYield(
        HIRNodeSharedPointer yieldedExpression,
        semantic::TypeSharedPointer resolvedType,
        const lookup::SourceSharedPointer& sourceLocation
    ) : HIRNode( HIRNodeKind::Yield, resolvedType, sourceLocation ),
        yieldedExpression( std::move( yieldedExpression ) ) {}
};
```

HIR lowering sets `isGeneratorFunction` on `HIRFunctionDefinition` (`lowering.cpp:265`).

### MIR Lowering

**Yield type extraction** (`mir/lowering.cpp:353-368`):

During MIR function setup, the yield type is extracted from the return type:

1. If return type is `Kind::Generator`: extract `genType->yieldType`
2. Fallback: parse type name string for `<` and `>` brackets to extract element type
3. Store as `mirFunction->generatorYieldType`

**Yield instruction** (`mir/lowering.cpp:1166-1173`):

HIR yield lowers to `MIRInstructionKind::Yield`:

```cpp
case hir::HIRNodeKind::Yield: {
    HIRYield& yieldNode = ...;
    MIRInstruction yieldInstruction( MIRInstructionKind::Yield );
    if( yieldNode.yieldedExpression != nullptr ) {
        MIRVariableIdentifier yieldedValue =
            this->lowerExpression( yieldNode.yieldedExpression );
        yieldInstruction.sourceOperands.push_back( yieldedValue );
    }
    this->emitInstruction( yieldInstruction );
}
```

### Code Generation — Generator State Machine

Generator functions compile to a **state machine** backed by a heap-allocated struct. The original function becomes an initializer that allocates the state and runs the first step. A separate `.next` function resumes execution from where the last `yield` left off.

**Defined in**: `src/uranite/ir/mir/codegen.cpp:7407-7646`

**Generator struct layout**:

```
__gen_{funcName} {
    i32  state       [field 0 — current resume point]
    T    value       [field 1 — last yielded value]
    i1   done        [field 2 — true when generator exhausted]
    ...  params      [field 3+ — captured parameters]
}
```

Created as an LLVM `StructType` (`codegen.cpp:7440-7445`):

```cpp
std::vector<llvm::Type*> structFields = { i32Type, yieldType, i1Type };
for( llvm::Type* paramType : paramLLVMTypes ) {
    structFields.push_back( paramType );
}
llvm::StructType* genStructType = llvm::StructType::create(
    ctx, structFields, structName );
```

**Architecture**:

```
function fibonacci() -> Generator<I64>
    │
    ├── fibonacci()              [initializer — alloc struct, run first step]
    │     ├── calloc(1, sizeof(__gen_fibonacci))
    │     ├── store state=0, done=false
    │     ├── call fibonacci.next(gen.ptr)
    │     └── return gen.ptr
    │
    └── fibonacci.next(ptr gen)  [state machine — resumes from yield points]
          ├── load state from gen
          ├── switch(state):
          │     case 0 → body.start    [initial execution]
          │     case 1 → restore.1     [resume after first yield]
          │     case 2 → restore.2     [resume after second yield]
          │     default → done         [generator exhausted]
          ├── [function body with yield points]
          ├── exit:
          │     save locals → globals
          │     store state, value, done → gen struct
          │     ret void
          └── done:
                store done=true → gen struct
                ret void
```

**Initializer function** (`codegen.cpp:7611-7645`):

1. Allocate generator struct via `calloc(1, sizeof(struct))`
2. Initialize `state = 0`, `done = false`
3. Store function parameters into struct fields at offset 3+
4. Call `.next(genPtr)` to execute until first yield
5. Return `genPtr` as the generator handle

**Next function** (`codegen.cpp:7447-7608`):

1. Create internal `{funcName}.next` function with signature `void(ptr)`
2. Load state from struct field 0
3. Create dispatch `switch` on state value:
   - `case 0` → jump to body start (initial execution)
   - `case N` → jump to `restore.N` block (resume after Nth yield)
   - `default` → jump to done block
4. Generate function body blocks
5. Unterminated blocks get `done = true; br exit`

**Yield code generation** (`codegen.cpp:7372-7405`):

Each `yield` statement:

1. Load yielded value, cast to yield type if needed
2. Store yielded value into `genCtx.valueVar`
3. Store `done = false`
4. Assign next state ID: `genCtx.nextStateId++`
5. Store state ID into `genCtx.stateVar`
6. Branch to exit block
7. Create `resume.{stateId}` block — execution continues here on next `.next()` call

**Local variable persistence** (`codegen.cpp:7526-7547`):

Local variables that need to survive across yields are persisted to global variables:

1. Collect all non-parameter `AllocaInst` variables
2. For each: create `llvm::GlobalVariable` named `{funcName}.local.v{id}`
3. At exit block: save all locals to globals
4. At each `restore.N` block: reload all locals from globals, reload parameters from struct

**Return in generator context** (`codegen.cpp:5511-5517`):

When a return statement is reached inside a generator:

```cpp
if( this->currentGeneratorContext != nullptr ) {
    this->irBuilder.CreateStore(
        llvm::ConstantInt::getTrue( this->llvmContext ),
        this->currentGeneratorContext->doneVar );
    this->irBuilder.CreateBr( this->currentGeneratorContext->exitBlock );
    return;
}
```

Sets `done = true` and branches to exit — no more values will be produced.

### LLVM Type Mapping

```cpp
case semantic::Type::Kind::Generator:
    return llvm::PointerType::getUnqual( this->llvmContext );
```

`Generator<T>` maps to an opaque pointer — the pointer to the heap-allocated generator state struct.

---

## For-In Loop Iteration

Both `Future` and `Generator` types participate in for-in loops differently:

| Type | For-In Behavior |
|---|---|
| `Generator<T>` | Directly iterable. Loop variable type = `yieldType`. Each iteration calls `.next()` on the generator struct, reads value, checks done flag. |
| `Future<T>` | Not iterable. Cannot be used in for-in loops. |

Generator iteration in MIR lowering (`mir/lowering.cpp:1478-1507`) extracts the yield type from the `GeneratorType` to determine loop variable types, supporting both single-variable and dual-variable destructuring patterns.

---

## Comparison

| Aspect | `Future<T>` | `Generator<T>` |
|---|---|---|
| **Purpose** | Async computation returning one value | Lazy sequence producing many values |
| **Keyword** | `async` (declaration), `await` (consumption) | `yield` (production) |
| **Return type enforcement** | Must declare `Future<T>` explicitly | Must declare `Generator<T>` explicitly; auto-wrapped if `yield` detected |
| **LLVM representation** | `i64` (task ID) | `ptr` (pointer to state struct) |
| **Code generation strategy** | Spawner + async wrapper with task runtime | State machine with dispatch switch |
| **Runtime dependency** | `stdlibs/async/runtime.urn` (auto-imported), C runtime `async-runtime` | No runtime dependency — pure state machine |
| **Iteration** | Not iterable | Directly iterable in for-in loops |
| **Exception handling** | Async catch block with `uraniteTaskError` | Standard exception handling |
| **State persistence** | Arguments stored in globals | Locals persisted to globals across yields |
| **Monomorphization** | Bypassed — built-in factory `makeFuture` | Bypassed — built-in factory `makeGenerator` |

---

## Practical Examples

### Async Pipeline with Error Handling

```uranite
package examples.async

from uranite.io.console import puts

async function fetchValue() -> Future<I64>:
    return 42

async function multiply(I64 a, I64 b) -> Future<I64>:
    return a * b

async function safeDivide(I64 a, I64 b) -> Future<I64>:
    if b is 0:
        raise new ArithmeticError("division by zero")
    return a / b

public async function main() -> Future<I32>:
    I64 val = await fetchValue()
    puts("fetched: ", val)

    I64 product = await multiply(6, 7)
    puts("product: ", product)

    try:
        I64 good = await safeDivide(100, 5)
        puts("100/5 = ", good)
        I64 bad = await safeDivide(10, 0)
    catch ArithmeticError error:
        puts("caught async division error")

    return 0
```

**Compilation trace**:
- `fetchValue` declared async with `Future<I64>` return → validated
- `await fetchValue()` inside async main:
  - Operand type `Future<I64>` → extract `innerType` = `I64`
  - Expression result type = `I64`
- Codegen: `fetchValue` split into spawner (stores no args, calls `uraniteSpawnTask`) and wrapper (returns 42 via `uraniteTaskComplete`)
- `main` calls `uraniteSchedulerInit()` at entry since program has async functions
- `safeDivide` wrapper has catch block: `uraniteTaskError(task, exception)`

### Generator Sequence

```uranite
package examples.generator

from uranite.io.console import puts

function countdown(I64 start) -> Generator<I64>:
    I64 index = start
    while index > 0:
        yield index
        index = index - 1

public function main() -> I32:
    for I64 value in countdown(5):
        puts(value)
    return 0
```

**Compilation trace**:
- `countdown` body contains `yield` → analyzer sets `isGenerator = true`
- Return type `Generator<I64>` validated as `Kind::Generator`
- `generatorYieldType` set to `I64`
- Codegen generates `__gen_countdown` struct: `{i32 state, i64 value, i1 done, i64 start}`
- `countdown()` allocates struct, stores `start` parameter at field 3, calls `.next()`, returns pointer
- `countdown.next(ptr gen)`: state 0 enters body, each `yield index` stores value and increments state
- For-in loop calls `.next()` repeatedly, reads value field, checks done flag

### Nested Await

```uranite
async function fetchValue() -> Future<I64>:
    return 42

async function multiply(I64 a, I64 b) -> Future<I64>:
    return a * b

public async function main() -> Future<I32>:
    I64 nested = await multiply(await fetchValue(), 3)
    puts("nested: ", nested)
    return 0
```

**Compilation trace**:
- Inner `await fetchValue()` resolves first — produces `I64`
- Result passed as argument to `multiply`
- Outer `await multiply(...)` resolves — produces `I64`
- Each async call spawns a task; await reads completed value

---

## Compilation Pipeline Summary

| Stage | Future Handling | Generator Handling |
|---|---|---|
| **Lexer** | `async`, `await` keywords | `yield` keyword |
| **Parser** | Sets `isAsync` flag; creates `AwaitExpression` | Creates `YieldExpression`; `isGenerator` inferred later |
| **Semantic Analysis** | Validates async return type is `Future<T>`; validates `await` inside async; extracts inner type from `Future<T>` on await | Detects `yield` in body; validates `Generator<T>` return type; extracts `yieldType` for loop variable inference |
| **Type Resolution** | Special-case bypass: `makeFuture(innerType)` | Special-case bypass: `makeGenerator(yieldType)` |
| **HIR Lowering** | Creates `HIRAwait`; preserves `isAsyncFunction` flag | Creates `HIRYield`; preserves `isGeneratorFunction` flag |
| **MIR Lowering** | Standard expression lowering | `Yield` instruction; extracts `generatorYieldType` |
| **MIR Codegen** | Spawner + wrapper function; task spawn/complete runtime calls; scheduler init in main | State machine struct; `.next()` function with dispatch switch; local persistence to globals |
| **LLVM Type** | `i64` (task ID) | `ptr` (opaque pointer to state struct) |
