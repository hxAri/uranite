# Await Expressions

Uranite uses `await` to suspend execution until an asynchronous operation completes, extracting the inner value from a `Future<T>`. `await` is only valid inside `async function` bodies — semantic analysis enforces this via the `isInsideAsyncFunction` flag and emits an error otherwise. The expression type unwraps `Future<T>` to `T`. At MIR level, `await` becomes a `CallFunction` to the synthetic `__runtime_await` target. LLVM codegen intercepts this name and generates a multi-block sequence: call `uraniteAwaitTask` to block until completion, check for errors via `uraniteTaskHasError`, and branch to either a success path (returning the task result) or an error path (constructing an `Exception` and throwing via `__uranite_throw`).

---

## Table of Contents

- [Syntax](#syntax)
- [Async Function Context](#async-function-context)
- [AST Representation — AwaitExpression](#ast-representation--awaitexpression)
- [Parsing](#parsing)
- [Semantic Analysis](#semantic-analysis)
  - [Async Context Validation](#async-context-validation)
  - [Future Type Unwrapping](#future-type-unwrapping)
- [HIR Representation — HIRAwait](#hir-representation--hirawait)
- [HIR Lowering](#hir-lowering)
- [HIR Validation](#hir-validation)
- [MIR Lowering](#mir-lowering)
- [LLVM Code Generation](#llvm-code-generation)
  - [Task ID Coercion](#task-id-coercion)
  - [Calling uraniteAwaitTask](#calling-uraniteawaittask)
  - [Error Checking](#error-checking)
  - [Error Path — Exception Construction and Throw](#error-path--exception-construction-and-throw)
  - [Success Path](#success-path)
- [Async Function Generation](#async-function-generation)
  - [Spawner Function](#spawner-function)
  - [Wrapper Function](#wrapper-function)
  - [Argument Passing via Globals](#argument-passing-via-globals)
  - [Error Handling in Wrapper](#error-handling-in-wrapper)
  - [Scheduler Initialization](#scheduler-initialization)
  - [Async Runtime Auto-Import](#async-runtime-auto-import)
- [FutureType](#futuretype)
- [Examples](#examples)

---

## Syntax

```
await asyncExpression
```

| Component | Description |
|---|---|
| `await` | Keyword that suspends until the future resolves |
| `asyncExpression` | Expression producing a `Future<T>` value |

```
package examples

public async function fetchValue() -> I64:
    return 42

public async function compute() -> I64:
    I64 value = await fetchValue()
    return value * 2
```

---

## Async Function Context

Async functions are declared with the `async` keyword before `function`. Semantic analysis tracks async context via `isInsideAsyncFunction`:

At `src/uranite/semantic/analyzer.cpp:2867-2870`:

```cpp
bool savedAsyncStatus = this->isInsideAsyncFunction;
// ...
this->isInsideAsyncFunction = declaration.isAsync;
```

The flag is saved and restored around each function body analysis. Functions declared `async` automatically wrap their return type in `Future<T>` if not already a `Future`:

At `src/uranite/semantic/analyzer.cpp:2828`:

```cpp
if( declaration.isAsync &&
    returnType->kind != Type::Kind::Future ) {
```

---

## AST Representation — AwaitExpression

Defined at `src/uranite/ast/node.hpp:1175-1192`:

```cpp
struct AwaitExpression : Expression {

    ExpressionSharedPointer operand;

    AwaitExpression(
        ExpressionSharedPointer operand,
        const lookup::SourceSharedPointer& source
    ) : Expression( Node::Kind::AwaitExpression, source ),
        operand( std::move( operand ) ) {
    }

};
```

| Field | Type | Description |
|---|---|---|
| `operand` | `ExpressionSharedPointer` | Expression producing the `Future<T>` value |

---

## Parsing

At `src/uranite/parser/parser.cpp:2225-2228`:

```cpp
case token::Type::KeywordAwait: {
    lookup::SourceSharedPointer source =
        this->current().source;
    this->advance();
    return std::make_shared<ast::nodes::AwaitExpression>(
        std::move( this->parseExpression() ), source );
}
```

`await` is a prefix unary operator in expression context. It consumes the keyword, then parses the following expression as its operand. No parentheses required — `await fetchValue()` parses `fetchValue()` as a call expression, then wraps it in `AwaitExpression`.

Since `parseExpression()` is called without a precedence limit, `await` binds loosely:
- `await x + y` parses as `await (x + y)`.
- `await fetchValue()` parses as `await (fetchValue())`.

---

## Semantic Analysis

At `src/uranite/semantic/analyzer.cpp:2106-2123`:

```cpp
case ast::Node::Kind::AwaitExpression: {
    ast::nodes::AwaitExpression& awaitExpression =
        static_cast<ast::nodes::AwaitExpression&>(
            *expression );
    if( this->isInsideAsyncFunction == false ) {
        this->diagnostic.error( expression->source,
            "\"await\" can only be used inside "
            "an async function" );
    }
    TypeSharedPointer operandType =
        this->analyzeExpression( awaitExpression.operand );
    if( operandType &&
        operandType->kind == Type::Kind::Future ) {
        expressionType =
            std::static_pointer_cast<FutureType>(
                operandType )->innerType;
    }
    else if( operandType ) {
        std::string awaitErrorMessage = fmt::format(
            "\"await\" requires a \"Future<T>\" operand, "
            "got \"{}\"", operandType->name );
        this->diagnostic.error( expression->source,
            awaitErrorMessage );
        expressionType = operandType;
    }
    else {
        expressionType = this->typeRegistry.getError();
    }
    break;
}
```

### Async Context Validation

First check: `isInsideAsyncFunction == false` emits error `"await" can only be used inside an async function`. Analysis continues even on error — allows further type checking for better diagnostics.

### Future Type Unwrapping

Three cases after analyzing the operand:

| Operand Type | Result Type | Behavior |
|---|---|---|
| `Future<T>` | `T` | Unwraps inner type from `FutureType` |
| Non-Future type | Operand type (with error) | Emits error, uses operand type as fallback |
| `nullptr` | `Error` | Operand analysis failed, fall back to error type |

`await fetchValue()` where `fetchValue` returns `Future<I64>`: operand type is `Future<I64>`, expression type unwraps to `I64`.

---

## HIR Representation — HIRAwait

Defined at `src/uranite/ir/hir.hpp:854-867`:

```cpp
struct HIRAwait : HIRNode {

    HIRNodeSharedPointer awaitedExpression;

    HIRAwait(
        HIRNodeSharedPointer awaitedExpression,
        semantic::TypeSharedPointer resolvedType,
        const lookup::SourceSharedPointer& sourceLocation
    ) : HIRNode( HIRNodeKind::Await,
            std::move( resolvedType ), sourceLocation ),
        awaitedExpression(
            std::move( awaitedExpression ) ) {
    }

};
```

| Field | Type | Description |
|---|---|---|
| `awaitedExpression` | `HIRNodeSharedPointer` | Lowered operand producing the future/task ID |

`resolvedType` carries the unwrapped `T` from `Future<T>`.

---

## HIR Lowering

At `src/uranite/ir/hir/lowering.cpp:1040-1043`:

```cpp
case ast::Node::Kind::AwaitExpression: {
    ast::nodes::AwaitExpression& awaitExpression =
        static_cast<ast::nodes::AwaitExpression&>(
            *expression );
    HIRNodeSharedPointer awaitedExpression =
        this->lowerExpression( awaitExpression.operand );
    return std::make_shared<HIRAwait>(
        std::move( awaitedExpression ),
        expression->semanticType, expression->source );
}
```

Straightforward: lowers operand expression, passes semantic type (unwrapped `T`) to HIR node.

---

## HIR Validation

At `src/uranite/ir/hir/validator.cpp:515`:

```cpp
case HIRNodeKind::Await:
```

Falls through to shared `break` with other leaf nodes. No structural validation performed.

---

## MIR Lowering

At `src/uranite/ir/mir/lowering.cpp:2789-2809`:

```cpp
case hir::HIRNodeKind::Await: {
    hir::HIRAwait& awaitNode =
        static_cast<hir::HIRAwait&>( *hirExpression );
    MIRVariableIdentifier awaitedValue =
        this->lowerExpression( awaitNode.awaitedExpression );
    MIRInstruction awaitCall(
        MIRInstructionKind::CallFunction );
    awaitCall.calledFunctionQualifiedName =
        "__runtime_await";
    awaitCall.sourceOperands.push_back( awaitedValue );
    semantic::TypeSharedPointer resultType =
        awaitNode.resolvedType;
    if( resultType == nullptr ) {
        resultType = std::make_shared<semantic::Type>(
            semantic::Type::Kind::Integer,
            semantic::qualname::classes::i64::Name );
    }
    MIRVariableIdentifier resultVariable =
        this->currentFunction->allocateVariable(
            "await.result", resultType, false );
    awaitCall.destinationVariable = resultVariable;
    awaitCall.operandType = resultType;
    awaitCall.sourceLocation = awaitNode.sourceLocation;
    if( this->activeLandingPad !=
            INVALID_BLOCK_IDENTIFIER ) {
        awaitCall.landingPadTarget = this->activeLandingPad;
    }
    this->emitInstruction( awaitCall );
    return resultVariable;
}
```

MIR lowering converts `await` to a `CallFunction` instruction with the synthetic name `__runtime_await`. Key details:

| MIR Field | Value | Description |
|---|---|---|
| `calledFunctionQualifiedName` | `"__runtime_await"` | Sentinel name intercepted by codegen |
| `sourceOperands[0]` | Awaited value | Task ID from the async function call |
| `operandType` | Unwrapped result type | `T` from `Future<T>` |
| `destinationVariable` | `await.result` | Receives the resolved value |
| `landingPadTarget` | Active landing pad (if in try block) | Exception handling integration |

If `resolvedType` is null, falls back to `I64`.

---

## LLVM Code Generation

Codegen intercepts `__runtime_await` at `src/uranite/ir/mir/codegen.cpp:2582-2709` and generates a multi-block CFG sequence.

### Task ID Coercion

```cpp
llvm::Value* taskIdValue =
    this->loadVariableValue(
        instruction.sourceOperands[0] );
llvm::Type* i64Type =
    llvm::Type::getInt64Ty( this->llvmContext );
if( taskIdValue->getType() != i64Type ) {
    if( taskIdValue->getType()->isPointerTy() ) {
        taskIdValue = this->irBuilder.CreatePtrToInt(
            taskIdValue, i64Type, "await.tid" );
    }
    else if( taskIdValue->getType()->isIntegerTy() ) {
        taskIdValue = this->irBuilder.CreateIntCast(
            taskIdValue, i64Type, true, "await.tid" );
    }
}
```

Task IDs are `i64` integers. If the awaited value comes as a pointer (from the spawner function return), it is converted via `PtrToInt`. Integer width mismatches are handled via `IntCast`.

The task ID is stored to a stack allocation (`await.taskid`) because it is needed multiple times (await call + error check + error retrieval).

### Calling uraniteAwaitTask

```cpp
llvm::Function* awaitFunc =
    this->llvmModule->getFunction( "runtimeAwait" );
if( awaitFunc == nullptr ) {
    awaitFunc = this->llvmModule->getFunction(
        "uraniteAwaitTask" );
    if( awaitFunc == nullptr ) {
        llvm::FunctionType* awaitSig =
            llvm::FunctionType::get(
                i64Type, { i64Type }, false );
        awaitFunc = llvm::Function::Create(
            awaitSig, llvm::Function::ExternalLinkage,
            "uraniteAwaitTask", this->llvmModule.get() );
    }
}
llvm::Value* awaitResult =
    this->irBuilder.CreateCall(
        awaitFunc, { taskIdValue }, "await.result" );
```

Function resolution order:
1. `runtimeAwait` — newer runtime API.
2. `uraniteAwaitTask` — legacy runtime API.
3. Auto-declare `uraniteAwaitTask(i64) -> i64` as external if neither exists.

`uraniteAwaitTask` blocks the calling task until the awaited task completes, returning the task result as `i64`.

### Error Checking

```cpp
llvm::Function* hasErrorFunc =
    this->llvmModule->getFunction(
        "runtimeTaskHasError" );
// ... or uraniteTaskHasError

llvm::Value* taskIdReload =
    this->irBuilder.CreateLoad(
        i64Type, taskIdAlloca, "await.tid.reload" );
llvm::Value* hasErrorResult =
    this->irBuilder.CreateCall(
        hasErrorFunc, { taskIdReload }, "await.haserr" );

llvm::BasicBlock* awaitOkBlock =
    llvm::BasicBlock::Create(
        this->llvmContext, "await.ok", currentFunc );
llvm::BasicBlock* awaitErrBlock =
    llvm::BasicBlock::Create(
        this->llvmContext, "await.err", currentFunc );
this->irBuilder.CreateCondBr(
    isError, awaitErrBlock, awaitOkBlock );
```

After the await call returns, codegen checks whether the completed task produced an error. Two basic blocks are created:
- `await.ok` — task completed successfully.
- `await.err` — task produced an error.

`uraniteTaskHasError(taskId)` returns non-zero if the task failed. The `CondBr` branches to the appropriate block.

Two return type variants are handled: `runtimeTaskHasError` returns `i1` (boolean), `uraniteTaskHasError` returns `i32` (compared against 0 via `ICmpNE`).

### Error Path — Exception Construction and Throw

In the `await.err` block:

```cpp
llvm::Value* errorMsg = this->irBuilder.CreateCall(
    getErrorFunc, { taskIdReload2 }, "await.errmsg" );
```

1. Call `uraniteTaskGetError(taskId)` to retrieve the error message pointer.
2. Allocate memory for an `Exception` struct via `malloc`.
3. Call the `Exception.Exception` constructor with the error message.
4. Call `__uranite_throw(exceptionPtr, "Exception")` to throw.

If a landing pad is active (inside a `try` block), the throw uses `invoke` with the landing pad as the unwind target:

```cpp
if( unwindTarget != nullptr ) {
    llvm::Function* personalityFunc =
        this->getOrCreatePersonality();
    currentFunc->setPersonalityFn( personalityFunc );
    this->irBuilder.CreateInvoke(
        throwFunc, throwUnreachBlock,
        unwindTarget, throwArgs );
}
else {
    this->irBuilder.CreateCall(
        throwFunc, throwArgs );
}
```

If inside an async wrapper function, `asyncWrapperCatchBlock` is used as the unwind target — errors propagate to the async catch handler.

The error block terminates with `CreateUnreachable()` — `__uranite_throw` never returns.

### Success Path

```cpp
this->irBuilder.SetInsertPoint( awaitOkBlock );
if( instruction.destinationVariable !=
        INVALID_VARIABLE_IDENTIFIER ) {
    this->setVariableValue(
        instruction.destinationVariable, awaitResult );
}
```

On success, the `awaitResult` (returned by `uraniteAwaitTask`) is set as the destination variable value. This is the unwrapped `T` value from `Future<T>`.

---

## Async Function Generation

Async functions are intercepted at `src/uranite/ir/mir/codegen.cpp:895-897` and redirected to `generateAsyncFunction` instead of normal codegen:

```cpp
if( functionDefinition.isAsyncFunction &&
    functionDefinition.functionName !=
        semantic::qualname::functions::main::Name ) {
    this->generateAsyncFunction(
        functionDefinition, llvmFunctionName, llvmFunction );
    return;
}
```

`main` is excluded — even if async, it uses normal codegen (with scheduler init).

`generateAsyncFunction` at codegen.cpp:7648-7879 produces **two LLVM functions** from a single MIR function definition:

### Spawner Function

The original LLVM function (e.g., `fetchValue`) becomes a **spawner**. Instead of containing the function body, it:

1. Stores all arguments into global variables (`functionName.arg.paramName`).
2. Calls `uraniteSpawnTask(wrapperPtr, userData)` to spawn the wrapper as a task.
3. Returns the task ID (i64) — this becomes the `Future<T>` value.

```
define i64 @fetchValue(i64 %value) {
entry:
    store i64 %value, @fetchValue.arg.value
    %wrapper.i64 = ptrtoint @fetchValue.async to i64
    %taskId = call i64 @uraniteSpawnTask(%wrapper.i64, 0)
    ret i64 %taskId
}
```

### Wrapper Function

A new internal function `functionName.async` contains the actual function body:

```
define internal void @fetchValue.async(ptr %task) {
entry:
    %value = load i64, @fetchValue.arg.value
    ; ... original function body MIR codegen ...
    call void @uraniteTaskComplete(%task, %result)
    ret void
}
```

The wrapper:
1. Has signature `void(ptr %task)` — receives the task handle.
2. Loads parameters from the same globals the spawner stored to.
3. Executes the original function body.
4. On normal return: calls `uraniteTaskComplete(task, result)` and `ret void`.
5. On exception: handled by the async catch block.

### Argument Passing via Globals

```cpp
for( size_t i = 0; i < paramNames.size(); i++ ) {
    std::string globalName = fmt::format(
        "{}.arg.{}", llvmFunctionName, paramNames[i] );
    llvm::GlobalVariable* globalVar =
        new llvm::GlobalVariable(
            *this->llvmModule, paramLLVMTypes[i], false,
            llvm::GlobalValue::InternalLinkage,
            llvm::Constant::getNullValue( paramLLVMTypes[i] ),
            globalName );
    argGlobals.push_back( globalVar );
}
```

Each parameter gets an internal global variable. The spawner stores args into these globals before spawning; the wrapper loads from them on entry. This mechanism avoids passing parameters through the task runtime — the runtime only needs a function pointer and a user data integer.

### Error Handling in Wrapper

The wrapper function has a catch-all landing pad at `async.catch`:

```cpp
llvm::LandingPadInst* landingPad =
    this->irBuilder.CreateLandingPad(
        llvm::StructType::get( this->llvmContext, {
            llvm::PointerType::getUnqual( this->llvmContext ),
            llvm::Type::getInt32Ty( this->llvmContext )
        } ), 1, "async.lp" );
landingPad->addClause(
    llvm::ConstantPointerNull::get(
        llvm::PointerType::getUnqual(
            this->llvmContext ) ) );
llvm::Value* exceptionPtr =
    this->irBuilder.CreateExtractValue(
        landingPad, 0, "async.exc.ptr" );
llvm::Value* uraniteObj =
    this->irBuilder.CreateCall(
        this->getOrCreateBeginCatch(),
        { exceptionPtr }, "async.exc.obj" );
// Call uraniteTaskError(task, exceptionObj)
```

Any exception thrown inside the async function body is caught by this landing pad. The exception object is extracted and passed to `uraniteTaskError(task, exceptionObj)`, which marks the task as failed. The awaiting caller detects this via `uraniteTaskHasError`.

### Scheduler Initialization

When `main` is compiled and the program contains async functions, scheduler initialization is injected at `src/uranite/ir/mir/codegen.cpp:1018-1027`:

```cpp
if( isMainFunction && this->programHasAsyncFunctions ) {
    llvm::Function* schedulerInitFunc =
        this->llvmModule->getFunction( "runtimeInit" );
    if( schedulerInitFunc == nullptr ) {
        schedulerInitFunc = this->llvmModule->getFunction(
            "uraniteSchedulerInit" );
        // ... auto-declare if needed
    }
    this->irBuilder.CreateCall( schedulerInitFunc );
}
```

`uraniteSchedulerInit()` is called at the start of `main` to initialize the async task scheduler before any tasks are spawned.

### Async Runtime Auto-Import

The compiler driver auto-imports the async runtime module when the program contains async functions at `src/uranite/compiler/driver.cpp:977-981`:

```cpp
std::filesystem::path asyncRuntimePath =
    std::filesystem::path( modulesDir )
        / "async" / "runtime.urn";
if( std::filesystem::exists( asyncRuntimePath ) ) {
    this->loadModule(
        asyncRuntimePath.string(), *programRoot );
}
```

`stdlibs/async/runtime.urn` provides the pure Uranite async runtime (raw Linux syscalls, no C runtime dependency) including the scheduler, task management, and epoll-based I/O.

---

## FutureType

Defined at `src/uranite/semantic/typeref.hpp:1077-1091`:

```cpp
struct FutureType : Type {

    TypeSharedPointer innerType;

    FutureType( TypeSharedPointer inner )
        : Type( Type::Kind::Future,
                fmt::format( "Future<{}>", inner->name ) ),
          innerType( std::move( inner ) ) {
    }

};
```

| Field | Type | Description |
|---|---|---|
| `innerType` | `TypeSharedPointer` | Wrapped result type `T` |

Created via `typeRegistry.makeFuture(innerType)`. The `name` is formatted as `"Future<T>"`.

---

## Examples

### Basic Await

```
package examples

from uranite.io.console import puts

public async function greet() -> String:
    return "hello"

public async function main() -> I32:
    String message = await greet()
    puts(message)
    return 0
```

**Compilation trace for `await greet()`**:
1. **Parsing**: `await` keyword consumed. `greet()` parsed as call expression. Creates `AwaitExpression(operand=CallExpression(greet, []))`.
2. **Semantic analysis**: `isInsideAsyncFunction` is `true` (inside `async function main`). Operand `greet()` analyzed — `greet` returns `Future<String>`. Operand type is `Future<String>`. Unwrap: expression type = `String`.
3. **HIR lowering**: `HIRAwait(awaitedExpression=HIRFunctionCall("greet"), resolvedType=String)`.
4. **MIR lowering**: Lower callee `greet()` — produces task ID variable. Emit `CallFunction("__runtime_await", [taskId])`, result type `String`, destination `await.result`.
5. **LLVM codegen**: Intercept `__runtime_await`. Coerce task ID to i64. Call `uraniteAwaitTask(taskId)`. Check `uraniteTaskHasError(taskId)`. Branch: `await.ok` sets result, `await.err` constructs Exception and throws.

### Await with Error Handling

```
package examples

from uranite.io.console import puts

public async function riskyOperation() -> I64:
    return 42

public async function main() -> I32:
    try:
        I64 value = await riskyOperation()
        puts("success")
    catch Exception error:
        puts("operation failed")
    return 0
```

**Compilation trace**:
1. **MIR lowering**: `await` inside `try` block — `activeLandingPad` is set. The `CallFunction("__runtime_await")` instruction has `landingPadTarget` pointing to the catch block.
2. **LLVM codegen**: In error path, `__uranite_throw` uses `invoke` with the catch block's landing pad as unwind target. Exception propagates to the `catch Exception error` handler.

### Chained Await

```
package examples

public async function step1() -> I64:
    return 10

public async function step2(I64 input) -> I64:
    return input * 2

public async function pipeline() -> I64:
    I64 first = await step1()
    I64 second = await step2(first)
    return second
```

**Compilation trace**:
- `await step1()`: spawner returns task ID, `uraniteAwaitTask` blocks, result stored as `first`.
- `await step2(first)`: uses `first` as argument to `step2` spawner, gets new task ID, awaits again.
- Each `await` generates its own `await.ok`/`await.err` block pair. Control flows linearly through success blocks.

### Async Function Codegen Structure

```
package examples

public async function compute(I64 value) -> I64:
    return value + 1
```

Generates three LLVM artifacts:

**1. Global variable**: `@compute.arg.value` (i64, internal linkage)

**2. Spawner** (`compute`):
```
define i64 @compute(i64 %value) {
entry:
    store i64 %value, @compute.arg.value
    %wrapper = ptrtoint @compute.async to i64
    %taskId = call i64 @uraniteSpawnTask(%wrapper, 0)
    ret i64 %taskId
}
```

**3. Wrapper** (`compute.async`):
```
define internal void @compute.async(ptr %task) {
entry:
    %value = load i64, @compute.arg.value
    ; ... body: value + 1 ...
    call void @uraniteTaskComplete(%task, %result)
    ret void
async.catch:
    %lp = landingpad { ptr, i32 } catch ptr null
    %exc = extractvalue %lp, 0
    %obj = call ptr @__cxa_begin_catch(%exc)
    call void @uraniteTaskError(%task, %obj)
    ret void
}
```

### Runtime Function Summary

| Function | Signature | Purpose |
|---|---|---|
| `uraniteSchedulerInit` | `() -> void` | Initialize async task scheduler (called in `main`) |
| `uraniteSpawnTask` | `(ptr, ptr) -> i64` | Spawn wrapper function as task, returns task ID |
| `uraniteAwaitTask` | `(i64) -> i64` | Block until task completes, return result |
| `uraniteTaskHasError` | `(i64) -> i32` | Check if completed task has error |
| `uraniteTaskGetError` | `(i64) -> ptr` | Retrieve error message from failed task |
| `uraniteTaskComplete` | `(ptr, i64) -> void` | Mark task as completed with result value |
| `uraniteTaskError` | `(ptr, ptr) -> void` | Mark task as failed with exception object |
