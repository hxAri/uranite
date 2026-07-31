# Constructor Expressions

A constructor expression creates a new instance of a class or struct using the `new` keyword. The syntax `new Type(args)` produces a `ConstructExpression` AST node carrying the target type and a list of field initializers — either positional arguments or named field assignments. The semantic analyzer resolves the constructed type, rejects non-instantiable types (interfaces, traits, abstract classes), and analyzes each initializer expression. MIR lowering emits a `ConstructObject` instruction with the type's qualified name and constructor arguments. LLVM code generation performs the heaviest work: heap-allocating the object via `malloc`, resolving the LLVM struct type from cache or module, initializing the vtable/itable pointer for polymorphic types, looking up and calling the user-defined constructor function with `self` prepended, applying field default values from the AST declaration, and handling compiler-intrinsic types (`Memory<T>`, `Arena<T>`, OOP wrappers) through specialized fast paths that bypass normal struct allocation entirely.

---

## Table of Contents

- [Syntax](#syntax)
- [AST Representation — ConstructExpression](#ast-representation--constructexpression)
- [Parsing](#parsing)
  - [Named Argument Detection](#named-argument-detection)
- [Semantic Analysis](#semantic-analysis)
  - [Non-Instantiable Type Guards](#non-instantiable-type-guards)
- [HIR Representation — HIRConstruct](#hir-representation--hirconstruct)
- [HIR Lowering](#hir-lowering)
  - [Type Fallback Resolution](#type-fallback-resolution)
- [MIR Lowering — lowerConstruct](#mir-lowering--lowerconstruct)
  - [Generic Bracket Stripping](#generic-bracket-stripping)
- [LLVM Code Generation — generateConstructObject](#llvm-code-generation--generateconstructobject)
  - [Intrinsic Type Fast Paths](#intrinsic-type-fast-paths)
  - [OOP Wrapper Construction](#oop-wrapper-construction)
  - [Struct Type Resolution](#struct-type-resolution)
  - [Heap Allocation](#heap-allocation)
  - [VTable/ITable Initialization](#vtableitable-initialization)
  - [Constructor Function Resolution](#constructor-function-resolution)
  - [Constructor Arity Overload Resolution](#constructor-arity-overload-resolution)
  - [Argument Type Coercion](#argument-type-coercion)
  - [Field Default Value Initialization](#field-default-value-initialization)
  - [Fallback Allocation](#fallback-allocation)
- [Concrete Class Propagation](#concrete-class-propagation)
- [Examples](#examples)

---

## Syntax

```
new Type(arguments)
new Type<TypeArgs>(arguments)
new Type(field=value, field=value)
new Type()
```

| Form | Description |
|---|---|
| `new ClassName(args)` | Construct class instance with positional arguments |
| `new ClassName<T>(args)` | Construct generic class instance with explicit type arguments |
| `new StructName(field=value)` | Construct struct with named field initializers |
| `new ClassName()` | Construct with no arguments (default constructor) |

```
package examples

from uranite.io.console import puts

public class Point:
    I64 x
    I64 y

    public function Point(self, I64 x, I64 y) -> Void:
        self.x = x
        self.y = y

public function main() -> I32:
    Point point = new Point(10, 20)
    puts(point.x.toString())
    return 0
```

The `new` keyword is required for all class and struct instantiation. Bare `ClassName(args)` is rejected by the semantic analyzer with a diagnostic suggesting `new ClassName(...)`.

---

## AST Representation — ConstructExpression

Defined at `src/uranite/ast/node.hpp:1371-1394`:

```cpp
struct ConstructExpression : Expression {

    std::vector<std::pair<std::string, ExpressionSharedPointer>>
        fields;
    TypeNodeSharedPointer type;

    ConstructExpression(
        std::vector<std::pair<std::string,
            ExpressionSharedPointer>> fields,
        const lookup::SourceSharedPointer& source,
        TypeNodeSharedPointer type
    ) : Expression( Node::Kind::ConstructExpression, source ),
        fields( std::move( fields ) ),
        type( std::move( type ) ) {
    }

};
```

| Field | Type | Description |
|---|---|---|
| `type` | `TypeNodeSharedPointer` | Type being constructed — `SimpleTypeNode` for plain types, `GenericTypeNode` for generic types like `ArrayList<I64>` |
| `fields` | `vector<pair<string, ExpressionSharedPointer>>` | Constructor arguments as name-value pairs. Positional arguments have an empty string as the name. Named arguments carry the field name. |

The `fields` vector uses a unified representation: positional arguments store `("", expression)` while named arguments store `("fieldName", expression)`. This allows mixed positional and named arguments in a single list.

---

## Parsing

At `src/uranite/parser/parser.cpp:2237-2265`:

```cpp
case token::Type::KeywordNew: {
    this->advance();
    ast::nodes::TypeNodeSharedPointer type =
        this->parseTypeNode();
    std::vector<std::pair<std::string,
        ast::nodes::ExpressionSharedPointer>> fields;
    if( this->match( token::Type::LeftParenthesis ) ) {
        bool seenNamedArg = false;
        if( this->check(
                token::Type::RightParenthesis, false ) ) {
            do {
                ast::nodes::ExpressionSharedPointer argExpr =
                    this->parseExpression();
                if( argExpr->kind ==
                        ast::Node::Kind::IdentifierExpression &&
                    this->check( token::Type::Assignment ) ) {
                    ast::nodes::IdentifierExpression& identExpr =
                        static_cast<
                            ast::nodes::IdentifierExpression&>(
                            *argExpr );
                    this->advance();
                    ast::nodes::ExpressionSharedPointer valueExpr =
                        this->parseExpression();
                    fields.emplace_back(
                        identExpr.name, std::move( valueExpr ) );
                    seenNamedArg = true;
                }
                else {
                    if( seenNamedArg ) {
                        this->diagnostic.error( argExpr->source,
                            "positional argument cannot follow "
                            "named argument" );
                    }
                    fields.emplace_back(
                        "", std::move( argExpr ) );
                }
            }
            while( this->match( token::Type::Comma ) );
        }
        this->expect( token::Type::RightParenthesis,
            "expected ')' after constructor arguments" );
    }
    return std::make_shared<ast::nodes::ConstructExpression>(
        std::move( fields ), source, type );
}
```

Parsing sequence:
1. Consume `new` keyword.
2. Parse target type via `parseTypeNode()` — handles both simple types (`Point`) and generic types (`ArrayList<I64>`).
3. If `(` follows, parse constructor arguments with the same keyword detection pattern as call expressions.
4. If no `(` follows, the constructor has no arguments — `fields` remains empty. This allows `new Type` without parentheses (though `new Type()` is more common).

### Named Argument Detection

Same mechanism as call expressions: if an argument parses as `IdentifierExpression` and the next token is `=` (Assignment), the identifier is reinterpreted as a named field initializer. The `seenNamedArg` flag enforces that positional arguments cannot follow named arguments.

Named arguments in constructor calls serve two purposes:
- For classes: passed to the constructor function as keyword arguments.
- For structs: direct field initialization by name.

---

## Semantic Analysis

At `src/uranite/semantic/analyzer.cpp:2282-2306`:

```cpp
case ast::Node::Kind::ConstructExpression: {
    ast::nodes::ConstructExpression& constructExpression =
        static_cast<ast::nodes::ConstructExpression&>(
            *expression );
    expressionType =
        this->resolveType( constructExpression.type );
    for( std::pair<std::string,
         ast::nodes::ExpressionSharedPointer>& field :
         constructExpression.fields ) {
        if( field.second ) {
            this->analyzeExpression( field.second );
        }
    }
    if( expressionType == nullptr ) {
        expressionType = this->typeRegistry.getError();
    }
    else if( expressionType->kind == Type::Kind::Interface ||
             expressionType->kind == Type::Kind::Trait ) {
        std::string errorMessage = fmt::format(
            "cannot instantiate {} \"{}\"",
            expressionType->kind == Type::Kind::Interface
                ? "interface" : "trait",
            expressionType->name );
        this->diagnostic.error(
            constructExpression.source, errorMessage );
        expressionType = this->typeRegistry.getError();
    }
    else if( expressionType->kind == Type::Kind::Class ) {
        ClassTypeSharedPointer classType =
            std::static_pointer_cast<ClassType>(
                expressionType );
        if( classType->isAbstract ) {
            std::string errorMessage = fmt::format(
                "cannot instantiate abstract class \"{}\"",
                expressionType->name );
            this->diagnostic.error(
                constructExpression.source, errorMessage );
            expressionType = this->typeRegistry.getError();
        }
    }
    break;
}
```

Semantic analysis for constructor expressions is intentionally lightweight compared to call expressions — constructor argument type checking is deferred to code generation where the constructor function signature is resolved against the concrete struct layout.

Steps:
1. **Resolve target type** via `resolveType()` — looks up the type in the registry, resolving generic type parameters if present.
2. **Analyze each field expression** — ensures sub-expressions are well-typed.
3. **Guard non-instantiable types** — reject with specific error messages.
4. **Result type** is the resolved constructed type itself (the expression's type is the class/struct being created).

### Non-Instantiable Type Guards

| Type Kind | Condition | Error Message |
|---|---|---|
| `Interface` | Always | `"cannot instantiate interface \"Name\""` |
| `Trait` | Always | `"cannot instantiate trait \"Name\""` |
| `Class` | `isAbstract == true` | `"cannot instantiate abstract class \"Name\""` |
| `Struct` | Never rejected | Structs are always instantiable |
| `Class` | `isAbstract == false` | Accepted — concrete class construction |

Note that unlike call expressions, constructor expressions do not perform argument count validation or overload resolution at the semantic level. These checks happen implicitly during LLVM code generation when the constructor function is resolved and arguments are matched.

---

## HIR Representation — HIRConstruct

Defined at `src/uranite/ir/hir.hpp:745-759`:

```cpp
struct HIRConstruct : HIRNode {

    semantic::TypeSharedPointer constructedType;
    std::vector<std::pair<std::string, HIRNodeSharedPointer>>
        constructorFields;

    HIRConstruct(
        semantic::TypeSharedPointer constructedType,
        std::vector<std::pair<std::string, HIRNodeSharedPointer>>
            constructorFields,
        const lookup::SourceSharedPointer& sourceLocation
    ) : HIRNode( HIRNodeKind::Construct,
            constructedType, sourceLocation ),
        constructedType( std::move( constructedType ) ),
        constructorFields( std::move( constructorFields ) ) {
    }

};
```

| Field | Type | Description |
|---|---|---|
| `constructedType` | `TypeSharedPointer` | Resolved semantic type of the constructed object |
| `constructorFields` | `vector<pair<string, HIRNodeSharedPointer>>` | Lowered constructor arguments — name-value pairs carried from AST |

---

## HIR Lowering

At `src/uranite/ir/hir/lowering.cpp:1009-1033`:

```cpp
case ast::Node::Kind::ConstructExpression: {
    ast::nodes::ConstructExpression& constructExpression =
        static_cast<ast::nodes::ConstructExpression&>(
            *expression );
    std::vector<std::pair<std::string, HIRNodeSharedPointer>>
        constructorFields;
    for( const std::pair<std::string,
         ast::nodes::ExpressionSharedPointer>& field :
         constructExpression.fields ) {
        constructorFields.push_back(
            { field.first,
              this->lowerExpression( field.second ) } );
    }
    semantic::TypeSharedPointer constructedType =
        expression->semanticType;
    if( constructedType == nullptr &&
        constructExpression.type != nullptr ) {
        std::string typeName;
        if( constructExpression.type->kind ==
                ast::Node::Kind::SimpleType ) {
            typeName =
                static_cast<ast::nodes::SimpleTypeNode&>(
                    *constructExpression.type ).name;
        }
        else if( constructExpression.type->kind ==
                     ast::Node::Kind::GenericType ) {
            typeName =
                static_cast<ast::nodes::GenericTypeNode&>(
                    *constructExpression.type ).name;
        }
        if( typeName.empty() == false ) {
            constructedType =
                std::make_shared<semantic::Type>(
                    semantic::Type::Kind::Class, typeName );
        }
    }
    return std::make_shared<HIRConstruct>(
        constructedType,
        std::move( constructorFields ),
        expression->source
    );
}
```

### Type Fallback Resolution

If `semanticType` is null (semantic analysis failed to resolve the type), the HIR lowering extracts the type name directly from the AST type node as a fallback:

- `SimpleTypeNode`: extracts the `name` field directly (e.g., "Point").
- `GenericTypeNode`: extracts the `name` field (e.g., "ArrayList" from `ArrayList<I64>`).

A synthetic `Type` with `Kind::Class` is created from the extracted name. This fallback ensures that construction can proceed even when the type registry lookup failed — codegen will attempt its own resolution.

---

## MIR Lowering — lowerConstruct

At `src/uranite/ir/mir/lowering.cpp:3907-3929`:

```cpp
MIRVariableIdentifier MIRLowering::lowerConstruct(
    hir::HIRConstruct& hirConstruct ) {
    std::vector<MIRVariableIdentifier> fieldVariables;
    for( const std::pair<std::string,
         hir::HIRNodeSharedPointer>& field :
         hirConstruct.constructorFields ) {
        fieldVariables.push_back(
            this->lowerExpression( field.second ) );
    }
    MIRInstruction constructInstruction(
        MIRInstructionKind::ConstructObject );
    constructInstruction.sourceOperands =
        std::move( fieldVariables );
    constructInstruction.operandType =
        hirConstruct.constructedType;
    constructInstruction.sourceLocation =
        hirConstruct.sourceLocation;
    if( hirConstruct.constructedType != nullptr ) {
        std::string constructedName =
            hirConstruct.constructedType->qualified.empty()
                == false
            ? hirConstruct.constructedType->qualified
            : hirConstruct.constructedType->name;
        size_t genericBracketPosition =
            constructedName.find( '<' );
        if( genericBracketPosition != std::string::npos ) {
            constructedName = constructedName.substr(
                0, genericBracketPosition );
        }
        constructInstruction.calledFunctionQualifiedName =
            constructedName;
    }
    MIRVariableIdentifier resultVariable =
        this->currentFunction->allocateVariable(
            "_construct",
            hirConstruct.constructedType, false );
    constructInstruction.destinationVariable = resultVariable;
    return this->emitInstruction( constructInstruction );
}
```

MIR lowering produces a single `ConstructObject` instruction:

1. **Lower each field expression** to MIR variables — stored as `sourceOperands`.
2. **Set type information** — `operandType` carries the full semantic type (including generic parameters).
3. **Set qualified name** — prefers `qualified` (fully-qualified path) over `name`. Used by codegen to look up the LLVM struct type and constructor function.
4. **Allocate result variable** `_construct` — typed with the constructed type.

### Generic Bracket Stripping

Generic type arguments are stripped from the `calledFunctionQualifiedName`:

```
"uranite.collection.array-list.ArrayList<I64>"
    → "uranite.collection.array-list.ArrayList"
```

The codegen resolves the concrete struct type independently from generic arguments. Generic specialization happens at the struct layout level (different LLVM struct types for different instantiations), not through name mangling in the constructor qualified name.

---

## LLVM Code Generation — generateConstructObject

At `src/uranite/ir/mir/codegen.cpp:6132-6540`, `generateConstructObject` is the most complex single-instruction code generator. It handles five distinct construction paths: compiler-intrinsic types, OOP wrappers, normal struct/class allocation with constructor invocation, field default initialization, and fallback raw allocation.

### Intrinsic Type Fast Paths

Two compiler-intrinsic types bypass normal struct allocation entirely.

**Memory\<T\> Construction** (codegen.cpp:6153-6178):

```cpp
if( shortTypeName ==
        semantic::qualname::classes::memory::Name ) {
    llvm::Type* elementType =
        this->resolveMemoryElementType(
            instruction.operandType );
    llvm::Value* capacityValue = nullptr;
    if( instruction.sourceOperands.empty() == false ) {
        capacityValue = this->loadVariableValue(
            instruction.sourceOperands[0] );
    }
    if( capacityValue == nullptr ) {
        capacityValue = llvm::ConstantInt::get(
            llvm::Type::getInt64Ty( this->llvmContext ), 16 );
    }
    llvm::Function* callocFunction =
        this->getOrCreateCalloc();
    llvm::DataLayout dataLayout( this->llvmModule.get() );
    uint64_t elementSize =
        dataLayout.getTypeAllocSize( elementType );
    llvm::Value* elementSizeValue = llvm::ConstantInt::get(
        llvm::Type::getInt64Ty( this->llvmContext ),
        elementSize );
    llvm::Value* rawPointer = this->irBuilder.CreateCall(
        callocFunction,
        { capacityValue, elementSizeValue }, "mem.raw" );
    this->setVariableValue(
        instruction.destinationVariable, rawPointer );
    this->memoryElementTypes[
        instruction.destinationVariable] = elementType;
    return;
}
```

`Memory<T>` is a raw heap-allocated array — not a struct. Construction:
1. Resolve element type `T` from generic parameter.
2. Extract capacity from first constructor argument (default: 16).
3. Call `calloc(capacity, elementSize)` — zero-initialized.
4. Store raw pointer as the result value.
5. Record element type in `memoryElementTypes` map for later index operations.

**Arena\<T\> Construction** (codegen.cpp:6182-6222):

`Arena<T>` is a struct with three fields: `{ ptr, i64 count, i64 capacity }`.
1. Allocate the arena struct via `malloc(sizeof(ArenaStruct))`.
2. Extract capacity from constructor argument (default: 1024).
3. Allocate element array via `calloc(capacity, elementSize)`.
4. Store element array pointer in field 0 (`arena.base.ptr`).
5. Store 0 in field 1 (`arena.count.ptr`).
6. Store capacity in field 2 (`arena.cap.ptr`).

### OOP Wrapper Construction

At codegen.cpp:6224-6249:

```cpp
if( descriptor::Builtin::oopWrapperNames.count(
        shortTypeName ) > 0 ) {
    semantic::TypeSharedPointer wrapperType =
        std::make_shared<semantic::Type>(
            semantic::Type::Kind::Class, typeName );
    llvm::Type* llvmTypeCheck =
        this->toLLVMType( wrapperType );
    if( instruction.sourceOperands.empty() == false ) {
        llvm::Value* argValue = this->loadVariableValue(
            instruction.sourceOperands[0] );
        if( argValue != nullptr ) {
            if( argValue->getType() != llvmTypeCheck ) {
                // ... type coercion (int cast, float/int
                //     conversion)
            }
            this->setVariableValue(
                instruction.destinationVariable, argValue );
            return;
        }
    }
    llvm::Value* defaultValue =
        llvm::Constant::getNullValue( llvmTypeCheck );
    this->setVariableValue(
        instruction.destinationVariable, defaultValue );
    return;
}
```

OOP wrapper types (`I64`, `F64`, `String`, `Bool`, `Char`, etc.) are not heap-allocated. Construction `new I64(42)` simply passes through the argument value with type coercion if needed. If no argument is provided, the default is null/zero for the underlying LLVM type.

Coercion rules for wrapper construction:

| From | To | Operation |
|---|---|---|
| Integer | Integer (different width) | `CreateIntCast` (sign-extending) |
| Integer | Float | `CreateSIToFP` |
| Float | Integer | `CreateFPToSI` |
| Same type | Same type | Direct pass-through |

### Struct Type Resolution

At codegen.cpp:6251-6267, the LLVM struct type is resolved through a three-step cascade:

```cpp
llvm::StructType* structType = nullptr;
if( this->structTypeCache.count( typeName ) > 0 ) {
    structType = this->structTypeCache[typeName];
}
else {
    structType = llvm::StructType::getTypeByName(
        this->llvmContext, typeName );
}
if( structType == nullptr && typeName.empty() == false ) {
    for( const std::pair<const std::string,
         llvm::StructType*>& cacheEntry :
         this->structTypeCache ) {
        size_t lastDot = cacheEntry.first.rfind( '.' );
        if( lastDot != std::string::npos &&
            cacheEntry.first.substr( lastDot + 1 ) ==
                shortTypeName ) {
            structType = cacheEntry.second;
            typeName = cacheEntry.first;
            break;
        }
    }
}
```

| Priority | Strategy | Description |
|---|---|---|
| 1 | `structTypeCache` by qualified name | Exact match in pre-built cache |
| 2 | LLVM context by name | `StructType::getTypeByName` global lookup |
| 3 | Short name suffix scan | Scans cache entries ending with `.ShortName` |

### Heap Allocation

At codegen.cpp:6268-6283:

```cpp
llvm::Function* mallocFunction =
    this->getOrCreateMalloc();
llvm::DataLayout dataLayout( this->llvmModule.get() );
uint64_t typeSize =
    dataLayout.getTypeAllocSize( structType );
if( typeSize < 64 ) {
    typeSize = 64;
}
llvm::Value* sizeValue = llvm::ConstantInt::get(
    llvm::Type::getInt64Ty( this->llvmContext ), typeSize );
llvm::Value* rawPointer = this->irBuilder.CreateCall(
    mallocFunction, { sizeValue }, "obj.raw" );
llvm::Value* typedPointer = this->irBuilder.CreateBitCast(
    rawPointer,
    llvm::PointerType::getUnqual( structType ), "obj" );
this->setVariableValue(
    instruction.destinationVariable, typedPointer );
this->concreteClassMap[
    instruction.destinationVariable] = typeName;
```

Key details:
- **Minimum allocation**: 64 bytes. Even empty structs get 64 bytes to prevent degenerate pointer aliasing and provide room for future field additions.
- **malloc, not calloc**: unlike `Memory<T>`, class/struct objects are allocated via `malloc` (uninitialized). Constructor and default field initialization handle zeroing.
- **Concrete class map**: the destination variable is recorded in `concreteClassMap` mapping variable ID to type name. This enables downstream method call dispatch to resolve the correct concrete class for virtual dispatch.

### VTable/ITable Initialization

At codegen.cpp:6284-6297:

```cpp
if( this->classesWithVtable.count( typeName ) > 0 ||
    this->classesWithVtable.count( shortTypeName ) > 0 ) {
    std::string itableKey = typeName;
    if( this->interfaceTableMap.count( itableKey ) == 0 ) {
        itableKey = shortTypeName;
    }
    if( this->interfaceTableMap.count( itableKey ) > 0 ) {
        llvm::GlobalVariable* itableGlobal =
            this->interfaceTableMap[itableKey];
        llvm::Value* vtableSlot =
            this->irBuilder.CreateStructGEP(
                structType, typedPointer, 0, "vtable.slot" );
        llvm::Value* itablePtr =
            this->irBuilder.CreateBitCast(
                itableGlobal,
                llvm::PointerType::getUnqual(
                    this->llvmContext ),
                "itable.ptr" );
        this->irBuilder.CreateStore( itablePtr, vtableSlot );
    }
}
```

For classes that implement interfaces (tracked in `classesWithVtable`):
1. Look up the interface dispatch table global variable from `interfaceTableMap`.
2. Get a GEP pointer to struct field 0 (the vtable slot — always the first field).
3. Store the itable global pointer into the vtable slot.

This enables runtime virtual dispatch: method calls on interface-typed references load the vtable pointer from field 0 and index into the dispatch table to find the concrete function pointer.

### Constructor Function Resolution

At codegen.cpp:6298-6345, the constructor function is resolved through a four-step cascade:

```cpp
std::string constructorName =
    typeName + "." + shortTypeName;
```

Constructor functions follow the naming convention `TypeName.ShortName` — the class name is both the namespace prefix and the method name. For `uranite.collection.array-list.ArrayList`, the constructor is `uranite.collection.array-list.ArrayList.ArrayList`.

| Priority | Strategy | Description |
|---|---|---|
| 1 | `functionResolutionMap[constructorName]` | Pre-registered function lookup |
| 2 | `llvmModule->getFunction(constructorName)` | Direct LLVM module lookup |
| 3 | `operandType->qualified` alternate name | Tries fully-qualified name from semantic type if different from `typeName` |
| 4 | Suffix scan of `functionResolutionMap` | Scans for entries ending with `.ShortName` where the preceding segment also matches `ShortName` |

### Constructor Arity Overload Resolution

At codegen.cpp:6346-6358:

```cpp
if( constructorFunction != nullptr &&
    constructorFunction->arg_size() !=
        instruction.sourceOperands.size() + 1 ) {
    std::string arityName = fmt::format(
        "{}#{}", constructorName,
        instruction.sourceOperands.size() + 1 );
    if( this->functionResolutionMap.count(
            arityName ) > 0 ) {
        constructorFunction =
            this->functionResolutionMap[arityName];
    }
    else {
        llvm::Function* arityFunction =
            this->llvmModule->getFunction( arityName );
        if( arityFunction != nullptr ) {
            constructorFunction = arityFunction;
        }
    }
}
```

When the found constructor's parameter count does not match the supplied argument count (plus 1 for `self`), arity-suffixed lookup is attempted. The mangled name format is `ConstructorName#ArgCount` — for example, `ArrayList.ArrayList#3` for a constructor taking `self` plus 2 arguments. This resolves constructor overloads at the LLVM level.

### Argument Type Coercion

At codegen.cpp:6359-6434, before calling the constructor, each argument is coerced to match the expected parameter type:

| Source Type | Target Type | Coercion |
|---|---|---|
| Integer | Integer (different width) | `CreateIntCast` (sign-extending) |
| Integer | Float | `CreateSIToFP` |
| Float | Integer | `CreateFPToSI` |
| Pointer | Pointer (different pointee) | `CreateBitCast` |
| Pointer | Integer | `CreatePtrToInt` |
| Integer | Pointer | `CreateIntToPtr` |

The `self` parameter receives the newly allocated typed pointer. If `self`'s expected type differs from the pointer type (rare — happens with certain inheritance patterns), `PtrToInt` or `SIToFP` conversions are applied.

Missing arguments (when `constructorArgs.size() < constructorFunction->arg_size()`) are filled with null values of the expected type:

```cpp
while( constructorArgs.size() <
       constructorFunction->arg_size() ) {
    unsigned argIndex = constructorArgs.size();
    llvm::Type* expectedType =
        constructorFunction->getArg( argIndex )->getType();
    constructorArgs.push_back(
        llvm::Constant::getNullValue( expectedType ) );
}
```

The constructor is called via `CreateCall` — not `CreateInvoke`. Constructor calls do not participate in exception handling at the construction site (exception handling within the constructor body is handled by the constructor function itself).

### Field Default Value Initialization

At codegen.cpp:6436-6529, after the constructor call, field default values from the class declaration are applied:

```cpp
if( constructSemanticType != nullptr &&
    constructSemanticType->kind ==
        semantic::Type::Kind::Class ) {
    semantic::ClassTypeSharedPointer constructClassType =
        std::static_pointer_cast<semantic::ClassType>(
            constructSemanticType );
    if( constructClassType->astDeclaration != nullptr ) {
        unsigned int defaultFieldIndex = 0;
        if( constructClassType->virtualTable.empty()
                == false ) {
            defaultFieldIndex = 1;
        }
        for( std::shared_ptr<ast::nodes::FieldDeclarationNode>&
             fieldDeclaration :
             constructClassType->astDeclaration->fields ) {
            if( fieldDeclaration->defaultValue != nullptr &&
                defaultFieldIndex <
                    structType->getNumElements() ) {
                // ... compile-time constant evaluation
                // ... CreateStructGEP + CreateStore
            }
            defaultFieldIndex++;
        }
    }
}
```

Key details:
- **VTable offset**: if the class has a virtual table (`virtualTable` non-empty), field indices start at 1 (field 0 is the vtable pointer). Otherwise, field indices start at 0.
- **Compile-time constants only**: default values are evaluated at compile time from the AST declaration. Supported literal types:

| AST Node Kind | LLVM Constant |
|---|---|
| `IntegerLiteral` | `ConstantInt` (or `ConstantFP` if field is float) |
| `FloatLiteral` | `ConstantFP` (or `ConstantInt` if field is integer) |
| `BooleanLiteral` | `ConstantInt` (1 or 0) |
| `CharLiteral` | `ConstantInt` (char code) |
| `NoneLiteral` | `Constant::getNullValue` |
| `StringLiteral` | `CreateGlobalStringPtr` |
| `UnaryExpression` (negation) | Negated `ConstantInt` or `ConstantFP` |

- **CreateStructGEP + CreateStore**: each default value is stored directly into the allocated struct via GEP to the field index and a store instruction.

Default values are applied regardless of whether the constructor explicitly initializes those fields — the constructor runs first and can overwrite them. This provides "safety net" initialization: fields have known values even if a constructor path misses them.

### Fallback Allocation

At codegen.cpp:6532-6539:

```cpp
else {
    llvm::Function* mallocFunction =
        this->getOrCreateMalloc();
    llvm::Value* sizeValue = llvm::ConstantInt::get(
        llvm::Type::getInt64Ty( this->llvmContext ), 64 );
    llvm::Value* rawPointer = this->irBuilder.CreateCall(
        mallocFunction, { sizeValue }, "obj.raw" );
    this->setVariableValue(
        instruction.destinationVariable, rawPointer );
}
```

When no LLVM struct type is found (type resolution completely failed), a raw 64-byte malloc is performed. No constructor is called, no fields are initialized. This is a graceful degradation path that prevents crashes from unresolvable types — the object is an opaque pointer.

---

## Concrete Class Propagation

At `src/uranite/ir/mir/codegen.cpp:60-100`, before code generation begins, a pre-pass scans all MIR functions to build a `concreteClassMap` that tracks which variables hold instances of which concrete classes:

```cpp
for( const MIRInstruction& instr :
     block->blockInstructions ) {
    if( instr.instructionKind ==
            MIRInstructionKind::ConstructObject &&
        instr.destinationVariable !=
            INVALID_VARIABLE_IDENTIFIER &&
        instr.calledFunctionQualifiedName.empty()
            == false ) {
        varConcreteClass[instr.destinationVariable] =
            instr.calledFunctionQualifiedName;
    }
}
```

Then a fixed-point propagation loop (up to 10 iterations) propagates concrete class information through `StoreVariable` and `LoadVariable` instructions:

- If `StoreVariable dest = source` and `source` maps to a concrete class, `dest` inherits that mapping.
- Propagation is skipped for `Interface` and `GenericParameter` typed destinations — these intentionally hold polymorphic references.

This propagation enables devirtualization: when a method call on a variable can be traced back to a specific `ConstructObject`, the codegen resolves the method directly instead of going through virtual dispatch.

---

## Examples

### Simple Class Construction

```
package examples

from uranite.io.console import puts

public class Greeter:
    String message

    public function Greeter(self, String message) -> Void:
        self.message = message

    public function greet(self) -> Void:
        puts(self.message)

public function main() -> I32:
    Greeter greeter = new Greeter("Hello, Uranite!")
    greeter.greet()
    return 0
```

**Compilation trace**:
1. **Parsing**: `new` keyword consumed. `parseTypeNode()` produces `SimpleTypeNode("Greeter")`. `(` matched, single string argument parsed as positional: `fields = [("", StringLiteral("Hello, Uranite!"))]`.
2. **Semantic analysis**: `resolveType("Greeter")` finds `ClassType("Greeter", isAbstract=false)`. Field expression analyzed. Expression type = `Greeter`.
3. **HIR lowering**: `semanticType` is resolved `Greeter`. Field lowered to `HIRStringLiteral`. Creates `HIRConstruct(constructedType=Greeter, fields=[("", HIRStringLiteral)])`.
4. **MIR lowering**: field lowered to `ConstantString` variable. `ConstructObject` emitted with `calledFunctionQualifiedName = "examples.Greeter"`, single source operand.
5. **LLVM codegen**:
   - Struct type resolved: `structTypeCache["examples.Greeter"]` found.
   - `malloc(max(sizeof(Greeter), 64))` allocates heap memory.
   - No vtable (no interfaces implemented) — vtable init skipped.
   - Constructor resolved: `"examples.Greeter.Greeter"` found in `functionResolutionMap`.
   - `CreateCall(@examples.Greeter.Greeter, %obj, %str_ptr)`.

### Generic Class Construction

```
package examples

from uranite.collection.array-list import ArrayList
from uranite.io.console import puts

public function main() -> I32:
    ArrayList<String> names = new ArrayList<String>()
    names.add("Alice")
    names.add("Bob")
    puts(names.size().toString())
    return 0
```

**Compilation trace**:
1. **Parsing**: `new` consumed. `parseTypeNode()` produces `GenericTypeNode("ArrayList", typeArgs=[SimpleTypeNode("String")])`. `()` matched with no arguments: `fields = []`.
2. **Semantic analysis**: `resolveType` resolves `ArrayList<String>` from type registry. No fields to analyze. Expression type = `ArrayList<String>`.
3. **MIR lowering**: `constructedType->qualified = "uranite.collection.array-list.ArrayList<String>"`. Generic bracket stripped: `calledFunctionQualifiedName = "uranite.collection.array-list.ArrayList"`. No source operands.
4. **LLVM codegen**:
   - Struct type resolved from cache.
   - `malloc` allocates heap.
   - Itable stored in vtable slot (ArrayList implements Iterable, etc.).
   - Constructor `"uranite.collection.array-list.ArrayList.ArrayList"` resolved. Arity check: 0 args + 1 self = 1, matches default constructor signature.
   - `CreateCall(@...ArrayList.ArrayList, %obj)`.

### Memory Intrinsic Construction

```
package examples

public function main() -> I32:
    Memory<I64> buffer = new Memory<I64>(256)
    buffer.set(0, 42)
    return 0
```

**Compilation trace**:
1. **Parsing/Semantic**: `new Memory<I64>(256)` parsed. Type resolves to `Memory<I64>`.
2. **MIR lowering**: `ConstructObject` with `calledFunctionQualifiedName = "Memory"` (or qualified variant), single operand `256`.
3. **LLVM codegen**: `shortTypeName == "Memory"` triggers intrinsic path. Element type resolved as `i64`. Capacity loaded: 256. `calloc(256, 8)` emits (8 = sizeof(i64)). Raw pointer stored. No constructor call, no struct allocation — just raw calloc'd array.

### Struct Construction with Named Fields

```
package examples

from uranite.io.console import puts

public struct Color:
    I64 red
    I64 green
    I64 blue

public function main() -> I32:
    Color color = new Color(red=255, green=128, blue=0)
    puts(color.red.toString())
    return 0
```

**Compilation trace**:
1. **Parsing**: `new Color(red=255, green=128, blue=0)`. Each argument detected as named: `IdentifierExpression` + `=` token. `fields = [("red", 255), ("green", 128), ("blue", 0)]`.
2. **Semantic analysis**: `resolveType("Color")` finds `StructType`. All field expressions analyzed as `I64`. No abstract/interface check needed — structs always pass.
3. **MIR lowering**: three source operands. `ConstructObject` with `calledFunctionQualifiedName = "examples.Color"`.
4. **LLVM codegen**: struct type resolved. `malloc` allocates. Constructor function `"examples.Color.Color"` resolved if user-defined. If no explicit constructor exists, arguments are stored directly to struct fields via GEP.

### Class with Field Defaults

```
package examples

from uranite.io.console import puts

public class Config:
    I64 timeout = 30
    String host = "localhost"
    Bool verbose = false

    public function Config(self) -> Void:
        pass

public function main() -> I32:
    Config config = new Config()
    puts(config.host)
    puts(config.timeout.toString())
    return 0
```

**Compilation trace**:
1. **Parsing/Semantic**: `new Config()` with no arguments.
2. **LLVM codegen**:
   - `malloc` allocates struct.
   - Constructor `Config.Config(self)` called — does nothing (`pass`).
   - Field default initialization loop runs:
     - `timeout`: `IntegerLiteral(30)` detected. `ConstantInt::get(i64, 30)`. `CreateStructGEP(structType, obj, 0)` + `CreateStore`.
     - `host`: `StringLiteral("localhost")` detected. `CreateGlobalStringPtr("localhost")`. GEP to field 1 + store.
     - `verbose`: `BooleanLiteral(false)` detected. `ConstantInt::get(i1, 0)`. GEP to field 2 + store.
   - Fields have known values even though constructor did not set them.

### OOP Wrapper Construction

```
package examples

from uranite.io.console import puts

public function main() -> I32:
    I64 value = new I64(42)
    F64 pi = new F64(3)
    puts(value.toString())
    puts(pi.toString())
    return 0
```

**Compilation trace**:
1. **Parsing/Semantic**: `new I64(42)` resolves to OOP wrapper type.
2. **LLVM codegen**: `shortTypeName = "I64"` found in `oopWrapperNames`. Argument `42` loaded as `i64`. Type matches expected `i64`. Value passed through directly — no malloc, no struct, no constructor call.
3. For `new F64(3)`: argument `3` is `i64`, expected type is `double`. `CreateSIToFP` coerces `i64 3` to `double 3.0`. Result stored.

### Abstract Class Rejection

```
package examples

public abstract class Shape:
    public abstract function area(self) -> F64;

public function main() -> I32:
    Shape shape = new Shape()
    return 0
```

**Compilation trace**:
1. **Semantic analysis**: `resolveType("Shape")` returns `ClassType` with `isAbstract = true`.
2. **Error emitted**: `"cannot instantiate abstract class \"Shape\""`.
3. Expression type set to `Error` — compilation continues but codegen for this expression is skipped.

### Interface Rejection

```
package examples

from uranite.collection.iterable import Iterable

public function main() -> I32:
    Iterable<I64> iter = new Iterable<I64>()
    return 0
```

**Compilation trace**:
1. **Semantic analysis**: `resolveType("Iterable")` returns `InterfaceType`.
2. **Error emitted**: `"cannot instantiate interface \"Iterable\""`.
