# Optional Types

Uranite's optional type system provides compile-time nullable safety through the `Optional<T>` wrapper. An optional type, written as `?T`, represents a value that is either a valid `T` or `None` (the null pointer). Optionals are first-class types with dedicated parsing syntax, semantic resolution, assignability rules, comparability rules, automatic unwrapping for member access and method calls, generic substitution support, and control-flow integration via pointer truthiness.

This document covers the complete optional type specification — the `OptionalType` struct, parsing via prefix and suffix `?` syntax, semantic resolution with `makeOptional()`, LLVM representation as an opaque pointer, the assignability hierarchy (None-to-Optional, T-to-Optional, Optional-to-Optional, Optional-to-T), comparability rules, automatic unwrapping in member access and method dispatch, generic parameter substitution, iterator unwrapping in for-in loops, and practical patterns for nullable programming.

---

## Table of Contents

- [Type Identity](#type-identity)
- [The OptionalType Struct](#the-optionaltype-struct)
  - [Structure](#structure)
  - [Display Representation](#display-representation)
  - [Factory Method](#factory-method)
- [LLVM Representation](#llvm-representation)
- [Syntax](#syntax)
  - [Prefix Syntax](#prefix-syntax)
  - [Suffix Syntax](#suffix-syntax)
  - [Parsing Implementation](#parsing-implementation)
- [Compilation Pipeline](#compilation-pipeline)
  - [Parser Stage](#parser-stage)
  - [Semantic Resolution](#semantic-resolution)
  - [HIR and MIR Stages](#hir-and-mir-stages)
  - [Codegen Stage](#codegen-stage)
- [Assignability Rules](#assignability-rules)
  - [None to Optional](#none-to-optional)
  - [T to Optional](#t-to-optional)
  - [Optional to Optional](#optional-to-optional)
  - [Optional to T](#optional-to-t)
  - [Complete Assignability Table](#complete-assignability-table)
- [Comparability Rules](#comparability-rules)
  - [Optional with None](#optional-with-none)
  - [Optional with Inner Type](#optional-with-inner-type)
  - [Complete Comparability Table](#complete-comparability-table)
- [Automatic Unwrapping](#automatic-unwrapping)
  - [Member Access Unwrapping](#member-access-unwrapping)
  - [Method Call Unwrapping](#method-call-unwrapping)
  - [Iterator Unwrapping](#iterator-unwrapping)
- [Generic Substitution](#generic-substitution)
  - [Type Parameter Substitution](#type-parameter-substitution)
  - [Generic Parameter Collection](#generic-parameter-collection)
- [None Checks](#none-checks)
  - [Equality Comparison](#equality-comparison)
  - [Identity with `is`](#identity-with-is)
  - [Truthiness Coercion](#truthiness-coercion)
- [Examples](#examples)
  - [Optional Declarations](#optional-declarations)
  - [Function Return Types](#function-return-types)
  - [None Checking Patterns](#none-checking-patterns)
  - [Optional Parameters](#optional-parameters)
  - [Practical Usage](#practical-usage)

---

## Type Identity

| Property | Value |
|---|---|
| Kind | `Type::Kind::Optional` |
| Display Name | `Optional<InnerType>` |
| Short Form | `?InnerType` |
| AST Node | `OptionalTypeNode` (kind: `Node::Kind::OptionalType`) |
| LLVM Type | `ptr` (opaque pointer) |
| Wraps | Any type `T` |

Optional is a parametric wrapper — it does not exist as a standalone type. `?String`, `?I64`, `?Boolean` are each distinct types with different inner types but the same LLVM representation.

---

## The OptionalType Struct

### Structure

```
struct OptionalType : Type {
    TypeSharedPointer inner;

    OptionalType(TypeSharedPointer inner)
        : Type(Type::Kind::Optional, fmt::format("Optional<{}>", inner->name))
        , inner(std::move(inner)) {}
};
```

Key fields:
- `kind`: Always `Type::Kind::Optional`.
- `name`: Formatted as `Optional<InnerTypeName>` — e.g., `Optional<String>`, `Optional<I64>`.
- `inner`: Shared pointer to the wrapped type. This is the type the optional can hold when it is not `None`.

`OptionalType` inherits from `Type` and adds the single `inner` field. No additional metadata (nullability flags, default values) is stored.

### Display Representation

The `toString()` method produces the shorthand `?` prefix form:

```
std::string toString() const override {
    return "?" + this->inner->toString();
}
```

| Full Form | Short Form |
|---|---|
| `Optional<String>` | `?String` |
| `Optional<I64>` | `?I64` |
| `Optional<Optional<Boolean>>` | `??Boolean` |
| `Optional<ArrayList<String>>` | `?ArrayList<String>` |

The `name` field uses the full `Optional<T>` form. The `toString()` output uses the `?T` shorthand. Both representations refer to the same type.

### Factory Method

Optional types are created through the type registry factory:

```
TypeSharedPointer Registry::makeOptional(TypeSharedPointer inner) {
    return std::make_shared<OptionalType>(std::move(inner));
}
```

This is a simple forwarding factory — no caching, deduplication, or validation. Each call creates a fresh `OptionalType` instance. Type identity is checked via structural comparison of the inner types, not pointer identity of the OptionalType instances.

---

## LLVM Representation

Optional types map to `llvm::PointerType::getUnqual(context)` — the same opaque pointer used for `String`, `None`, class instances, and other reference types:

```
case semantic::Type::Kind::Optional:
    return llvm::PointerType::getUnqual(this->llvmContext)
```

At the LLVM level, an optional value is a pointer that is either:
- **Non-null** — points to a valid value (or is the value itself for pointer-sized types).
- **Null** — represents `None`.

There is no separate "tag" or "discriminator" field. Nullability is determined by pointer value alone. This makes optionals zero-cost at the representation level — `?String` and `String` have identical LLVM types (`ptr`). The distinction exists only in the semantic type system.

| Source Type | LLVM Type | None Representation |
|---|---|---|
| `?String` | `ptr` | `null` |
| `?I64` | `ptr` | `null` |
| `?Boolean` | `ptr` | `null` |
| `?ArrayList<T>` | `ptr` | `null` |

---

## Syntax

### Prefix Syntax

The `?` prefix before a type name marks it as optional:

```uranite
?String maybeName = "Alice"
?I64 maybeCount = None
?Boolean maybeFlag = None
```

This is the primary syntax for declaring optional types.

### Suffix Syntax

The `?` can also appear as a suffix after a type (including after union types):

```uranite
String? maybeName = "Alice"
I64? maybeCount = None
```

Both `?String` and `String?` produce the same `OptionalType` wrapping `String`.

### Parsing Implementation

The `parseTypeNode()` method handles both prefix and suffix optional syntax:

```
TypeNodeSharedPointer Parser::parseTypeNode() {
    if( this->check(token::Type::Question) ) {
        // Prefix: ?Type
        prefixSource = this->current().source;
        this->advance();
        innerType = this->parseBaseTypeNode();
        return std::make_shared<OptionalTypeNode>(innerType, prefixSource);
    }

    baseType = this->parseBaseTypeNode();

    // Union types: Type | Type | ...
    if( this->check(token::Type::Pipe) ) {
        // ... parse union ...
    }

    if( this->check(token::Type::Question) ) {
        // Suffix: Type?
        suffixSource = this->current().source;
        this->advance();
        return std::make_shared<OptionalTypeNode>(baseType, suffixSource);
    }

    return baseType;
}
```

Parsing order:
1. Check for leading `?` — if present, parse as prefix optional.
2. Parse base type.
3. Check for `|` — if present, parse as union type.
4. Check for trailing `?` — if present, wrap in optional.

This means `?String | I64` parses as `Optional<String> | I64` (the `?` binds to `String` only), while `String | I64?` parses as `Optional<String | I64>` (the `?` wraps the entire union).

---

## Compilation Pipeline

### Parser Stage

The parser creates an `OptionalTypeNode` AST node:

```
struct OptionalTypeNode : TypeNode {
    TypeNodeSharedPointer innerType;

    OptionalTypeNode(TypeNodeSharedPointer innerType, const SourceSharedPointer& source)
        : TypeNode(Node::Kind::OptionalType, source)
        , innerType(std::move(innerType)) {}
};
```

The `innerType` field holds the AST representation of the wrapped type, which is itself a `TypeNode` — it may be a simple type name, a generic type, an array type, or another optional (for nested optionals like `??String`).

### Semantic Resolution

The semantic analyzer resolves `OptionalTypeNode` to an `OptionalType`:

```
case ast::Node::Kind::OptionalType: {
    OptionalTypeNode& optionalNode = static_cast<OptionalTypeNode&>(*typeNode);
    TypeSharedPointer innerType = this->resolveType(optionalNode.innerType);
    return innerType
        ? this->typeRegistry.makeOptional(innerType)
        : this->typeRegistry.getError();
}
```

Resolution proceeds recursively — the inner type is resolved first, then wrapped via `makeOptional()`. If the inner type fails to resolve (returns `nullptr`), an error type is returned instead of creating a malformed optional.

### HIR and MIR Stages

Optional types pass through HIR and MIR transparently. There are no special HIR or MIR node kinds for optional operations. The type information travels as `resolvedType` metadata on expressions and variables. Optional-specific behavior (unwrapping, None comparison) is handled at the semantic level (type checking) and codegen level (pointer null checks), not at the IR level.

In MIR lowering, method calls on optional-typed receivers are unwrapped:

```
if( receiverType->kind == semantic::Type::Kind::Optional ) {
    OptionalType* optionalType = static_cast<OptionalType*>(receiverType.get());
    if( optionalType->inner != nullptr ) {
        receiverType = optionalType->inner;
    }
}
```

This allows method dispatch to proceed as if the receiver were the inner type.

### Codegen Stage

At the LLVM level, optional values are plain pointers. No special codegen instruction exists for optional wrapping/unwrapping. The optionality is enforced purely through the semantic type system — by the time codegen runs, optional and non-optional values of the same base type produce identical LLVM IR.

---

## Assignability Rules

The `isAssignable()` function in the type registry handles optionals through several rules that form a bidirectional compatibility system.

### None to Optional

`None` and `Void` are unconditionally assignable to any optional type:

```
if( target->kind == Type::Kind::Optional ) {
    if( source->isVoid() || source->isNone() ) {
        return true;
    }
    ...
}
```

This enables the fundamental nullable initialization:

```uranite
?String name = None
?I64 count = None
```

### T to Optional

A value of type `T` is assignable to `Optional<T>` if `T` is assignable to the optional's inner type:

```
OptionalTypeSharedPointer targetOptionalType = std::static_pointer_cast<OptionalType>(target);
return this->isAssignable(targetOptionalType->inner, source);
```

This enables assigning concrete values to optional slots:

```uranite
?String name = "Alice"
?I64 count = 42
```

The check is recursive — if the inner type has its own assignability rules (e.g., integer widening), those rules apply through the optional wrapper.

### Optional to Optional

When both source and target are optional, the inner types are unwrapped and compared:

```
if( source->kind == Type::Kind::Optional ) {
    OptionalTypeSharedPointer sourceOptionalType = std::static_pointer_cast<OptionalType>(source);
    return this->isAssignable(targetOptionalType->inner, sourceOptionalType->inner);
}
```

This enables optional-to-optional assignment when the inner types are compatible:

```uranite
?I64 narrow = 42
?I64 wide = narrow
```

### Optional to T

When the source is optional but the target is not, the optional is unwrapped and the inner type is checked against the target:

```
if( source->kind == Type::Kind::Optional ) {
    OptionalTypeSharedPointer sourceOptionalType = std::static_pointer_cast<OptionalType>(source);
    return this->isAssignable(target, sourceOptionalType->inner);
}
```

This allows passing optional values where concrete values are expected — with the implicit risk that the optional might be `None` at runtime:

```uranite
?String maybeName = getName()
String name = maybeName
```

### Complete Assignability Table

| Source | Target | Assignable? | Mechanism |
|---|---|---|---|
| `None` | `?T` | Yes | `source->isNone()` check |
| `Void` | `?T` | Yes | `source->isVoid()` check |
| `T` | `?T` | Yes | Unwrap target, check inner |
| `?T` | `?T` | Yes | Unwrap both, check inners |
| `?T` | `T` | Yes | Unwrap source, check inner |
| `None` | `T` | No | None is not assignable to non-optional types |
| `?T` | `?U` | Depends | Unwrap both, check `isAssignable(U, T)` |
| `T` | `?U` | Depends | Unwrap target, check `isAssignable(U, T)` |

---

## Comparability Rules

### Optional with None

Optional types are always comparable with `None` and `Void`:

```
if( x->kind == Type::Kind::Optional && ( y->isVoid() || y->isNone() ) ) {
    return true;
}
if( y->kind == Type::Kind::Optional && ( x->isVoid() || x->isNone() ) ) {
    return true;
}
```

This enables the fundamental None check:

```uranite
?String name = getValue()
if name == None:
    puts( "No value" )
```

### Optional with Inner Type

When one operand is optional, the comparability check unwraps it and compares the inner type:

```
if( x->kind == Type::Kind::Optional ) {
    OptionalTypeSharedPointer optionalType = std::static_pointer_cast<OptionalType>(x);
    return this->isComparable(optionalType->inner, y);
}
if( y->kind == Type::Kind::Optional ) {
    OptionalTypeSharedPointer optionalType = std::static_pointer_cast<OptionalType>(y);
    return this->isComparable(x, optionalType->inner);
}
```

This allows comparing optional values with concrete values of the same type:

```uranite
?I64 maybeCount = 42
if maybeCount == 42:
    puts( "Found" )
```

### Complete Comparability Table

| Left | Right | Comparable? | Mechanism |
|---|---|---|---|
| `?T` | `None` | Yes | Direct None check |
| `?T` | `Void` | Yes | Direct Void check |
| `None` | `?T` | Yes | Symmetric None check |
| `?T` | `T` | Yes | Unwrap left, compare inner |
| `T` | `?T` | Yes | Unwrap right, compare inner |
| `?T` | `?T` | Yes | Identity match via `equals()` |
| `?T` | `?U` | Depends | Unwrap, check inner comparability |

---

## Automatic Unwrapping

The semantic analyzer and MIR lowering automatically unwrap optional types in several contexts, allowing optional values to be used directly without explicit null checks in member access, method dispatch, and iterator consumption.

### Member Access Unwrapping

When accessing a member on an optional-typed value, the semantic analyzer unwraps the optional and resolves the member against the inner type:

```
if( objectType->kind == Type::Kind::Optional ) {
    OptionalTypeSharedPointer optionalType = std::static_pointer_cast<OptionalType>(objectType);
    if( optionalType->inner ) {
        if( optionalType->inner->kind == Type::Kind::Class ) {
            ClassTypeSharedPointer classType = std::static_pointer_cast<ClassType>(optionalType->inner);
            FieldInfo* fieldInformation = classType->findField(expression.member);
            if( fieldInformation ) {
                return fieldInformation->type;
            }
            MethodInfo* methodInformation = classType->findMethod(expression.member);
            if( methodInformation && methodInformation->isProperty ) {
                FunctionTypeSharedPointer functionType = std::dynamic_pointer_cast<FunctionType>(methodInformation->type);
                if( functionType ) {
                    return functionType->returnType;
                }
            }
        }
        else if( optionalType->inner->kind == Type::Kind::Struct ) {
            StructTypeSharedPointer structType = std::static_pointer_cast<StructType>(optionalType->inner);
            FieldInfo* fieldInformation = structType->findField(expression.member);
            if( fieldInformation ) {
                return fieldInformation->type;
            }
        }
    }
}
```

This handles both class fields and struct fields. Property-style methods (methods marked `isProperty`) are also resolved through the optional wrapper.

### Method Call Unwrapping

In MIR lowering, method calls on optional-typed receivers unwrap the optional to determine the owner class name for dispatch:

```
if( receiverType->kind == semantic::Type::Kind::Optional ) {
    semantic::OptionalType* optionalType = static_cast<semantic::OptionalType*>(receiverType.get());
    if( optionalType->inner != nullptr ) {
        receiverType = optionalType->inner;
    }
}
ownerClassName = receiverType->name;
```

This allows calling methods on optional values without explicit unwrapping:

```uranite
?String name = "Alice"
I64 length = name.length()
```

The method is dispatched as if `name` were a `String`. If `name` is `None` at runtime, the behavior is a null pointer dereference — the type system permits it but runtime safety is the developer's responsibility.

### Iterator Unwrapping

When a for-in loop iterates over a collection whose `next()` method returns an optional (the standard iterator protocol — `next()` returns `?T`, yielding `None` to signal exhaustion), the semantic analyzer unwraps the optional to determine the loop variable type:

```
TypeSharedPointer nextReturnType = hasIteratorInterface(iterableType);
if( nextReturnType ) {
    if( nextReturnType->kind == Type::Kind::Optional ) {
        variableType = std::static_pointer_cast<OptionalType>(nextReturnType)->inner;
    }
    else {
        variableType = nextReturnType;
    }
}
```

This means `for String item in collection:` works even when the iterator's `next()` returns `?String` — the optional is stripped, and the loop variable is typed as `String`.

---

## Generic Substitution

### Type Parameter Substitution

When substituting concrete types for generic parameters, optional types are preserved through the substitution:

```
if( type->kind == Type::Kind::Optional ) {
    TypeSharedPointer inner = this->substituteGenericParameters(
        std::static_pointer_cast<OptionalType>(type)->inner, substitutionMap
    );
    return this->typeRegistry.makeOptional(inner);
}
```

Given `?T` where `T` is mapped to `String` in the substitution map, the result is `?String`. The optional wrapper is reconstructed around the substituted inner type.

The same pattern applies in the `substituteType` helper used for method return type monomorphization:

```
if( targetType->kind == Type::Kind::Optional ) {
    return this->typeRegistry.makeOptional(
        substituteType(std::static_pointer_cast<OptionalType>(targetType)->inner, substitutionMap)
    );
}
```

### Generic Parameter Collection

When collecting generic parameter names from type annotations (for scope analysis), optionals are recursively descended:

```
case ast::Node::Kind::OptionalType: {
    OptionalTypeNode& optionalNode = static_cast<OptionalTypeNode&>(*typeNode);
    collectGenericParamNamesFromType(optionalNode.innerType, typeRegistry, results);
    break;
}
```

This ensures that `?T` correctly registers `T` as a generic parameter that needs resolution.

---

## None Checks

Three mechanisms exist for checking whether an optional value is `None`.

### Equality Comparison

The `==` and `!=` operators can compare any value with `None`:

```uranite
?String name = getValue()
if name == None:
    puts( "absent" )
if name != None:
    puts( name )
```

The semantic analyzer has a special bypass that permits None comparisons regardless of type:

```
bool isNoneComparison = rightSideType->isNone() || leftSideType->isNone();
if( isEqualityOp && isNoneComparison ) {
    return this->typeRegistry.getBool();
}
```

At the LLVM level, `value == None` compiles to `ICmpEQ(value, ConstantPointerNull)`.

### Identity with `is`

The `is` keyword performs identity (pointer equality) comparison:

```uranite
if name is None:
    puts( "absent" )
if name is not None:
    puts( name )
```

`is` lowers to `CompareEqual` in MIR — identical to `==` at the instruction level. The distinction is semantic: `is` tests identity (same pointer), `==` tests equality (same content). For None checks, both produce the same result since None is always the null pointer.

`is not` is parsed as `is` followed by `not`, producing a `UnaryExpression(KeywordNot, BinaryExpression(KeywordIs, left, right))`.

### Truthiness Coercion

Optional values (being pointers) participate in truthiness coercion in conditional contexts:

```uranite
?String name = getValue()
if name:
    puts( name )
```

The `generateBranchConditional()` function coerces pointer values:

```
if( conditionValue->getType()->isPointerTy() ) {
    conditionValue = ICmpNE(conditionValue, ConstantPointerNull, "cond.bool")
}
```

Non-null pointer (has value) is truthy. Null pointer (None) is falsy. This provides the most concise None check syntax.

| Check Style | Syntax | Equivalent LLVM |
|---|---|---|
| Equality | `value == None` | `ICmpEQ(value, null)` |
| Identity | `value is None` | `ICmpEQ(value, null)` |
| Truthiness | `if value:` | `ICmpNE(value, null)` |
| Negated equality | `value != None` | `ICmpNE(value, null)` |
| Negated identity | `value is not None` | `not ICmpEQ(value, null)` |

---

## Examples

### Optional Declarations

```uranite
?String maybeName = "Alice"
?String noName = None

?I64 maybeCount = 42
?I64 noCount = None

?Boolean maybeFlag = True
?Boolean noFlag = None
```

### Function Return Types

```uranite
from uranite.io.console import puts

public function findByName( String target ) -> ?String:
    if target == "admin":
        return "Administrator"
    if target == "root":
        return "Superuser"
    return None

public function parseInt( String text ) -> ?I64:
    if text == "0":
        return 0
    if text == "1":
        return 1
    return None

public function main() -> I32:
    ?String admin = findByName( "admin" )
    ?String unknown = findByName( "guest" )

    if admin is not None:
        puts( admin )

    if unknown is None:
        puts( "User not found" )

    return 0
```

### None Checking Patterns

```uranite
from uranite.io.console import puts

public function checkEquality( ?String value ) -> Void:
    if value == None:
        puts( "None via ==" )

public function checkIdentity( ?String value ) -> Void:
    if value is None:
        puts( "None via is" )

public function checkTruthiness( ?String value ) -> Void:
    if value:
        puts( "Has value" )
    else:
        puts( "None via truthiness" )

public function checkNegatedIdentity( ?String value ) -> Void:
    if value is not None:
        puts( "Not None" )

public function main() -> I32:
    ?String present = "hello"
    ?String absent = None

    checkEquality( absent )
    checkIdentity( absent )
    checkTruthiness( present )
    checkTruthiness( absent )
    checkNegatedIdentity( present )

    return 0
```

### Optional Parameters

```uranite
from uranite.io.console import puts

public function greet( String name, ?String title ) -> String:
    if title is not None:
        return title + " " + name
    return name

public function connect( String host, ?I64 port ) -> String:
    if port is not None:
        return host + ":8080"
    return host + ":80"

public function main() -> I32:
    puts( greet( "Alice", "Dr." ) )
    puts( greet( "Bob", None ) )

    puts( connect( "localhost", 8080 ) )
    puts( connect( "localhost", None ) )

    return 0
```

### Practical Usage

```uranite
from uranite.io.console import puts

public function firstPresent( ?String alpha, ?String beta, ?String gamma ) -> ?String:
    if alpha is not None:
        return alpha
    if beta is not None:
        return beta
    if gamma is not None:
        return gamma
    return None

public function withDefault( ?String value, String fallback ) -> String:
    if value:
        return value
    return fallback

public function mapOptional( ?I64 value, I64 offset ) -> ?I64:
    if value is None:
        return None
    return value + offset

public function allPresent( ?String first, ?String second ) -> Boolean:
    return first is not None and second is not None

public function nonePresent( ?String first, ?String second ) -> Boolean:
    return first is None and second is None

public function formatEntry( String key, ?String value ) -> String:
    if value is None:
        return key + " = <none>"
    return key + " = " + value

public function main() -> I32:
    ?String result = firstPresent( None, "backup", "fallback" )
    puts( withDefault( result, "default" ) )

    puts( formatEntry( "name", "Alice" ) )
    puts( formatEntry( "email", None ) )

    ?I64 base = 10
    ?I64 shifted = mapOptional( base, 5 )
    ?I64 noShift = mapOptional( None, 5 )

    if shifted is not None:
        puts( "Shifted value exists" )
    if noShift is None:
        puts( "No shift possible" )

    Boolean both = allPresent( "a", "b" )
    Boolean neither = nonePresent( None, None )
    puts( both.toString() )
    puts( neither.toString() )

    return 0
```

This example demonstrates first-present selection across multiple optionals, default value substitution via truthiness, optional mapping (applying a transformation only when a value exists), multi-optional presence checks with short-circuit `and`, formatted output with None fallback text, and the zero-overhead nature of optional checks — all compiled to `ICmpEQ`/`ICmpNE` against null pointer constants with no boxing, tagging, or wrapper allocation.
