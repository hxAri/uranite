# Type Compatibility

Uranite enforces type compatibility at compile time through two core predicates in the type registry: `isAssignable` determines whether a source type can be stored, passed, or returned where a target type is expected, and `isComparable` determines whether two types can participate in comparison operations (`==`, `!=`, `<`, `>`, `<=`, `>=`). Both predicates are implemented as methods on the `Registry` class in `src/uranite/semantic/typeref.cpp` and are invoked throughout the semantic analyzer at every assignment, function call argument, return statement, comparison expression, and logical operator usage. Type identity — determining whether two types represent the same logical type — uses the `typeIdentityMatch` helper and the `Type::equals` virtual method, both of which compare fully-qualified names as the authoritative key with fallback to short names for primitives and error-recovery types.

This document covers the `Type::equals` virtual method for structural type identity, the `typeIdentityMatch` static helper, the complete `isAssignable` predicate with all 20+ compatibility rules (error passthrough, exact match, Void equivalence, Object universality, class-to-class identity and inheritance, interface conformance with transitive super-interface traversal, numeric widening, OOP wrapper interchangeability, primitive-OOP bidirectional bridging, generic parameter erasure, optional unwrapping, reference unwrapping, union member containment, callable-function structural subtyping, Future/Meta covariance), the `isComparable` predicate, the `ClassType::implementsInterface` method, the `InterfaceType::extendsInterface` method, and how these predicates are invoked across the semantic analyzer for assignments, arguments, returns, comparisons, overload resolution, and interface conformance verification.

---

## Table of Contents

- [Overview](#overview)
- [Type Identity](#type-identity)
  - [Type::equals](#typeequals)
  - [typeIdentityMatch](#typeidentitymatch)
  - [Qualified Name Authority](#qualified-name-authority)
- [Assignability — isAssignable](#assignability--isassignable)
  - [Early Returns](#early-returns)
  - [Object Universality](#object-universality)
  - [Class-to-Class Compatibility](#class-to-class-compatibility)
  - [Interface Conformance](#interface-conformance)
  - [Inheritance Chain Walking](#inheritance-chain-walking)
  - [Numeric Compatibility](#numeric-compatibility)
  - [OOP Wrapper Interchangeability](#oop-wrapper-interchangeability)
  - [Primitive-OOP Bridging](#primitive-oop-bridging)
  - [Generic Parameter Erasure](#generic-parameter-erasure)
  - [Optional Unwrapping](#optional-unwrapping)
  - [Reference Unwrapping](#reference-unwrapping)
  - [Union Member Containment](#union-member-containment)
  - [Callable-Function Structural Subtyping](#callable-function-structural-subtyping)
  - [Future and Meta Covariance](#future-and-meta-covariance)
- [Comparability — isComparable](#comparability--iscomparable)
- [Interface Hierarchy Checking](#interface-hierarchy-checking)
  - [ClassType::implementsInterface](#classtypeimplementsinterface)
  - [InterfaceType::extendsInterface](#interfacetypeextendsinterface)
- [Semantic Analyzer Usage](#semantic-analyzer-usage)
  - [Assignment Checking](#assignment-checking)
  - [Variable Declaration Checking](#variable-declaration-checking)
  - [Function Argument Checking](#function-argument-checking)
  - [Overload Resolution Scoring](#overload-resolution-scoring)
  - [Return Type Checking](#return-type-checking)
  - [Comparison Expression Checking](#comparison-expression-checking)
  - [Logical Operator Checking](#logical-operator-checking)
  - [Interface Conformance Verification](#interface-conformance-verification)
- [Compatibility Matrix](#compatibility-matrix)
- [Examples](#examples)
  - [Numeric Widening](#numeric-widening)
  - [Interface Assignability](#interface-assignability)
  - [Optional and Union Compatibility](#optional-and-union-compatibility)

---

## Overview

Type compatibility in Uranite is **directional** — `isAssignable(target, source)` answers whether `source` can flow into `target`. The relationship is not symmetric: `I64` is assignable to `F64`, but `F64` is also assignable to `I64` (both directions permitted for numeric types). Compatibility checking occurs at semantic analysis time; the compiler emits errors for incompatible assignments, arguments, and returns before any IR is generated.

Three separate mechanisms compose the compatibility system:

| Mechanism | Purpose | Method |
|---|---|---|
| Type identity | Exact same type | `Type::equals`, `typeIdentityMatch` |
| Assignability | Can source flow into target | `Registry::isAssignable` |
| Comparability | Can two values be compared | `Registry::isComparable` |

---

## Type Identity

### Type::equals

Defined in `src/uranite/semantic/typeref.hpp:109-120`:

```cpp
virtual bool equals( const std::shared_ptr<Type>& other ) const {
    if( other == nullptr || this->kind != other->kind ) {
        return false;
    }
    if( this->qualified.empty() == false &&
        other->qualified.empty() == false ) {
        return this->qualified == other->qualified;
    }
    if( this->package.empty() == false &&
        other->package.empty() == false ) {
        return this->package == other->package &&
               this->name == other->name;
    }
    return this->name == other->name;
}
```

Three-tier identity resolution:

1. **Qualified name comparison**: When both types have non-empty `qualified` fields, compare them directly. This is the authoritative path — fully-qualified names are globally unique (`"uranite.language.i64.I64"`).
2. **Package + name comparison**: When qualified names are absent but packages are set, compare both package and name.
3. **Short name fallback**: When neither qualified name nor package is available (primitives, error recovery types, monomorphized generics whose qualified name was not yet propagated), compare short names only.

The `kind` field must match before any name comparison — an `Integer` kind and a `Class` kind are never equal regardless of names.

### typeIdentityMatch

A static helper in `src/uranite/semantic/typeref.cpp:103-108`:

```cpp
static bool typeIdentityMatch(
    const TypeSharedPointer& left,
    const TypeSharedPointer& right ) {
    if( left->qualified.empty() == false &&
        right->qualified.empty() == false ) {
        return left->qualified == right->qualified;
    }
    return left->name == right->name;
}
```

Simplified two-tier identity check used inside `isAssignable` for comparing types during inheritance chain walking and interface matching. Unlike `Type::equals`, this does **not** require `kind` to match, which allows it to match across type representations (e.g., a `ClassType` against an `InterfaceType` when their qualified names correspond).

### Qualified Name Authority

The `qualified` field is the canonical identity key for all types. It follows the `package.module.TypeName` convention defined in `src/uranite/semantic/qualnames.hpp`:

```
uranite.language.i64.I64           (I64 OOP wrapper)
uranite.builtin.i64                (i64 primitive)
uranite.operators.addable.Addable  (Addable interface)
uranite.collection.pair.Pair       (Pair class)
```

Short names (`"I64"`, `"String"`, `"Addable"`) are ambiguous across modules. The `isAssignable` method relies on `typeIdentityMatch` with qualified names to avoid false positives from name collisions.

---

## Assignability — isAssignable

The `isAssignable` method in `src/uranite/semantic/typeref.cpp:110-538` implements over 20 compatibility rules evaluated in sequence. The first rule that matches determines the result. Rules are ordered from most common (exact match) to most specialized (callable structural subtyping).

### Early Returns

```cpp
if( target == nullptr || source == nullptr ) {
    return false;
}
if( target->isError() || source->isError() ) {
    return true;
}
if( target->equals( source ) ) {
    return true;
}
if( target->isVoid() && source->isVoid() ) {
    return true;
}
```

| Rule | Condition | Result | Purpose |
|---|---|---|---|
| Null guard | Either type is null | `false` | Safety check |
| Error passthrough | Either type is `Error` kind | `true` | Error recovery — prevents cascading diagnostics |
| Exact match | `target->equals(source)` | `true` | Same type identity |
| Void equivalence | Both are Void (primitive or OOP) | `true` | `void` and `Void` are interchangeable |

Error passthrough is critical: when a previous analysis error produces an `Error` sentinel type, all downstream compatibility checks pass silently. This prevents one error from producing dozens of follow-on "type mismatch" diagnostics.

### Object Universality

```cpp
if( target->kind == Type::Kind::Class ) {
    ClassTypeSharedPointer classTargetType =
        std::static_pointer_cast<ClassType>( target );
    if( classTargetType->qualified == qualname::Object ||
        classTargetType->name == "Object" ) {
        return true;
    }
}
if( source->kind == Type::Kind::Class ) {
    ClassTypeSharedPointer classSourceType =
        std::static_pointer_cast<ClassType>( source );
    if( classSourceType->qualified == qualname::Object ||
        classSourceType->name == "Object" ) {
        return true;
    }
}
```

`Object` is the universal type — any value is assignable to `Object`, and `Object` is assignable to any class target. This enables generic containers and heterogeneous collections without explicit casting. The check is bidirectional: both `Object` as target and `Object` as source match.

### Class-to-Class Compatibility

When both target and source are `Class` kind, multiple identity checks are attempted in sequence:

1. **Qualified name match**: If both have non-empty qualified names, compare directly.
2. **AST declaration identity**: If both have `astDeclaration` pointers and the declaration names match, they represent the same class (handles monomorphized generics where qualified names may differ).
3. **Cross-reference AST name**: If one type has an `astDeclaration` whose name matches the other type's `name`, they are compatible (handles template base vs. monomorphized instance).

```cpp
if( targetClassType->astDeclaration &&
    sourceClassType->astDeclaration &&
    targetClassType->astDeclaration->name ==
        sourceClassType->astDeclaration->name ) {
    return true;
}
```

This ensures that `ArrayList<I64>` (monomorphized) is assignable to a target typed as `ArrayList` (template base).

### Interface Conformance

When target is `Interface` and source is `Class`, the method checks whether the source class implements the target interface — directly or transitively through super-interfaces:

```cpp
if( target->kind == Type::Kind::Interface &&
    source->kind == Type::Kind::Class ) {
    // ... transitive interface traversal ...
}
```

The algorithm:

1. Collect all interfaces from the source class's `interfaces` list.
2. If the source has an `astDeclaration`, also look up the base template's interfaces.
3. Walk the collected interfaces using a worklist with cycle detection (`checkedTypes` vector):
   - For each interface, check `typeIdentityMatch` against the target.
   - If the interface has an `astDeclaration`, check AST pointer equality.
   - Push all `superInterfaces` onto the worklist for transitive checking.
   - If `superInterfaces` is empty but `astDeclaration` exists, resolve super-interfaces from the AST nodes (handles lazy resolution).
4. Walk the source class's inheritance chain (`baseClass`) and check each ancestor's interfaces.

The same pattern applies in reverse (target=Class, source=Interface) and for interface-to-interface assignability — both use the same worklist-with-cycle-detection algorithm to traverse the super-interface DAG.

### Inheritance Chain Walking

For class-to-class assignability, the inheritance chain is walked separately from interface conformance:

```cpp
if( target->kind == Type::Kind::Class &&
    source->kind == Type::Kind::Class ) {
    ClassTypeSharedPointer sourceClassType =
        std::static_pointer_cast<ClassType>( source );
    TypeSharedPointer baseType = sourceClassType->baseClass;
    while( baseType ) {
        if( typeIdentityMatch( baseType, target ) ) {
            return true;
        }
        if( baseType->kind == Type::Kind::Class ) {
            baseType = std::static_pointer_cast<ClassType>(
                baseType )->baseClass;
        }
        else {
            break;
        }
    }
}
```

Walks from source up through `baseClass` pointers. If any ancestor matches the target via `typeIdentityMatch`, the assignment is valid. This enables standard polymorphism — a derived class is assignable to its base class.

### Numeric Compatibility

Numeric types are broadly interchangeable in Uranite:

```cpp
if( target->kind == Type::Kind::Integer &&
    source->kind == Type::Kind::Integer ) {
    return true;
}
if( target->isIntegral() && source->isIntegral() ) {
    return true;
}
if( target->isFloatingPoint() && source->isIntegral() ) {
    return true;
}
if( target->isFloatingPoint() && source->isFloatingPoint() ) {
    return true;
}
if( target->isIntegral() && source->isFloatingPoint() ) {
    return true;
}
```

| Source | Target | Allowed | Notes |
|---|---|---|---|
| Integer (any width) | Integer (any width) | Yes | Includes narrowing (`I64` to `I8`) |
| Integer | Float | Yes | Promotion |
| Float | Float | Yes | Includes narrowing (`F64` to `F32`) |
| Float | Integer | Yes | Truncation (fractional part lost) |

The `isIntegral()` and `isFloatingPoint()` methods check both the primitive `Kind` and OOP wrapper qualified names (via `qualname::isIntegerOop` / `qualname::isFloatOop`), so `I64` (OOP class) and `i64` (primitive) are both treated as integral.

Notably, Uranite allows implicit narrowing at the semantic level. The actual codegen inserts the appropriate LLVM instruction (`Trunc`, `FPTrunc`, `FPToSI`) based on bit widths, but no compile-time warning is issued for lossy conversions.

### OOP Wrapper Interchangeability

```cpp
if( target->kind == Type::Kind::Class &&
    source->kind == Type::Kind::Class ) {
    bool targetIsIntOop = qualname::isIntegerOop( target->qualified );
    bool sourceIsIntOop = qualname::isIntegerOop( source->qualified );
    bool targetIsFloatOop = qualname::isFloatOop( target->qualified );
    bool sourceIsFloatOop = qualname::isFloatOop( source->qualified );
    if( ( targetIsIntOop && sourceIsIntOop ) ||
        ( targetIsFloatOop && sourceIsFloatOop ) ||
        ( targetIsFloatOop && sourceIsIntOop ) ) {
        return true;
    }
}
```

OOP wrapper classes for numeric types are interchangeable among themselves:
- Integer OOP to integer OOP: `I8` to `I64`, `U32` to `I16`, etc.
- Float OOP to float OOP: `F32` to `F64`, `Double` to `Float`, etc.
- Integer OOP to float OOP: `I64` to `F64`, `Int` to `Float`, etc.
- Float OOP to integer OOP: checked separately with reversed flags.

The `qualname::isIntegerOop` and `qualname::isFloatOop` functions check the qualified name against sets defined in `qualnames.hpp`:

```cpp
inline const std::unordered_set<std::string>& integerOopQualified() {
    static const std::unordered_set<std::string> qualified = {
        Int, I8, I16, I32, I64, Integer, Long, Byte,
        UInt, U8, U16, U32, U64
    };
    return qualified;
}

inline const std::unordered_set<std::string>& floatOopQualified() {
    static const std::unordered_set<std::string> qualified = {
        Float, F32, F64, Double
    };
    return qualified;
}
```

### Primitive-OOP Bridging

Bidirectional maps connect primitive types to their OOP wrappers and vice versa:

```cpp
if( target->isPrimitive() && source->kind == Type::Kind::Class ) {
    static const std::unordered_map<std::string,std::string>
        oopToPrimitiveMap = {
            {"Boolean","bool"}, {"String","str"}, {"Char","char"},
            {"Byte","u8"}, {"Integer","i32"}, {"Long","i64"},
            {"Double","f64"}, {"I8","i8"}, {"I16","i16"},
            {"I32","i32"}, {"I64","i64"}, {"U8","u8"},
            {"U16","u16"}, {"U32","u32"}, {"U64","u64"},
            {"F32","f32"}, {"F64","f64"}, {"Int","i64"},
            {"UInt","u64"}, {"Float","f64"}
        };
    // ... lookup and compare ...
}
```

This enables seamless interchange between `bool` (primitive) and `Boolean` (OOP wrapper), `str` (primitive) and `String` (OOP wrapper), etc. The bridging works in both directions — primitive to OOP class and OOP class to primitive.

Additionally, any primitive integer is assignable to any integer OOP wrapper, and any primitive float is assignable to any float OOP wrapper:

```cpp
if( source->kind == Type::Kind::Integer &&
    qualname::isIntegerOop( target->qualified ) ) {
    return true;
}
if( source->kind == Type::Kind::Float &&
    qualname::isFloatOop( target->qualified ) ) {
    return true;
}
```

### Generic Parameter Erasure

```cpp
if( target->kind == Type::Kind::GenericParameter ||
    source->kind == Type::Kind::GenericParameter ) {
    return true;
}
```

Unresolved generic parameters (`T`, `E`, `K`, `V`) are compatible with everything. This allows generic code to type-check before monomorphization. At instantiation time, the generic parameter is replaced with a concrete type via `typeSubstitutions`, and full compatibility checking applies to the concrete types.

### Optional Unwrapping

```cpp
if( target->kind == Type::Kind::Optional ) {
    if( source->isVoid() || source->isNone() ) {
        return true;
    }
    OptionalTypeSharedPointer targetOptionalType =
        std::static_pointer_cast<OptionalType>( target );
    if( source->kind == Type::Kind::Optional ) {
        OptionalTypeSharedPointer sourceOptionalType =
            std::static_pointer_cast<OptionalType>( source );
        return this->isAssignable(
            targetOptionalType->inner, sourceOptionalType->inner );
    }
    return this->isAssignable( targetOptionalType->inner, source );
}
if( source->kind == Type::Kind::Optional ) {
    OptionalTypeSharedPointer sourceOptionalType =
        std::static_pointer_cast<OptionalType>( source );
    return this->isAssignable( target, sourceOptionalType->inner );
}
```

Optional compatibility rules:
- `None` and `Void` are assignable to any optional target — they represent the absent value.
- `?T` is assignable to `?U` if `T` is assignable to `U` (covariant unwrapping).
- A non-optional `T` is assignable to `?T` — implicit wrapping.
- `?T` is assignable to `T` — implicit unwrapping (the programmer assumes presence).

### Reference Unwrapping

```cpp
if( target->kind == Type::Kind::Reference ) {
    ReferenceTypeSharedPointer referenceTargetType =
        std::static_pointer_cast<ReferenceType>( target );
    return this->isAssignable( referenceTargetType->inner, source );
}
```

When the target is a reference type `&T`, the inner type `T` is extracted and compatibility is checked against the source. References are transparent for assignment purposes — a value of type `T` is assignable to `&T`.

### Union Member Containment

```cpp
if( target->kind == Type::Kind::Union ) {
    UnionTypeSharedPointer unionTargetType =
        std::static_pointer_cast<UnionType>( target );
    for( TypeSharedPointer& memberType : unionTargetType->types ) {
        if( this->isAssignable( memberType, source ) ) {
            return true;
        }
    }
}
if( source->kind == Type::Kind::Union ) {
    UnionTypeSharedPointer unionSourceType =
        std::static_pointer_cast<UnionType>( source );
    bool isAllMemberAssignable = true;
    for( TypeSharedPointer& memberType : unionSourceType->types ) {
        if( this->isAssignable( target, memberType ) == false ) {
            isAllMemberAssignable = false;
            break;
        }
    }
    if( isAllMemberAssignable ) {
        return true;
    }
}
```

Union compatibility is **asymmetric**:
- **Source to union target**: The source need only be assignable to **any one** member of the union. `I64` is assignable to `I64 | String` because `I64` matches one member.
- **Union source to target**: **Every** member of the source union must be assignable to the target. `I64 | String` is only assignable to `Object` if both `I64` and `String` are assignable to `Object`.

### Callable-Function Structural Subtyping

```cpp
if( target->kind == Type::Kind::Callable &&
    source->kind == Type::Kind::Function ) {
    CallableTypeSharedPointer targetCallableType =
        std::static_pointer_cast<CallableType>( target );
    FunctionTypeSharedPointer sourceFunctionType =
        std::static_pointer_cast<FunctionType>( source );
    if( targetCallableType->parameterTypes.size() !=
        sourceFunctionType->parameterTypes.size() ) {
        return false;
    }
    for( size_t parameterIndex = 0;
         parameterIndex < targetCallableType->parameterTypes.size();
         parameterIndex++ ) {
        if( this->isAssignable(
            targetCallableType->parameterTypes[parameterIndex],
            sourceFunctionType->parameterTypes[parameterIndex]
            ) == false ) {
            return false;
        }
    }
    return this->isAssignable(
        targetCallableType->returnType,
        sourceFunctionType->returnType );
}
```

`Callable<Return, <Params...>>` and `Function` types are structurally compatible when:
- Parameter counts match exactly.
- Each parameter type is pairwise assignable (covariant — target parameter must accept source parameter).
- Return types are assignable (covariant).

This check works in all three combinations: `Callable` target with `Function` source, `Function` target with `Callable` source, and `Callable` target with `Callable` source.

### Future and Meta Covariance

```cpp
if( target->kind == Type::Kind::Future &&
    source->kind == Type::Kind::Future ) {
    FutureTypeSharedPointer targetFutureType =
        std::static_pointer_cast<FutureType>( target );
    FutureTypeSharedPointer sourceFutureType =
        std::static_pointer_cast<FutureType>( source );
    return this->isAssignable(
        targetFutureType->innerType,
        sourceFutureType->innerType );
}
if( target->kind == Type::Kind::Meta &&
    source->kind == Type::Kind::Meta ) {
    MetaTypeSharedPointer targetMetaType =
        std::static_pointer_cast<MetaType>( target );
    MetaTypeSharedPointer sourceMetaType =
        std::static_pointer_cast<MetaType>( source );
    return this->isAssignable(
        targetMetaType->innerType,
        sourceMetaType->innerType );
}
```

Both `Future<T>` and `Meta<T>` are covariant: `Future<Derived>` is assignable to `Future<Base>` if `Derived` is assignable to `Base`. Same for `Meta<T>`.

---

## Comparability — isComparable

The `isComparable` method in `src/uranite/semantic/typeref.cpp:540-583` determines whether two types can participate in comparison operations:

```cpp
bool Registry::isComparable(
    const TypeSharedPointer& x,
    const TypeSharedPointer& y ) const {
    if( x == nullptr || y == nullptr ) {
        return false;
    }
    if( x->isError() || y->isError() ) {
        return true;
    }
    if( x->kind == Type::Kind::Reference ) {
        return this->isComparable(
            std::static_pointer_cast<ReferenceType>( x )->inner, y );
    }
    if( y->kind == Type::Kind::Reference ) {
        return this->isComparable(
            x, std::static_pointer_cast<ReferenceType>( y )->inner );
    }
    if( x->isNumeric() && y->isNumeric() ) {
        return true;
    }
    if( x->equals( y ) ) {
        return true;
    }
    if( x->kind == Type::Kind::Optional &&
        ( y->isVoid() || y->isNone() ) ) {
        return true;
    }
    if( y->kind == Type::Kind::Optional &&
        ( x->isVoid() || x->isNone() ) ) {
        return true;
    }
    if( x->isNone() || y->isNone() ) {
        return true;
    }
    if( x->kind == Type::Kind::Optional ) {
        OptionalTypeSharedPointer optionalType =
            std::static_pointer_cast<OptionalType>( x );
        return this->isComparable( optionalType->inner, y );
    }
    if( y->kind == Type::Kind::Optional ) {
        OptionalTypeSharedPointer optionalType =
            std::static_pointer_cast<OptionalType>( y );
        return this->isComparable( x, optionalType->inner );
    }
    if( x->kind == Type::Kind::Meta &&
        y->kind == Type::Kind::Meta ) {
        return true;
    }
    if( this->isAssignable( x, y ) ||
        this->isAssignable( y, x ) ) {
        return true;
    }
    return false;
}
```

Comparability rules in evaluation order:

| Priority | Rule | Description |
|---|---|---|
| 1 | Null guard | Either null returns `false` |
| 2 | Error passthrough | Either `Error` returns `true` |
| 3 | Reference unwrap | Unwrap `&T` to `T`, recurse |
| 4 | Numeric compatibility | Any two numeric types comparable |
| 5 | Exact type match | Same type via `equals` |
| 6 | Optional-None | `?T` compared with `None`/`Void` |
| 7 | None universality | `None` compared with anything |
| 8 | Optional unwrap | `?T` compared with `U` checks `T` vs `U` |
| 9 | Meta-Meta | Any two `Meta<T>` types comparable |
| 10 | Bidirectional assignability | Falls back to `isAssignable` in both directions |

Key difference from `isAssignable`: comparability is **symmetric**. The final fallback tries `isAssignable` in both directions — if either direction succeeds, the types are comparable. This means `I64` and `F64` are comparable even though they are different types, because they are assignable in both directions.

---

## Interface Hierarchy Checking

### ClassType::implementsInterface

Defined in `src/uranite/semantic/typeref.cpp:676-692`:

```cpp
bool ClassType::implementsInterface(
    const std::string& qualifiedName ) const {
    for( const TypeSharedPointer& iface : this->interfaces ) {
        if( iface->qualified == qualifiedName ||
            qualname::startsWith(
                iface->qualified, qualifiedName ) ) {
            return true;
        }
        if( iface->kind == Type::Kind::Interface ) {
            if( std::static_pointer_cast<InterfaceType>(
                iface )->extendsInterface( qualifiedName ) ) {
                return true;
            }
        }
    }
    if( this->baseClass &&
        this->baseClass->kind == Type::Kind::Class ) {
        return std::static_pointer_cast<ClassType>(
            this->baseClass )->implementsInterface( qualifiedName );
    }
    return false;
}
```

Checks three paths:
1. Direct interface match (qualified name or prefix match for generic instances).
2. Transitive super-interface match via `extendsInterface`.
3. Inherited interface match by recursing up the base class chain.

### InterfaceType::extendsInterface

Defined in `src/uranite/semantic/typeref.hpp:881-894`:

```cpp
bool extendsInterface(
    const std::string& qualifiedName ) const {
    for( const TypeSharedPointer& superIface :
         this->superInterfaces ) {
        if( superIface->qualified == qualifiedName ||
            qualname::startsWith(
                superIface->qualified, qualifiedName ) ) {
            return true;
        }
        if( superIface->kind == Type::Kind::Interface ) {
            if( std::static_pointer_cast<InterfaceType>(
                superIface )->extendsInterface(
                    qualifiedName ) ) {
                return true;
            }
        }
    }
    return false;
}
```

Recursively walks the super-interface hierarchy. Uses `qualname::startsWith` for prefix matching to handle generic interface instances where the qualified name includes type parameters.

---

## Semantic Analyzer Usage

### Assignment Checking

In `src/uranite/semantic/analyzer.cpp:1119-1122`, assignment statements validate target-source compatibility:

```cpp
if( targetType && valueType &&
    this->typeRegistry.isAssignable(
        targetType, valueType ) == false ) {
    std::string assignErrorMessage = fmt::format(
        "cannot assign value of type \"{}\" "
        "to target of type \"{}\"",
        valueType->toString(), targetType->toString() );
    this->diagnostic.error(
        statement.source, assignErrorMessage );
}
```

### Variable Declaration Checking

In `src/uranite/semantic/analyzer.cpp:4046-4049`, variable initializers are checked against the declared type:

```cpp
else if( initializerType &&
    this->typeRegistry.isAssignable(
        variableType, initializerType ) == false ) {
    std::string typeMismatchErrorMessage = fmt::format(
        "cannot assign value of type \"{}\" "
        "to variable of type \"{}\"",
        initializerType->toString(),
        variableType->toString() );
    this->diagnostic.error(
        statement.source, typeMismatchErrorMessage );
}
```

When no type annotation is given (`variableType == nullptr`), the variable type is inferred from the initializer — no compatibility check needed.

### Function Argument Checking

In `src/uranite/semantic/analyzer.cpp:1487-1491`, each positional argument is checked against the expected parameter type:

```cpp
if( argumentType &&
    this->typeRegistry.isAssignable(
        expectedType, argumentType ) == false ) {
    std::string functionArgumentMismatchErrorMessage =
        fmt::format(
            "argument type mismatch: "
            "expected \"{}\" but found \"{}\"",
            expectedTypeName, foundTypeName );
    this->diagnostic.error(
        expression.arguments[index]->source,
        functionArgumentMismatchErrorMessage );
}
```

Variadic arguments are checked against `functionType->variadicElementType`, and keyword arguments against `functionType->keywordValueType` or `functionType->keywordOnlyParamTypes`.

### Overload Resolution Scoring

In `src/uranite/semantic/analyzer.cpp:1366-1406`, overload resolution uses `isAssignable` for scoring:

```cpp
if( candidateFunction->parameterTypes[paramIndex]->toString()
    == argumentTypes[paramIndex]->toString() ) {
    score += 2;
}
else if( this->typeRegistry.isAssignable(
    candidateFunction->parameterTypes[paramIndex],
    argumentTypes[paramIndex] ) ) {
    score += 1;
}
else {
    compatible = false;
    break;
}
```

Scoring system:
- **Exact type match** (via `toString()` comparison): +2 points per parameter.
- **Assignable but not exact**: +1 point per parameter (e.g., `I32` argument to `I64` parameter).
- **Not assignable**: candidate is eliminated.

Highest-scoring candidate wins. Ties are broken by exact arity match (non-variadic with exact argument count preferred). If tied candidates remain, an "ambiguous call" error is emitted.

### Return Type Checking

In `src/uranite/semantic/analyzer.cpp:3605-3607`:

```cpp
else if( this->currentReturnType && valueType &&
    this->typeRegistry.isAssignable(
        this->currentReturnType, valueType ) == false ) {
    std::string returnTypeMismatchErrorMessage = fmt::format(
        "return type mismatch: "
        "expected \"{}\" but found \"{}\"",
        this->currentReturnType->toString(),
        valueType->toString() );
    this->diagnostic.error(
        statement.source, returnTypeMismatchErrorMessage );
}
```

The returned value must be assignable to the function's declared return type.

### Comparison Expression Checking

In `src/uranite/semantic/analyzer.cpp:1187-1191`:

```cpp
if( this->typeRegistry.isComparable(
    leftSideType, rightSideType ) == false ) {
    std::string compareErrorMessage = fmt::format(
        "cannot compare types \"{}\" and \"{}\"",
        leftSideType->toString(),
        rightSideType->toString() );
    this->diagnostic.error(
        expression.source, compareErrorMessage );
}
return this->typeRegistry.getBool();
```

Comparison operators (`==`, `!=`, `<`, `>`, `<=`, `>=`) always produce `Bool` type. The comparability check determines whether the comparison is valid.

### Logical Operator Checking

In `src/uranite/semantic/analyzer.cpp:1217-1220`:

```cpp
if( this->typeRegistry.isAssignable(
    boolType, leftSideType ) == false ||
    this->typeRegistry.isAssignable(
        boolType, rightSideType ) == false ) {
    this->diagnostic.error( expression.source,
        "logical operators require boolean operands" );
}
```

`and` and `or` operators require both operands to be assignable to `Bool`.

### Interface Conformance Verification

In `src/uranite/semantic/analyzer.cpp:5310-5338`, when a class declares it implements an interface, each interface method must have a matching implementation:

```cpp
returnTypeMatches = this->typeRegistry.isAssignable(
    interfaceFunction->returnType,
    implFunction->returnType );
```

Parameter counts must match, and the implementation's return type must be assignable to the interface method's return type. If assignability fails, a secondary fallback compares base names (stripping generic parameters) — handles cases where generic instantiation produces different type representations.

---

## Compatibility Matrix

Summary of assignability relationships between type categories:

| Source ↓ / Target → | Integer | Float | Bool | Char | String | Class | Interface | Optional | Union | Callable | Future | Meta | GenericParam |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| **Integer** | Yes | Yes | No | No | No | If OOP int/float | If implements | Yes (wrap) | If member | No | No | No | Yes |
| **Float** | Yes | Yes | No | No | No | If OOP float/int | If implements | Yes (wrap) | If member | No | No | No | Yes |
| **Bool** | No | No | Yes | No | No | If Boolean OOP | No | Yes (wrap) | If member | No | No | No | Yes |
| **Char** | No | No | No | Yes | No | If Char OOP | No | Yes (wrap) | If member | No | No | No | Yes |
| **String** | No | No | No | No | Yes | If String OOP | No | Yes (wrap) | If member | No | No | No | Yes |
| **Class** | If numeric OOP | If float OOP | If Boolean | If Char | If String | Identity/inherit | If implements | Yes (wrap) | If member | No | No | No | Yes |
| **Interface** | No | No | No | No | No | If class implements | Extends chain | Yes (wrap) | If member | No | No | No | Yes |
| **None/Void** | No | No | No | No | No | No | No | Yes | No | No | No | No | Yes |
| **Function** | No | No | No | No | No | No | No | No | No | Structural | No | No | Yes |
| **GenericParam** | Yes | Yes | Yes | Yes | Yes | Yes | Yes | Yes | Yes | Yes | Yes | Yes | Yes |

---

## Examples

### Numeric Widening

Implicit numeric compatibility without explicit casts:

```
package examples

from uranite.io.console import puts

public function sumAsFloat(F64 first, F64 second) -> F64:
    return first + second

public function main() -> I32:
    I32 smallInt = 42
    I64 bigInt = smallInt
    F64 floatVal = bigInt
    I8 tinyInt = 127
    F32 smallFloat = 3.14

    F64 result = sumAsFloat(smallInt, tinyInt)
    puts(result.toString())

    return 0
```

**Compatibility trace**:
1. `I64 bigInt = smallInt`: `isAssignable(I64, I32)` — both `Integer` kind — `true`. Codegen emits `SExt`.
2. `F64 floatVal = bigInt`: `isAssignable(F64, I64)` — `target.isFloatingPoint() && source.isIntegral()` — `true`. Codegen emits `SIToFP`.
3. `sumAsFloat(smallInt, tinyInt)`: For each argument, `isAssignable(F64, I32)` and `isAssignable(F64, I8)` — both pass via floating-point target + integral source rule.
4. `I8 tinyInt = 127`: Literal `127` typed as `I64`. `isAssignable(I8, I64)` — both integer kind — `true`. Codegen emits `Trunc`.

### Interface Assignability

Class-to-interface compatibility through implementation:

```
package examples

from uranite.operators.addable import Addable
from uranite.operators.equatable import Equatable
from uranite.io.console import puts

public interface Printable:
    public function display(self) -> Void;

public class Point implements Addable, Equatable, Printable:
    public I64 xCoord
    public I64 yCoord

    public function Point(self, I64 xCoord, I64 yCoord) -> Void:
        self.xCoord = xCoord
        self.yCoord = yCoord

    public function add(self, Object other) -> Object:
        Point otherPoint = other as Point
        return new Point(
            self.xCoord + otherPoint.xCoord,
            self.yCoord + otherPoint.yCoord)

    public function equals(self, Object other) -> Boolean:
        Point otherPoint = other as Point
        return self.xCoord == otherPoint.xCoord and
               self.yCoord == otherPoint.yCoord

    public function display(self) -> Void:
        puts("(" + self.xCoord.toString() + ", " +
             self.yCoord.toString() + ")")

public function render(Printable item) -> Void:
    item.display()

public function main() -> I32:
    Point origin = new Point(0, 0)
    Printable printableRef = origin
    Addable addableRef = origin
    Object objectRef = origin

    render(origin)

    return 0
```

**Compatibility trace**:
1. `Printable printableRef = origin`: `isAssignable(Printable, Point)` — target is Interface, source is Class. Worklist traversal finds `Printable` in `Point.interfaces` — `true`.
2. `Addable addableRef = origin`: Same pattern, finds `Addable` in `Point.interfaces` — `true`.
3. `Object objectRef = origin`: Object universality rule — target is Object — `true`.
4. `render(origin)`: `isAssignable(Printable, Point)` for the argument — same interface conformance check — `true`.

### Optional and Union Compatibility

Optional wrapping/unwrapping and union member containment:

```
package examples

from uranite.io.console import puts

public function findValue(
    Boolean condition) -> ?I64:
    if condition:
        return 42
    return None

public function processResult(
    I64 | String value) -> Void:
    puts("processing value")

public function main() -> I32:
    ?I64 maybeValue = findValue(true)
    ?I64 noneValue = None
    I64 | String flexible = 100
    I64 | String alsoFlexible = "hello"

    if maybeValue is not None:
        I64 unwrapped = maybeValue
        processResult(unwrapped)

    processResult("fallback")

    return 0
```

**Compatibility trace**:
1. `return 42` (inside `findValue`): Return type is `?I64`. `isAssignable(?I64, I64)` — target is Optional, source assignable to inner type `I64` — `true`.
2. `return None`: `isAssignable(?I64, None)` — target is Optional, source is None — `true`.
3. `?I64 noneValue = None`: Same optional-None rule — `true`.
4. `I64 | String flexible = 100`: `isAssignable(I64 | String, I64)` — target is Union, source assignable to member `I64` — `true`.
5. `I64 | String alsoFlexible = "hello"`: Source is `String`, assignable to union member `String` — `true`.
6. `I64 unwrapped = maybeValue`: `isAssignable(I64, ?I64)` — source is Optional, inner `I64` assignable to target `I64` — `true`.
7. `processResult(unwrapped)`: `isAssignable(I64 | String, I64)` — union member match — `true`.
8. `processResult("fallback")`: `isAssignable(I64 | String, String)` — union member match — `true`.
