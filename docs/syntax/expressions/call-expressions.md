# Call Expressions

A call expression invokes a function or callable object with positional arguments, keyword arguments, and optional generic type arguments. The syntax `callee(args)` or `callee<Types>(args)` produces a `CallExpression` AST node carrying the callee sub-expression, argument list, keyword argument list, and type argument list. Call expressions differ from method calls — method calls dispatch on a receiver object (`object.method(args)`), while call expressions invoke standalone functions, closures, or callable-typed values directly. The semantic analyzer resolves call expressions through two primary type paths: `Callable` types (closures, function references) perform direct parameter-by-parameter assignability checking, while `Function` types trigger the full overload resolution pipeline with scoring, generic validation, variadic/kwargs handling, and exception safety enforcement. MIR lowering translates call expressions into `CallFunction` or `InvokeFunction` instructions depending on exception context, with special handling for `super()` constructor delegation and nested function closure captures. LLVM codegen shares the same `generateCallFunction` dispatch used by method calls — the 11-priority function resolution cascade, builtin descriptor interception, interface itable dispatch, variadic/kwargs packing, and unified `emitCallOrInvoke` emission.

---

## Table of Contents

- [Syntax](#syntax)
- [AST Representation — CallExpression](#ast-representation--callexpression)
- [Parsing](#parsing)
  - [Path 1 — With Generic Type Arguments](#path-1--with-generic-type-arguments)
  - [Path 2 — Without Generic Type Arguments](#path-2--without-generic-type-arguments)
  - [Keyword Argument Detection](#keyword-argument-detection)
- [Semantic Analysis](#semantic-analysis)
  - [Callable Type Path](#callable-type-path)
  - [Non-Callable Guards](#non-callable-guards)
  - [Function Type Path](#function-type-path)
  - [Overload Resolution](#overload-resolution)
  - [Argument Count Validation](#argument-count-validation)
  - [Keyword Argument Validation](#keyword-argument-validation)
  - [Generic Substitution](#generic-substitution)
  - [Per-Argument Type Checking](#per-argument-type-checking)
  - [Variadic Argument Checking](#variadic-argument-checking)
  - [Kwargs Parameter Checking](#kwargs-parameter-checking)
  - [Exception Safety Enforcement](#exception-safety-enforcement)
- [HIR Representation — HIRFunctionCall](#hir-representation--hirfunctioncall)
- [HIR Lowering](#hir-lowering)
- [MIR Lowering — lowerFunctionCall](#mir-lowering--lowerfunctioncall)
  - [Callee Resolution](#callee-resolution)
  - [Super Constructor Delegation](#super-constructor-delegation)
  - [Nested Function Captures](#nested-function-captures)
  - [Keyword Argument Emission](#keyword-argument-emission)
  - [Exception-Aware Instruction Selection](#exception-aware-instruction-selection)
- [LLVM Code Generation — generateCallFunction](#llvm-code-generation--generatecallfunction)
- [Examples](#examples)

---

## Syntax

```
callee(arguments)
callee<TypeArgs>(arguments)
callee(positional, keyword=value)
```

| Form | Description |
|---|---|
| `function(args)` | Direct function call by name |
| `function<T>(args)` | Generic function call with explicit type arguments |
| `variable(args)` | Callable variable invocation (closure, function reference) |
| `function(a, b, name=value)` | Mixed positional and keyword arguments |
| `super(args)` | Parent class constructor delegation |

```
package examples

from uranite.io.console import puts

function greet(String name) -> Void:
    puts("Hello, " + name)

public function main() -> I32:
    greet("World")
    return 0
```

Positional arguments must precede keyword arguments. Keyword arguments use the `name=value` syntax where the name matches a parameter name in the target function signature.

---

## AST Representation — CallExpression

Defined at `src/uranite/ast/node.hpp:1258-1284`:

```cpp
struct CallExpression : Expression {

    std::vector<ExpressionSharedPointer> arguments;
    ExpressionSharedPointer callee;
    std::vector<KeywordArgument> keywordArguments;
    std::vector<TypeNodeSharedPointer> typeArguments;

    CallExpression(
        ExpressionSharedPointer callee,
        std::vector<ExpressionSharedPointer> arguments,
        const lookup::SourceSharedPointer& source
    ) : Expression( Node::Kind::CallExpression, source ),
        arguments( std::move( arguments ) ),
        callee( std::move( callee ) ) {
    }

};
```

| Field | Type | Description |
|---|---|---|
| `callee` | `ExpressionSharedPointer` | Expression being invoked — typically `IdentifierExpression` for named functions, but can be any expression producing a callable value |
| `arguments` | `vector<ExpressionSharedPointer>` | Positional arguments in order |
| `keywordArguments` | `vector<KeywordArgument>` | Named keyword arguments (`name=value` pairs), each carrying name string, value expression, and source location |
| `typeArguments` | `vector<TypeNodeSharedPointer>` | Explicit generic type arguments (empty when no `<Types>` provided) |

The `KeywordArgument` struct carries three fields: `name` (the parameter name string), `value` (the expression for the argument value), and `source` (source location for error reporting).

---

## Parsing

Call expressions are parsed as postfix operations in `parsePostfixExpression` at `src/uranite/parser/parser.cpp:1950-2011`. Two paths handle the distinction between calls with and without generic type arguments.

### Path 1 — With Generic Type Arguments

At `src/uranite/parser/parser.cpp:1950-1981`:

```cpp
std::vector<ast::nodes::TypeNodeSharedPointer> typeArgs =
    this->parseGenericArguments();
lookup::SourceSharedPointer source = this->current().source;
this->advance();
std::vector<ast::nodes::ExpressionSharedPointer> arguments;
std::vector<ast::nodes::KeywordArgument> keywordArguments;
bool seenKeywordArg = false;
if( this->check( token::Type::RightParenthesis, false ) ) {
    do {
        ast::nodes::ExpressionSharedPointer argExpr =
            this->parseExpression();
        if( argExpr->kind == ast::Node::Kind::IdentifierExpression &&
            this->check( token::Type::Assignment ) ) {
            ast::nodes::IdentifierExpression& identExpr =
                static_cast<ast::nodes::IdentifierExpression&>(
                    *argExpr );
            this->advance();
            ast::nodes::ExpressionSharedPointer valueExpr =
                this->parseExpression();
            keywordArguments.push_back(
                { identExpr.name, valueExpr, argExpr->source } );
            seenKeywordArg = true;
        }
        else {
            if( seenKeywordArg ) {
                this->diagnostic.error( argExpr->source,
                    "positional argument cannot follow keyword argument" );
            }
            arguments.push_back( argExpr );
        }
    }
    while( this->match( token::Type::Comma ) );
}
this->expect( token::Type::RightParenthesis,
    "expected ')' after arguments" );
ast::nodes::CallExpression* callExpr =
    new ast::nodes::CallExpression(
        expression, std::move( arguments ), source );
callExpr->keywordArguments = std::move( keywordArguments );
callExpr->typeArguments = std::move( typeArgs );
expression = std::shared_ptr<ast::nodes::CallExpression>( callExpr );
```

This path triggers when `parseGenericArguments()` successfully parses a `<Type, ...>` sequence followed by `(`. The type arguments are attached to the resulting `CallExpression` node.

### Path 2 — Without Generic Type Arguments

At `src/uranite/parser/parser.cpp:1982-2011`:

```cpp
else if( this->check( token::Type::LeftParenthesis ) ) {
    lookup::SourceSharedPointer source = this->current().source;
    this->advance();
    std::vector<ast::nodes::ExpressionSharedPointer> arguments;
    std::vector<ast::nodes::KeywordArgument> keywordArguments;
    bool seenKeywordArg = false;
    if( this->check( token::Type::RightParenthesis, false ) ) {
        do {
            ast::nodes::ExpressionSharedPointer argExpr =
                this->parseExpression();
            if( argExpr->kind ==
                    ast::Node::Kind::IdentifierExpression &&
                this->check( token::Type::Assignment ) ) {
                ast::nodes::IdentifierExpression& identExpr =
                    static_cast<ast::nodes::IdentifierExpression&>(
                        *argExpr );
                this->advance();
                ast::nodes::ExpressionSharedPointer valueExpr =
                    this->parseExpression();
                keywordArguments.push_back(
                    { identExpr.name, valueExpr, argExpr->source } );
                seenKeywordArg = true;
            }
            else {
                if( seenKeywordArg ) {
                    this->diagnostic.error( argExpr->source,
                        "positional argument cannot follow "
                        "keyword argument" );
                }
                arguments.push_back( argExpr );
            }
        }
        while( this->match( token::Type::Comma ) );
    }
    this->expect( token::Type::RightParenthesis,
        "expected ')' after arguments" );
    ast::nodes::CallExpression* callExpr =
        new ast::nodes::CallExpression(
            expression, std::move( arguments ), source );
    callExpr->keywordArguments = std::move( keywordArguments );
    expression =
        std::shared_ptr<ast::nodes::CallExpression>( callExpr );
}
```

This path triggers on a bare `(` token following an expression. Identical argument parsing logic, but no `typeArguments` are attached.

### Keyword Argument Detection

Both paths use the same inline detection: after parsing an argument expression, if the result is an `IdentifierExpression` and the next token is `Assignment` (`=`), the parser reinterprets the identifier as a keyword argument name. The `=` is consumed, the value expression is parsed, and the pair is added to `keywordArguments`.

The `seenKeywordArg` flag enforces strict ordering — once a keyword argument appears, all subsequent positional arguments produce the error "positional argument cannot follow keyword argument".

---

## Semantic Analysis

`analyzeCallExpression` at `src/uranite/semantic/analyzer.cpp:1271-1622` is the most complex expression analysis function. It handles callable types, function types with overload resolution, generic validation, variadic/kwargs checking, and exception safety.

### Callable Type Path

At `analyzer.cpp:1276-1288`:

```cpp
if( calleeType->kind == Type::Kind::Callable ) {
    CallableTypeSharedPointer callableType =
        std::static_pointer_cast<CallableType>( calleeType );
    for( size_t index = 0; index < expression.arguments.size();
         index++ ) {
        TypeSharedPointer argumentType =
            this->analyzeExpression( expression.arguments[index] );
        if( index < callableType->parameterTypes.size() &&
            argumentType &&
            this->typeRegistry.isAssignable(
                callableType->parameterTypes[index],
                argumentType ) == false ) {
            std::string expectedTypeName =
                callableType->parameterTypes[index]->toString();
            std::string foundTypeName = argumentType->toString();
            std::string argumentMismatchErrorMessage = fmt::format(
                "argument type mismatch: expected \"{}\" "
                "but found \"{}\"",
                expectedTypeName, foundTypeName );
            this->diagnostic.error(
                expression.arguments[index]->source,
                argumentMismatchErrorMessage );
        }
    }
    return callableType->returnType;
}
```

`Callable` types represent closures and function references. Analysis is direct: iterate arguments, check each against the callable's parameter types via `isAssignable`, return the callable's `returnType`. No overload resolution, no generic handling — callable types are already fully resolved.

### Non-Callable Guards

At `analyzer.cpp:1289-1313`, five guard clauses reject non-callable callee types with specific error messages:

| Callee Type | Error Message |
|---|---|
| `Interface` | `"cannot instantiate interface \"Name\""` |
| `Trait` | `"cannot instantiate trait \"Name\""` |
| `Class` (abstract) | `"cannot instantiate abstract class \"Name\""` |
| `Class` (concrete) | `"cannot call class \"Name\" as a function — use \"new Name(...)\" to construct an instance"` |
| `Struct` | `"cannot call struct \"Name\" as a function — use \"new Name(...)\" to construct an instance"` |
| Other | `"expression is not callable"` |

Classes and structs require `new` for construction — bare `ClassName(args)` is rejected. This separates function call syntax from constructor syntax.

### Function Type Path

At `analyzer.cpp:1314-1330`, the function type path begins with generic parameter validation:

```cpp
FunctionTypeSharedPointer functionType =
    std::static_pointer_cast<FunctionType>( calleeType );
if( functionType->genericParameterNames.empty() == false ) {
    std::string calleeName = "anonymous";
    if( expression.callee->kind ==
            ast::Node::Kind::IdentifierExpression ) {
        calleeName = static_cast<ast::nodes::IdentifierExpression&>(
            *expression.callee ).name;
    }
    if( expression.typeArguments.empty() ) {
        std::string genericErrorMessage = fmt::format(
            "generic function \"{}\" requires explicit type "
            "arguments: {}<{}>(…)",
            calleeName, calleeName,
            fmt::join(
                functionType->genericParameterNames, ", " ) );
        this->diagnostic.error(
            expression.source, genericErrorMessage );
        return this->typeRegistry.getError();
    }
    if( expression.typeArguments.size() !=
            functionType->genericParameterNames.size() ) {
        std::string countErrorMessage = fmt::format(
            "generic function \"{}\" expects {} type argument(s) "
            "but {} provided",
            calleeName,
            functionType->genericParameterNames.size(),
            expression.typeArguments.size() );
        this->diagnostic.error(
            expression.source, countErrorMessage );
        return this->typeRegistry.getError();
    }
}
```

Generic functions require explicit type arguments — Uranite does not infer generic type parameters for standalone function calls. The count must match exactly.

### Overload Resolution

At `analyzer.cpp:1331-1417`, when the callee is an `IdentifierExpression` and `lookupAll` returns multiple candidates, overload resolution activates:

```cpp
std::vector<SymbolSharedPointer> overloadCandidates =
    this->currentScope->lookupAll( identifierCallee.name );
if( overloadCandidates.size() > 1 ) {
    std::vector<TypeSharedPointer> argumentTypes;
    for( ast::nodes::ExpressionSharedPointer& argument :
         expression.arguments ) {
        argumentTypes.push_back(
            this->analyzeExpression( argument ) );
    }
    size_t argumentCount = argumentTypes.size();
    int bestScore = -1;
    FunctionTypeSharedPointer bestFunctionType = nullptr;
    SymbolSharedPointer bestSymbol = nullptr;
    bool bestIsExactArity = false;
    bool ambiguous = false;
    for( const SymbolSharedPointer& candidate :
         overloadCandidates ) {
        // ... scoring logic
    }
}
```

**Scoring algorithm** — each candidate is scored as follows:

1. **Filter non-functions**: candidates that are not `Symbol::Kind::Function` or lack a `FunctionType` are skipped.

2. **Fixed parameter count**: computed from `variadicParameterIndex` (if variadic), `keywordParameterIndex` (if kwargs), or `parameterTypes.size()` (if neither).

3. **Arity filter**: candidates with fewer fixed parameters than arguments are skipped. Candidates with more fixed parameters than arguments are skipped unless variadic.

4. **Per-parameter scoring**:
   - Exact type match (string comparison of `toString()`): **+2 points**
   - Assignable but not exact: **+1 point**
   - Not assignable: candidate rejected (`compatible = false`)

5. **Best candidate selection**: highest score wins. On tied scores, exact arity (`argumentCount == parameterTypes.size()` with no variadic) breaks the tie — exact arity wins over variadic candidates.

6. **Ambiguity detection**: if two candidates tie in both score and arity class, `ambiguous = true` and the error "ambiguous call to overloaded function" is emitted.

The winning candidate replaces the original `functionType` and updates `identifierCallee.resolvedSymbol`.

### Argument Count Validation

At `analyzer.cpp:1419-1451`:

```cpp
size_t fixedParamCount;
if( functionType->variadicParameterIndex >= 0 ) {
    fixedParamCount = static_cast<size_t>(
        functionType->variadicParameterIndex );
}
else if( functionType->keywordParameterIndex >= 0 ) {
    fixedParamCount = static_cast<size_t>(
        functionType->keywordParameterIndex );
}
else {
    fixedParamCount =
        functionType->parameterTypes.size();
}
size_t positionalArgCount = expression.arguments.size();
size_t matchedKeywordArgCount = 0;
for( const ast::nodes::KeywordArgument& kwarg :
     expression.keywordArguments ) {
    for( size_t paramIndex = 0;
         paramIndex < functionType->parameterNames.size();
         paramIndex++ ) {
        if( functionType->parameterNames[paramIndex] ==
                kwarg.name ) {
            matchedKeywordArgCount++;
            break;
        }
    }
}
size_t totalSuppliedArgCount =
    positionalArgCount + matchedKeywordArgCount;
```

Total supplied arguments count both positional and keyword arguments that match declared parameter names. Two error conditions:

| Condition | Error |
|---|---|
| `totalSuppliedArgCount < fixedParamCount` | `"expected at least N arguments but got M"` |
| `positionalArgCount > fixedParamCount` (non-variadic) | `"expected N arguments but got M"` |

### Keyword Argument Validation

At `analyzer.cpp:1452-1471`, unknown keyword arguments are rejected:

```cpp
if( expression.keywordArguments.empty() == false &&
    functionType->keywordParameterIndex < 0 &&
    functionType->keywordOnlyParamNames.empty() ) {
    bool hasUnmatchedKeywordArg = false;
    for( const ast::nodes::KeywordArgument& kwarg :
         expression.keywordArguments ) {
        bool matched = false;
        for( const std::string& paramName :
             functionType->parameterNames ) {
            if( paramName == kwarg.name ) {
                matched = true;
                break;
            }
        }
        if( matched == false ) {
            std::string unknownKwargMessage = fmt::format(
                "unknown keyword argument \"{}\"", kwarg.name );
            this->diagnostic.error(
                kwarg.source, unknownKwargMessage );
            hasUnmatchedKeywordArg = true;
        }
    }
    if( hasUnmatchedKeywordArg &&
        functionType->parameterNames.empty() ) {
        this->diagnostic.error( expression.source,
            "function does not accept keyword arguments" );
    }
}
```

When a function has no kwargs parameter (`keywordParameterIndex < 0`) and no keyword-only parameters, each keyword argument name is checked against declared `parameterNames`. Unmatched names produce "unknown keyword argument" errors. If the function has no parameters at all, the umbrella error "function does not accept keyword arguments" is also emitted.

### Generic Substitution

At `analyzer.cpp:1472-1480`:

```cpp
std::unordered_map<std::string, TypeSharedPointer>
    genericSubstitutionMap;
if( functionType->genericParameterNames.empty() == false &&
    expression.typeArguments.empty() == false ) {
    for( size_t genericIndex = 0;
         genericIndex <
             functionType->genericParameterNames.size();
         genericIndex++ ) {
        TypeSharedPointer resolvedTypeArg =
            this->resolveType(
                expression.typeArguments[genericIndex] );
        if( resolvedTypeArg != nullptr ) {
            genericSubstitutionMap[
                functionType->genericParameterNames[
                    genericIndex]] = resolvedTypeArg;
        }
    }
}
```

Each explicit type argument is resolved and mapped to its corresponding generic parameter name. This map is used in two places: per-argument type checking (expected types are substituted before assignability check) and return type resolution (the function's return type is substituted before returning).

### Per-Argument Type Checking

At `analyzer.cpp:1481-1493`:

```cpp
for( size_t index = 0;
     index < fixedParamCount && index < positionalArgCount;
     index++ ) {
    TypeSharedPointer argumentType =
        this->analyzeExpression( expression.arguments[index] );
    TypeSharedPointer expectedType =
        functionType->parameterTypes[index];
    if( genericSubstitutionMap.empty() == false ) {
        expectedType = this->substituteGenericParameters(
            expectedType, genericSubstitutionMap );
    }
    if( argumentType &&
        this->typeRegistry.isAssignable(
            expectedType, argumentType ) == false ) {
        std::string functionArgumentMismatchErrorMessage =
            fmt::format(
                "argument type mismatch: expected \"{}\" "
                "but found \"{}\"",
                expectedType->toString(),
                argumentType->toString() );
        this->diagnostic.error(
            expression.arguments[index]->source,
            functionArgumentMismatchErrorMessage );
    }
}
```

Each positional argument up to `fixedParamCount` is analyzed, its type checked against the expected parameter type (with generic substitution applied if present). Mismatches produce "argument type mismatch" errors with expected and found type names.

### Variadic Argument Checking

At `analyzer.cpp:1494-1533`:

```cpp
if( functionType->variadicParameterIndex >= 0 &&
    functionType->variadicElementType ) {
    size_t keywordOnlyCount =
        functionType->keywordOnlyParamNames.size();
    size_t variadicEndIndex = positionalArgCount;
    if( keywordOnlyCount > 0 &&
        positionalArgCount > fixedParamCount ) {
        bool hasVariadicForward = false;
        size_t firstExtraArgIndex = fixedParamCount;
        if( firstExtraArgIndex < expression.arguments.size() ) {
            TypeSharedPointer firstExtraArgType =
                this->analyzeExpression(
                    expression.arguments[firstExtraArgIndex] );
            if( firstExtraArgType &&
                firstExtraArgType->name.find(
                    qualname::classes::args::Prefix ) == 0 ) {
                hasVariadicForward = true;
            }
        }
        if( hasVariadicForward ) {
            variadicEndIndex = fixedParamCount + 1;
        }
    }
    for( size_t index = fixedParamCount;
         index < variadicEndIndex; index++ ) {
        TypeSharedPointer argumentType =
            this->analyzeExpression(
                expression.arguments[index] );
        if( argumentType &&
            argumentType->name.find(
                qualname::classes::args::Prefix ) == 0 ) {
            continue;
        }
        if( argumentType &&
            this->typeRegistry.isAssignable(
                functionType->variadicElementType,
                argumentType ) == false ) {
            this->diagnostic.error(
                expression.arguments[index]->source,
                fmt::format(
                    "variadic argument type mismatch: "
                    "expected \"{}\" but found \"{}\"",
                    functionType->variadicElementType->toString(),
                    argumentType->toString() ) );
        }
    }
}
```

When the function has a variadic parameter (`variadicParameterIndex >= 0`), arguments beyond the fixed parameters are checked against `variadicElementType`. Two special cases:

1. **Variadic forwarding**: if the first extra argument is an `Args<T>` type (detected via `qualname::classes::args::Prefix`), it represents a forwarded variadic pack. The argument is accepted without element-type checking — the pack was already validated at the original call site.

2. **Keyword-only parameters after variadic**: when `keywordOnlyParamNames` exist and variadic forwarding is detected, `variadicEndIndex` is clamped to `fixedParamCount + 1` so remaining arguments are checked as keyword-only positional fills.

### Kwargs Parameter Checking

At `analyzer.cpp:1540-1589`, three paths handle keyword argument type validation:

**Path 1 — Kwargs parameter** (`keywordParameterIndex >= 0`): each keyword argument value is checked against `keywordValueType`. Duplicate keyword names are detected via a `seenKeywords` set.

**Path 2 — Keyword-only parameters** (`keywordOnlyParamNames` non-empty): each keyword argument name is matched against `keywordOnlyParamNames`. Matched arguments have their value type checked against the corresponding `keywordOnlyParamTypes` entry. Unmatched names produce "unknown keyword argument" errors. Duplicates are detected.

**Path 3 — Neither**: keyword argument values are analyzed (for side effects and type inference) but not validated against any parameter — these fall through as unmatched.

### Exception Safety Enforcement

At `analyzer.cpp:1590-1617`:

```cpp
if( functionType->exceptionTypes.empty() == false &&
    this->isInsideTryBlock == false ) {
    // ... build raisedTypes string
    bool callerRaises = false;
    for( TypeSharedPointer& raisedType :
         this->currentExceptionTypes ) {
        for( TypeSharedPointer& functionRaisedType :
             functionType->exceptionTypes ) {
            if( raisedType->equals( functionRaisedType ) ) {
                callerRaises = true;
                break;
            }
        }
        if( callerRaises ) {
            break;
        }
    }
    if( callerRaises == false ) {
        std::string exceptionSafetyErrorMessage = fmt::format(
            "call to function that raises \"{}\" must be "
            "wrapped in try/except or caller must "
            "declare \"raises\"",
            raisedTypes );
        this->diagnostic.error(
            expression.source, exceptionSafetyErrorMessage );
    }
}
```

When calling a function that declares `raises` exception types, the call must either be inside a `try` block or the caller must also declare those same exception types in its own `raises` clause. If neither condition holds, the compiler emits an error requiring explicit exception handling.

**Return type**: at `analyzer.cpp:1618-1622`, the function's return type is substituted with the generic substitution map (if any) and returned as the expression's resolved type.

---

## HIR Representation — HIRFunctionCall

Defined at `src/uranite/ir/hir.hpp:665-682`:

```cpp
struct HIRFunctionCall : HIRNode {

    HIRNodeSharedPointer calleeExpression;
    std::vector<HIRNodeSharedPointer> callArguments;
    std::vector<std::pair<std::string, HIRNodeSharedPointer>>
        keywordArguments;
    std::vector<semantic::TypeSharedPointer> typeArguments;

    HIRFunctionCall(
        HIRNodeSharedPointer calleeExpression,
        std::vector<HIRNodeSharedPointer> callArguments,
        semantic::TypeSharedPointer resolvedType,
        const lookup::SourceSharedPointer& sourceLocation
    ) : HIRNode( HIRNodeKind::FunctionCall,
            std::move( resolvedType ), sourceLocation ),
        calleeExpression( std::move( calleeExpression ) ),
        callArguments( std::move( callArguments ) ) {
    }

};
```

| Field | Type | Description |
|---|---|---|
| `calleeExpression` | `HIRNodeSharedPointer` | Lowered callee — `HIRIdentifier` for named functions, `HIRSuperReference` for `super()` |
| `callArguments` | `vector<HIRNodeSharedPointer>` | Lowered positional argument expressions |
| `keywordArguments` | `vector<pair<string, HIRNodeSharedPointer>>` | Keyword argument name-value pairs |
| `typeArguments` | `vector<TypeSharedPointer>` | Resolved generic type arguments |

---

## HIR Lowering

At `src/uranite/ir/hir/lowering.cpp:900-917`:

```cpp
case ast::Node::Kind::CallExpression: {
    ast::nodes::CallExpression& callExpression =
        static_cast<ast::nodes::CallExpression&>( *expression );
    HIRNodeSharedPointer calleeExpression =
        this->lowerExpression( callExpression.callee );
    std::vector<HIRNodeSharedPointer> callArguments;
    for( const ast::nodes::ExpressionSharedPointer& argument :
         callExpression.arguments ) {
        callArguments.push_back(
            this->lowerExpression( argument ) );
    }
    std::shared_ptr<HIRFunctionCall> hirCall =
        std::make_shared<HIRFunctionCall>(
            std::move( calleeExpression ),
            std::move( callArguments ),
            expression->semanticType,
            expression->source
        );
    for( const ast::nodes::KeywordArgument& keywordArgument :
         callExpression.keywordArguments ) {
        hirCall->keywordArguments.push_back(
            { keywordArgument.name,
              this->lowerExpression( keywordArgument.value ) } );
    }
    return hirCall;
}
```

HIR lowering is straightforward: lower the callee expression, lower each positional argument, create `HIRFunctionCall`, then attach keyword arguments as name-value pairs. The `semanticType` from the AST node carries the resolved return type established during semantic analysis.

---

## MIR Lowering — lowerFunctionCall

At `src/uranite/ir/mir/lowering.cpp:3502-3567`:

```cpp
MIRVariableIdentifier MIRLowering::lowerFunctionCall(
    hir::HIRFunctionCall& hirCall ) {
    std::vector<MIRVariableIdentifier> argumentVariables;
    for( const hir::HIRNodeSharedPointer& argument :
         hirCall.callArguments ) {
        argumentVariables.push_back(
            this->lowerExpression( argument ) );
    }
    bool useInvoke =
        ( this->activeLandingPad != INVALID_BLOCK_IDENTIFIER );
    MIRInstruction callInstruction(
        useInvoke
            ? MIRInstructionKind::InvokeFunction
            : MIRInstructionKind::CallFunction );
    // ... callee resolution, captures, keyword args
    MIRVariableIdentifier resultVariable =
        this->currentFunction->allocateVariable(
            "_call", hirCall.resolvedType, false );
    callInstruction.destinationVariable = resultVariable;
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
}
```

### Callee Resolution

At `mir/lowering.cpp:3509-3517`, the callee expression determines the function's qualified name:

```cpp
if( hirCall.calleeExpression != nullptr ) {
    if( hirCall.calleeExpression->nodeKind ==
            hir::HIRNodeKind::Identifier ) {
        hir::HIRIdentifier& calleeIdentifier =
            static_cast<hir::HIRIdentifier&>(
                *hirCall.calleeExpression );
        if( calleeIdentifier.qualifiedScopeName.empty() ==
                false ) {
            callInstruction.calledFunctionQualifiedName =
                calleeIdentifier.qualifiedScopeName;
        }
        else {
            callInstruction.calledFunctionQualifiedName =
                calleeIdentifier.identifierName;
        }
    }
```

When the callee is an `HIRIdentifier`, the qualified scope name is preferred (fully-qualified path like "module.package.functionName"). If no qualified name exists (local functions), the raw identifier name is used.

### Super Constructor Delegation

At `mir/lowering.cpp:3519-3533`:

```cpp
else if( hirCall.calleeExpression->nodeKind ==
             hir::HIRNodeKind::SuperReference ) {
    if( this->currentParentClassName.empty() == false ) {
        std::string parentShortName =
            this->currentParentClassName;
        size_t lastDotPos = parentShortName.rfind( '.' );
        if( lastDotPos != std::string::npos ) {
            parentShortName =
                parentShortName.substr( lastDotPos + 1 );
        }
        callInstruction.calledFunctionQualifiedName =
            this->currentParentClassName + "." +
            parentShortName;
        std::unordered_map<std::string,
            MIRVariableIdentifier>::iterator selfLookup =
            this->variableNameMap.find(
                semantic::qualname::identifier::Self );
        if( selfLookup != this->variableNameMap.end() ) {
            argumentVariables.insert(
                argumentVariables.begin(),
                selfLookup->second );
        }
    }
}
```

When the callee is `HIRSuperReference` (from `super(args)` in a constructor), the MIR lowering:

1. Extracts the parent class short name from `currentParentClassName` — strips the package prefix by finding the last dot.
2. Constructs the constructor qualified name as `"parentClassName.ShortName"` (constructors use the class name as method name).
3. Prepends `self` as the first argument — parent constructors need the current object reference.

For example, if `currentParentClassName` is "mypackage.Animal", the constructor name becomes "mypackage.Animal.Animal", and `self` is inserted before all explicit arguments.

### Nested Function Captures

At `mir/lowering.cpp:3536-3546`:

```cpp
std::unordered_map<std::string,
    std::vector<std::string>>::iterator captureIt =
    this->nestedFunctionCaptures.find(
        callInstruction.calledFunctionQualifiedName );
if( captureIt != this->nestedFunctionCaptures.end() ) {
    for( const std::string& captureName :
         captureIt->second ) {
        std::unordered_map<std::string,
            MIRVariableIdentifier>::iterator varIt =
            this->variableNameMap.find( captureName );
        if( varIt != this->variableNameMap.end() ) {
            callInstruction.sourceOperands.push_back(
                varIt->second );
        }
    }
}
```

When calling a nested (inner) function, captured variables from the enclosing scope are appended as additional arguments. The `nestedFunctionCaptures` map records which variables each nested function captures. These captured variables are looked up in the current scope's `variableNameMap` and appended to the argument list after the explicit arguments.

This mechanism implements closures without a heap-allocated environment — captured variables are passed as extra parameters directly.

### Keyword Argument Emission

At `mir/lowering.cpp:3549-3553`:

```cpp
for( const std::pair<std::string, hir::HIRNodeSharedPointer>&
     keywordArgument : hirCall.keywordArguments ) {
    callInstruction.keywordArgumentKeys.push_back(
        keywordArgument.first );
    MIRVariableIdentifier valueVariable =
        this->lowerExpression( keywordArgument.second );
    callInstruction.keywordArgumentValues.push_back(
        valueVariable );
}
```

Keyword arguments are lowered into parallel vectors on the MIR instruction: `keywordArgumentKeys` holds the parameter names, `keywordArgumentValues` holds the lowered value variables. These are packed into a `Kwargs<K,V>` struct during LLVM code generation.

### Exception-Aware Instruction Selection

The `useInvoke` flag determines the MIR instruction kind:

| Context | `activeLandingPad` | Instruction | Behavior |
|---|---|---|---|
| Outside try block | `INVALID_BLOCK_IDENTIFIER` | `CallFunction` | Direct call, no exception handling |
| Inside try block | Valid block ID | `InvokeFunction` | Creates continuation block + landing pad |

For `InvokeFunction`:
1. A new `invoke.cont` basic block is created for the normal return path.
2. `trueBranchTarget` points to this continuation block.
3. `landingPadTarget` points to the exception handler's landing pad block.
4. The instruction is emitted as a terminator (it transfers control).
5. Execution switches to the continuation block for subsequent instructions.

For `CallFunction`:
1. The instruction is emitted as a regular (non-terminator) instruction.
2. Control flow falls through to the next instruction in the same block.

The result variable `_call` is allocated with the function's resolved return type regardless of instruction kind.

---

## LLVM Code Generation — generateCallFunction

Call expressions and method calls share the same LLVM code generation path: `generateCallFunction` at `src/uranite/ir/mir/codegen.cpp:2497-5490`. The MIR instruction's `calledFunctionQualifiedName` is used to resolve the target function through an 11-priority cascade.

The full `generateCallFunction` implementation is documented in [Method Call Expressions — LLVM Code Generation](method-call-expressions.md#llvm-code-generation--generatecallfunction). The key stages relevant to standalone function calls:

**Function Resolution Cascade** (codegen.cpp:4223-4560):

| Priority | Strategy | Description |
|---|---|---|
| 1 | Direct lookup by qualified name | Exact match in LLVM module's function list |
| 2 | Arity-suffixed lookup | Appends argument count: `"func.2"` for two arguments |
| 3 | Base class method inheritance | Walks class hierarchy for inherited methods |
| 4 | Interface dispatch table | Loads vtable pointer from receiver, indexes into itable slot |
| 5 | Module-prefixed lookup | Prepends current module name to function name |
| 6 | Package-prefixed lookup | Tries package-qualified name variants |
| 7 | Generic monomorphization lookup | Tries mangled generic instantiation names |
| 8 | Abstract class vtable dispatch | Pre-computed dispatch targets via vtable identifiers |
| 9 | Extern function lookup | Matches declared extern function signatures |
| 10 | Standard library function lookup | Searches linked standard library modules |
| 11 | Indirect function pointer call | Loads function pointer from variable and invokes via `CreateCall` with function type |

**Builtin Descriptor Interception** (codegen.cpp:3078-3089): before the resolution cascade, `tryBuiltinDescriptor` checks if the callee matches an OOP wrapper type's method. If matched, the descriptor's `irGenerator` callback emits inline LLVM IR directly — bypassing function call overhead entirely. This applies to calls like `someInteger.toString()` or `someString.length()`.

**Variadic/Kwargs Packing** (codegen.cpp:4765-5455): when the target function has variadic or kwargs parameters, excess arguments are packed into `Args<T>` or `Kwargs<K,V>` struct allocations. The struct contains a count field and a data pointer to a heap-allocated array of values. Keyword argument keys are stored as an array of string pointers alongside the value array.

**emitCallOrInvoke** (codegen.cpp:7252-7279): the final emission point. `InvokeFunction` MIR instructions produce LLVM `CreateInvoke` with normal and unwind destinations. `CallFunction` instructions produce LLVM `CreateCall`. Both set calling convention, attach metadata, and handle async function wrapping.

---

## Examples

### Simple Function Call

```
package examples

from uranite.io.console import puts

function add(I64 first, I64 second) -> I64:
    return first + second

public function main() -> I32:
    I64 result = add(10, 20)
    puts(result.toString())
    return 0
```

**Compilation trace**:
1. **Parsing**: `add(10, 20)` — `IdentifierExpression("add")` followed by `(`. Path 2 (no generic args). Two `IntegerLiteral` arguments parsed. Creates `CallExpression(callee=IdentifierExpression("add"), arguments=[10, 20])`.
2. **Semantic analysis**: `add` resolves to `FunctionType(params=[I64, I64], return=I64)`. No overloads (single candidate). Fixed param count = 2, positional arg count = 2. Both arguments analyzed as `I64`, both match parameter types exactly. Return type = `I64`.
3. **HIR lowering**: callee lowered to `HIRIdentifier("add", qualifiedScopeName="examples.add")`. Arguments lowered to `HIRIntegerLiteral(10)` and `HIRIntegerLiteral(20)`. Creates `HIRFunctionCall`.
4. **MIR lowering**: arguments lowered to `ConstantInteger` MIR variables. `activeLandingPad == INVALID_BLOCK_IDENTIFIER` so `CallFunction` selected. `calledFunctionQualifiedName = "examples.add"`. Result variable `_call` allocated with type `I64`.
5. **LLVM codegen**: `generateCallFunction` resolves `"examples.add"` via priority 1 (direct lookup). `CreateCall` emits `call i64 @examples.add(i64 10, i64 20)`.

### Generic Function Call

```
package examples

from uranite.io.console import puts

function identity<T>(T value) -> T:
    return value

public function main() -> I32:
    I64 number = identity<I64>(42)
    String text = identity<String>("hello")
    puts(number.toString())
    puts(text)
    return 0
```

**Compilation trace**:
1. **Parsing**: `identity<I64>(42)` — Path 1 triggered. `parseGenericArguments()` parses `<I64>` producing one type argument. Then `(42)` parsed as single positional argument. Creates `CallExpression(callee="identity", typeArguments=[I64], arguments=[42])`.
2. **Semantic analysis**: `identity` resolves to `FunctionType(genericParameterNames=["T"], params=[T], return=T)`. Type arguments not empty, count matches (1 == 1). Generic substitution map built: `{"T" -> I64}`. Argument `42` analyzed as `I64`. Expected type `T` substituted to `I64`. `isAssignable(I64, I64)` passes. Return type `T` substituted to `I64`.
3. **MIR lowering**: `calledFunctionQualifiedName = "examples.identity"`. `CallFunction` with `ConstantInteger(42)` as operand.
4. **LLVM codegen**: monomorphized function lookup resolves the `I64` instantiation. Emits `call i64 @examples.identity.I64(i64 42)`.

### Keyword Arguments

```
package examples

from uranite.io.console import puts

function connect(String host, I64 port, Bool secure) -> Void:
    puts("Connecting to " + host + ":" + port.toString())

public function main() -> I32:
    connect("localhost", port=8080, secure=false)
    return 0
```

**Compilation trace**:
1. **Parsing**: `connect("localhost", port=8080, secure=false)`. First argument `"localhost"` is positional. Second argument: `port` parsed as `IdentifierExpression`, next token is `=` (Assignment), so reinterpreted as keyword argument `port=8080`. Third: same pattern, keyword argument `secure=false`. `seenKeywordArg = true` after `port=8080`.
2. **Semantic analysis**: `connect` resolves to `FunctionType(params=[String, I64, Bool], parameterNames=["host", "port", "secure"])`. Positional count = 1, keyword count = 2 matched. Total supplied = 3, fixed param count = 3. Keyword arg "port" matches parameterNames[1], "secure" matches parameterNames[2]. Each value type checked against corresponding parameter type.
3. **MIR lowering**: positional argument lowered normally. Keyword arguments stored in `keywordArgumentKeys = ["port", "secure"]` and `keywordArgumentValues = [8080_var, false_var]`.
4. **LLVM codegen**: keyword arguments matched to parameter positions and passed in correct order.

### Overloaded Functions

```
package examples

from uranite.io.console import puts

function format(String value) -> String:
    return value

function format(I64 value) -> String:
    return value.toString()

function format(String prefix, I64 value) -> String:
    return prefix + value.toString()

public function main() -> I32:
    puts(format("hello"))
    puts(format(42))
    puts(format("value: ", 100))
    return 0
```

**Compilation trace for `format(42)`**:
1. **Parsing**: `format(42)` — Path 2. Creates `CallExpression(callee="format", arguments=[42])`.
2. **Semantic analysis**: `lookupAll("format")` returns 3 candidates. Argument types analyzed: `[I64]`. Scoring:
   - `format(String)`: param[0] is `String`, arg is `I64`. Not assignable. `compatible = false`. Skipped.
   - `format(I64)`: param[0] is `I64`, arg is `I64`. Exact match = +2. Score = 2. Exact arity (1 == 1). Best candidate.
   - `format(String, I64)`: fixed param count = 2, argument count = 1. `1 < 2`. Arity filter rejects.
   - Winner: `format(I64) -> String` with score 2.
3. **MIR lowering**: `calledFunctionQualifiedName = "examples.format"`. Arity suffix may be used at codegen to disambiguate.
4. **LLVM codegen**: arity-suffixed lookup (`"examples.format.1"`) or direct match resolves correct overload.

### Super Constructor Delegation

```
package examples

from uranite.io.console import puts

public class Animal:
    String name

    public function Animal(self, String name) -> Void:
        self.name = name

public class Dog extends Animal:
    String breed

    public function Dog(self, String name, String breed) -> Void:
        super(name)
        self.breed = breed

public function main() -> I32:
    Dog dog = new Dog("Rex", "Labrador")
    puts(dog.name)
    puts(dog.breed)
    return 0
```

**Compilation trace for `super(name)`**:
1. **Parsing**: `super` parsed as `SuperExpression`. `(name)` triggers call expression creation with `SuperExpression` as callee.
2. **Semantic analysis**: `super(name)` resolves through the parent class `Animal`'s constructor. Argument `name` typed as `String`, matches `Animal(self, String)` parameter.
3. **HIR lowering**: callee lowered to `HIRSuperReference`. Argument `name` lowered to `HIRIdentifier`.
4. **MIR lowering**: `HIRSuperReference` branch activates. `currentParentClassName = "examples.Animal"`. Short name extracted: `"Animal"`. Constructor qualified name constructed: `"examples.Animal.Animal"`. `self` looked up in `variableNameMap` and prepended to arguments. Instruction becomes `CallFunction/InvokeFunction "examples.Animal.Animal"(self, name_var)`.
5. **LLVM codegen**: `"examples.Animal.Animal"` resolved via direct lookup. Parent constructor initializes `name` field on the shared object reference.

### Nested Function with Captures

```
package examples

from uranite.io.console import puts

public function main() -> I32:
    I64 multiplier = 3

    function scale(I64 value) -> I64:
        return value * multiplier

    I64 result = scale(10)
    puts(result.toString())
    return 0
```

**Compilation trace**:
1. **Parsing/Semantic**: `scale(10)` parsed as normal `CallExpression`. `scale` resolves to the nested function. `multiplier` is captured from outer scope.
2. **HIR lowering**: standard `HIRFunctionCall` creation.
3. **MIR lowering**: after setting `calledFunctionQualifiedName`, the `nestedFunctionCaptures` map is checked. Entry found for `"examples.main.scale"` with captures `["multiplier"]`. Variable `multiplier` looked up in `variableNameMap`, found, and appended to `sourceOperands` after the explicit argument `10`. Instruction becomes `CallFunction "examples.main.scale"(_arg_10, _var_multiplier)`.
4. **LLVM codegen**: `scale` function takes two parameters (value + captured multiplier). Call emits `call i64 @examples.main.scale(i64 10, i64 3)`.

### Function Call Inside Try Block

```
package examples

from uranite.io.console import puts

function riskyOperation(I64 value) -> I64 raises Exception:
    if value < 0:
        raise new Exception("negative value")
    return value * 2

public function main() -> I32:
    try:
        I64 result = riskyOperation(-5)
        puts(result.toString())
    except Exception error:
        puts("Error: " + error.getMessage())
    return 0
```

**Compilation trace for `riskyOperation(-5)` inside try**:
1. **Semantic analysis**: `riskyOperation` has `exceptionTypes = [Exception]`. Call is inside try block (`isInsideTryBlock = true`), so exception safety check passes without requiring caller to declare `raises`.
2. **MIR lowering**: `activeLandingPad != INVALID_BLOCK_IDENTIFIER` (set by try block lowering). `useInvoke = true`. Instruction kind = `InvokeFunction`. Continuation block `invoke.cont` created. `trueBranchTarget = invoke.cont.id`, `landingPadTarget = activeLandingPad`. Instruction emitted as terminator. Execution switches to `invoke.cont`.
3. **LLVM codegen**: `CreateInvoke` emitted instead of `CreateCall`. Normal destination = `invoke.cont` block. Unwind destination = landing pad block (which contains `landingpad` instruction with catch clause for `Exception` type). On normal return, control flows to `invoke.cont` where `result` receives the return value.

### Callable Variable Invocation

```
package examples

from uranite.io.console import puts

public function main() -> I32:
    Callable<I64, I64> doubler = (I64 value) -> I64:
        return value * 2
    I64 result = doubler(21)
    puts(result.toString())
    return 0
```

**Compilation trace**:
1. **Semantic analysis**: `doubler` resolves to `Callable<I64, I64>` type (closure). The `Callable` type path activates. Parameter types = `[I64]`, return type = `I64`. Argument `21` analyzed as `I64`, `isAssignable(I64, I64)` passes. Returns `I64` directly — no overload resolution, no generic handling.
2. **MIR lowering**: callee lowered as variable reference (not identifier). `calledFunctionQualifiedName` set from variable's resolved function name or left for indirect call resolution.
3. **LLVM codegen**: priority 11 (indirect function pointer call) activates. Function pointer loaded from the callable variable. `CreateCall` with the loaded pointer and function type signature `i64(i64)`.
