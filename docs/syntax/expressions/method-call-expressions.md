# Method Call Expressions

Uranite supports method calls via the dot operator (`object.method(args)`) and the double-colon operator (`Type::method(args)`). A method call expression `object.method(args)` produces a `MethodCallExpression` AST node carrying the receiver object, method name, positional arguments, keyword arguments, and generic type arguments. The semantic analyzer resolves method calls through four type paths: classes (with base class fallback, generic substitution, and argument type checking), structs (linear method search), interfaces (DFS through super-interface hierarchy), and optionals (transparent unwrap). At HIR level, method calls lower to `HIRMethodCall` nodes with lowered receiver and argument sub-expressions. At MIR level, `lowerMethodCall` builds the qualified method name (`OwnerClass.methodName`), prepends the receiver as the first argument (self), strips it for static methods, and emits either `CallFunction` or `InvokeFunction` (when a landing pad is active for exception handling). At LLVM codegen, `generateCallFunction` is a large dispatch function that resolves the callee through a multi-step cascade: builtin descriptor interception for OOP wrapper types, direct `functionResolutionMap` lookup, arity-disambiguated overloads, base class method inheritance walk, concrete class resolution for interface-typed receivers, vtable-based virtual dispatch for interfaces with itables, abstract class dispatch with vtable identifier matching, variadic/kwargs argument packing, and finally `emitCallOrInvoke` which emits either `CreateCall` or `CreateInvoke` depending on exception context.

---

## Table of Contents

- [Syntax](#syntax)
- [AST Representation — MethodCallExpression](#ast-representation--methodcallexpression)
- [Parsing](#parsing)
- [Semantic Analysis — Four Type Paths](#semantic-analysis--four-type-paths)
  - [Path 1: Class Types](#path-1-class-types)
  - [Path 2: Struct Types](#path-2-struct-types)
  - [Path 3: Interface Types](#path-3-interface-types)
  - [Optional Unwrap](#optional-unwrap)
- [HIR Representation — HIRMethodCall](#hir-representation--hirmethodcall)
- [MIR Lowering — lowerMethodCall](#mir-lowering--lowermethodcall)
  - [Receiver as Self](#receiver-as-self)
  - [Owner Class Name Resolution](#owner-class-name-resolution)
  - [Static Method Detection](#static-method-detection)
  - [Exception-Safe Invoke](#exception-safe-invoke)
- [LLVM Codegen — generateCallFunction](#llvm-codegen--generatecallfunction)
  - [Builtin Descriptor Interception](#builtin-descriptor-interception)
  - [Function Resolution Cascade](#function-resolution-cascade)
  - [Virtual Dispatch — Interface Itables](#virtual-dispatch--interface-itables)
  - [Abstract Class Dispatch](#abstract-class-dispatch)
  - [Variadic and Kwargs Packing](#variadic-and-kwargs-packing)
  - [emitCallOrInvoke](#emitcallorinvoke)
- [Examples](#examples)

---

## Syntax

```
object.method(arguments)
object.method<TypeArgs>(arguments)
Type::method(arguments)
object.method(arg1, arg2, name=value)
```

Method calls use the dot operator on an instance or the double-colon operator on a type name. Arguments are comma-separated within parentheses. Named keyword arguments use `name=value` syntax after positional arguments. Generic type arguments appear between angle brackets before the parentheses.

```
list.add(42)
list.get<String>(0)
HashMap::fromPairs(pairs)
formatter.format("hello {0}", name=value)
```

---

## AST Representation — MethodCallExpression

Defined at `src/uranite/ast/node.hpp:1642-1677`:

```cpp
struct MethodCallExpression : Expression {

    std::vector<ExpressionSharedPointer> arguments;
    std::vector<KeywordArgument> keywordArguments;
    std::string method;
    ExpressionSharedPointer object;
    std::vector<TypeNodeSharedPointer> typeArguments;

    MethodCallExpression(
        ExpressionSharedPointer object,
        const std::string& method,
        std::vector<ExpressionSharedPointer> arguments,
        const lookup::SourceSharedPointer& source
    ) : Expression( Node::Kind::MethodCallExpression, source ),
        arguments( std::move( arguments ) ),
        method( method ),
        object( std::move( object ) ) {
    }

};
```

| Field | Type | Description |
|---|---|---|
| `object` | `ExpressionSharedPointer` | Receiver object expression |
| `method` | `std::string` | Method name identifier |
| `arguments` | `vector<ExpressionSharedPointer>` | Positional arguments |
| `keywordArguments` | `vector<KeywordArgument>` | Named `name=value` arguments |
| `typeArguments` | `vector<TypeNodeSharedPointer>` | Generic type arguments (`<T>`) |

---

## Parsing

Method calls are parsed as postfix operators inside `parsePostfixExpression` at `src/uranite/parser/parser.cpp:2049-2093`. The parser disambiguates between field access and method call at the token level:

```cpp
else if( this->check( token::Type::Dot ) ) {
    this->advance();
    std::string member = this->advance().value;

    // Parse optional generic type arguments
    std::vector<TypeNodeSharedPointer> methodTypeArgs;
    if( this->check( token::Type::LessThan ) &&
        this->looksLikeGenericCall() ) {
        methodTypeArgs = this->parseGenericArguments();
    }

    if( this->check( token::Type::LeftParenthesis ) ) {
        // Method call — parse arguments
        this->advance();
        std::vector<ExpressionSharedPointer> arguments;
        std::vector<KeywordArgument> keywordArguments;
        bool seenKeywordArg = false;
        if( this->check( token::Type::RightParenthesis,
                false ) ) {
            do {
                ExpressionSharedPointer argExpr =
                    this->parseExpression();
                if( argExpr->kind ==
                        Node::Kind::IdentifierExpression &&
                    this->check( token::Type::Assignment ) ) {
                    // Keyword argument: name=value
                    ...
                    keywordArguments.push_back( ... );
                    seenKeywordArg = true;
                }
                else {
                    if( seenKeywordArg ) {
                        this->diagnostic.error( ... ,
                            "positional argument cannot "
                            "follow keyword argument" );
                    }
                    arguments.push_back( argExpr );
                }
            }
            while( this->match( token::Type::Comma ) );
        }
        this->expect( token::Type::RightParenthesis );
        expression = MethodCallExpression(
            expression, member,
            std::move( arguments ), source );
        // Attach keyword args and type args
    }
    else {
        // No parenthesis — MemberAccessExpression
        expression = MemberAccessExpression(
            expression, member, source );
    }
}
```

Key details:
- **Disambiguation**: `(` after member name creates `MethodCallExpression`. No `(` creates `MemberAccessExpression`.
- **Generic arguments**: `looksLikeGenericCall()` lookahead distinguishes `list.get<String>(0)` (generic method) from `list.get < x` (comparison).
- **Keyword arguments**: Detected when an identifier is followed by `=` (Assignment token). Positional arguments after keyword arguments produce error: "positional argument cannot follow keyword argument".
- **Double-colon**: `Type::method(args)` follows the same pattern at `parser.cpp:2102-2119` — simpler logic without generic type argument parsing between name and parenthesis.

---

## Semantic Analysis — Four Type Paths

At `src/uranite/semantic/analyzer.cpp:3457-3593`, `analyzeMethodCallExpression` resolves method calls.

All arguments (positional and keyword) are analyzed first, then the method is resolved based on receiver type.

### Path 1: Class Types

At `analyzer.cpp:3473-3563`:

1. **Method lookup**: `classType->findMethod(method)`. If not found and `astDeclaration` exists, look up unmonomorphized base class and retry.
2. **Generic type argument resolution**: If the method has generic parameters (`genericParameterNames`):
   - If explicit type arguments provided (`list.get<String>(0)`): validate count matches, resolve each type argument, build `genericSubstitutionMap`.
   - If no explicit type arguments: check if all method generic parameters are covered by class-level `typeSubstitutions`. If not, error: "generic method requires explicit type arguments".
3. **Full substitution map**: Merge method-level generic substitutions with class-level `typeSubstitutions`. Method-level takes priority.
4. **Argument type checking**: For each fixed parameter (up to variadic/kwargs boundary), substitute generic parameters in expected type, then check `isAssignable(expected, actual)`. Mismatch produces error: "argument type mismatch: expected X but found Y".
5. **Return type resolution**: Apply generic substitution to return type. If still a `GenericParameter`, look up in class `typeSubstitutions`. Return the resolved type.

### Path 2: Struct Types

At `analyzer.cpp:3565-3572`:

Linear search through `structType->methods`. Match by method name. Return `FunctionType->returnType` directly. No generic substitution or argument type checking — simpler than class methods.

### Path 3: Interface Types

At `analyzer.cpp:3573-3591`:

Iterative DFS through super-interface hierarchy (same pattern as member access). For each interface, search its `methods` list. Match by name, return `FunctionType->returnType`. Traverses super-interfaces if method not found at current level.

### Optional Unwrap

At `analyzer.cpp:3469-3472`:

Optional types transparently unwrap before method resolution:

```cpp
if( resolvedObjectType->kind == Type::Kind::Optional ) {
    OptionalTypeSharedPointer optionalType =
        std::static_pointer_cast<OptionalType>(
            resolvedObjectType );
    resolvedObjectType = optionalType->inner;
}
```

`Optional<ArrayList<String>>` unwraps to `ArrayList<String>` before searching for methods. The `?` operator for safe chaining is separate.

---

## HIR Representation — HIRMethodCall

Defined at `src/uranite/ir/hir.hpp:685-705`:

```cpp
struct HIRMethodCall : HIRNode {

    HIRNodeSharedPointer receiverObject;
    std::string methodName;
    std::vector<HIRNodeSharedPointer> callArguments;
    std::vector<std::pair<std::string, HIRNodeSharedPointer>>
        keywordArguments;
    std::vector<semantic::TypeSharedPointer> typeArguments;

    HIRMethodCall(
        HIRNodeSharedPointer receiverObject,
        const std::string& methodName,
        std::vector<HIRNodeSharedPointer> callArguments,
        semantic::TypeSharedPointer resolvedType,
        const lookup::SourceSharedPointer& sourceLocation
    ) : HIRNode( HIRNodeKind::MethodCall,
            std::move( resolvedType ), sourceLocation ),
        receiverObject( std::move( receiverObject ) ),
        methodName( methodName ),
        callArguments( std::move( callArguments ) ) {
    }

};
```

| Field | Type | Description |
|---|---|---|
| `receiverObject` | `HIRNodeSharedPointer` | Lowered receiver expression |
| `methodName` | `std::string` | Method name |
| `callArguments` | `vector<HIRNodeSharedPointer>` | Lowered positional arguments |
| `keywordArguments` | `vector<pair<string, HIRNodeSharedPointer>>` | Lowered keyword arguments |
| `typeArguments` | `vector<TypeSharedPointer>` | Generic type arguments |

HIR lowering at `src/uranite/ir/hir/lowering.cpp:918-935` is straightforward — lower receiver and each argument from AST to HIR nodes, carry keyword arguments:

```cpp
case ast::Node::Kind::MethodCallExpression: {
    HIRNodeSharedPointer receiverObject =
        this->lowerExpression( methodCallExpression.object );
    std::vector<HIRNodeSharedPointer> callArguments;
    for( const ExpressionSharedPointer& argument :
            methodCallExpression.arguments ) {
        callArguments.push_back(
            this->lowerExpression( argument ) );
    }
    auto hirMethodCall = std::make_shared<HIRMethodCall>(
        std::move( receiverObject ),
        methodCallExpression.method,
        std::move( callArguments ),
        expression->semanticType,
        expression->source
    );
    for( const KeywordArgument& kw :
            methodCallExpression.keywordArguments ) {
        hirMethodCall->keywordArguments.push_back(
            { kw.name, this->lowerExpression( kw.value ) } );
    }
    return hirMethodCall;
}
```

---

## MIR Lowering — lowerMethodCall

At `src/uranite/ir/mir/lowering.cpp:3569-3761`, `lowerMethodCall` transforms `HIRMethodCall` into `CallFunction` or `InvokeFunction` MIR instructions.

### Receiver as Self

```cpp
MIRVariableIdentifier receiverVariable =
    this->lowerExpression( hirMethodCall.receiverObject );
std::vector<MIRVariableIdentifier> argumentVariables;
argumentVariables.push_back( receiverVariable );
for( const HIRNodeSharedPointer& argument :
        hirMethodCall.callArguments ) {
    argumentVariables.push_back(
        this->lowerExpression( argument ) );
}
```

The receiver is always prepended as the first argument — the implicit `self` parameter. `list.add(42)` becomes `CallFunction("ArrayList.add", [list, 42])` where `list` is the self argument.

### Owner Class Name Resolution

The MIR lowering resolves the owner class name through multiple strategies at `lowering.cpp:3579-3713`:

1. **Receiver resolved type**: Extract type name from `receiverObject->resolvedType`. Strip generic brackets (`"ArrayList<String>"` becomes `"ArrayList"`).
2. **Generic parameter resolution**: If receiver type is `GenericParameter`, look up concrete type in `genericClassSubstitutions` for the current class.
3. **Interface-to-concrete resolution**: If receiver type is an interface and current class has generic substitutions, search for a concrete class that implements the interface.
4. **Qualified name lookup**: Look up the type via `typeRegistry->lookupType()` and use its `qualified` name.
5. **Self reference fallback**: If receiver is `SelfReference`, use `currentClassName`.
6. **Field access fallback**: If receiver is `FieldAccess`, look up the field type in `typeLayoutTable` to determine the owner class.
7. **Variable descriptor fallback**: If receiver is an identifier, look up its type from `variableDescriptorTable`.
8. **Type name lookup fallback**: Last resort — look up the identifier name itself as a type.

Final method name: `ownerClassName + "." + hirMethodCall.methodName`.

### Static Method Detection

At `lowering.cpp:3721-3739`:

```cpp
if( ownerType != nullptr &&
    ownerType->kind == semantic::Type::Kind::Class ) {
    semantic::ClassType* classType =
        static_cast<semantic::ClassType*>(
            ownerType.get() );
    semantic::MethodInfo* methodInfo =
        classType->findMethod( hirMethodCall.methodName );
    if( methodInfo != nullptr && methodInfo->isStatic ) {
        argumentVariables.erase(
            argumentVariables.begin() );
    }
}
```

Static methods do not take `self`. When the method is marked `isStatic`, the receiver variable is removed from the argument list. `Type::staticMethod(arg)` emits `CallFunction("Type.staticMethod", [arg])` without prepending self.

### Exception-Safe Invoke

At `lowering.cpp:3576-3758`:

```cpp
bool useInvoke =
    ( this->activeLandingPad != INVALID_BLOCK_IDENTIFIER );
MIRInstruction callInstruction(
    useInvoke
        ? MIRInstructionKind::InvokeFunction
        : MIRInstructionKind::CallFunction );
```

When a landing pad is active (inside `try` block), `InvokeFunction` replaces `CallFunction`. Creates a continuation block (`invoke.cont`) and sets the landing pad target. If the method throws, control transfers to the exception handler.

```cpp
if( useInvoke ) {
    std::shared_ptr<MIRBasicBlock> continuationBlock =
        this->currentFunction->createBasicBlock(
            "invoke.cont" );
    callInstruction.trueBranchTarget =
        continuationBlock->blockIdentifier;
    callInstruction.landingPadTarget =
        this->activeLandingPad;
    this->emitTerminator( callInstruction );
    this->switchToBlock( continuationBlock );
    return resultVariable;
}
return this->emitInstruction( callInstruction );
```

---

## LLVM Codegen — generateCallFunction

At `src/uranite/ir/mir/codegen.cpp:2497-5490`, `generateCallFunction` is the largest codegen function. Both `CallFunction` and `InvokeFunction` MIR instructions route to this same function (codegen.cpp:1137-1138, 1259-1260).

### Builtin Descriptor Interception

At `codegen.cpp:3078-3089`:

Before general function resolution, OOP wrapper method calls are intercepted by the builtin descriptor system:

```cpp
size_t dotPosition = shortCalledName.find( '.' );
if( dotPosition != std::string::npos ) {
    std::string wrapperName =
        shortCalledName.substr( 0, dotPosition );
    std::string methodName =
        shortCalledName.substr( dotPosition + 1 );
    if( this->tryBuiltinDescriptor(
            wrapperName, methodName,
            instruction, functionDefinition ) ) {
        return;
    }
}
```

`tryBuiltinDescriptor` at `codegen.cpp:7292-7368`:
1. Checks if the receiver is an OOP wrapper type (via `isOopWrapper` on qualified name). Non-wrapper types bail out immediately.
2. Looks up the method in `builtinRegistry` — a table of `BuiltinMethodDescriptor` entries with custom `irGenerator` callbacks.
3. Coerces the self value to the expected LLVM type (pointer-to-int, int-to-float, etc.).
4. Calls `methodDescriptor->irGenerator(irBuilder, llvmContext, selfValue, arguments, keywordArguments)` — the callback emits custom LLVM IR inline. No function call overhead.
5. Handles integer width extension on the result (sign-extend or zero-extend to i64 for unsigned types).

This is how `42.toString()`, `3.14.negate()`, and `"hello".length` bypass function dispatch entirely — they emit inline LLVM IR via descriptor callbacks.

### Function Resolution Cascade

At `codegen.cpp:4223-4560`, the callee is resolved through a multi-step cascade:

| Priority | Strategy | Example |
|---|---|---|
| 1 | `functionResolutionMap[calledName]` | Direct qualified name match |
| 2 | `llvmModule->getFunction(calledName)` | LLVM module-level lookup |
| 3 | Arity-disambiguated: `"name#argCount"` | Overloaded methods |
| 4 | Base class walk: `parent.method` | Inherited methods |
| 5 | Concrete class map: `concreteClass.method` | Interface-typed receivers |
| 6 | Interface implementor scan | Find class implementing interface |
| 7 | Vtable dispatch (itable) | Virtual method on interface with itable |
| 8 | Abstract class dispatch | Vtable-based polymorphic dispatch |
| 9 | Function pointer (indirect call) | Lambda/closure calls |
| 10 | Extern declaration | External C function |
| 11 | Create extern stub | Last resort forward declaration |

**Arity disambiguation** (priority 3): When the callee exists but its parameter count does not match the argument count, the codegen tries `"methodName#argCount"` — enabling method overloading by parameter count.

**Base class walk** (priority 4): If the method is not found on the class itself, walk up the inheritance chain (`baseClass->baseClass->...`) trying `parent.method` at each level.

**Concrete class map** (priority 5): When the receiver is typed as an interface but the codegen knows the concrete class (tracked in `concreteClassMap`), it resolves `concreteClass.method` directly — avoiding virtual dispatch overhead.

### Virtual Dispatch — Interface Itables

At `codegen.cpp:4320-4391`:

When the callee is an interface method and direct resolution fails, the codegen emits vtable-based virtual dispatch:

1. Look up the method slot index in `interfaceMethodOrder` for the interface.
2. Load the vtable pointer from the first field of the receiver object (`CreateStructGEP` index 0).
3. Compute the function pointer address via `CreateGEP` at the method slot index.
4. Load the function pointer.
5. Build the call type from actual argument types and expected return type.
6. `CreateBitCast` the function pointer to the expected function type.
7. `CreateCall` through the cast pointer — indirect virtual dispatch.

This is the itable (interface table) dispatch mechanism. Each class that implements an interface has a vtable with function pointers in the order defined by `interfaceMethodOrder`.

### Abstract Class Dispatch

At `codegen.cpp:4585-4760`:

When the receiver is an abstract class and the method is virtual:

1. Find all concrete subclasses from `abstractClassSubclasses`.
2. For each subclass, look up its concrete method implementation.
3. Load the vtable identifier from the receiver (first field, `CreateStructGEP` index 0).
4. Build a chain of `CreateICmpEQ` comparisons against each subclass vtable identifier.
5. `CreateCondBr` to the matching concrete method call.
6. PHI node merges results from all dispatch targets.

This enables polymorphic dispatch without a global vtable — each concrete subclass has a unique identifier stored in the object header.

### Variadic and Kwargs Packing

At `codegen.cpp:4765-5455`:

When the callee has a variadic parameter (`Args<T>`):

1. Detect `calleeVariadicIndex` — the parameter position where variadic begins.
2. Fixed parameters before the variadic index are passed normally with type coercion.
3. Extra arguments after the variadic index are packed into an `Args<T>` struct: allocate the struct, store element count, allocate an array for values, store each value.
4. The `Args<T>` struct pointer becomes a single argument to the callee.

Kwargs (`Kwargs<K,V>`) are similarly packed: allocate a kwargs struct with keys array, values array, and count field.

### emitCallOrInvoke

At `codegen.cpp:7252-7279`:

The final call emission is unified in `emitCallOrInvoke`:

```cpp
llvm::Value* MIRCodegen::emitCallOrInvoke(
    const MIRInstruction& instruction,
    llvm::Function* callee,
    std::vector<llvm::Value*>& arguments
) {
    if( instruction.instructionKind ==
            MIRInstructionKind::InvokeFunction &&
        this->blockMap.count(
            instruction.trueBranchTarget ) > 0 &&
        this->blockMap.count(
            instruction.landingPadTarget ) > 0 ) {
        // Set personality function for exception handling
        llvm::Function* enclosingFunction =
            this->irBuilder.GetInsertBlock()->getParent();
        if( enclosingFunction->hasPersonalityFn() == false )
        {
            enclosingFunction->setPersonalityFn(
                this->getOrCreatePersonality() );
        }
        llvm::BasicBlock* normalDest =
            this->blockMap[instruction.trueBranchTarget];
        llvm::BasicBlock* unwindDest =
            this->blockMap[instruction.landingPadTarget];
        return this->irBuilder.CreateInvoke(
            callee, normalDest, unwindDest, arguments );
    }
    if( this->asyncWrapperCatchBlock != nullptr ) {
        // Async context — always invoke with catch block
        ...
        return this->irBuilder.CreateInvoke(
            callee, contBlock,
            this->asyncWrapperCatchBlock, arguments );
    }
    return this->irBuilder.CreateCall(
        callee, arguments );
}
```

Three paths:
1. **InvokeFunction with landing pad**: `CreateInvoke` with normal continuation and unwind destination. Sets LLVM personality function for EH.
2. **Async context**: Even `CallFunction` uses `CreateInvoke` when inside async wrapper — routes exceptions to the async catch block.
3. **Normal call**: Simple `CreateCall` — no exception routing.

---

## Examples

### Basic Method Call

```
package examples

from uranite.io.console import puts
from uranite.collection.array-list import ArrayList

public function main() -> I32:
    ArrayList<String> names = new ArrayList<>()
    names.add("Alice")
    names.add("Bob")
    puts(names.get(0))
    return 0
```

Output: `Alice`.

**Compilation trace**:
1. **Parsing**: `names.add("Alice")` — dot, identifier `add`, `(` follows. Creates `MethodCallExpression(object=names, method="add", arguments=["Alice"])`.
2. **Semantic analysis**: `names` is `ArrayList<String>` (class type). `findMethod("add")` succeeds. Generic substitution `E→String`. Argument type `String` matches expected `E→String`. Return type: `Void`.
3. **MIR lowering**: Receiver `names` prepended as self. Emits `CallFunction _mcall = names, "Alice"` targeting `"_UR_collection_ArrayList.add"`.
4. **LLVM codegen**: Resolve `"_UR_collection_ArrayList.add"` in `functionResolutionMap`. Emit `CreateCall`.

### Method Call with Generic Type Arguments

```
package examples

from uranite.collection.array-list import ArrayList

public function main() -> I32:
    ArrayList<Object> items = new ArrayList<>()
    items.add("hello")
    String value = items.get<String>(0)
    return 0
```

**Compilation trace**:
1. **Parsing**: `items.get<String>(0)` — `looksLikeGenericCall()` confirms `<String>` is generic, not comparison. Type arguments: `[String]`. Creates `MethodCallExpression` with `typeArguments=[String]`.
2. **Semantic analysis**: `get` has generic parameter. Explicit type argument `String` provided. Build `genericSubstitutionMap["E" → String]`. Return type substituted: `E → String`.

### Keyword Arguments

```
package examples

from uranite.io.console import puts

class Logger:
    public function Logger(self) -> Void:
        pass

    public function log(self, String message, String level{}) -> Void:
        puts(message)

public function main() -> I32:
    Logger logger = new Logger()
    logger.log("Server started", level="INFO")
    return 0
```

**Compilation trace**:
1. **Parsing**: `level="INFO"` — identifier followed by `=`. Parsed as keyword argument `{name="level", value="INFO"}`.
2. **MIR lowering**: Keyword arguments stored separately in `callInstruction.keywordArgumentKeys` and `callInstruction.keywordArgumentValues`.
3. **LLVM codegen**: Kwargs packed into `Kwargs` struct — keys array, values array, count — passed as single struct pointer argument.

### Static Method Call

```
package examples

from uranite.io.console import puts

class MathUtils:
    public static function square(I64 value) -> I64:
        return value * value

public function main() -> I32:
    I64 result = MathUtils::square(5)
    puts(result.toString())
    return 0
```

Output: `25`.

**Compilation trace**:
1. **Parsing**: `MathUtils::square(5)` — `DoubleColon` postfix, `(` follows. Creates `MethodCallExpression(object=MathUtils, method="square", arguments=[5])`.
2. **MIR lowering**: Receiver `MathUtils` prepended as self initially. Static method detected — `methodInfo->isStatic == true`. Receiver removed from argument list. Emits `CallFunction _mcall = 5` targeting `"MathUtils.square"`.

### Method Call in Try Block

```
package examples

from uranite.io.console import puts
from uranite.collection.array-list import ArrayList

public function main() -> I32:
    ArrayList<I64> values = new ArrayList<>()
    try:
        I64 element = values.get(0)
        puts(element.toString())
    catch Exception error:
        puts("Error: empty list")
    return 0
```

**Compilation trace**:
1. **MIR lowering**: Inside `try` block, `activeLandingPad` is set. Emits `InvokeFunction` instead of `CallFunction`. Creates continuation block `invoke.cont`. Landing pad target points to catch block.
2. **LLVM codegen**: `emitCallOrInvoke` detects `InvokeFunction` with valid landing pad. Sets personality function. Emits `CreateInvoke(callee, normalDest, unwindDest, args)`. If `get` throws `IndexOutOfBoundsException`, control transfers to landing pad.

### Chained Method Calls

```
package examples

from uranite.io.console import puts

public function main() -> I32:
    String result = "Hello World".substring(0, 5).toUpperCase()
    puts(result)
    return 0
```

**Compilation trace**:
1. **Parsing**: `"Hello World".substring(0, 5)` creates first `MethodCallExpression`. `.toUpperCase()` wraps it: `MethodCallExpression(MethodCallExpression(...), "toUpperCase", [])`.
2. **MIR lowering**: Two sequential `CallFunction` instructions. First call returns `_mcall_1`. Second call uses `_mcall_1` as receiver (self).

### OOP Wrapper Builtin Method

```
package examples

from uranite.io.console import puts

public function main() -> I32:
    I64 number = 42
    String text = number.toString()
    puts(text)
    return 0
```

Output: `42`.

**Compilation trace**:
1. **MIR lowering**: Emits `CallFunction _mcall = number` targeting `"I64.toString"`.
2. **LLVM codegen**: `tryBuiltinDescriptor("I64", "toString", ...)` succeeds. `I64` is an OOP wrapper. Builtin descriptor's `irGenerator` callback emits inline LLVM IR: `snprintf` into 48-byte malloc buffer with `"%ld"` format. No function call to user-defined code. Returns immediately.

### Interface Method Call

```
package examples

from uranite.io.console import puts

public interface Greetable:
    public function greet(self) -> String;

class Person implements Greetable:
    public String personName

    public function Person(self, String personName) -> Void:
        self.personName = personName

    public function greet(self) -> String:
        return "Hello, " + self.personName

public function sayHello(Greetable entity) -> Void:
    puts(entity.greet())

public function main() -> I32:
    Person user = new Person("Ari")
    sayHello(user)
    return 0
```

Output: `Hello, Ari`.

**Compilation trace**:
1. **Semantic analysis**: `entity` is `Greetable` (interface type). DFS finds `greet` method. Return type: `String`.
2. **MIR lowering**: Emits `CallFunction` targeting `"Greetable.greet"`.
3. **LLVM codegen**: Direct `functionResolutionMap` lookup fails. Concrete class map lookup — if `entity` is known to be `Person`, resolves directly to `Person.greet`. Otherwise, itable dispatch: load vtable pointer from receiver, index into method slot, indirect call through function pointer.

### Error Case — Argument Type Mismatch

```
package examples

from uranite.collection.array-list import ArrayList

public function main() -> I32:
    ArrayList<I64> numbers = new ArrayList<>()
    numbers.add("not a number")
    return 0
```

**Diagnostic output**:
```
error: argument type mismatch: expected "I64" but found "String"
```

Generic substitution resolves `E→I64`. Argument `"not a number"` is `String`. `isAssignable(I64, String)` fails.
