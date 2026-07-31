# Function Types

Uranite functions are first-class values with full type-system representation. Every function declaration produces a `FunctionType` in the type registry, encoding parameter types, return type, variadic and keyword parameter positions, generic parameter names, and declared exception types. Functions can be passed as arguments, stored in variables, and returned from other functions via the `Callable<ReturnType, <ParamTypes>>` wrapper type. Lambda expressions create anonymous functions that capture free variables through a global-variable-backed closure mechanism. Nested functions are hoisted to module-level `MIRFunctionDefinition` entries with captured variables threaded as extra parameters. At the LLVM level, both `FunctionType` and `CallableType` lower to opaque pointers — all function-typed values are function pointer addresses resolved through the `functionResolutionMap`.

This document covers the `FunctionType` and `CallableType` structs with their parameter descriptor fields, function declaration syntax with all modifier keywords, parameter kinds (self, mutable, property, variadic, keyword, default values), lambda expression syntax (shorthand and block forms), generic parameters with constraints, the `raises` clause, parsing implementation, semantic analysis (parameter resolution with `Args<T>`/`Kwargs<T>` monomorphization, overload resolution with score-based ranking, lambda return type inference), HIR lowering to `HIRFunctionDefinition` and `HIRLambda`, MIR lowering (entry block setup, parameter allocation, variadic/keyword index tracking, default value capture, nested function capture analysis, lambda closure via global variables and wrapper functions), codegen (function pre-registration with arity suffixes for overloads, LLVM function creation, shadow stack frame push/pop, return value type coercion), and practical examples.

---

## Table of Contents

- [Type Identity](#type-identity)
- [The FunctionType Struct](#the-functiontype-struct)
- [The CallableType Struct](#the-callabletype-struct)
- [Function Declaration Syntax](#function-declaration-syntax)
  - [Basic Functions](#basic-functions)
  - [Function Modifiers](#function-modifiers)
  - [Parameters](#parameters)
  - [Self Parameter](#self-parameter)
  - [Variadic Parameters](#variadic-parameters)
  - [Keyword Parameters](#keyword-parameters)
  - [Default Values](#default-values)
  - [Property Parameters](#property-parameters)
  - [Generic Parameters](#generic-parameters)
  - [Raises Clause](#raises-clause)
  - [Forward Declarations](#forward-declarations)
  - [Nested Functions](#nested-functions)
- [Lambda Expressions](#lambda-expressions)
  - [Shorthand Lambda](#shorthand-lambda)
  - [Block Lambda](#block-lambda)
  - [Closure Capture](#closure-capture)
- [Callable Type](#callable-type-1)
- [AST Representation](#ast-representation)
  - [FunctionDeclaration](#functiondeclaration)
  - [FunctionParameterNode](#functionparameternode)
  - [LambdaExpression](#lambdaexpression)
- [Parsing Implementation](#parsing-implementation)
  - [Function Header Parsing](#function-header-parsing)
  - [Parameter Parsing](#parameter-parsing)
  - [Lambda Parsing](#lambda-parsing)
- [Semantic Analysis](#semantic-analysis)
  - [Function Declaration Analysis](#function-declaration-analysis)
  - [Call Expression Analysis](#call-expression-analysis)
  - [Overload Resolution](#overload-resolution)
  - [Lambda Analysis](#lambda-analysis)
- [Compilation Pipeline](#compilation-pipeline)
  - [HIR Stage — Functions](#hir-stage--functions)
  - [HIR Stage — Lambdas](#hir-stage--lambdas)
  - [MIR Stage — Function Lowering](#mir-stage--function-lowering)
  - [MIR Stage — Nested Function Lowering](#mir-stage--nested-function-lowering)
  - [MIR Stage — Lambda Lowering](#mir-stage--lambda-lowering)
  - [Codegen — Function Pre-Registration](#codegen--function-pre-registration)
  - [Codegen — Function Generation](#codegen--function-generation)
  - [Codegen — Shadow Stack](#codegen--shadow-stack)
  - [Codegen — Return Emission](#codegen--return-emission)
  - [Codegen — toLLVMType](#codegen--tollvmtype)
- [Assignability Rules](#assignability-rules)
- [Examples](#examples)
  - [Recursion and Nested Functions](#recursion-and-nested-functions)
  - [Higher-Order Functions and Lambdas](#higher-order-functions-and-lambdas)
  - [Defer Statements](#defer-statements)

---

## Type Identity

| Property | Value |
|---|---|
| Type Kind | `Type::Kind::Function` (named functions), `Type::Kind::Callable` (callable values) |
| LLVM Type | `PointerType::getUnqual` (opaque pointer for both) |
| String Format (Function) | `function( ParamType1, ParamType2 ) -> ReturnType` |
| String Format (Callable) | `Callable<ReturnType,<ParamType1,ParamType2>>` |
| Overload Naming | `FunctionName#arity` for same-name overloads |
| Shadow Stack | `emitPushFrame` at entry, `emitPopFrame` before every return |

---

## The FunctionType Struct

Defined in `src/uranite/semantic/typeref.hpp:334-411`:

```
struct FunctionType : Type
    exceptionTypes         : std::vector<TypeSharedPointer>
    isVariadic             : bool (default false)
    variadicParameterIndex : int (default -1)
    keywordParameterIndex  : int (default -1)
    variadicElementType    : TypeSharedPointer
    keywordValueType       : TypeSharedPointer
    keywordOnlyParamNames  : std::vector<std::string>
    keywordOnlyParamTypes  : std::vector<TypeSharedPointer>
    genericParameterNames  : std::vector<std::string>
    parameterNames         : std::vector<std::string>
    parameterTypes         : std::vector<TypeSharedPointer>
    returnType             : TypeSharedPointer
```

| Field | Description |
|---|---|
| `exceptionTypes` | Exception types declared via `raises` clause |
| `isVariadic` | True for extern variadic functions (C-style `...`) |
| `variadicParameterIndex` | Position of `Type name[]` parameter, or -1 |
| `keywordParameterIndex` | Position of `Type name{}` parameter, or -1 |
| `variadicElementType` | Element type `T` of the variadic `Args<T>` parameter |
| `keywordValueType` | Value type `T` of the keyword `Kwargs<T>` parameter |
| `keywordOnlyParamNames` | Parameters with defaults appearing after variadic parameter |
| `keywordOnlyParamTypes` | Types of keyword-only parameters |
| `genericParameterNames` | Generic type parameters declared on the function |
| `parameterNames` | Parameter names for snippet generation and keyword matching |
| `parameterTypes` | Concrete resolved types for each parameter |
| `returnType` | Return type (defaults to Void if omitted) |

Constructor sets `Kind::Function`:

```
FunctionType(parameters, returnType, isVariadic=false)
    → Type(Type::Kind::Function, "")
```

`toString()` produces: `"function( ParamType1, ParamType2, ... ) -> ReturnType"`

---

## The CallableType Struct

Defined in `src/uranite/semantic/typeref.hpp:1038-1069`:

```
struct CallableType : Type
    parameterTypes : std::vector<TypeSharedPointer>
    returnType     : TypeSharedPointer
```

`CallableType` wraps a function signature for first-class function values — variables, parameters, and return types that hold function pointers.

Constructor sets `Kind::Callable` and generates name: `Callable<ReturnType,<Param1,Param2>>`

```
CallableType(returnType, parameterTypes)
    → Type(Type::Kind::Callable, "Callable")
    → name = "Callable<ReturnType,<Param1,Param2>>"
```

Semantic analysis dispatches through `CallableType` when the callee expression resolves to `Kind::Callable` — it checks argument types against `parameterTypes` and returns `returnType`.

---

## Function Declaration Syntax

### Basic Functions

```uranite
public function add( I64 a, I64 b ) -> I64:
    return a + b

function greet() -> Void:
    puts( "Hello" )
```

Function header: access modifier, `function` keyword (or `property`), name, optional generic parameters, parenthesized parameter list, optional `-> ReturnType` (defaults to Void), optional `raises` clause, colon, indented body.

### Function Modifiers

| Modifier | Description |
|---|---|
| `public` / `private` / `protect` | Access control |
| `static` | No `self` parameter, called on type |
| `virtual` | Can be overridden by subclass |
| `override` | Overrides a parent class method |
| `abstract` | No body, must be overridden |
| `final` | Cannot be overridden |
| `const` | Immutable method |
| `native` | Implemented in native code |
| `async` | Returns `Future<T>`, enables `await` |
| `property` | Field-syntax access at call site |

### Parameters

Parameters have the form `Type name`, with optional modifiers:

```uranite
function process( I64 count, String label, mutable F64 ratio ) -> Void:
    ratio = ratio * 2.0
```

`mutable` allows the parameter to be reassigned within the function body.

### Self Parameter

Instance methods receive `self` as their first parameter. `self` binds to the enclosing class type:

```uranite
class Counter:
    public I64 value

    public function increment( self ) -> Void:
        self.value = self.value + 1
```

Reference `self` is also supported via `&self` — the parameter is marked `isReference = true`.

`self` is rejected in free functions with: `"parameter \"self\" is not allowed in free function; \"self\" can only be used in class or struct methods"`

### Variadic Parameters

Variadic parameters use `Type name[]` syntax. One per function. Internally monomorphized to `Args<T>`:

```uranite
function printAll( String items[] ) -> Void:
    for String item in items:
        puts( item )
```

The semantic analyzer resolves `String items[]` to `Args<String>` — a struct with `has()` and `next()` methods for iteration.

### Keyword Parameters

Keyword parameters use `Type name{}` syntax. Must be the last parameter. Internally monomorphized to `Kwargs<T>`:

```uranite
function configure( String options{} ) -> Void:
    for String key, String value in options:
        puts( key, " = ", value )
```

### Default Values

Parameters can have default values. Parameters with defaults after a variadic parameter become keyword-only:

```uranite
function connect( String host, I64 port = 8080, String protocol = "tcp" ) -> Void:
    puts( host, ":", port, " via ", protocol )
```

### Property Parameters

Constructor parameters with access modifiers automatically become class fields:

```uranite
class Point:
    public function Point( self, public I64 x, public I64 y ) -> Void:
        pass
```

`public I64 x` sets `isProperty = true`, `propertyAccess = Public` on the parameter, creating both a parameter and field.

### Generic Parameters

Functions support generic type parameters with optional constraints:

```uranite
public function identity<T>( T value ) -> T:
    return value

public function compare<T: Comparable>( T a, T b ) -> I32:
    return a.compareTo( b )
```

Generic parameters declared with `<T>` are registered as `GenericParameterType` in the type registry during analysis. Constraints after `:` restrict to types implementing the specified interface. Multiple constraints combine with `+`:

```uranite
public function process<T: Hashable + Equatable>( T item ) -> Void:
    pass
```

Calls to generic functions require explicit type arguments:

```uranite
I64 result = identity<I64>( 42 )
```

### Raises Clause

Functions can declare exception types they may throw:

```uranite
public function divide( I64 a, I64 b ) -> I64 raises ArithmeticError:
    if b == 0:
        throw new ArithmeticError( "division by zero" )
    return a / b
```

Multiple exception types are separated with `|`:

```uranite
public function parse( String input ) -> I64 raises FormatError | OverflowError:
    pass
```

### Forward Declarations

Functions can be forward-declared with a semicolon:

```uranite
public function compute( I64 x ) -> I64;
```

Forward declarations set `isAbstract = true` and produce no body.

### Nested Functions

Functions can contain inner function declarations:

```uranite
public function outer( I64 x ) -> I64:
    function inner( I64 y ) -> I64:
        return y * y
    return inner( x ) + 1
```

Nested functions are hoisted to module-level during MIR lowering with captured variables threaded as extra parameters.

---

## Lambda Expressions

### Shorthand Lambda

Single-expression lambdas use the `lambda` keyword:

```uranite
lambda I64 x: x * 2
```

Syntax: `lambda Type1 name1, Type2 name2: expression`

The expression is wrapped in an implicit `return` statement. `final` before a parameter type makes it immutable (default); omitting `final` makes the parameter mutable.

### Block Lambda

Multi-statement lambdas use the `function` keyword with parenthesized parameters:

```uranite
function( I64 a, I64 b ) -> I64:
    I64 sum = a + b
    return sum * 2
```

Block lambdas support inline single-expression form when the colon is followed by an expression (no newline/indent):

```uranite
function( I64 x ) -> I64: x * x
```

### Closure Capture

Lambdas capture variables from the enclosing scope:

```uranite
I64 factor = 3
Callable<I64, <I64>> multiplier = lambda I64 x: x * factor
puts( multiplier( 7 ) )
```

Capture is implemented through global variables — at the lambda definition site, captured values are stored to globals (`_UR_lambda_N.cap.varName`), and a wrapper function loads these globals before calling the actual lambda function.

---

## Callable Type

The `Callable<ReturnType, <ParamTypes>>` type annotation declares a variable that holds a function pointer:

```uranite
Callable<I64, <I64>> doubler = lambda I64 x: x * 2
I64 result = doubler( 21 )
```

The first generic argument is the return type. The second is a type list of parameter types wrapped in angle brackets.

Higher-order functions accept `Callable` parameters:

```uranite
function apply( Callable<I64, <I64>> operation, I64 value ) -> I64:
    return operation( value )

I64 squared = apply( lambda I64 x: x * x, 5 )
```

---

## AST Representation

### FunctionDeclaration

Defined in `src/uranite/ast/node.hpp:1058-1120`:

```
struct FunctionDeclaration : Declaration
    body              : std::vector<StatementSharedPointer>
    genericParameters : std::vector<GenericParameterSharedPointer>
    isAbstract        : bool (default false)
    isAsync           : bool (default false)
    isConst           : bool (default false)
    isFinal           : bool (default false)
    isGenerator       : bool (default false)
    isNative          : bool (default false)
    isOverride        : bool (default false)
    isProperty        : bool (default false)
    isStatic          : bool (default false)
    isVirtual         : bool (default false)
    name              : std::string
    nestedFunctions   : std::vector<DeclarationSharedPointer>
    parameters        : std::vector<FunctionParameterSharedPointer>
    raisesTypes       : std::vector<TypeNodeSharedPointer>
    returnType        : TypeNodeSharedPointer
```

### FunctionParameterNode

Defined in `src/uranite/ast/node.hpp:729-779`:

```
struct FunctionParameterNode : Node
    propertyAccess : AccessModifier (default Default)
    defaultValue   : ExpressionSharedPointer
    isMutable      : bool (default false)
    isProperty     : bool (default false)
    isReadonly     : bool (default false)
    isReference    : bool (default false)
    isSelf         : bool (default false)
    isVariadic     : bool (default false)
    isKeyword      : bool (default false)
    name           : std::string
    type           : TypeNodeSharedPointer
```

### LambdaExpression

Defined in `src/uranite/ast/node.hpp:1537-1569`:

```
struct LambdaExpression : Expression
    body       : std::vector<StatementSharedPointer>
    isAsync    : bool (default false)
    parameters : std::vector<FunctionParameterSharedPointer>
    returnType : TypeNodeSharedPointer
```

---

## Parsing Implementation

### Function Header Parsing

At `parser.cpp:1384-1438`:

```
parseFunctionDeclaration(access, isVirtual, isOverride, isAbstract, isStatic):
    match(KeywordFunction) or expect(KeywordProperty)
    name = expect(Identifier)
    genericParameters = parseGenericParameters()
    expect(LeftParenthesis)
    parameters = parseFunctionParameters()
    expect(RightParenthesis)

    if match(Arrow):
        returnType = parseTypeNode()

    if check(KeywordRaises):
        advance()
        raisesTypes.push_back(parseTypeNode())
        while match(Pipe):
            raisesTypes.push_back(parseTypeNode())

    if match(Semicolon):             // forward declaration
        isAbstract = true
        return declaration

    if match(Colon):
        expectNewline()
        if not isAbstract and match(Indent):
            while not Dedent or Eof:
                if KeywordFunction:  // nested function
                    nestedFunctions.push_back(parseFunctionDeclaration())
                else:
                    body.push_back(parseStatement())
            match(Dedent)
```

### Parameter Parsing

At `parser.cpp:1440-1541`:

```
parseFunctionParameter():
    // self parameter
    if check(KeywordSelf):
        return parameter(isSelf=true)
    if check(Ampersand) and peek(KeywordSelf):
        return parameter(isSelf=true, isReference=true)

    // property parameter (access modifier prefix)
    if check(KeywordPublic|KeywordPrivate|KeywordProtect):
        propertyAccess = resolveAccess()
        isProperty = true
        if match(KeywordReadonly): isReadonly = true

    isMutable = match(KeywordMutable)
    type = parseTypeNode()
    name = expect(Identifier)

    // variadic: Type name[]
    if check(LeftBracket) and peek(RightBracket):
        type = ArrayTypeNode(type)
        isVariadic = true

    // keyword: Type name{}
    if check(LeftBrace) and peek(RightBrace):
        isKeyword = true

    // default value: = expression
    if match(Assignment):
        defaultValue = parseExpression()

parseFunctionParameters():
    parameters = []
    do: parameters.push_back(parseFunctionParameter())
    while match(Comma)

    // validation
    only one variadic allowed
    only one keyword allowed (must be last)
    no positional parameters after variadic (use default values)
```

### Lambda Parsing

At `parser.cpp:1802-1843`:

```
parseLambdaExpression():
    // shorthand: lambda Type name, Type name: expression
    if match(KeywordLambda):
        do:
            isFinal = match(KeywordFinal)
            paramType = parseTypeNode()
            paramName = expect(Identifier)
            parameter.isMutable = (isFinal == false)
        while match(Comma)
        expect(Colon)
        expression = parseExpression()
        body = [ReturnStatement(expression)]
        return LambdaExpression(parameters, null, body)

    // block: function( params ) -> ReturnType: body
    expect(KeywordFunction)
    expect(LeftParenthesis)
    parameters = parseFunctionParameters()
    expect(RightParenthesis)
    if match(Arrow): returnType = parseTypeNode()
    if match(Colon):
        if not newline/indent:       // inline expression
            expression = parseExpression()
            body = [ReturnStatement(expression)]
        else:                         // block body
            body = parseStatementBlock()
    return LambdaExpression(parameters, returnType, body)
```

---

## Semantic Analysis

### Function Declaration Analysis

At `analyzer.cpp:2696-2900`:

```
analyzeFunctionDeclaration(declaration):
    // register generic parameters
    for generic in declaration.genericParameters:
        save existing type, register GenericParameterType with constraints

    // collect allowed generic names (function + enclosing class)
    allowedGenericNames = functionGenericNames + enclosingClassGenerics
    validate: unknown types in parameter/return positions flagged

    // detect generator
    if body contains yield and return type is not Generator<T>: error

    // resolve parameters
    for parameter in declaration.parameters:
        if self:
            parameterTypes.push_back(currentScope.classType)
            if not inside class: error
        elif variadic:
            variadicElementType = extractElementType(paramType)
            parameterTypes.push_back(monomorphize("Args", [elementType]))
        elif keyword:
            keywordValueType = paramType
            parameterTypes.push_back(monomorphize("Kwargs", [paramType]))
        else:
            parameterTypes.push_back(paramType)
            if after variadic and has default: keyword-only param

    // resolve return type
    returnType = declaration.returnType or Void
    if async and not Future<T>: error
    if generator: wrap return type in Generator<T>

    // create FunctionType and register symbol
    functionType = makeFunction(parameterTypes, returnType)
    set parameterNames, variadicParameterIndex, keywordParameterIndex,
        variadicElementType, keywordValueType, keywordOnlyParams,
        genericParameterNames, exceptionTypes
    define function symbol in current scope

    // analyze body in new scope
    pushScope(Function)
    register parameters as local symbols
    analyze body statements
    popScope()
```

### Call Expression Analysis

At `analyzer.cpp:1271-1570`:

```
analyzeCallExpression(expression):
    calleeType = analyzeExpression(callee)

    if calleeType.kind == Callable:
        check argument types against parameterTypes
        return callableType.returnType

    if calleeType.kind != Function:
        if Interface or Trait: error "cannot instantiate"
        if Class: error "use new ClassName(...)"
        if Struct: error "use new StructName(...)"
        error "expression is not callable"

    // overload resolution (if multiple candidates)
    if identifier callee and multiple symbols exist:
        resolve overloads (see below)

    // arity validation
    fixedParamCount = variadicParameterIndex or keywordParameterIndex or parameterTypes.size()
    check: totalSupplied >= fixedParamCount
    check: positional <= fixedParamCount (unless variadic)
    validate keyword arguments against keywordParameterIndex or keywordOnlyParamNames

    // generic substitution
    if generic function: apply type arguments to parameter/return types

    // type-check each argument
    for each positional: check isAssignable(expectedType, argumentType)
    for variadic args: check against variadicElementType
    for keyword args: check against keywordValueType or keywordOnlyParamTypes

    return substituted returnType
```

### Overload Resolution

At `analyzer.cpp:1334-1417`:

```
overloadResolution(candidates, argumentTypes):
    for each candidate function:
        check arity compatibility (with variadic consideration)
        score = 0
        for each parameter:
            exact type match: score += 2
            assignable match: score += 1
            incompatible: skip candidate

        if score > bestScore:
            bestScore = score
            bestCandidate = candidate
        elif score == bestScore:
            prefer exact-arity match over variadic
            else: mark ambiguous

    if ambiguous: error "ambiguous call to overloaded function"
    return bestCandidate
```

Score-based ranking: exact type matches score 2 points, assignable matches score 1. On tied scores, exact-arity candidates win over variadic candidates.

### Lambda Analysis

At `analyzer.cpp:2333-2358`:

```
analyzeLambdaExpression(expression):
    pushScope(Function)
    for parameter in expression.parameters:
        resolve parameter type
        register as local symbol

    returnType = expression.returnType or inferred
    if returnType null and body[0] is ReturnStatement:
        returnType = analyzeExpression(body[0].value)
    if returnType still null: returnType = Void

    popScope()
    return makeFunction(parameterTypes, returnType)
```

Lambda return type inference examines the first statement — if it is a `ReturnStatement`, the return type is inferred from the returned expression. This enables shorthand lambdas (`lambda I64 x: x * 2`) to have their return type automatically determined.

---

## Compilation Pipeline

### HIR Stage — Functions

`HIRFunctionDefinition` at `hir.hpp:1052-1084`:

```
struct HIRFunctionDefinition : HIRNode
    functionName                : std::string
    mangledName                 : std::string
    ownerClassName              : std::string
    accessModifier              : AccessModifier
    parameterDescriptors        : std::vector<HIRParameterDescriptor>
    returnTypeDescriptor        : TypeSharedPointer
    functionBody                : std::shared_ptr<HIRBlock>
    genericParameters           : std::vector<HIRGenericParameterDescriptor>
    isVirtualMethod             : bool
    isStaticMethod              : bool
    isAsyncFunction             : bool
    isOverrideMethod            : bool
    isAbstractMethod            : bool
    isFinalMethod               : bool
    isConstMethod               : bool
    isGeneratorFunction         : bool
    isNativeMethod              : bool
    isPropertyMethod            : bool
    raisesTypes                 : std::vector<TypeSharedPointer>
    nestedFunctionDefinitions   : std::vector<HIRFunctionDefinition>
```

HIR lowering at `hir/lowering.cpp:243-296` creates `HIRFunctionDefinition` by:
1. Saving and re-registering generic parameters in the type registry
2. Resolving return type from AST type node
3. Copying all modifier flags from AST declaration
4. Generating mangled name from module path: `modulePath.functionName`
5. Lowering parameters via `lowerParameter()`
6. Lowering generic parameters via `lowerGenericParameter()`
7. Lowering body statements
8. Restoring original generic type bindings

### HIR Stage — Lambdas

`HIRLambda` at `hir.hpp:839-852`:

```
struct HIRLambda : HIRNode
    parameterDescriptors : std::vector<HIRParameterDescriptor>
    returnTypeDescriptor : TypeSharedPointer
    lambdaBody           : std::shared_ptr<HIRBlock>
    isAsyncLambda        : bool (default false)
```

### MIR Stage — Function Lowering

At `mir/lowering.cpp:327-500`:

```
lowerFunctionDefinition(hirFunction):
    mirFunction = new MIRFunctionDefinition()
    copy name, mangledName, ownerClassQualifiedName, returnType, source
    copy isGeneratorFunction, isAsyncFunction

    // extract async/generator inner types
    if async: extract Future<T>.innerType → asyncInnerReturnType
    if generator: extract Generator<T>.yieldType → generatorYieldType

    // create entry block
    entryBlock = mirFunction.createBasicBlock("entry")
    mirFunction.entryBlockIdentifier = entryBlock.blockIdentifier

    // allocate parameters
    for each parameterDescriptor:
        paramVariable = mirFunction.allocateVariable(name, type, isMutable)
        parameterVariableIdentifiers.push_back(paramVariable)
        variableNameMap[name] = paramVariable

        if variadic:
            mirFunction.variadicParameterIndex = paramIndex
            extract element type from Args<T> typeSubstitutions
        if keyword:
            mirFunction.keywordParameterIndex = paramIndex
            extract value type from Kwargs<T> typeSubstitutions

        if defaultValue:
            capture as MIRModuleConstant (String/Integer/Float/Boolean/Null)
            store in parameterDefaultValues[paramIndex]

    // lower body
    lowerBlock(hirFunction.functionBody)

    // lower nested functions
    for nestedFunc in hirFunction.nestedFunctionDefinitions:
        lowerNestedFunction(nestedFunc)
```

`MIRFunctionDefinition` at `mir.hpp:256-303`:

```
struct MIRFunctionDefinition
    functionName                    : std::string
    mangledFunctionName             : std::string
    ownerClassQualifiedName         : std::string
    returnTypeDescriptor            : TypeSharedPointer
    sourceLocation                  : SourceSharedPointer
    parameterVariableIdentifiers    : std::vector<MIRVariableIdentifier>
    controlFlowBlocks               : std::vector<MIRBasicBlock>
    variableDescriptorTable         : std::unordered_map<MIRVariableIdentifier, MIRVariableDescriptor>
    nextAvailableVariableIdentifier : MIRVariableIdentifier (default 0)
    entryBlockIdentifier            : MIRBlockIdentifier (default 0)
    variadicParameterIndex          : int (default -1)
    variadicElementType             : TypeSharedPointer
    keywordParameterIndex           : int (default -1)
    keywordValueType                : TypeSharedPointer
    parameterDefaultValues          : std::unordered_map<int, MIRModuleConstant>
    isGeneratorFunction             : bool (default false)
    generatorYieldType              : TypeSharedPointer
    isAsyncFunction                 : bool (default false)
    asyncInnerReturnType            : TypeSharedPointer
```

### MIR Stage — Nested Function Lowering

At `mir/lowering.cpp:642-728`, nested functions are hoisted to module-level with captured variables as extra parameters:

```
lowerNestedFunction(hirNested):
    // collect captured variables (free variable analysis)
    boundNames = parameter names
    capturedNames = identifiers used in body that exist in outer scope

    // save current function state
    savedFunction, savedBlock, savedVarMap = current state

    // create new MIR function
    mirNested = new MIRFunctionDefinition()
    copy name, mangledName, returnType, source

    // allocate parameters (skip self)
    for each parameterDescriptor:
        allocate as parameter variable

    // add captured variables as extra parameters
    for captureName in capturedNames:
        captureType = outer function's variable type
        captureParam = allocateVariable(captureName, captureType)
        mark as parameter variable
        variableNameMap[captureName] = captureParam

    // lower body
    lowerBlock(hirNested.functionBody)
    ensureBlockTerminated()

    // push to module-level functions
    currentModule.functionDefinitions.push_back(mirNested)

    // restore state, store capture map
    nestedFunctionCaptures[name] = capturedNames
```

Call sites to nested functions inject captured variable values as extra arguments — the capture map is consulted during instruction post-processing.

### MIR Stage — Lambda Lowering

At `mir/lowering.cpp:2547-2776`, lambda expressions produce standalone functions with closure support:

```
lowerLambdaExpression(hirLambda):
    lambdaName = "_UR_lambda_{counter++}"

    // free variable analysis (recursive walk of lambda body)
    capturedNames = identifiers from outer scope used in body
    hasCaptures = capturedNames not empty

    // create lambda function
    lambdaFunction = new MIRFunctionDefinition()
    lambdaFunction.functionName = lambdaName
    allocate parameters + captured variables as extra parameters
    lower body
    push to module-level functions

    // if captures exist: create wrapper function
    if hasCaptures:
        wrapperName = "{lambdaName}.wrap"
        wrapperFunction = new MIRFunctionDefinition()
        wrapperFunction takes only original parameters
        wrapper body:
            for each capture: LoadVariable from global "@{lambdaName}.cap.{captureName}"
            CallFunction lambdaName with [params..., captures...]
            ReturnValue result
        push wrapper to module-level functions

    // back in original function: store captures to globals
    if hasCaptures:
        for captureName in capturedNames:
            create MIRGlobalVariable "{lambdaName}.cap.{captureName}"
            StoreVariable outerVar → "@{globalName}"

    // emit AddressOf instruction
    addressFunctionName = hasCaptures ? wrapperName : lambdaName
    emit AddressOf(addressFunctionName) → resultVariable
    return resultVariable
```

Capture mechanism:
1. Lambda body becomes a standalone function with captures as extra parameters
2. A wrapper function loads captured values from global variables and forwards them
3. At the lambda definition site, outer variable values are stored to these globals
4. The wrapper function address (not the lambda itself) becomes the callable pointer

### Codegen — Function Pre-Registration

At `codegen.cpp:277-310`, all functions are pre-registered before codegen begins:

```
for functionDefinition in mirModule.functionDefinitions:
    llvmFunctionName = compute name (ownerClass.functionName or mangledName)

    if name already registered:
        // overload: rename both with arity suffix
        existingSuffix = "{name}#{existingArgCount}"
        functionResolutionMap[existingSuffix] = existingFunction
        llvmFunctionName = "{name}#{newArgCount}"

    // create LLVM function declaration
    returnType = toLLVMType(returnTypeDescriptor)
    if main: returnType = i32
    create FunctionType and Function
    functionResolutionMap[llvmFunctionName] = llvmFunction
```

Arity suffix naming (`FunctionName#paramCount`) enables overloaded functions with different parameter counts to coexist in the same `functionResolutionMap`.

### Codegen — Function Generation

At `codegen.cpp:795-1047`:

```
generateFunction(functionDefinition):
    // resolve or create LLVM function
    llvmFunctionName = ownerClass.name or mangledName or functionName
    llvmFunction = lookup or create in functionResolutionMap

    // dispatch async/generator to specialized generators
    if isGenerator: generateGeneratorFunction(); return
    if isAsync: generateAsyncFunction(); return

    // create LLVM basic blocks
    for each MIR control flow block:
        blockMap[blockId] = BasicBlock::Create(label, llvmFunction)

    // allocate parameters
    SetInsertPoint(entryBlock)
    for each parameter:
        argument = llvmFunction.getArg(index)
        alloca = createEntryBlockAllocation(parameterName, argType)
        CreateStore(argument, alloca)
        variableValueMap[paramVariable] = alloca

    // main function special handling: argc/argv setup
    if isMain:
        argc → global "argc", argv → ArrayList struct setup

    // shadow stack push
    emitPushFrame(filename, line, column, displayName)

    // async runtime init
    if isMain and programHasAsyncFunctions:
        CreateCall(uraniteSchedulerInit)

    // generate basic blocks
    for each controlFlowBlock:
        generateBasicBlock(block, functionDefinition)

    // ensure all blocks terminate
    for each LLVM basic block without terminator:
        emitPopFrame()
        CreateRetVoid() or CreateRet(null)
```

Method names are qualified: `ClassName.methodName`. Main function receives `(i32 argc, ptr argv)` and returns `i32`.

### Codegen — Shadow Stack

At `codegen.cpp:7238-7250`:

```
emitPushFrame(file, line, column, functionName):
    pushFrame = getOrCreatePushFrame()
    fileStr = CreateGlobalStringPtr(file)
    lineVal = ConstantInt(line)
    colVal = ConstantInt(column)
    funcStr = CreateGlobalStringPtr(functionName)
    CreateCall(pushFrame, [fileStr, lineVal, colVal, funcStr])

emitPopFrame():
    popFrame = getOrCreatePopFrame()
    CreateCall(popFrame, [])
```

Push frame is called once at function entry. Pop frame is called before every `return` statement — return values are evaluated before the pop. The runtime functions (`__uranite_push_frame`, `__uranite_pop_frame`) are resolved through the `RuntimeInterface`.

### Codegen — Return Emission

At `codegen.cpp:5540-5625`, return statements emit `emitPopFrame()` before `CreateRet()`:

```
generateReturn(instruction):
    // async main: call uraniteRunScheduler before returning
    if main and hasAsyncFunctions:
        CreateCall(uraniteRunScheduler)

    if void return:
        emitPopFrame()
        CreateRetVoid()
        return

    returnValue = loadVariableValue(sourceOperands[0])

    // type coercion for return value
    if returnValue.type != expectedReturnType:
        integer widening/narrowing: CreateSExt/CreateTrunc
        pointer cast: CreateBitCast
        pointer to integer: CreatePtrToInt
        integer to pointer: CreateIntToPtr
        integer to float: CreateSIToFP
        float to integer: CreateFPToSI

    emitPopFrame()
    CreateRet(returnValue)
```

### Codegen — toLLVMType

At `codegen.cpp:6661-6663`:

```
case Kind::Function:
case Kind::Callable:
    return PointerType::getUnqual(context)
```

Both function types become opaque pointers — function values are addresses of LLVM functions.

---

## Assignability Rules

| Assignment | Rule |
|---|---|
| `Function` → `Function` | Parameter types and return type must match |
| `Function` → `Callable` | Allowed — function pointer is compatible with callable |
| `Lambda` → `Callable` | Allowed — lambda produces a function pointer |
| `Callable` → `Function` | Not directly assignable (different type kinds) |
| `GenericParameter` | Always assignable |

---

## Examples

### Recursion and Nested Functions

```uranite
from uranite.io.console import puts

public function factorial( I64 n ) -> I64:
    if n <= 1:
        return 1
    return n * factorial( n - 1 )

public function outer( I64 x ) -> I64:
    function inner( I64 y ) -> I64:
        return y * y
    return inner( x ) + 1

public function compute( I64 a, I64 b ) -> I64:
    function square( I64 x ) -> I64:
        return x * x
    function add( I64 x, I64 y ) -> I64:
        return x + y
    return add( square( a ), square( b ) )

public function main() -> I32:
    puts( "factorial(5) = ", factorial( 5 ) )
    puts( "outer(5) = ", outer( 5 ) )
    puts( "compute(3, 4) = ", compute( 3, 4 ) )
    return 0
```

`factorial` demonstrates direct recursion. `outer` contains a nested `inner` function — during MIR lowering, `inner` is hoisted to module-level as a standalone function. `compute` nests multiple functions that call each other.

### Higher-Order Functions and Lambdas

```uranite
from uranite.io.console import puts

function apply( Callable<I64, <I64>> operation, I64 value ) -> I64:
    return operation( value )

function applyBinary( Callable<I64, <I64, I64>> operation, I64 a, I64 b ) -> I64:
    return operation( a, b )

public function main() -> I32:
    Callable<I64, <I64>> doubler = lambda I64 x: x * 2
    I64 result = doubler( 21 )
    puts( "doubler(21) = ", result )

    I64 squared = apply( lambda I64 x: x * x, 5 )
    puts( "apply(square, 5) = ", squared )

    I64 sum = applyBinary( lambda I64 a, I64 b: a + b, 10, 20 )
    puts( "applyBinary(add, 10, 20) = ", sum )

    I64 factor = 3
    Callable<I64, <I64>> multiplier = lambda I64 x: x * factor
    puts( "multiplier(7) = ", multiplier( 7 ) )
    return 0
```

`apply` and `applyBinary` accept `Callable` parameters — at the LLVM level, these are opaque pointers to function addresses. `doubler` stores a lambda's address in a `Callable` variable. `multiplier` captures `factor` from the enclosing scope — the MIR lowering creates a global variable `_UR_lambda_N.cap.factor`, stores `factor`'s value to it at the definition site, and generates a wrapper function that loads the capture before calling the actual lambda.

### Defer Statements

```uranite
from uranite.io.console import puts

public function with_defer() -> Void:
    defer puts( "third: cleanup" )
    defer puts( "second: release" )
    puts( "first: work" )

public function with_defer_block() -> Void:
    defer:
        puts( "deferred block executed" )
    puts( "main work done" )

public function main() -> I32:
    with_defer()
    with_defer_block()
    return 0
```

`defer` schedules statements for execution in reverse order before the function returns. Multiple `defer` statements stack — the last deferred executes first. `defer:` with an indented block defers an entire block of statements. Deferred statements execute after the return value is evaluated but before `emitPopFrame()` pops the shadow stack frame.
