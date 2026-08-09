# Lambda Expressions

Uranite supports anonymous functions through two syntactic forms: the concise `lambda` form (single-expression body, no return type annotation) and the full `function` form (multi-statement body, optional return type annotation, full parameter syntax). Both produce `LambdaExpression` AST nodes, resolve to `FunctionType` during semantic analysis, and are lowered to standalone MIR function definitions during MIR lowering. Lambda expressions support closures — free variables referenced inside the lambda body are automatically captured by value through a global variable bridging mechanism. The `async` keyword can prefix either form to create asynchronous lambdas.

---

## Table of Contents

- [Lambda Expressions](#lambda-expressions)
  - [Table of Contents](#table-of-contents)
  - [Syntax](#syntax)
  - [AST Representation — LambdaExpression](#ast-representation--lambdaexpression)
  - [Parsing](#parsing)
    - [Concise Lambda Form](#concise-lambda-form)
    - [Full Function Form](#full-function-form)
    - [Async Lambdas](#async-lambdas)
  - [Semantic Analysis](#semantic-analysis)
    - [Parameter Type Resolution](#parameter-type-resolution)
    - [Return Type Inference](#return-type-inference)
    - [FunctionType Construction](#functiontype-construction)
  - [HIR Representation — HIRLambda](#hir-representation--hirlambda)
  - [HIR Lowering](#hir-lowering)
  - [HIR Validation](#hir-validation)
  - [MIR Lowering](#mir-lowering)
    - [Lambda Name Generation](#lambda-name-generation)
    - [Free Variable Collection](#free-variable-collection)
    - [Lambda Function Definition](#lambda-function-definition)
    - [Capture Parameters](#capture-parameters)
    - [Wrapper Function for Closures](#wrapper-function-for-closures)
    - [Global Variable Bridging](#global-variable-bridging)
    - [Function Pointer Result](#function-pointer-result)
  - [LLVM Code Generation](#llvm-code-generation)
    - [AddressOf — Function Pointer](#addressof--function-pointer)
    - [StoreVariable — Global Capture Storage](#storevariable--global-capture-storage)
  - [Examples](#examples)
    - [Concise Lambda — Map Callback](#concise-lambda--map-callback)
    - [Full Function Form — Multi-Statement](#full-function-form--multi-statement)
    - [Closure — Capturing Outer Variable](#closure--capturing-outer-variable)
    - [Async Lambda](#async-lambda)
    - [Lambda with Final Parameter](#lambda-with-final-parameter)
    - [Lambda Capturing Multiple Variables](#lambda-capturing-multiple-variables)

---

## Syntax

Two forms:

```
lambda Type param1, Type param2: expression
```

```
function(Type param1, Type param2) -> ReturnType:
    statement1
    statement2
    return result
```

Both can be prefixed with `async`:

```
async lambda I64 value: value * 2
async function(I64 value) -> I64:
    return value * 2
```

| Form | Parameters | Return Type | Body | Use Case |
|---|---|---|---|---|
| `lambda` | Typed, comma-separated, no parens | Inferred | Single expression (auto-wrapped in return) | Short transformations, callbacks |
| `function(...)` | Full parameter syntax with parens | Optional `-> Type` annotation | Multi-statement block | Complex logic, multiple statements |

---

## AST Representation — LambdaExpression

Defined at `src/uranite/ast/node.hpp:1537-1567`:

```cpp
struct LambdaExpression : Expression {

    std::vector<StatementSharedPointer> body;
    bool isAsync = false;
    std::vector<FunctionParameterSharedPointer> parameters;
    TypeNodeSharedPointer returnType;

    LambdaExpression(
        std::vector<FunctionParameterSharedPointer> parameters,
        TypeNodeSharedPointer returnType,
        std::vector<StatementSharedPointer> body,
        const lookup::SourceSharedPointer& source
    ) : Expression( Node::Kind::LambdaExpression, source ),
        body( std::move( body ) ),
        parameters( std::move( parameters ) ),
        returnType( std::move( returnType ) ) {}
};
```

| Field | Type | Description |
|---|---|---|
| `body` | `vector<StatementSharedPointer>` | Function body statements |
| `isAsync` | `bool` | Whether prefixed with `async` |
| `parameters` | `vector<FunctionParameterSharedPointer>` | Parameter list with types |
| `returnType` | `TypeNodeSharedPointer` | Explicit return type (null for concise lambda form) |

Both syntactic forms produce the same AST node type. Concise lambdas wrap their expression body in a `ReturnStatement` during parsing, so at AST level the body is always a statement list.

---

## Parsing

At `src/uranite/parser/parser.cpp:1802-1842`, `parseLambdaExpression()` handles both forms.

### Concise Lambda Form

`src/uranite/parser/parser.cpp:1804-1819`:

```cpp
if( this->match( token::Type::KeywordLambda ) ) {
    std::vector<ast::nodes::FunctionParameterSharedPointer>
        parameters;
    do {
        bool isFinal =
            this->match( token::Type::KeywordFinal );
        ast::nodes::TypeNodeSharedPointer parameterType =
            this->parseTypeNode();
        std::string parameterName = this->expect(
            token::Type::Identifier,
            "expected parameter name" ).value;
        auto parameter = std::make_shared<
            ast::nodes::FunctionParameterNode>(
                parameterName, parameterType, source );
        parameter->isMutable = isFinal == false;
        parameters.push_back( parameter );
    }
    while( this->match( token::Type::Comma ) );
    this->expect( token::Type::Colon,
        "expected ':' after lambda parameters" );
    ast::nodes::ExpressionSharedPointer expression =
        this->parseExpression();
    auto returnStatement = std::make_shared<
        ast::nodes::ReturnStatement>(
            expression, expression->source );
    std::vector<ast::nodes::StatementSharedPointer> body(
        { returnStatement } );
    return std::make_shared<ast::nodes::LambdaExpression>(
        std::move( parameters ), nullptr,
        std::move( body ), source );
}
```

Syntax: `lambda [final] Type name [, [final] Type name ...]: expression`

Key details:
- Parameters are comma-separated with required type annotations.
- `final` keyword before a parameter makes it immutable (`isMutable = false`). Default is mutable.
- Colon separates parameters from body.
- Body is a single expression, automatically wrapped in a `ReturnStatement`.
- Return type is `nullptr` — inferred during semantic analysis.

### Full Function Form

`src/uranite/parser/parser.cpp:1821-1842`:

```cpp
this->expect( token::Type::KeywordFunction, "" );
this->expect( token::Type::LeftParenthesis,
    "expected '(' in lambda" );
std::vector<ast::nodes::FunctionParameterSharedPointer>
    parameters = this->parseFunctionParameters();
this->expect( token::Type::RightParenthesis,
    "expected ')' after lambda parameters" );
ast::nodes::TypeNodeSharedPointer returnType;
if( this->match( token::Type::Arrow ) ) {
    returnType = this->parseTypeNode();
}
if( this->match( token::Type::Colon ) ) {
    if( this->check(
            { token::Type::Indent, token::Type::Newline },
            false ) ) {
        // single-line body (no indent/newline after colon)
        auto expression = this->parseExpression();
        auto returnStatement = std::make_shared<
            ast::nodes::ReturnStatement>(
                expression, expression->source );
        std::vector<ast::nodes::StatementSharedPointer>
            body( { returnStatement } );
        return std::make_shared<
            ast::nodes::LambdaExpression>(
                std::move( parameters ), returnType,
                std::move( body ), source );
    }
    this->expectNewline( "lambda" );
    auto body = this->parseStatementBlock();
    return std::make_shared<ast::nodes::LambdaExpression>(
        std::move( parameters ), returnType,
        std::move( body ), source );
}
this->expectNewline( "lambda" );
auto body = this->parseStatementBlock();
return std::make_shared<ast::nodes::LambdaExpression>(
    std::move( parameters ), returnType,
    std::move( body ), source );
```

Syntax: `function(params) [-> ReturnType] [: body]`

Key details:
- Uses `parseFunctionParameters()` — supports full parameter syntax including `self`, `&self`, variadic (`Type name[]`), keyword (`Type name{}`), default values, and property parameters.
- Optional return type via `-> Type` arrow syntax.
- Three body paths: single-line after colon (when no indent/newline follows), indented block after colon, or indented block without colon.
- Single-line body gets auto-wrapped in `ReturnStatement`, same as concise form.

### Async Lambdas

At `src/uranite/parser/parser.cpp:2215-2224`:

```cpp
case token::Type::KeywordAsync:
    if( this->peek().type == token::Type::KeywordLambda ||
        this->peek().type == token::Type::KeywordFunction ) {
        this->advance();
        auto lambda = std::static_pointer_cast<
            ast::nodes::LambdaExpression>(
                this->parseLambdaExpression() );
        lambda->isAsync = true;
        return lambda;
    }
    this->diagnostic.error( this->current().source,
        "expected \"lambda\" or \"function\" "
        "after \"async\"" );
    return nullptr;
```

`async` prefix is handled in the primary expression dispatch. It advances past `async`, calls `parseLambdaExpression()` for the next token (`lambda` or `function`), then sets `isAsync = true` on the resulting node.

---

## Semantic Analysis

At `src/uranite/semantic/analyzer.cpp:2333-2358`:

```cpp
case ast::Node::Kind::LambdaExpression: {
    ast::nodes::LambdaExpression& lambdaExpression =
        static_cast<ast::nodes::LambdaExpression&>(
            *expression );
    this->pushScope( Scope::Kind::Function );
    std::vector<TypeSharedPointer> parameterTypes;
    for( auto& parameter : lambdaExpression.parameters ) {
        TypeSharedPointer parameterType =
            parameter->type
                ? this->resolveType( parameter->type )
                : this->typeRegistry.getError();
        parameterTypes.push_back( parameterType );
        SymbolSharedPointer parameterSymbol =
            std::make_shared<Symbol>(
                parameter->name, Symbol::Kind::Variable,
                parameter->source, parameterType );
        parameterSymbol->isInitialized = true;
        this->currentScope->define(
            parameter->name, parameterSymbol );
    }
    TypeSharedPointer returnType =
        lambdaExpression.returnType
            ? this->resolveType( lambdaExpression.returnType )
            : TypeSharedPointer( nullptr );
    if( returnType == nullptr &&
        lambdaExpression.body.empty() == false ) {
        if( lambdaExpression.body[0] &&
            lambdaExpression.body[0]->kind ==
                ast::Node::Kind::ReturnStatement ) {
            ast::nodes::ReturnStatement& returnStatement =
                static_cast<ast::nodes::ReturnStatement&>(
                    *lambdaExpression.body[0] );
            if( returnStatement.value ) {
                returnType = this->analyzeExpression(
                    returnStatement.value );
            }
        }
    }
    if( returnType == nullptr ) {
        returnType = this->typeRegistry.getVoid();
    }
    this->popScope();
    expressionType = this->typeRegistry.makeFunction(
        parameterTypes, returnType );
    break;
}
```

### Parameter Type Resolution

1. Push a new `Function` scope — lambda parameters are local to the lambda body.
2. For each parameter: resolve type annotation, or fall back to `Error` type if no annotation exists.
3. Create a `Symbol` for each parameter and define it in the lambda scope — enables the body to reference parameters.

### Return Type Inference

Three paths for return type determination:

| Priority | Condition | Result |
|---|---|---|
| 1 | Explicit `-> Type` annotation | Resolved type from annotation |
| 2 | No annotation, body starts with `ReturnStatement` | Type of the returned expression |
| 3 | No annotation, no return statement | `Void` |

For concise lambdas, the parser wraps the expression in a `ReturnStatement`, so path 2 fires — the return type is inferred from the expression type.

### FunctionType Construction

After resolving parameters and return type, the expression type is set via `typeRegistry.makeFunction(parameterTypes, returnType)`, which creates a `FunctionType` at `src/uranite/semantic/typeref.cpp:616-618`:

```cpp
TypeSharedPointer Registry::makeFunction(
    std::vector<TypeSharedPointer> parameters,
    TypeSharedPointer returnType, bool isVariadic ) {
    return std::make_shared<FunctionType>(
        std::move( parameters ),
        std::move( returnType ), isVariadic );
}
```

`FunctionType` (typeref.hpp:334) carries parameter types, return type, variadic/keyword parameter indices, and exception types.

---

## HIR Representation — HIRLambda

Defined at `src/uranite/ir/hir.hpp:838-852`:

```cpp
struct HIRLambda : HIRNode {

    std::vector<HIRParameterDescriptor> parameterDescriptors;
    semantic::TypeSharedPointer returnTypeDescriptor;
    std::shared_ptr<HIRBlock> lambdaBody;
    bool isAsyncLambda = false;

    HIRLambda(
        semantic::TypeSharedPointer resolvedType,
        const lookup::SourceSharedPointer& sourceLocation
    ) : HIRNode( HIRNodeKind::Lambda,
            std::move( resolvedType ), sourceLocation ) {
    }

};
```

| Field | Type | Description |
|---|---|---|
| `parameterDescriptors` | `vector<HIRParameterDescriptor>` | Lowered parameter descriptions |
| `returnTypeDescriptor` | `TypeSharedPointer` | Resolved return type |
| `lambdaBody` | `shared_ptr<HIRBlock>` | Lowered body block |
| `isAsyncLambda` | `bool` | Async flag |

The `resolvedType` inherited field carries the `FunctionType` from semantic analysis.

---

## HIR Lowering

At `src/uranite/ir/hir/lowering.cpp:1050-1061`:

```cpp
case ast::Node::Kind::LambdaExpression: {
    ast::nodes::LambdaExpression& lambdaExpression =
        static_cast<ast::nodes::LambdaExpression&>(
            *expression );
    std::shared_ptr<HIRLambda> hirLambda =
        std::make_shared<HIRLambda>(
            expression->semanticType, expression->source );
    hirLambda->isAsyncLambda = lambdaExpression.isAsync;
    for( const auto& parameter :
             lambdaExpression.parameters ) {
        if( parameter != nullptr ) {
            hirLambda->parameterDescriptors.push_back(
                this->lowerParameter( *parameter ) );
        }
    }
    hirLambda->returnTypeDescriptor =
        this->resolveTypeNode( lambdaExpression.returnType );
    hirLambda->lambdaBody = this->lowerStatementBlock(
        lambdaExpression.body, lambdaExpression.source );
    return hirLambda;
}
```

Straightforward: lowers each parameter via `lowerParameter`, resolves the return type node, lowers the body into an `HIRBlock`.

---

## HIR Validation

At `src/uranite/ir/hir/validator.cpp:504-509`:

```cpp
case HIRNodeKind::Lambda: {
    HIRLambda& lambda =
        static_cast<HIRLambda&>( *expression );
    if( lambda.lambdaBody != nullptr ) {
        this->validateBlock( *lambda.lambdaBody );
    }
    break;
}
```

Recursively validates the lambda body block. No structural validation on parameters or return type — only the body statements are checked.

---

## MIR Lowering

MIR lowering for lambdas is the most complex expression lowering in the compiler. At `src/uranite/ir/mir/lowering.cpp:2547-2776`, a single `HIRLambda` node generates one or two standalone function definitions, global variables for captures, and a function pointer result.

### Lambda Name Generation

```cpp
std::string lambdaName = fmt::format(
    "_UR_lambda_{}", this->lambdaCounter++ );
```

Each lambda gets a unique name `_UR_lambda_0`, `_UR_lambda_1`, etc., using a monotonically increasing counter. This name becomes the LLVM function name.

### Free Variable Collection

`src/uranite/ir/mir/lowering.cpp:2550-2637`:

```cpp
std::unordered_set<std::string> boundNames;
for( const auto& paramDescriptor :
         hirLambda.parameterDescriptors ) {
    boundNames.insert( paramDescriptor.parameterName );
}
std::vector<std::string> capturedNames;
std::unordered_set<std::string> seenCaptures;
std::function<void( const hir::HIRNodeSharedPointer& )>
    collectFreeVars = [&]( const hir::HIRNodeSharedPointer&
        node ) {
    if( node == nullptr ) return;
    switch( node->nodeKind ) {
        case hir::HIRNodeKind::Identifier: {
            hir::HIRIdentifier& ident =
                static_cast<hir::HIRIdentifier&>( *node );
            if( boundNames.count(
                    ident.identifierName ) == 0 &&
                this->variableNameMap.count(
                    ident.identifierName ) > 0 &&
                seenCaptures.count(
                    ident.identifierName ) == 0 ) {
                capturedNames.push_back(
                    ident.identifierName );
                seenCaptures.insert(
                    ident.identifierName );
            }
            break;
        }
        // ... recursive cases for BinaryOperation,
        //     UnaryOperation, FunctionCall, MethodCall,
        //     Return, Block, ExpressionStatement,
        //     FieldAccess, IndexAccess, Cast
    }
};
if( hirLambda.lambdaBody != nullptr ) {
    for( const auto& stmt :
             hirLambda.lambdaBody->blockStatements ) {
        collectFreeVars( stmt );
    }
}
```

Capture detection algorithm:
1. Build `boundNames` from lambda parameters — these are not captures.
2. Walk the entire lambda body recursively via `collectFreeVars`.
3. For each `HIRIdentifier` encountered: if the name is NOT a bound parameter AND exists in the outer `variableNameMap` AND has not been seen yet, add it to `capturedNames`.
4. The walk covers: identifiers, binary/unary operations, function/method calls, return statements, blocks, expression statements, field access, index access, and cast expressions.

This is a simple lexical capture — any outer variable referenced inside the lambda body is captured.

### Lambda Function Definition

`src/uranite/ir/mir/lowering.cpp:2642-2698`:

```cpp
std::shared_ptr<MIRFunctionDefinition> savedFunction =
    this->currentFunction;
std::shared_ptr<MIRBasicBlock> savedBlock = this->currentBlock;
std::unordered_map<std::string, MIRVariableIdentifier>
    savedVariableNameMap = this->variableNameMap;
// ... save all state

std::shared_ptr<MIRFunctionDefinition> lambdaFunction =
    std::make_shared<MIRFunctionDefinition>();
lambdaFunction->functionName = lambdaName;
lambdaFunction->returnTypeDescriptor =
    hirLambda.returnTypeDescriptor;
// ... return type fallback chain

this->currentFunction = lambdaFunction;
this->variableNameMap.clear();
this->currentClassName = "";
this->currentParentClassName = "";
```

State saving and restoration: MIR lowering saves all current state (function, block, variable map, class names, instruction counter), creates a fresh `MIRFunctionDefinition`, and switches context to lower the lambda body as a standalone function.

Return type resolution has three fallback levels:

| Priority | Source | Description |
|---|---|---|
| 1 | `hirLambda.returnTypeDescriptor` | Explicit from HIR |
| 2 | `hirLambda.resolvedType` as `FunctionType` | Extract from semantic function type |
| 3 | `I64` | Default fallback when no type information exists |

### Capture Parameters

`src/uranite/ir/mir/lowering.cpp:2680-2693`:

```cpp
for( const std::string& captureName : capturedNames ) {
    MIRVariableDescriptor* outerDescriptor = nullptr;
    MIRVariableIdentifier outerVariable =
        savedVariableNameMap[captureName];
    if( savedFunction->variableDescriptorTable.count(
            outerVariable ) > 0 ) {
        outerDescriptor = &savedFunction
            ->variableDescriptorTable[outerVariable];
    }
    semantic::TypeSharedPointer captureType =
        ( outerDescriptor != nullptr )
            ? outerDescriptor->variableType : nullptr;
    MIRVariableIdentifier captureParam =
        lambdaFunction->allocateVariable(
            fmt::format( "cap.{}", captureName ),
            captureType, false );
    lambdaFunction->variableDescriptorTable[captureParam]
        .isParameterVariable = true;
    lambdaFunction->parameterVariableIdentifiers.push_back(
        captureParam );
    this->variableNameMap[captureName] = captureParam;
}
```

Captured variables become additional parameters on the lambda function, named `cap.variableName`. The outer variable type is preserved. The `variableNameMap` is updated so references to the captured name inside the lambda body resolve to the capture parameter.

Lambda parameters come first in the parameter list, followed by capture parameters.

### Wrapper Function for Closures

`src/uranite/ir/mir/lowering.cpp:2700-2745`:

When captures exist, a wrapper function is generated:

```cpp
if( hasCaptures ) {
    std::shared_ptr<MIRFunctionDefinition> wrapperFunction =
        std::make_shared<MIRFunctionDefinition>();
    wrapperFunction->functionName = wrapperName;
    // ... setup

    // Wrapper has ONLY the lambda's declared parameters
    for( const auto& paramDescriptor :
             hirLambda.parameterDescriptors ) {
        MIRVariableIdentifier wrapperParam =
            wrapperFunction->allocateVariable( ... );
        wrapperFunction->parameterVariableIdentifiers
            .push_back( wrapperParam );
        wrapperParams.push_back( wrapperParam );
    }

    // Call inner lambda with declared params + captured globals
    MIRInstruction callInner(
        MIRInstructionKind::CallFunction );
    callInner.calledFunctionQualifiedName = lambdaName;
    for( MIRVariableIdentifier wrapperParam : wrapperParams ) {
        callInner.sourceOperands.push_back( wrapperParam );
    }
    for( const std::string& captureName : capturedNames ) {
        std::string globalName = fmt::format(
            "{}.cap.{}", lambdaName, captureName );
        // Load from global, append to call args
        MIRInstruction loadGlobal(
            MIRInstructionKind::LoadVariable );
        loadGlobal.calledFunctionQualifiedName =
            fmt::format( "@{}", globalName );
        // ...
        callInner.sourceOperands.push_back( loadedCapture );
    }
    // Return inner call result
}
```

Wrapper function name: `_UR_lambda_N.wrap`. The wrapper has the same parameter signature as the lambda declaration (without captures), making it callable as a regular function pointer. Inside, it:
1. Loads each captured variable from a global variable (`@_UR_lambda_N.cap.varName`).
2. Calls the inner lambda function with declared parameters + loaded captures.
3. Returns the inner call result.

This separation exists because function pointers have a fixed signature — the caller does not know about captures. The wrapper bridges the gap: the caller invokes the wrapper with declared arguments, and the wrapper loads captures from globals.

### Global Variable Bridging

`src/uranite/ir/mir/lowering.cpp:2753-2767`:

```cpp
if( hasCaptures ) {
    for( const std::string& captureName : capturedNames ) {
        std::string globalName = fmt::format(
            "{}.cap.{}", lambdaName, captureName );
        MIRVariableIdentifier outerVariable =
            this->variableNameMap[captureName];
        MIRGlobalVariable globalVar;
        globalVar.variableName = globalName;
        if( savedFunction->variableDescriptorTable.count(
                outerVariable ) > 0 ) {
            globalVar.variableType =
                savedFunction->variableDescriptorTable[
                    outerVariable].variableType;
        }
        this->currentModule->globalVariables[globalName] =
            globalVar;
        MIRInstruction storeCapture(
            MIRInstructionKind::StoreVariable );
        storeCapture.sourceOperands.push_back(
            outerVariable );
        storeCapture.calledFunctionQualifiedName =
            fmt::format( "@{}", globalName );
        this->emitInstruction( storeCapture );
    }
}
```

Back in the outer function context (after restoring state), the lowering:
1. Creates a `MIRGlobalVariable` for each captured variable, named `_UR_lambda_N.cap.varName`.
2. Emits a `StoreVariable` instruction that copies the outer variable value into the global.

This captures the value at the point where the lambda expression is evaluated — it is capture-by-value, not capture-by-reference. Changes to the outer variable after the lambda is created are not reflected inside the lambda.

### Function Pointer Result

`src/uranite/ir/mir/lowering.cpp:2769-2776`:

```cpp
std::string addressFunctionName =
    hasCaptures ? wrapperName : lambdaName;
MIRVariableIdentifier resultVariable =
    this->currentFunction->allocateVariable(
        "_lambda_ptr", hirLambda.resolvedType, false );
MIRInstruction addressOf(
    MIRInstructionKind::AddressOf );
addressOf.destinationVariable = resultVariable;
addressOf.calledFunctionQualifiedName =
    addressFunctionName;
return this->emitInstruction( addressOf );
```

The final result of a lambda expression is an `AddressOf` instruction that takes the address of either:
- The wrapper function (if captures exist) — callers invoke the wrapper, which loads globals and forwards.
- The lambda function directly (no captures) — callers invoke the lambda function directly.

The result is stored in a `_lambda_ptr` variable typed as the lambda's `FunctionType`.

---

## LLVM Code Generation

### AddressOf — Function Pointer

At `src/uranite/ir/mir/codegen.cpp:1282-1299`:

```cpp
case MIRInstructionKind::AddressOf: {
    if( instruction.destinationVariable !=
            INVALID_VARIABLE_IDENTIFIER ) {
        if( instruction.calledFunctionQualifiedName
                .empty() == false ) {
            llvm::Function* targetFunction = nullptr;
            if( this->functionResolutionMap.count(
                    instruction
                        .calledFunctionQualifiedName ) > 0 ) {
                targetFunction =
                    this->functionResolutionMap[
                        instruction
                            .calledFunctionQualifiedName];
            }
            if( targetFunction == nullptr ) {
                targetFunction =
                    this->llvmModule->getFunction(
                        instruction
                            .calledFunctionQualifiedName );
            }
            if( targetFunction != nullptr ) {
                this->setVariableValue(
                    instruction.destinationVariable,
                    targetFunction );
            }
            else {
                this->setVariableValue(
                    instruction.destinationVariable,
                    llvm::Constant::getNullValue(
                        llvm::PointerType::getUnqual(
                            this->llvmContext ) ) );
            }
        }
    }
}
```

Looks up the function name in the `functionResolutionMap` (pre-registered functions) or the LLVM module. If found, sets the destination variable to the LLVM `Function*` pointer directly. If not found, falls back to null pointer.

The LLVM `Function*` value IS a pointer — it can be stored, passed as argument, or called via `CreateCall`. No explicit `PtrToInt` is needed here because the function pointer is used as-is.

### StoreVariable — Global Capture Storage

At `src/uranite/ir/mir/codegen.cpp:1682-1701`:

```cpp
void MIRCodegen::generateStoreVariable(
        const MIRInstruction& instruction ) {
    if( instruction.calledFunctionQualifiedName
            .empty() == false &&
        instruction.calledFunctionQualifiedName[0] == '@' ) {
        std::string globalName =
            instruction.calledFunctionQualifiedName
                .substr( 1 );
        llvm::GlobalVariable* globalVar =
            this->llvmModule->getGlobalVariable(
                globalName, true );
        if( globalVar != nullptr &&
            instruction.sourceOperands.empty() == false ) {
            llvm::Value* sourceValue =
                this->loadVariableValue(
                    instruction.sourceOperands[0] );
            if( sourceValue != nullptr ) {
                // ... type coercion if needed
                this->irBuilder.CreateStore(
                    sourceValue, globalVar );
            }
            return;
        }
    }
}
```

Names prefixed with `@` are treated as global variable references. The `@` is stripped, the global is looked up in the LLVM module, and the source operand value is stored into it. Type coercion (int-to-int cast, float-to-float cast) is applied when the source value type does not match the global variable type.

---

## Examples

### Concise Lambda — Map Callback

```
package examples

from uranite.io.console import puts

public function apply(I64 value, Callable<I64, <I64>> transform) -> I64:
    return transform(value)

public function main() -> I32:
    I64 result = apply(5, lambda I64 value: value * 2)
    return 0
```

**Compilation trace for `lambda I64 value: value * 2`**:
1. **Parsing**: `lambda` keyword matched. Parameter: type `I64`, name `value`. Colon consumed. Expression `value * 2` parsed. Wrapped in `ReturnStatement`. Creates `LambdaExpression(params=[I64 value], returnType=null, body=[return value * 2])`.
2. **Semantic analysis**: Push function scope. Resolve `I64` for parameter. Define `value` symbol. No explicit return type — body[0] is `ReturnStatement`, analyze `value * 2` to get `I64`. Expression type = `FunctionType(params=[I64], return=I64)`.
3. **HIR lowering**: `HIRLambda(parameterDescriptors=[{name="value", type=I64}], returnTypeDescriptor=I64, lambdaBody=HIRBlock([HIRReturn(HIRBinaryOperation(mul, value, 2))]))`.
4. **MIR lowering**: Name = `_UR_lambda_0`. No captures (only `value` referenced, which is a bound parameter). Creates `MIRFunctionDefinition("_UR_lambda_0")` with one parameter. No wrapper needed. Emits `AddressOf(_UR_lambda_0)`.
5. **LLVM codegen**: `_UR_lambda_0` compiled as standalone LLVM function: `define i64 @_UR_lambda_0(i64 %value) { ... mul ... ret ... }`. `AddressOf` resolves to `@_UR_lambda_0` function pointer.

### Full Function Form — Multi-Statement

```
package examples

public function main() -> I32:
    Callable<I64, <I64, I64>> adder = function(I64 first, I64 second) -> I64:
        I64 sum = first + second
        return sum
    I64 result = adder(3, 4)
    return 0
```

**Compilation trace**:
1. **Parsing**: `function` keyword. `(I64 first, I64 second)` parsed via `parseFunctionParameters`. `-> I64` return type. `:` then indented block with two statements.
2. **Semantic**: Parameters typed `[I64, I64]`, return `I64`. Expression type = `FunctionType([I64, I64], I64)`.
3. **MIR**: `_UR_lambda_0` with two parameters, multi-statement body. No captures.

### Closure — Capturing Outer Variable

```
package examples

from uranite.io.console import puts

public function makeCounter(I64 start) -> Callable<I64, <>>:
    I64 current = start
    return lambda: current

public function main() -> I32:
    Callable<I64, <> counter = makeCounter(10)
    I64 value = counter()
    return 0
```

**Compilation trace for `lambda: current`**:
1. **MIR lowering**: Lambda name = `_UR_lambda_0`. Free variable collection walks body, finds `HIRIdentifier("current")`. `current` is NOT in bound parameters (none) and IS in outer `variableNameMap`. Captured: `["current"]`.
2. **Lambda function**: `_UR_lambda_0(cap.current)` — one capture parameter, no declared parameters.
3. **Wrapper function**: `_UR_lambda_0.wrap()` — no declared parameters. Body: loads `@_UR_lambda_0.cap.current` from global, calls `_UR_lambda_0(loadedCapture)`, returns result.
4. **Global bridging**: Creates `MIRGlobalVariable("_UR_lambda_0.cap.current")`. Emits `StoreVariable(current -> @_UR_lambda_0.cap.current)` in outer function.
5. **Result**: `AddressOf(_UR_lambda_0.wrap)` — callers get the wrapper pointer.

### Async Lambda

```
package examples

public async function fetchData(String url) -> String:
    return url

public function main() -> I32:
    Callable<Future<String>, <String>> fetcher = async lambda String url: fetchData(url)
    return 0
```

**Compilation trace**:
1. **Parsing**: `async` keyword consumed. Next is `lambda`. `parseLambdaExpression()` called. Sets `isAsync = true` on resulting node.
2. **HIR lowering**: `hirLambda->isAsyncLambda = true`.
3. **MIR lowering**: `isAsyncLambda` flag preserved on the generated function definition for async runtime integration.

### Lambda with Final Parameter

```
package examples

public function main() -> I32:
    Callable<I64, <I64>> square = lambda final I64 value: value * value
    I64 result = square(5)
    return 0
```

**Compilation trace**:
1. **Parsing**: `final` keyword matched before type. `isMutable = false` (since `isFinal == true`).
2. **Semantic**: Parameter defined as immutable symbol.
3. **Borrow checker**: Assignments to `value` inside the lambda body would be rejected.

### Lambda Capturing Multiple Variables

```
package examples

public function makeTransform(I64 scale, I64 offset) -> Callable<I64, <I64>>:
    return lambda I64 input: input * scale + offset

public function main() -> I32:
    Callable<I64, <I64>> transform = makeTransform(3, 10)
    I64 result = transform(5)
    return 0
```

**Compilation trace**:
1. **Free variable collection**: `input` is bound (parameter). `scale` is free (in outer scope). `offset` is free (in outer scope). Captured: `["scale", "offset"]`.
2. **Lambda function**: `_UR_lambda_0(I64 input, cap.scale, cap.offset)`.
3. **Wrapper**: `_UR_lambda_0.wrap(I64 input)`. Loads `@_UR_lambda_0.cap.scale` and `@_UR_lambda_0.cap.offset` from globals. Calls `_UR_lambda_0(input, loadedScale, loadedOffset)`.
4. **Global bridging**: Two globals created: `_UR_lambda_0.cap.scale` and `_UR_lambda_0.cap.offset`. Both stored from outer function variables.
5. **Result**: `AddressOf(_UR_lambda_0.wrap)`.
