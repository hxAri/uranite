# Member Access Expressions

Uranite supports member access via the dot operator (`object.member`) and the double-colon operator (`Type::member`). A member access expression `object.member` produces a `MemberAccessExpression` AST node carrying the object sub-expression and the member name string. The semantic analyzer resolves member access through six distinct type paths: classes (field lookup with base class fallback, then property method check), structs (same field-then-property pattern), tuples (numeric index access into element types), enums (`.name` string, `.value` backed value, variant lookup), interfaces (property method search with super-interface traversal), and optionals (transparent unwrap into inner type). At HIR level, field accesses lower to `HIRFieldAccess` nodes, but property accesses (getter methods with no arguments) desugar into `HIRMethodCall` nodes instead. At MIR level, `lowerFieldAccess` handles two cases: enum variant access folds to `ConstantInteger` at compile time, while struct/class field access emits `ComputeFieldAddress` with a resolved field layout index. At LLVM codegen, `generateComputeFieldAddress` produces `CreateStructGEP` for known struct layouts, falls back to getter function calls for property accessors, and handles enum `.name`/`.value` via discriminant-driven `CreateSelect` chains.

---

## Table of Contents

- [Syntax](#syntax)
- [AST Representation — MemberAccessExpression](#ast-representation--memberaccessexpression)
- [Parsing — Postfix Dot and Double-Colon](#parsing--postfix-dot-and-double-colon)
  - [Dot Operator](#dot-operator)
  - [Double-Colon Operator](#double-colon-operator)
  - [Method Call vs Field Access Disambiguation](#method-call-vs-field-access-disambiguation)
- [Semantic Analysis — Six Type Paths](#semantic-analysis--six-type-paths)
  - [Path 1: Class Types](#path-1-class-types)
  - [Path 2: Struct Types](#path-2-struct-types)
  - [Path 3: Tuple Types](#path-3-tuple-types)
  - [Path 4: Enum Types](#path-4-enum-types)
  - [Path 5: Interface Types](#path-5-interface-types)
  - [Path 6: Optional Types](#path-6-optional-types)
  - [Error: No Member](#error-no-member)
- [HIR Representation — Field Access vs Property Desugaring](#hir-representation--field-access-vs-property-desugaring)
- [MIR Lowering — lowerFieldAccess](#mir-lowering--lowerfieldaccess)
  - [Enum Variant Path — Compile-Time Constant](#enum-variant-path--compile-time-constant)
  - [Struct and Class Path — ComputeFieldAddress](#struct-and-class-path--computefieldaddress)
  - [TypeLayoutDescriptor](#typelayoutdescriptor)
- [MIR Lowering — Field Assignment](#mir-lowering--field-assignment)
- [LLVM Codegen — generateComputeFieldAddress](#llvm-codegen--generatecomputefieldaddress)
  - [Enum Name and Value](#enum-name-and-value)
  - [Struct GEP Path](#struct-gep-path)
  - [Property Getter Fallback](#property-getter-fallback)
  - [Struct Type Resolution Chain](#struct-type-resolution-chain)
- [Examples](#examples)

---

## Syntax

```
object.member
Type::member
```

The dot operator `.` accesses fields, properties, and enum members on an instance. The double-colon operator `::` accesses static members on a type name.

```
I64 length = list.size
String name = direction.name
I64 value = Color::Red
String greeting = person.firstName
```

---

## AST Representation — MemberAccessExpression

Defined at `src/uranite/ast/node.hpp:1614-1637`:

```cpp
struct MemberAccessExpression : Expression {

    std::string member;
    ExpressionSharedPointer object;

    MemberAccessExpression(
        ExpressionSharedPointer object,
        const std::string& member,
        const lookup::SourceSharedPointer& source
    ) : Expression( Node::Kind::MemberAccessExpression, source ),
        member( member ),
        object( std::move( object ) ) {
    }

};
```

| Field | Type | Description |
|---|---|---|
| `object` | `ExpressionSharedPointer` | Instance being accessed |
| `member` | `std::string` | Name of the field or member |

The member name is always a plain string identifier — not an expression. Complex member selection (computed field names) is not supported.

---

## Parsing — Postfix Dot and Double-Colon

Both `.` and `::` are postfix operators parsed inside `parsePostfixExpression` at `src/uranite/parser/parser.cpp:2049-2123`.

### Dot Operator

At `parser.cpp:2049-2093`:

```cpp
else if( this->check( token::Type::Dot ) ) {
    lookup::SourceSharedPointer source = this->current().source;
    this->advance();
    std::string member;
    if( this->check( token::Type::Identifier ) ) {
        member = this->advance().value;
    }
    std::vector<ast::nodes::TypeNodeSharedPointer> methodTypeArgs;
    if( this->check( token::Type::LessThan ) &&
        this->looksLikeGenericCall() ) {
        methodTypeArgs = this->parseGenericArguments();
    }
    if( this->check( token::Type::LeftParenthesis ) ) {
        // Method call path — creates MethodCallExpression
        ...
    }
    else {
        expression = std::make_shared<
            ast::nodes::MemberAccessExpression>(
                expression, member, source );
    }
}
```

### Double-Colon Operator

At `parser.cpp:2102-2123`:

```cpp
else if( this->check( token::Type::DoubleColon ) ) {
    lookup::SourceSharedPointer source = this->current().source;
    this->advance();
    std::string member;
    if( this->check( token::Type::Identifier ) ) {
        member = this->advance().value;
    }
    if( this->check( token::Type::LeftParenthesis ) ) {
        // Static method call
        ...
        expression = std::make_shared<
            ast::nodes::MethodCallExpression>(
                expression, member,
                std::move( arguments ), source );
    }
    else {
        expression = std::make_shared<
            ast::nodes::MemberAccessExpression>(
                expression, member, source );
    }
}
```

Both operators produce `MemberAccessExpression` for non-call access. The `::` variant does not parse generic type arguments — it follows simpler identifier-then-optional-call logic.

### Method Call vs Field Access Disambiguation

The parser disambiguates at the token level:

| After member name | Result |
|---|---|
| `(` (LeftParenthesis) | `MethodCallExpression` — method call |
| Anything else | `MemberAccessExpression` — field/property access |

`object.member(args)` is a method call. `object.member` is a field access. This is purely syntactic — the semantic analyzer later determines whether a "field access" is actually a property getter.

Generic type arguments (`<T>`) are parsed between the member name and the opening parenthesis for dot-operator method calls, using `looksLikeGenericCall()` lookahead to disambiguate from comparison operators.

Member access expressions chain left-to-right as postfix operators. `a.b.c` parses as `MemberAccessExpression(MemberAccessExpression(a, "b"), "c")`.

---

## Semantic Analysis — Six Type Paths

At `src/uranite/semantic/analyzer.cpp:3266-3454`, `analyzeMemberAccessExpression` resolves member access through six type-specific paths.

### Path 1: Class Types

At `analyzer.cpp:3276-3318`:

1. **Field lookup**: Call `classType->findField(member)`. If found, return field type.
2. **Base class fallback**: If not found and `astDeclaration` exists, look up the unmonomorphized base class via `typeRegistry.lookupType(name)` and retry `findField`.
3. **Generic substitution**: If the field type is a `GenericParameter`, look up the concrete type in `classType->typeSubstitutions`. `ArrayList<String>` with field type `E` resolves to `String`.
4. **Property check**: If no field found, call `classType->findMethod(member)`. If the method exists and `isProperty == true`, return the method's return type (with generic substitution). Properties are getter methods accessed without parentheses.

```
class Person:
    public String firstName
    public String lastName

    @property
    public function fullName(self) -> String:
        return self.firstName + " " + self.lastName
```

`person.firstName` resolves through field lookup. `person.fullName` resolves through property check — `fullName` is a method marked `@property`, so `isProperty == true`.

### Path 2: Struct Types

At `analyzer.cpp:3319-3361`:

Same pattern as classes: field lookup with base struct fallback, generic parameter substitution, then property method check. Structs are value types but share the same member resolution logic.

### Path 3: Tuple Types

At `analyzer.cpp:3362-3372`:

```cpp
else if( objectType->kind == Type::Kind::Tuple ) {
    TupleTypeSharedPointer tupleType =
        std::static_pointer_cast<TupleType>( objectType );
    try {
        int tupleIndex = std::stoi( expression.member );
        if( tupleIndex >= 0 &&
            static_cast<size_t>(tupleIndex) <
                tupleType->elements.size() ) {
            return tupleType->elements[tupleIndex];
        }
    }
    catch( ... ) {
    }
}
```

Tuple members are accessed by numeric index: `tuple.0`, `tuple.1`, `tuple.2`. The member name string is parsed as an integer via `std::stoi`. Bounds checking is performed against the tuple element count. Non-numeric member names silently fall through to the error path.

### Path 4: Enum Types

At `analyzer.cpp:3373-3399`:

```cpp
else if( objectType->kind == Type::Kind::Enum ) {
    if( expression.member == qualname::fields::Name ) {
        return this->typeRegistry.lookupPrimitive(
            qualname::classes::string::Name );
    }
    if( expression.member == qualname::fields::Value ) {
        return enumType->backedType
            ? enumType->backedType
            : this->typeRegistry.getInteger32();
    }
    if( enumType->findVariant( expression.member ) ) {
        return objectType;
    }
    // Error: "no variant or property in enum"
}
```

| Member | Result Type | Description |
|---|---|---|
| `"name"` | `String` | Variant name as string |
| `"value"` | Backed type or `I32` | Variant discriminant value |
| Variant name | Same enum type | Variant access (e.g., `Direction.North`) |

If none match, error: "no variant or property \"X\" in enum \"Y\"".

### Path 5: Interface Types

At `analyzer.cpp:3401-3421`:

```cpp
if( objectType->kind == Type::Kind::Interface ) {
    InterfaceTypeSharedPointer interfaceType =
        std::static_pointer_cast<InterfaceType>( objectType );
    std::vector<InterfaceType*> typesToCheck;
    typesToCheck.push_back( interfaceType.get() );
    while( typesToCheck.empty() == false ) {
        InterfaceType* currentInterface = typesToCheck.back();
        typesToCheck.pop_back();
        for( MethodInfo& methodInformation :
                currentInterface->methods ) {
            if( methodInformation.name == expression.member &&
                methodInformation.isProperty ) {
                FunctionTypeSharedPointer functionType =
                    std::dynamic_pointer_cast<FunctionType>(
                        methodInformation.type );
                if( functionType ) {
                    return functionType->returnType;
                }
            }
        }
        for( TypeSharedPointer& superInterfaceType :
                currentInterface->superInterfaces ) {
            if( superInterfaceType &&
                superInterfaceType->kind == Type::Kind::Interface ) {
                typesToCheck.push_back(
                    std::static_pointer_cast<InterfaceType>(
                        superInterfaceType ).get() );
            }
        }
    }
}
```

Interfaces only support property methods — no fields. Uses iterative DFS through the super-interface chain. If `Sequence` defines a property `size` and `MutableSequence` extends `Sequence`, then `mutableSequence.size` resolves through the super-interface traversal.

### Path 6: Optional Types

At `analyzer.cpp:3423-3451`:

Optional types transparently unwrap. `Optional<Person>` with member access `.firstName` looks inside the inner type (`Person`) and resolves the field there. Supports both class and struct inner types. If the inner class has the field, returns its type. If the inner class has a property method matching the name, returns the property return type.

This enables safe member access on nullable values without explicit unwrapping in the type checker — runtime null checks are a separate concern.

### Error: No Member

If none of the six paths match:

```
error: no member "X" in type "Y"
```

---

## HIR Representation — Field Access vs Property Desugaring

At `src/uranite/ir/hir/lowering.cpp:937-996`, the HIR lowering makes a critical distinction:

**Field access** — plain `HIRFieldAccess`:

```cpp
return std::make_shared<HIRFieldAccess>(
    std::move( objectExpression ),
    memberAccess.member,
    expression->semanticType,
    expression->source
);
```

**Property access** — desugars to `HIRMethodCall` with empty arguments:

```cpp
if( isPropertyAccess ) {
    std::vector<HIRNodeSharedPointer> emptyArguments;
    return std::make_shared<HIRMethodCall>(
        std::move( objectExpression ),
        memberAccess.member,
        std::move( emptyArguments ),
        expression->semanticType,
        expression->source
    );
}
```

The lowering checks each type kind (Class, Interface, Trait, Struct) for a matching method with `isProperty == true` and no corresponding field. When both a field and a property method share the same name, the field wins — `hasField` is checked before property promotion.

`HIRFieldAccess` at `src/uranite/ir/hir.hpp:708-724`:

```cpp
struct HIRFieldAccess : HIRNode {

    HIRNodeSharedPointer objectExpression;
    std::string fieldName;
    int fieldLayoutIndex = -1;

    HIRFieldAccess(
        HIRNodeSharedPointer objectExpression,
        const std::string& fieldName,
        semantic::TypeSharedPointer resolvedType,
        const lookup::SourceSharedPointer& sourceLocation
    ) : HIRNode( HIRNodeKind::FieldAccess,
            std::move( resolvedType ), sourceLocation ),
        objectExpression( std::move( objectExpression ) ),
        fieldName( fieldName ) {
    }

};
```

| Field | Type | Description |
|---|---|---|
| `objectExpression` | `HIRNodeSharedPointer` | Lowered object expression |
| `fieldName` | `std::string` | Name of the accessed field |
| `fieldLayoutIndex` | `int` | Pre-resolved struct layout index (-1 if unresolved) |

---

## MIR Lowering — lowerFieldAccess

At `src/uranite/ir/mir/lowering.cpp:3763-3850`, `lowerFieldAccess` takes two paths.

### Enum Variant Path — Compile-Time Constant

When the object type is `Kind::Enum`:

```cpp
if( hirFieldAccess.objectExpression != nullptr &&
    hirFieldAccess.objectExpression->resolvedType != nullptr &&
    hirFieldAccess.objectExpression->resolvedType->kind ==
        semantic::Type::Kind::Enum ) {
    std::string enumName =
        hirFieldAccess.objectExpression->resolvedType->name;
    std::string constantKey = fmt::format(
        "{}.{}", enumName, hirFieldAccess.fieldName );
    // Look up in moduleConstants table
    ...
    MIRInstruction constantInstruction(
        MIRInstructionKind::ConstantInteger );
    constantInstruction.integerConstantValue = variantValue;
    ...
    return this->emitInstruction( constantInstruction );
}
```

Enum variant access is **fully resolved at compile time**:
1. First checks `moduleConstants` table for a pre-registered constant key `"EnumName.VariantName"`.
2. If not found, looks up the variant in the `EnumType` and extracts the discriminant value. For backed enums with explicit values, reads the backed value from the AST declaration.
3. Emits `ConstantInteger` with the discriminant — no runtime field access at all.

`Direction.North` with discriminant 0 emits `ConstantInteger _enum_Direction_North = 0`.

### Struct and Class Path — ComputeFieldAddress

For non-enum types:

```cpp
MIRVariableIdentifier objectVariable =
    this->lowerExpression( hirFieldAccess.objectExpression );
int resolvedFieldIndex = hirFieldAccess.fieldLayoutIndex;

// If fieldLayoutIndex unresolved, look up in typeLayoutTable
if( resolvedFieldIndex < 0 && objectType != nullptr ) {
    std::string objectTypeName = objectType->name;
    // Strip generic brackets
    ...
    if( this->currentModule->typeLayoutTable.count(
            objectTypeName ) > 0 ) {
        const TypeLayoutDescriptor& layout =
            this->currentModule->typeLayoutTable[objectTypeName];
        for( size_t fieldIndex = 0;
             fieldIndex < layout.fieldNames.size();
             fieldIndex++ ) {
            if( layout.fieldNames[fieldIndex] ==
                    hirFieldAccess.fieldName ) {
                resolvedFieldIndex =
                    static_cast<int>( fieldIndex );
                break;
            }
        }
    }
}

MIRInstruction gepInstruction(
    MIRInstructionKind::ComputeFieldAddress );
gepInstruction.sourceOperands.push_back( objectVariable );
gepInstruction.fieldAccessName = hirFieldAccess.fieldName;
gepInstruction.fieldLayoutIndex = resolvedFieldIndex;
gepInstruction.operandType = hirFieldAccess.resolvedType;
...
return this->emitInstruction( gepInstruction );
```

Key details:
1. **Lower object expression** to get the object variable.
2. **Resolve field index**: If `fieldLayoutIndex` is -1 (unresolved from HIR), look up the field name in the `TypeLayoutDescriptor` from `typeLayoutTable`. Strips generic brackets from type name first (`"ArrayList<String>"` becomes `"ArrayList"`).
3. **Emit `ComputeFieldAddress`**: Carries the field name, resolved layout index, and source operand (object pointer).

### TypeLayoutDescriptor

Defined at `src/uranite/ir/mir.hpp:306-315`:

```cpp
struct TypeLayoutDescriptor {
    std::string typeQualifiedName;
    int typeSizeInBytes = 0;
    int typeAlignmentInBytes = 0;
    std::vector<int> fieldByteOffsets;
    std::vector<std::string> fieldNames;
    std::vector<semantic::TypeSharedPointer> fieldTypes;
    bool hasVirtualTable = false;
    int virtualTableEntryCount = 0;
};
```

Each class/struct has a `TypeLayoutDescriptor` in the module-level `typeLayoutTable`. The `fieldNames` vector maps field indices to field names. The `fieldByteOffsets` vector maps field indices to byte offsets within the struct. `hasVirtualTable` indicates whether the first field is a vtable pointer for virtual dispatch.

---

## MIR Lowering — Field Assignment

At `src/uranite/ir/mir/lowering.cpp:922-1007`, field assignment (`object.field = value`) follows this sequence:

1. **Lower value expression** to get the value variable.
2. **Lower target expression**: For `HIRFieldAccess` targets, `lowerExpression` calls `lowerFieldAccess`, which emits `ComputeFieldAddress` — the result is a pointer to the field.
3. **Compound assignment** (`+=`, `-=`, `*=`, `/=`): If the assignment operator is not plain `=`, first emit `LoadVariable` from the target, then perform the arithmetic operation (`AddInteger`/`SubtractInteger`/etc.), then store the result.
4. **Emit `StoreVariable`**: Store the value into the field address pointer.

`object.field = value` lowers to:
- `ComputeFieldAddress _field_field = object` (get field pointer)
- `StoreVariable _field_field = valueVariable` (write value through pointer)

`object.field += value` lowers to:
- `ComputeFieldAddress _field_field = object`
- `LoadVariable _compound_lhs = _field_field`
- `AddInteger _compound_result = _compound_lhs, valueVariable`
- `StoreVariable _field_field = _compound_result`

---

## LLVM Codegen — generateComputeFieldAddress

At `src/uranite/ir/mir/codegen.cpp:5710-6040`, `generateComputeFieldAddress` handles four distinct scenarios.

### Enum Name and Value

At `codegen.cpp:5722-5799`:

When `fieldAccessName` is `"name"` or `"value"` and the source variable is an enum type:

**`.name` access**: Builds a `CreateSelect` chain over all enum variants. For each variant, compares the discriminant via `CreateICmpEQ`, then selects between the variant name string (as a `CreateGlobalStringPtr`) and the previous result. Produces a cascading select: if discriminant == 0, result = "North"; else if discriminant == 1, result = "South"; etc. Fallback is `"?"` for unknown discriminants.

**`.value` access on backed enums**: Same `CreateSelect` chain but maps discriminants to backed values (integer, float, or string constants from AST declarations). For non-backed enums or when no backed values exist, returns the raw discriminant integer directly.

### Struct GEP Path

At `codegen.cpp:5890-5916`:

When `pointedType` is a resolved struct type:

```cpp
if( pointedType != nullptr && pointedType->isStructTy() ) {
    unsigned fieldIndex =
        static_cast<unsigned>( instruction.fieldLayoutIndex );
    // Validate and correct fieldIndex via typeLayoutTable
    // if out of bounds
    ...
    if( fieldIndex < pointedType->getStructNumElements() ) {
        llvm::Value* fieldPointer =
            this->irBuilder.CreateStructGEP(
                pointedType, basePointer,
                fieldIndex, "gep.field" );
        this->setVariableValue(
            instruction.destinationVariable, fieldPointer );
    }
}
```

`CreateStructGEP` computes the address of field at `fieldIndex` within the LLVM struct. The index is validated against `getStructNumElements()`. If the index is out of bounds (possible with stale layout data), the codegen re-resolves the index by searching `typeLayoutTable` for the matching field name.

For `person.firstName` where `firstName` is field 0:
```llvm
%gep.field = getelementptr inbounds %Person, ptr %person, i32 0, i32 0
```

### Property Getter Fallback

At `codegen.cpp:5919-5959` and `codegen.cpp:5965-6035`:

When the field index exceeds struct element count or no struct type is resolved, codegen attempts to call a getter function:

1. **Struct type known**: Build getter name as `"StructName.memberName"`. Look up in `functionResolutionMap`, fall back to `llvmModule->getFunction()`.
2. **Struct type unknown**: Look up source variable type from `variableDescriptorTable`, strip generic brackets, build getter name. If not found, try `concreteClassMap` for the concrete type. For interface types, scan all struct types in cache for a matching method.
3. **Call getter**: If getter function exists and takes exactly 1 argument (self), emit `CreateCall(getterFunction, {basePointer})`.
4. **Concrete class propagation**: After calling a getter, if `functionReturnConcreteClass` maps the getter name to a concrete class, propagate that to `concreteClassMap` for the destination variable — enabling accurate struct type resolution for subsequent field accesses on the result. Skipped for interface or generic parameter destination types.

### Struct Type Resolution Chain

The codegen uses a multi-step chain to resolve `pointedType` (the LLVM struct type for the base pointer). At `codegen.cpp:5806-5888`:

1. **AllocaInst**: If the base pointer is an `alloca`, check if the allocated type is a struct.
2. **GetElementPtrInst**: If the base pointer is a GEP result, check if the result element type is a struct.
3. **Raw alloca**: Try getting the raw (unloaded) variable value and check its alloca.
4. **Variable descriptor table**: Look up the variable's semantic type, find it in `structTypeCache` (by qualified name, short name, or generic-stripped name).
5. **Owner class**: If inside a method, try the owning class name from `structTypeCache`.
6. **Concrete class map**: If the variable has a concrete class mapping (from constructor or property getter), use that.
7. **Field name scan**: Last resort — scan all struct types in `structTypeCache` and their `TypeLayoutDescriptor` field lists, matching by field name.

This chain ensures field access works even when pointer types are opaque (LLVM opaque pointer mode) and type information must be recovered from semantic metadata.

---

## Examples

### Class Field Access

```
package examples

from uranite.io.console import puts

class Point:
    public F64 positionX
    public F64 positionY

    public function Point(self, F64 positionX, F64 positionY) -> Void:
        self.positionX = positionX
        self.positionY = positionY

public function main() -> I32:
    Point origin = new Point(0.0, 0.0)
    F64 coordinateX = origin.positionX
    puts(coordinateX.toString())
    return 0
```

Output: `0`.

**Compilation trace**:
1. **Parsing**: `origin.positionX` — `Dot` token after identifier. No `(` follows member name. Creates `MemberAccessExpression(object=origin, member="positionX")`.
2. **Semantic analysis**: `origin` is `Point` (class kind). `findField("positionX")` returns `F64`. Result type: `F64`.
3. **HIR lowering**: No property match (`positionX` is a field, not a `@property` method). Creates `HIRFieldAccess(object, "positionX")`.
4. **MIR lowering**: Non-enum. Looks up `Point` in `typeLayoutTable`. Field `"positionX"` is at index 0. Emits `ComputeFieldAddress _field_positionX = origin` with `fieldLayoutIndex=0`.
5. **LLVM codegen**: `CreateStructGEP(%Point, %origin, 0, "gep.field")`.

### Property Access (Getter Desugaring)

```
package examples

from uranite.io.console import puts
from uranite.collection.array-list import ArrayList

public function main() -> I32:
    ArrayList<I64> numbers = new ArrayList<>()
    numbers.add(10)
    numbers.add(20)
    I64 count = numbers.size
    puts(count.toString())
    return 0
```

**Compilation trace**:
1. **Parsing**: `numbers.size` — `Dot` followed by identifier, no `(`. Creates `MemberAccessExpression`.
2. **Semantic analysis**: `numbers` is `ArrayList<I64>`. `findField("size")` returns null. `findMethod("size")` returns a method with `isProperty == true`. Returns method return type `I64`.
3. **HIR lowering**: Property detected (`isProperty && !hasField`). Desugars to `HIRMethodCall(numbers, "size", emptyArgs)` instead of `HIRFieldAccess`.
4. **MIR lowering**: Lowered as a method call (not `lowerFieldAccess`). Emits `CallFunction` targeting `"ArrayList.size"`.

### Enum Variant Access

```
package examples

from uranite.io.console import puts

public enum Direction:
    unit North
    unit South
    unit East
    unit West

public function main() -> I32:
    Direction heading = Direction.North
    puts(heading.name)
    return 0
```

Output: `North`.

**Compilation trace**:
1. **Semantic analysis**: `Direction.North` — enum type, `findVariant("North")` succeeds. Returns `Direction` type. `heading.name` — `qualname::fields::Name` match. Returns `String`.
2. **MIR lowering**: `Direction.North` — enum variant path. Looks up `"Direction.North"` in `moduleConstants` or extracts discriminant 0. Emits `ConstantInteger _enum_Direction_North = 0`.
3. **LLVM codegen**: `heading.name` — enum `.name` path. Builds `CreateSelect` chain: if discriminant == 0, "North"; if == 1, "South"; etc.

### Enum Backed Value

```
package examples

from uranite.io.console import puts

public enum HttpStatus I32:
    unit OK 200
    unit NotFound 404
    unit ServerError 500

public function main() -> I32:
    HttpStatus status = HttpStatus.OK
    I32 code = status.value
    puts(code.toString())
    return 0
```

Output: `200`.

**Compilation trace**:
1. **Semantic analysis**: `status.value` — `qualname::fields::Value` match. Backed type is `I32`. Returns `I32`.
2. **MIR lowering**: Enum variant `HttpStatus.OK` folds to `ConstantInteger 200` (backed value from AST).
3. **LLVM codegen**: `.value` on backed enum — `CreateSelect` chain mapping discriminant to backed values: if == 0, 200; if == 1, 404; if == 2, 500.

### Tuple Element Access

```
package examples

from uranite.io.console import puts

public function main() -> I32:
    (I64, String) pair = (42, "hello")
    I64 number = pair.0
    String text = pair.1
    puts(number.toString())
    puts(text)
    return 0
```

Output: `42`, `hello`.

**Compilation trace**:
1. **Semantic analysis**: `pair.0` — tuple type. `std::stoi("0")` = 0. Index 0 within bounds (tuple has 2 elements). Returns `tupleType->elements[0]` = `I64`. `pair.1` returns `tupleType->elements[1]` = `String`.

### Chained Member Access

```
package examples

from uranite.io.console import puts

class Address:
    public String city

    public function Address(self, String city) -> Void:
        self.city = city

class Person:
    public String fullName
    public Address homeAddress

    public function Person(self, String fullName, Address homeAddress) -> Void:
        self.fullName = fullName
        self.homeAddress = homeAddress

public function main() -> I32:
    Address home = new Address("Jakarta")
    Person user = new Person("Ari", home)
    String userCity = user.homeAddress.city
    puts(userCity)
    return 0
```

Output: `Jakarta`.

**Compilation trace**:
1. **Parsing**: `user.homeAddress.city` — two postfix dots. First creates `MemberAccessExpression(user, "homeAddress")`. Second wraps: `MemberAccessExpression(MemberAccessExpression(user, "homeAddress"), "city")`.
2. **MIR lowering**: Two `ComputeFieldAddress` instructions:
   - `ComputeFieldAddress _field_homeAddress = user` (field index for `homeAddress` in `Person`)
   - `ComputeFieldAddress _field_city = _field_homeAddress` (field index for `city` in `Address`)

### Field Assignment

```
package examples

from uranite.io.console import puts

class Counter:
    public I64 count

    public function Counter(self) -> Void:
        self.count = 0

public function main() -> I32:
    Counter tracker = new Counter()
    tracker.count = 10
    tracker.count += 5
    puts(tracker.count.toString())
    return 0
```

Output: `15`.

**Compilation trace**:
1. **`tracker.count = 10`**: Lower target (ComputeFieldAddress for `count`), lower value (ConstantInteger 10), StoreVariable.
2. **`tracker.count += 5`**: ComputeFieldAddress for `count`, LoadVariable from field pointer, AddInteger with 5, StoreVariable back to field pointer.

### Optional Member Access

```
package examples

from uranite.io.console import puts

class Config:
    public String hostname

    public function Config(self, String hostname) -> Void:
        self.hostname = hostname

public function main() -> I32:
    Config? setting = new Config("localhost")
    String host = setting.hostname
    puts(host)
    return 0
```

**Compilation trace**:
1. **Semantic analysis**: `setting` is `Optional<Config>`. Member access `.hostname` unwraps to inner type `Config`. `findField("hostname")` returns `String`.

### Double-Colon Static Access

```
package examples

from uranite.io.console import puts

public enum Color:
    unit Red
    unit Green
    unit Blue

public function main() -> I32:
    Color selected = Color::Red
    puts(selected.name)
    return 0
```

Output: `Red`.

Both `Color.Red` and `Color::Red` produce equivalent `MemberAccessExpression` nodes. The double-colon operator provides an alternative syntax for static/type-level member access, following the convention used in languages like C++ and Rust.

### Error Case — No Member

```
package examples

public function main() -> I32:
    I64 number = 42
    I64 result = number.nonExistent
    return 0
```

**Diagnostic output**:
```
error: no member "nonExistent" in type "I64"
```

Primitive types have no fields. Member access on primitives that do not match any of the six type paths produces this error.
