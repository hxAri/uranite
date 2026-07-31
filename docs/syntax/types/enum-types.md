# Enum Types

Uranite enums are discriminant-based sum types. Each variant is declared with the `unit` keyword and distinguished at runtime by an integer discriminant — `i32` at the LLVM level. Enums support backing types (mapping variants to explicit integer, float, or string values), per-variant method overrides dispatched via `SwitchBranch`, enum-level methods shared across all variants, generic parameters, interface implementation, and inheritance. Variant access uses `EnumName.VariantName` syntax, which resolves to loading the variant's discriminant constant. Two built-in fields — `.value` and `.name` — provide access to the backing value and variant name string respectively, emitted as cascaded `Select` instructions over discriminant comparisons.

This document covers the complete enum type specification — the `EnumType` and `EnumVariantInfo` structs, the `unit` keyword for variant declarations, `backed` keyword for typed backing values, parsing with variant bodies and method definitions, two-pass semantic registration with auto-incrementing discriminants, HIR lowering to `HIREnumDefinition` with variant descriptors, MIR lowering to module-level constants and `SwitchBranch` method dispatch, codegen with `i32` discriminant representation and `.value`/`.name` field access via `Select` chains, `match`/`switch` pattern matching on enum variants, and practical usage patterns.

---

## Table of Contents

- [Type Identity](#type-identity)
- [The EnumType and EnumVariantInfo Structs](#the-enumtype-and-enumvariantinfo-structs)
  - [EnumVariantInfo](#enumvariantinfo)
  - [EnumType](#enumtype)
  - [Variant Lookup](#variant-lookup)
  - [Method Lookup](#method-lookup)
- [Enum Declaration Syntax](#enum-declaration-syntax)
  - [Basic Enum](#basic-enum)
  - [Backed Enum](#backed-enum)
  - [Variant Methods](#variant-methods)
  - [Enum-Level Methods](#enum-level-methods)
  - [Extends and Implements](#extends-and-implements)
  - [Generic Enums](#generic-enums)
  - [Forward Declarations](#forward-declarations)
- [AST Representation](#ast-representation)
  - [EnumDeclaration Node](#enumdeclaration-node)
  - [EnumVariantNode](#enumvariantnode)
- [Parsing Implementation](#parsing-implementation)
  - [Enum Declaration Parsing](#enum-declaration-parsing)
  - [Variant Parsing](#variant-parsing)
  - [Variant Body Parsing](#variant-body-parsing)
- [Semantic Analysis](#semantic-analysis)
  - [Two-Pass Registration](#two-pass-registration)
  - [Discriminant Assignment](#discriminant-assignment)
  - [Variant Symbol Registration](#variant-symbol-registration)
  - [Method Analysis](#method-analysis)
- [Compilation Pipeline](#compilation-pipeline)
  - [HIR Stage](#hir-stage)
  - [MIR Stage — Constant Registration](#mir-stage--constant-registration)
  - [MIR Stage — Variant Access](#mir-stage--variant-access)
  - [MIR Stage — Method Dispatch](#mir-stage--method-dispatch)
  - [Codegen Stage — LLVM Representation](#codegen-stage--llvm-representation)
  - [Codegen Stage — .value Field Access](#codegen-stage--value-field-access)
  - [Codegen Stage — .name Field Access](#codegen-stage--name-field-access)
- [Pattern Matching](#pattern-matching)
  - [Match Expressions](#match-expressions)
  - [Switch Statements](#switch-statements)
  - [MIR Lowering of Enum Patterns](#mir-lowering-of-enum-patterns)
- [Assignability Rules](#assignability-rules)
- [Examples](#examples)
  - [Basic Enums](#basic-enums)
  - [Backed Enums](#backed-enums)
  - [Variant Methods](#variant-method-examples)
  - [Match and Switch](#match-and-switch)
  - [Practical Usage](#practical-usage)

---

## Type Identity

| Property | Value |
|---|---|
| Type Kind | `Type::Kind::Enum` |
| LLVM Type | `i32` |
| Variant Keyword | `unit` |
| Backing Keyword | `backed` |
| Qualified Name Pattern | `package.module.EnumName` |
| Variant Access | `EnumName.VariantName` |
| Built-in Fields | `.value`, `.name` |

Enums are distinct from classes and structs. At the LLVM level, an enum value is a single `i32` integer — the discriminant. No heap allocation, no pointer, no vtable. Variant identity is entirely determined by comparing this integer against known discriminant values.

---

## The EnumType and EnumVariantInfo Structs

### EnumVariantInfo

Defined in `src/uranite/semantic/typeref.hpp:734-756`:

```
struct EnumVariantInfo
    associatedTypes : std::vector<TypeSharedPointer>
    discriminant    : int (default 0)
    methods         : std::vector<MethodInfo>
    name            : std::string
```

| Field | Type | Description |
|---|---|---|
| `associatedTypes` | `std::vector<TypeSharedPointer>` | Types for tagged union variants (sum type payloads) |
| `discriminant` | `int` | Runtime integer distinguishing this variant |
| `methods` | `std::vector<MethodInfo>` | Per-variant method overrides |
| `name` | `std::string` | Variant identifier |

### EnumType

Defined in `src/uranite/semantic/typeref.hpp:764-827`:

```
struct EnumType : Type
    astDeclaration     : ast::nodes::EnumDeclaration*
    backedType         : TypeSharedPointer
    baseClass          : TypeSharedPointer
    genericParameters  : std::vector<TypeSharedPointer>
    interfaces         : std::vector<TypeSharedPointer>
    methods            : std::vector<MethodInfo>
    typeSubstitutions  : std::unordered_map<std::string, TypeSharedPointer>
    variants           : std::vector<EnumVariantInfo>
```

| Field | Type | Description |
|---|---|---|
| `astDeclaration` | `EnumDeclaration*` | Back-pointer to AST for codegen access |
| `backedType` | `TypeSharedPointer` | Underlying type for backed enums (`Integer`, `I64`, etc.) |
| `baseClass` | `TypeSharedPointer` | Parent type if enum extends another |
| `genericParameters` | `std::vector<TypeSharedPointer>` | Generic type parameters |
| `interfaces` | `std::vector<TypeSharedPointer>` | Implemented interfaces |
| `methods` | `std::vector<MethodInfo>` | Enum-level methods shared by all variants |
| `typeSubstitutions` | Map | Generic parameter concrete type bindings |
| `variants` | `std::vector<EnumVariantInfo>` | All declared variants |

Constructor sets `Kind::Enum`:

```
EnumType(name) → Type(Type::Kind::Enum, name)
```

### Variant Lookup

The `findVariant()` method at `typeref.hpp:816-823` searches variants by name:

```
EnumVariantInfo* findVariant(name):
    for variant in variants:
        if variant.name == name:
            return &variant
    return nullptr
```

### Method Lookup

The `findMethod()` method at `typeref.hpp:802-809` searches enum-level methods by name:

```
MethodInfo* findMethod(name):
    for method in methods:
        if method.name == name:
            return &method
    return nullptr
```

---

## Enum Declaration Syntax

### Basic Enum

Enums without a backing type use auto-incrementing integer discriminants starting from 0:

```uranite
enum Direction:
    unit North
    unit South
    unit East
    unit West
```

Each `unit` declares a variant. Variants have no explicit value — discriminants are assigned automatically (North=0, South=1, East=2, West=3).

### Backed Enum

The `backed` keyword specifies an underlying type that maps variants to explicit values:

```uranite
enum HttpStatus backed I64:
    unit OK 200
    unit NOT_FOUND 404
    unit SERVER_ERROR 500

enum Color backed I64:
    unit RED 0xFF0000
    unit GREEN 0x00FF00
    unit BLUE 0x0000FF
```

Backed value expressions follow the variant name on the same line. Supported value types include integer literals, float literals, string literals, boolean literals, and negated expressions.

When a backed value is provided, accessing `.value` returns the backed value rather than the auto-assigned discriminant. Variants without an explicit backed value use their positional index as the value.

### Variant Methods

Each variant can define its own method overrides inside an indented body after a colon:

```uranite
enum Color backed I64:
    unit RED 0xFF0000:
        override function label( self ) -> String:
            return "Red"
    unit GREEN 0x00FF00:
        override function label( self ) -> String:
            return "Green"
    unit BLUE 0x0000FF:
        override function label( self ) -> String:
            return "Blue"

    public function label( self ) -> String:
        return "Unknown Color"
```

Variant methods override enum-level methods. At MIR lowering, a dispatch function is generated using `SwitchBranch` on the discriminant — each variant's override executes in its own case block, with the enum-level method as the default.

Variant methods **cannot** be `abstract` — the parser emits a diagnostic error if `abstract` is used on variant methods.

### Enum-Level Methods

Methods declared at the enum level (not inside a variant body) are shared by all variants and serve as defaults:

```uranite
enum Season backed I64:
    unit SPRING 1
    unit SUMMER 2
    unit AUTUMN 3
    unit WINTER 4

    public function temperature( self ) -> I64:
        return -1

    public function isWarm( self ) -> Boolean:
        return False
```

Enum methods support `public`, `private`, `protect` access modifiers and `final`, `static`, `virtual`, `override` modifiers. They **cannot** be `abstract`.

### Extends and Implements

Enums can extend a base class and implement interfaces:

```uranite
enum Shape extends GeometricType implements Drawable, Serializable:
    unit Circle
    unit Square
    unit Triangle
```

- `extends` sets `baseClass` on the `EnumType`
- `implements` adds interface types to `interfaces`

### Generic Enums

Enums support generic type parameters:

```uranite
enum Result<T>:
    unit Ok(T)
    unit Error(String)
```

Generic parameters are parsed and stored in `genericParameters`. Associated types (the parenthesized types after variant names) support tagged union / sum type patterns.

### Forward Declarations

Enums can be forward-declared with a semicolon instead of a body:

```uranite
enum Direction;
```

Forward declarations create the `EnumType` in the registry without populating variants.

---

## AST Representation

### EnumDeclaration Node

Defined in `src/uranite/ast/node.hpp:945-977`:

```
struct EnumDeclaration : Declaration
    backedType       : TypeNodeSharedPointer
    baseClassType    : TypeNodeSharedPointer
    genericParameters: std::vector<GenericParameterSharedPointer>
    interfaces       : std::vector<TypeNodeSharedPointer>
    methods          : std::vector<DeclarationSharedPointer>
    name             : std::string
    variants         : std::vector<EnumVariantSharedPointer>
```

### EnumVariantNode

Defined in `src/uranite/ast/node.hpp:911-940`:

```
struct EnumVariantNode : Node
    associatedTypes : std::vector<TypeNodeSharedPointer>
    backedValue     : ExpressionSharedPointer
    discriminant    : ExpressionSharedPointer
    fields          : std::vector<FieldDeclarationSharedPointer>
    methods         : std::vector<DeclarationSharedPointer>
    name            : std::string
```

| Field | Description |
|---|---|
| `associatedTypes` | Tuple-style types for sum type variants: `unit Ok(String)` |
| `backedValue` | Explicit backing value: `unit OK 200` |
| `discriminant` | Explicit discriminant override |
| `fields` | Struct-style fields for complex variants |
| `methods` | Per-variant method overrides |

---

## Parsing Implementation

### Enum Declaration Parsing

The parser at `parser.cpp:992-1176` handles enum declarations:

```
parseEnumDeclaration(access):
    expect(KeywordEnum)
    name = expect(Identifier)
    declaration = EnumDeclaration(name)
    declaration.access = access
    declaration.genericParameters = parseGenericParameters()
    if match(KeywordBacked):
        declaration.backedType = parseTypeNode()
    if match(KeywordExtends):
        declaration.baseClassType = parseTypeNode()
    if match(KeywordImplements):
        do: declaration.interfaces.push_back(parseTypeNode())
        while match(Comma)
    if match(Semicolon):
        return declaration    // forward declaration
    expect(Colon)
    expectNewline()
    // parse indented body...
```

Header parsing order: `enum Name<Generic> backed Type extends Base implements Interface:`.

### Variant Parsing

Inside the enum body, the `unit` keyword triggers variant parsing at `parser.cpp:1084-1163`:

```
if match(KeywordUnit):
    variantName = expect(Identifier)
    variant = EnumVariantNode(variantName)
    if check({False, Float, Integer, String, True, Minus}):
        variant.backedValue = parseExpression()
    if match(LeftParenthesis):
        while not check({Eof, RightParenthesis}):
            variant.associatedTypes.push_back(parseTypeNode())
            if match(Comma): break
        expect(RightParenthesis)
    if match(Colon):
        // parse variant body (methods)
    declaration.variants.push_back(variant)
```

Backed value detection checks for literal tokens — integer, float, string, boolean, or negated expression — immediately after the variant name. Associated types are parsed inside parentheses.

If an identifier appears without the `unit` keyword, the parser emits a helpful error:

```
"enum variants must be declared with \"unit\" keyword"
hint: "use \"unit VariantName\" to declare an enum variant"
```

### Variant Body Parsing

Variant bodies (after colon + indent) can contain methods with access modifiers:

```
// inside variant body:
methodAccess = parseAccessModifier()
// parse modifiers: final, override, static, virtual
// abstract is rejected with error
if check(KeywordFunction or KeywordProperty):
    method = parseFunctionDeclaration(...)
    variant.methods.push_back(method)
```

---

## Semantic Analysis

### Two-Pass Registration

Enum types are registered across two semantic analysis passes:

**Pass 1 — Module Registration** (`analyzer.cpp:459-482`): Creates `EnumType`, registers generic parameters, resolves interfaces, and pre-populates variant names with auto-incrementing discriminants. Variants are registered with `name` and `discriminant` only — no method analysis yet.

**Pass 2 — Full Analysis** (`analyzer.cpp:1996-2072`): Clears and re-populates variants with complete information. Resolves backed types, base class types, interfaces, associated types per variant, variant methods, and enum-level methods.

### Discriminant Assignment

Discriminants are assigned as sequential integers starting from 0:

```
variantDiscriminant = 0
for variant in declaration.variants:
    variantInfo.discriminant = variantDiscriminant++
```

| Declaration Order | Discriminant |
|---|---|
| First variant | 0 |
| Second variant | 1 |
| Third variant | 2 |
| ... | N |

For backed enums, the discriminant is the positional index — the backed value is a separate concept accessed via `.value`. The discriminant determines variant identity; the backed value is application-level data.

### Variant Symbol Registration

Each variant is registered as a scoped symbol at `analyzer.cpp:2047-2049`:

```
variantSymbol = Symbol(variant.name, Kind::EnumVariant, source, enumType)
scopedName = "EnumName::VariantName"
currentScope.define(scopedName, variantSymbol)
```

The variant symbol's type is the parent `EnumType` — accessing `Direction.North` resolves to the `Direction` type.

### Method Analysis

Enum methods cannot be `abstract` — both enum-level and variant-level methods emit diagnostic errors if `abstract` is specified:

```
"enum method \"methodName\" cannot be abstract"
"enum variant method \"methodName\" cannot be abstract"
```

Methods are registered as `MethodInfo` with standard fields (access, isVirtual, isOverride, isStatic, isFinal, isProperty). virtualTableIndex is always -1 — enums use discriminant-based dispatch, not vtable dispatch.

---

## Compilation Pipeline

### HIR Stage

HIR lowering at `hir/lowering.cpp:432-486` creates `HIREnumDefinition`:

```
struct HIREnumDefinition
    enumName                         : std::string
    enumQualifiedName                : std::string
    accessModifier                   : AccessModifier
    backedType                       : TypeSharedPointer
    parentClassQualifiedName         : std::string
    variantDescriptors               : std::vector<HIREnumVariantDescriptor>
    methodDefinitions                : std::vector<HIRFunctionDefinition>
    genericParameters                : std::vector<HIRGenericParameterDescriptor>
    implementedInterfaceQualifiedNames : std::vector<std::string>
```

Each variant is lowered to `HIREnumVariantDescriptor`:

```
struct HIREnumVariantDescriptor
    variantName            : std::string
    backedValueExpression  : HIRNodeSharedPointer
    discriminantExpression : HIRNodeSharedPointer
    associatedTypes        : std::vector<TypeSharedPointer>
    variantFields          : std::vector<HIRFieldDescriptor>
    variantMethods         : std::vector<HIRFunctionDefinition>
```

Backed values and variant methods are lowered to HIR expressions/functions. Enum-level methods are lowered with `ownerClassName` set to the enum's qualified name.

### MIR Stage — Constant Registration

MIR lowering pre-registers every variant as a module-level integer constant at `mir/lowering.cpp:237-256`:

```
for enumDefinition in hirModule.enumDefinitions:
    for variantIndex, variant in enumDefinition.variantDescriptors:
        constantKey = "EnumName.VariantName"
        moduleConstant.kind = Integer
        if variant.backedValueExpression is IntegerLiteral:
            moduleConstant.integerValue = intLit.integerValue
        else:
            moduleConstant.integerValue = variantIndex
        currentModule.moduleConstants[constantKey] = moduleConstant
```

The constant key follows `EnumName.VariantName` format. For backed enums with integer literal values, the backed value is stored. Otherwise, the positional index is used.

### MIR Stage — Variant Access

When code references `EnumName.VariantName`, field access lowering at `mir/lowering.cpp:3763-3810` emits the variant's constant:

```
lowerFieldAccess(hirFieldAccess):
    if objectExpression.resolvedType.kind == Enum:
        enumName = resolvedType.name
        constantKey = "enumName.fieldName"
        if moduleConstants contains constantKey:
            emit ConstantInteger(moduleConstants[constantKey].integerValue)
            return resultVariable
        // fallback: look up variant in EnumType
        variantInfo = enumType.findVariant(fieldName)
        if variantInfo:
            emit ConstantInteger(variantInfo.discriminant or backedValue)
            return resultVariable
```

This means `Direction.North` compiles to a single `ConstantInteger` instruction — no allocation, no lookup, just an immediate integer value.

### MIR Stage — Method Dispatch

Enum methods with per-variant overrides are compiled into dispatch functions using `SwitchBranch` at `mir/lowering.cpp:778-880`:

```
lowerEnumMethodDefinitions(hirEnum):
    // collect variant overrides by method name
    for variant in hirEnum.variantDescriptors:
        for variantMethod in variant.variantMethods:
            variantOverrides[methodName].push_back({discriminant, variantMethod})

    for baseMethod in hirEnum.methodDefinitions:
        if no overrides for this method:
            lowerFunctionDefinition(baseMethod)  // standard lowering
            continue

        // generate dispatch function:
        // self parameter is i32 (enum backing type)
        selfParam = allocateVariable("self", I32)
        loadSelf → selfValue

        SwitchBranch(selfValue):
            case discriminant_0 → caseBlock_0:
                lower overrideMethod_0 body
            case discriminant_1 → caseBlock_1:
                lower overrideMethod_1 body
            ...
            default → defaultBlock:
                lower baseMethod body
```

The `self` parameter for enum methods is the backing integer type (`I32`), not a pointer. Each case block executes the variant's override body. The default block executes the base enum-level method.

### Codegen Stage — LLVM Representation

The `toLLVMType()` function at `codegen.cpp:6650-6651` maps enums to `i32`:

```
case Kind::Enum:
    return Type::getInt32Ty(llvmContext)
```

Enum values are plain 32-bit integers. No wrapping, no pointer, no heap allocation.

### Codegen Stage — .value Field Access

The `.value` field at `codegen.cpp:5733-5777` returns the variant's backing value via cascaded `Select` instructions:

```
if fieldAccessName == "value" and enumType has variants:
    if hasBacked:
        result = null
        for variant in enumType.variants:
            // resolve backed value from AST
            backedVal = ConstantInt/ConstantFP/GlobalStringPtr(variant.backedValue)
            isMatch = CreateICmpEQ(basePointer, discriminant)
            if result is null: result = backedVal
            result = CreateSelect(isMatch, backedVal, result)
        setVariableValue(destination, result)
    else:
        // unbacked: value IS the discriminant
        setVariableValue(destination, basePointer)
```

For backed enums, this generates a chain: compare discriminant, select backed value if match, otherwise try next variant. For unbacked enums, `.value` returns the discriminant integer directly.

**Backed value types supported:**

| AST Node | LLVM Constant |
|---|---|
| `IntegerLiteral` | `ConstantInt::get(i64, value)` |
| `FloatLiteral` | `ConstantFP::get(double, value)` |
| `StringLiteral` | `CreateGlobalStringPtr(value)` |

### Codegen Stage — .name Field Access

The `.name` field at `codegen.cpp:5783-5797` returns the variant name as a string via cascaded `Select` instructions:

```
if enumType has variants:
    result = CreateGlobalStringPtr("?", "enum.unknown")
    for variant in enumType.variants:
        variantName = CreateGlobalStringPtr(variant.name)
        isMatch = CreateICmpEQ(basePointer, discriminant)
        result = CreateSelect(isMatch, variantName, result)
    setVariableValue(destination, result)
```

Each variant name is emitted as a global string. The discriminant is compared against each variant's value, selecting the matching name string. Unknown discriminants produce `"?"`.

---

## Pattern Matching

### Match Expressions

Enum variants work as match arm patterns:

```uranite
I32 value = match direction in \
    Direction.North => 0, \
    Direction.South => 1, \
    Direction.East => 2, \
    Direction.West => 3, \
    * => -1
```

Match arms compare the subject's discriminant against each variant's discriminant value.

### Switch Statements

Switch statements dispatch on enum values:

```uranite
switch direction:
    case Direction.North:
        puts( "North" )
    case Direction.South:
        puts( "South" )
    case Direction.East:
        puts( "East" )
    case Direction.West:
        puts( "West" )
```

### MIR Lowering of Enum Patterns

When match/switch arms contain enum variant patterns, MIR lowering at `mir/lowering.cpp:2013-2055` resolves variant discriminants:

```
if arm.pattern is FieldAccess:
    if objectExpression.resolvedType is EnumType:
        variantInfo = enumType.findVariant(fieldName)
        if variantInfo:
            variantValue = variantInfo.discriminant
            // check for backed value override from AST
            if astVariant has backedValue (IntegerLiteral):
                variantValue = intLit.value
            armPatternValues[armIndex] = variantValue
```

When all arms resolve to integer constants, the match/switch compiles to a `SwitchBranch` instruction — an LLVM `switch` instruction that jumps directly to the matching arm's block. This avoids cascaded if-else comparison chains.

---

## Assignability Rules

Enum assignability uses identity matching — no implicit conversions:

| Assignment | Rule |
|---|---|
| `EnumName` → `EnumName` | Identity match via `equals()` or qualified name comparison |
| `EnumName` → `Interface` | Allowed if enum implements the interface |
| `EnumName` → `Object` | Universal supertype accepts any type |
| `EnumName` → `BaseClass` | Allowed if enum extends the base class |
| `GenericParameter` | Always assignable |

Enum variants are not separate types — `Direction.North` has type `Direction`, not a distinct `Direction.North` type. All variants of an enum share the same `EnumType`.

---

## Examples

### Basic Enums

```uranite
from uranite.io.console import puts

enum Direction:
    unit North
    unit South
    unit East
    unit West

public function main() -> I32:
    Direction north = Direction.North
    Direction south = Direction.South
    puts( north.name )
    puts( south.name )
    puts( north.value )
    return 0
```

Variants without backing values use auto-incrementing discriminants. `.name` returns the variant name string. `.value` returns the discriminant integer.

### Backed Enums

```uranite
from uranite.io.console import puts

enum HttpStatus backed I64:
    unit OK 200
    unit NOT_FOUND 404
    unit SERVER_ERROR 500

public function main() -> I32:
    HttpStatus status = HttpStatus.OK
    puts( "status = ", status.value )
    puts( "name = ", status.name )

    HttpStatus error = HttpStatus.SERVER_ERROR
    puts( "error = ", error.value )
    return 0
```

Backed enums map each variant to an explicit value. `.value` returns the backed value (200, 404, 500), not the positional discriminant.

### Variant Method Examples

```uranite
from uranite.io.console import puts

enum Season backed I64:
    unit SPRING 1:
        override function temperature( self ) -> I64:
            return 20
    unit SUMMER 2:
        override function temperature( self ) -> I64:
            return 35
    unit AUTUMN 3:
        override function temperature( self ) -> I64:
            return 15
    unit WINTER 4:
        override function temperature( self ) -> I64:
            return 0

    public function temperature( self ) -> I64:
        return -1

    public function isWarm( self ) -> Boolean:
        return False

public function main() -> I32:
    Season spring = Season.SPRING
    Season summer = Season.SUMMER
    Season winter = Season.WINTER
    puts( "spring temp = ", spring.temperature() )
    puts( "summer temp = ", summer.temperature() )
    puts( "winter temp = ", winter.temperature() )
    return 0
```

Each variant overrides `temperature()` with its own return value. The compiler generates a `SwitchBranch` dispatch function — the discriminant selects which override to execute. `isWarm()` has no overrides and compiles as a standard function returning `False` for all variants.

### Match and Switch

```uranite
from uranite.io.console import puts

enum Direction:
    unit North
    unit South
    unit East
    unit West

public function directionName( Direction d ) -> Void:
    switch d:
        case Direction.North:
            puts( "North" )
        case Direction.South:
            puts( "South" )
        case Direction.East:
            puts( "East" )
        case Direction.West:
            puts( "West" )

public function directionValue( Direction d ) -> I32:
    return match d in \
        Direction.North => 0, \
        Direction.South => 1, \
        Direction.East => 2, \
        Direction.West => 3, \
        * => -1

public function main() -> I32:
    Direction north = Direction.North
    directionName( north )
    I32 value = directionValue( north )
    puts( "value = ", value )
    return 0
```

Both `switch` and `match` compile to `SwitchBranch` when all arm patterns are enum variants with known discriminants. `*` in match expressions is the default/wildcard arm.

### Practical Usage

```uranite
from uranite.io.console import puts

enum Color backed I64:
    unit RED 0xFF0000:
        override function label( self ) -> String:
            return "Red"
    unit GREEN 0x00FF00:
        override function label( self ) -> String:
            return "Green"
    unit BLUE 0x0000FF:
        override function label( self ) -> String:
            return "Blue"

    public function label( self ) -> String:
        return "Unknown Color"

public function colorHex( Color c ) -> I64:
    return match c in \
        Color.RED => 16711680, \
        Color.GREEN => 65280, \
        Color.BLUE => 255, \
        * => 0

public function printColor( Color c ) -> Void:
    puts( c.label() )
    puts( "name = ", c.name )
    puts( "hex = ", c.value )

public function main() -> I32:
    Color red = Color.RED
    Color green = Color.GREEN
    Color blue = Color.BLUE

    printColor( red )
    printColor( green )
    printColor( blue )

    I64 redHex = colorHex( red )
    puts( "red hex via match = ", redHex )

    return 0
```

This example demonstrates backed enums with hex color values, per-variant method overrides for `label()`, match expression dispatch on variants, and built-in `.name`/`.value` field access — all compiled to `i32` discriminants with `SwitchBranch` dispatch and cascaded `Select` chains for field access, with zero heap allocation overhead.
