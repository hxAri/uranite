# Interface Types

Uranite interfaces are abstract contracts that define method signatures without implementations. Classes implement interfaces via the `implements` keyword, and the compiler validates that every required method is present with a compatible signature. At the LLVM level, interface-typed variables are opaque pointers — dispatch happens through interface tables (itables), which are constant arrays of function pointers generated per class-interface pair. Interfaces support multiple inheritance via `extends`, generic parameters, property methods, and nested type declarations. A fixed-point method propagation algorithm ensures inherited methods from super-interfaces are available to all descendants, and a stable `methodOrder` vector determines itable slot assignment for correct cross-module dispatch.

This document covers the complete interface type specification — the `InterfaceType` struct with `methodOrder` and `superInterfaces`, interface declaration syntax with `extends` and forward declarations, parsing implementation, three-phase semantic registration (method pre-registration, super-interface resolution, method propagation with circular detection, method order stabilization), interface validation against implementing classes, HIR lowering to `HIRInterfaceDefinition`, codegen with itable generation (`_MIR_itable_ClassName_InterfaceName` globals), interface dispatch via vtable slot lookup, abstract class dispatch with vtable identity comparison, the `implementsInterface()` and `extendsInterface()` recursive checks, builtin operator interfaces, and practical usage patterns.

---

## Table of Contents

- [Type Identity](#type-identity)
- [The InterfaceType Struct](#the-interfacetype-struct)
  - [Method Lookup](#method-lookup)
  - [Super-Interface Check](#super-interface-check)
- [Interface Declaration Syntax](#interface-declaration-syntax)
  - [Basic Interface](#basic-interface)
  - [Interface Inheritance](#interface-inheritance)
  - [Generic Interfaces](#generic-interfaces)
  - [Property Methods](#property-methods)
  - [Forward Declarations](#forward-declarations)
  - [Nested Declarations](#nested-declarations)
- [AST Representation](#ast-representation)
- [Parsing Implementation](#parsing-implementation)
  - [Interface Header Parsing](#interface-header-parsing)
  - [Interface Body Parsing](#interface-body-parsing)
- [Semantic Analysis](#semantic-analysis)
  - [Phase 1 — Method Pre-Registration](#phase-1--method-pre-registration)
  - [Phase 2 — Super-Interface Resolution](#phase-2--super-interface-resolution)
  - [Phase 3 — Method Propagation](#phase-3--method-propagation)
  - [Phase 4 — Method Order Stabilization](#phase-4--method-order-stabilization)
  - [Full Interface Analysis](#full-interface-analysis)
  - [Interface Validation](#interface-validation)
- [Implementation Checking](#implementation-checking)
  - [implementsInterface](#implementsinterface)
  - [extendsInterface](#extendsinterface)
- [Compilation Pipeline](#compilation-pipeline)
  - [HIR Stage](#hir-stage)
  - [Codegen Stage — Interface Table Generation](#codegen-stage--interface-table-generation)
  - [Codegen Stage — Interface Dispatch](#codegen-stage--interface-dispatch)
  - [Codegen Stage — Abstract Class Dispatch](#codegen-stage--abstract-class-dispatch)
  - [Codegen Stage — toLLVMType](#codegen-stage--tollvmtype)
- [Builtin Operator Interfaces](#builtin-operator-interfaces)
- [Assignability Rules](#assignability-rules)
- [Examples](#examples)
  - [Basic Interface Implementation](#basic-interface-implementation)
  - [Multiple Interfaces](#multiple-interfaces)
  - [Interface Dispatch](#interface-dispatch)

---

## Type Identity

| Property | Value |
|---|---|
| Type Kind | `Type::Kind::Interface` |
| LLVM Type | `PointerType::getUnqual` (opaque pointer) |
| Dispatch Mechanism | Interface table (itable) with function pointer array |
| Itable Naming | `_MIR_itable_ClassName_InterfaceName` |
| All Methods | Implicitly `isVirtual = true` |
| Method Slot Order | Determined by `methodOrder` vector |

Interfaces have no runtime representation of their own — they are purely a type-system concept. Variables typed as an interface hold opaque pointers to concrete class instances. Dispatch to the correct method implementation happens via the itable stored in the object's vtable slot.

---

## The InterfaceType Struct

Defined in `src/uranite/semantic/typeref.hpp:835-896`:

```
struct InterfaceType : Type
    astDeclaration      : InterfaceDeclaration* (default nullptr)
    genericParameters   : std::vector<TypeSharedPointer>
    methods             : std::vector<MethodInfo>
    methodsPopulating   : bool (default false)
    inheritanceResolved : bool (default false)
    methodOrder         : std::vector<std::string>
    superInterfaces     : std::vector<TypeSharedPointer>
    typeSubstitutions   : std::unordered_map<std::string, TypeSharedPointer>
```

| Field | Description |
|---|---|
| `astDeclaration` | Back-pointer to AST for codegen and monomorphization |
| `genericParameters` | Generic type parameters |
| `methods` | All methods including those inherited from super-interfaces |
| `methodsPopulating` | Guard flag to prevent circular recursion during population |
| `inheritanceResolved` | Flag indicating super-interface methods have been propagated |
| `methodOrder` | Stable ordered list of method names — determines itable slot indices |
| `superInterfaces` | List of interfaces this interface extends |
| `typeSubstitutions` | Generic parameter concrete type bindings |

Constructor sets `Kind::Interface`:

```
InterfaceType(name) → Type(Type::Kind::Interface, name)
```

The `methodOrder` vector is critical for codegen. When a class implements an interface, the itable entries are ordered exactly as `methodOrder` specifies. All classes implementing the same interface share the same slot assignment, enabling correct polymorphic dispatch.

### Method Lookup

`findMethod(name)` at `typeref.hpp:872-879`:

```
MethodInfo* findMethod(name):
    for method in methods:
        if method.name == name: return &method
    return nullptr
```

### Super-Interface Check

`extendsInterface(qualifiedName)` at `typeref.hpp:881-894` recursively checks inheritance:

```
extendsInterface(qualifiedName):
    for superIface in superInterfaces:
        if superIface.qualified == qualifiedName or
           qualname::startsWith(superIface.qualified, qualifiedName):
            return true
        if superIface.kind == Interface:
            if superIface.extendsInterface(qualifiedName):
                return true
    return false
```

This enables transitive interface checks — if `C extends B extends A`, then `C.extendsInterface("A")` returns true.

---

## Interface Declaration Syntax

### Basic Interface

Interfaces declare method signatures without bodies:

```uranite
interface Describable:
    public function describe( self ) -> Void
```

Methods in interfaces are bodyless — the declaration ends with the return type annotation, no colon or indented body. All interface methods are implicitly virtual.

Methods that have bodies (default implementations) are also supported — the parser delegates to the standard `parseFunctionDeclaration`, which handles both bodyless and bodied methods.

### Interface Inheritance

Interfaces extend other interfaces with `extends` (or `implements` — both accepted):

```uranite
interface Serializable extends Stringable:
    public function serialize( self ) -> String

interface ReadWritable extends Readable, Writable:
    public function readWrite( self ) -> Void
```

Super-interface methods are inherited — a class implementing `ReadWritable` must implement methods from `Readable`, `Writable`, and `ReadWritable` itself.

### Generic Interfaces

Interfaces support generic type parameters:

```uranite
interface Iterable<E>:
    public function iterator( self ) -> Iterator<E>

interface Comparable<T>:
    public function compareTo( self, T other ) -> I32
```

### Property Methods

Property methods use the `property` keyword:

```uranite
interface Measurable:
    public property size( self ) -> I64
```

Property methods set `isProperty = true` on `MethodInfo`, allowing field-syntax access at the call site.

### Forward Declarations

Interfaces can be forward-declared:

```uranite
interface Serializable;
```

Forward declarations create the `InterfaceType` in the registry without populating methods.

### Nested Declarations

Interface bodies can contain nested type declarations:

```uranite
interface Container<E>:
    public function size( self ) -> I64

    public class Entry:
        public E value
```

Nested classes, interfaces, structs, and enums are parsed recursively and stored in `nestedDeclarations`.

---

## AST Representation

Defined in `src/uranite/ast/node.hpp:2629-2663`:

```
struct InterfaceDeclaration : Declaration
    genericParameters   : std::vector<GenericParameterSharedPointer>
    isFinal             : bool (default false)
    methods             : std::vector<DeclarationSharedPointer>
    name                : std::string
    nestedDeclarations  : std::vector<DeclarationSharedPointer>
    superInterfaces     : std::vector<TypeNodeSharedPointer>
```

| Field | Description |
|---|---|
| `genericParameters` | Generic type parameters for the interface |
| `isFinal` | Prevents further extension |
| `methods` | Abstract or default method declarations |
| `name` | Interface identifier |
| `nestedDeclarations` | Nested classes, interfaces, structs, enums |
| `superInterfaces` | Parent interfaces via `extends` |

---

## Parsing Implementation

### Interface Header Parsing

The parser at `parser.cpp:1732-1758` handles interface headers:

```
parseInterfaceDeclaration(access):
    expect(KeywordInterface)
    name = expect(Identifier)
    declaration.access = access
    declaration.genericParameters = parseGenericParameters()

    if match(Semicolon):
        return declaration    // forward declaration

    if check(KeywordExtends) or check(KeywordImplements):
        advance()
        declaration.superInterfaces.push_back(parseTypeNode())
        while match(Comma):
            declaration.superInterfaces.push_back(parseTypeNode())

    if match(Colon):
        if not newline or eof:
            // inline super-interface after colon (edge case)
            declaration.superInterfaces.push_back(parseTypeNode())
            while match(Comma):
                declaration.superInterfaces.push_back(parseTypeNode())
            expect(Colon)    // second colon for body
    expectNewline()
```

Both `extends` and `implements` keywords are accepted for super-interfaces. The parser handles an edge case where super-interfaces can appear after the first colon, requiring a second colon for the body.

### Interface Body Parsing

At `parser.cpp:1760-1798`:

```
if match(Indent):
    while not Dedent or Eof:
        methodAccess = parseAccessModifier()

        if KeywordClass:
            nestedDeclarations.push_back(parseClassDeclaration(methodAccess))
        elif KeywordInterface:
            nestedDeclarations.push_back(parseInterfaceDeclaration(methodAccess))
        elif KeywordStruct:
            nestedDeclarations.push_back(parseStructDeclaration(methodAccess))
        elif KeywordEnum:
            nestedDeclarations.push_back(parseEnumDeclaration(methodAccess))
        else:
            isProperty = check(KeywordProperty)
            method = parseFunctionDeclaration(methodAccess, false, false, false, false)
            if isProperty: method.isProperty = true
            declaration.methods.push_back(method)
    match(Dedent)
```

Interface methods are parsed with all modifier flags set to `false` (no virtual, override, abstract, or static) — the semantic analyzer sets `isVirtual = true` on all interface methods.

---

## Semantic Analysis

Interface analysis spans four phases during module registration, plus a full analysis pass.

### Phase 1 — Method Pre-Registration

At `analyzer.cpp:280-327`, interface methods are eagerly registered during the first pass of `analyzeModuleRegistration()`:

```
for declaration in program.declarations where InterfaceDeclaration:
    interfaceType = lookupType(declaration.name)
    if methods already populated: skip

    register generic parameters as GenericParameterType

    for method in declaration.methods where FunctionDeclaration:
        resolve parameter types (skip self)
        resolve return type (default Void)
        create FunctionType

        methodInfo.access = method.access
        methodInfo.isFinal = false
        methodInfo.isOverride = false
        methodInfo.isVirtual = true
        methodInfo.isStatic = method.isStatic
        methodInfo.isProperty = method.isProperty
        methodInfo.virtualTableIndex = -1
        interfaceType.methods.push_back(methodInfo)
```

All interface methods are marked `isVirtual = true` and `isFinal = false` regardless of source declaration.

### Phase 2 — Super-Interface Resolution

At `analyzer.cpp:329-346`, super-interfaces are resolved:

```
for declaration in program.declarations where InterfaceDeclaration:
    interfaceType = lookupType(declaration.name)
    pushScope(Class)
    for superInterfaceNode in declaration.superInterfaces:
        resolvedSuperInterface = resolveType(superInterfaceNode)
        if resolvedSuperInterface.kind == Interface:
            interfaceType.superInterfaces.push_back(resolvedSuperInterface)
    popScope()
```

### Phase 3 — Method Propagation

At `analyzer.cpp:347-385`, a fixed-point iteration propagates methods from super-interfaces to child interfaces:

```
hasChanges = true
iteration = 0
while hasChanges and iteration < 1000:
    hasChanges = false
    for declaration in program.declarations where InterfaceDeclaration:
        interfaceType = lookupType(declaration.name)
        for superInterface in interfaceType.superInterfaces:
            for superMethod in superInterface.methods:
                if no method with superMethod.name exists in interfaceType:
                    interfaceType.methods.push_back(superMethod)
                    hasChanges = true

if iteration >= 1000:
    error: "circular interface inheritance detected: method propagation did not converge"
```

This iterates until no new methods are added (fixed-point convergence). The 1000-iteration cap protects against circular inheritance.

### Phase 4 — Method Order Stabilization

At `analyzer.cpp:387-438`, method order is stabilized for itable slot assignment:

```
methodOrderChanged = true
while methodOrderChanged and limit > 0:
    methodOrderChanged = false
    for declaration in program.declarations where InterfaceDeclaration:
        interfaceType = lookupType(declaration.name)

        orderedNames = []
        addedNames = set()

        // super-interface methods first (in their order)
        for superInterface in interfaceType.superInterfaces:
            for methodName in superInterface.methodOrder:
                if methodName not in addedNames:
                    orderedNames.push_back(methodName)
                    addedNames.insert(methodName)

        // own methods after super-interface methods
        for method in interfaceType.methods:
            if method.name not in addedNames:
                orderedNames.push_back(method.name)
                addedNames.insert(method.name)

        if orderedNames != interfaceType.methodOrder:
            // reorder methods to match orderedNames
            reorderedMethods = []
            for name in orderedNames:
                find matching method, push to reorderedMethods
            interfaceType.methods = reorderedMethods
            interfaceType.methodOrder = orderedNames
            // assign interfaceTableIndex per method
            for index, method in interfaceType.methods:
                method.interfaceTableIndex = index
            methodOrderChanged = true
```

Method ordering ensures:
1. Super-interface methods appear first, in the order defined by the super-interface
2. Own methods appear after inherited ones
3. `interfaceTableIndex` matches position in `methodOrder`
4. All classes implementing the same interface agree on slot assignments

### Full Interface Analysis

At `analyzer.cpp:3186-3249`, the second-pass analysis handles:

```
analyzeInterfaceDeclaration(declaration):
    interfaceType = lookupType(declaration.name)
    if not already pre-registered:
        register generic parameters
        resolve super-interfaces and copy their methods
    else:
        re-register existing generic parameters

    pushScope(Class)
    currentScope.classType = interfaceType

    for method in declaration.methods:
        analyzeDeclaration(method)
        if not pre-registered:
            resolve parameter types, return type
            create MethodInfo (isVirtual = true)
            interfaceType.methods.push_back(methodInfo)

    for nestedDeclaration in declaration.nestedDeclarations:
        analyzeDeclaration(nestedDeclaration)
    popScope()
```

### Interface Validation

`validateInterfaceImplementation()` at `analyzer.cpp:5289-5351` checks class compliance:

```
validateInterfaceImplementation(classType, interfaceType, source):
    for method in interfaceType.methods:
        found = false
        nameExists = false
        for implMethod in classType.methods:
            if implMethod.name != method.name:
                continue
            nameExists = true
            if parameter counts match and return type assignable:
                found = true
                break

        if not found:
            if not nameExists:
                error: "class does not implement method required by interface"
            else:
                error: "method has wrong signature for interface"
```

Validation checks:
1. Method name exists in the class
2. Parameter count matches
3. Return type is assignable (with generic base-name fallback for generic return types)

Missing methods emit: `"class \"Circle\" does not implement method \"describe\" required by interface \"Describable\""`

Wrong signatures emit: `"method \"describe\" in class \"Circle\" has wrong signature for interface \"Describable\""`

---

## Implementation Checking

### implementsInterface

`ClassType::implementsInterface()` at `typeref.cpp:676-692` checks if a class directly or transitively implements an interface:

```
implementsInterface(qualifiedName):
    for iface in interfaces:
        if iface.qualified == qualifiedName or
           qualname::startsWith(iface.qualified, qualifiedName):
            return true
        if iface.kind == Interface:
            if iface.extendsInterface(qualifiedName):
                return true
    if baseClass exists and baseClass.kind == Class:
        return baseClass.implementsInterface(qualifiedName)
    return false
```

Three-level check:
1. Direct interface match
2. Transitive super-interface match via `extendsInterface()`
3. Inherited interface match via base class chain

### extendsInterface

`InterfaceType::extendsInterface()` at `typeref.hpp:881-894` recursively walks the super-interface chain:

```
extendsInterface(qualifiedName):
    for superIface in superInterfaces:
        if superIface.qualified == qualifiedName: return true
        if superIface is Interface:
            if superIface.extendsInterface(qualifiedName): return true
    return false
```

---

## Compilation Pipeline

### HIR Stage

HIR lowering at `hir/lowering.cpp:488-514` creates `HIRInterfaceDefinition`:

```
struct HIRInterfaceDefinition : HIRNode
    interfaceName                    : std::string
    interfaceQualifiedName           : std::string
    accessModifier                   : AccessModifier
    methodDefinitions                : std::vector<HIRFunctionDefinition>
    genericParameters                : std::vector<HIRGenericParameterDescriptor>
    superInterfaceQualifiedNames     : std::vector<std::string>
    nestedDeclarations               : std::vector<HIRNodeSharedPointer>
    isFinalInterface                 : bool
```

Interface methods are lowered as standard functions with `ownerClassName` set to the interface qualified name. These function definitions serve as default implementations — classes without their own override call the interface's default.

### Codegen Stage — Interface Table Generation

At `codegen.cpp:380-468`, itables are generated per class that implements interfaces:

```
for (className, classType) in userTypes where kind == Class:
    if classType.interfaces empty: skip

    for interface in classType.interfaces:
        methodNames = interface.methodOrder (or interface.methods order)
        if methodNames empty: skip

        funcPtrType = PointerType::getUnqual(VoidFuncType)
        itableEntries = []

        for methodName in methodNames:
            implFunc = lookup "ClassName.methodName" in functionResolutionMap
            // try: fullQualified.methodName, then qualifiedBase.methodName, then shortName.methodName
            if found:
                itableEntries.push_back(BitCast(implFunc, funcPtrType))
            else:
                itableEntries.push_back(ConstantPointerNull)

        itableArrayType = ArrayType(funcPtrType, entryCount)
        itableGlobal = GlobalVariable(
            itableArrayType, true, InternalLinkage,
            ConstantArray(itableEntries),
            "_MIR_itable_ClassName_InterfaceName"
        )
        interfaceTableMap[classQualified] = itableGlobal
        interfaceTableMap[className] = itableGlobal
```

Each itable is a constant array of function pointers. Methods not found in the class get null entries. The itable is stored as an LLVM `GlobalVariable` with internal linkage.

When a class is instantiated via `ConstructObject`, the itable pointer is stored in the object's first struct field (slot 0) — see `codegen.cpp:6284-6296`.

### Codegen Stage — Interface Dispatch

At `codegen.cpp:4330-4390`, interface method calls dispatch through the itable:

```
// load itable pointer from object's first field
vtableSlotPtr = CreateStructGEP(wrapperStruct, receiverPtr, 0, "vtable.slot.ptr")
vtablePtr = CreateLoad(ptrType, vtableSlotPtr, "vtable.ptr")

// index into itable by method slot
funcSlotPtr = CreateGEP(funcPtrType, vtablePtr, methodSlotIndex, "vfunc.slot.ptr")
funcPtr = CreateLoad(funcPtrType, funcSlotPtr, "vfunc.ptr")

// cast to expected function type and call
castedFunc = CreateBitCast(funcPtr, ptr-to-callType, "vfunc.cast")
callResult = CreateCall(callType, castedFunc, [receiverPtr, ...args])
```

The `methodSlotIndex` comes from the interface's `methodOrder` — the slot where the implementing class's function pointer was placed during itable construction. This ensures uniform dispatch regardless of the concrete class.

### Codegen Stage — Abstract Class Dispatch

At `codegen.cpp:4600-4757`, abstract class method calls use vtable identity comparison:

```
// collect concrete subclasses of abstract class
concreteSubclasses = abstractClassSubclasses[abstractClassName]

for each concrete subclass:
    // find concrete method implementation
    concreteMethod = functionResolutionMap["SubclassName.methodName"]
    // get subclass vtable identifier
    vtableId = classVtableIdentifier[subclassName]

// at call site:
vtablePtr = load receiver's vtable pointer (field 0)

for each (concreteMethod, vtableId) in dispatchTargets:
    castedId = BitCast(vtableId, ptrType)
    isMatch = CreateICmpEQ(vtablePtr, castedId, "type.match")
    if isMatch: branch to call block
    else: branch to next check

// call block: call concreteMethod with args
// fallback block: call abstract base method
// merge block: PHI node to unify return values
```

This generates a cascade of vtable identity comparisons — each concrete subclass's itable pointer is compared against the receiver's itable pointer. When a match is found, the concrete method is called directly (devirtualized). The fallback calls the base abstract method.

The `abstractClassSubclasses` map is populated at `codegen.cpp:523-568` by walking all non-abstract classes and checking if any ancestor is abstract.

### Codegen Stage — toLLVMType

At `codegen.cpp:6648-6649`:

```
case Kind::Interface:
    return PointerType::getUnqual(context)
```

Interface-typed variables are always opaque pointers — the concrete struct type is unknown at the call site.

---

## Builtin Operator Interfaces

Uranite defines operator interfaces in `src/uranite/semantic/qualnames.hpp` that enable operator overloading on classes:

| Interface | Qualified Name | Methods | Operators |
|---|---|---|---|
| `Addable` | `uranite.operators.addable.Addable` | `add` | `+` |
| `Subtractable` | `uranite.operators.subtractable.Subtractable` | `subtract`, `sub` | `-` |
| `Multipliable` | `uranite.operators.multipliable.Multipliable` | `multiply`, `mul` | `*` |
| `Dividable` | `uranite.operators.dividable.Dividable` | `divide`, `div` | `/` |
| `Modulable` | `uranite.operators.modulable.Modulable` | `modulo`, `mod`, `remainder` | `%` |
| `Equatable` | `uranite.operators.equatable.Equatable` | `equals`, `notEquals` | `==`, `!=` |
| `Comparable` | `uranite.operators.comparable.Comparable` | `lessThan`, `greaterThan`, `lessOrEqual`, `greaterOrEqual` | `<`, `>`, `<=`, `>=` |
| `Negatable` | `uranite.operators.negatable.Negatable` | `negate`, `neg` | unary `-` |
| `Stringable` | `uranite.operators.stringable.Stringable` | `toString` | string conversion |
| `Hashable` | `uranite.operators.hashable.Hashable` | `hashCode` | hash computation |
| `Indexable` | `uranite.operators.indexable.Indexable` | `get` | `[]`, `in` |
| `Iterable` | `uranite.iterators.iterable.Iterable` | `iterator` | `for-in` loops |
| `Iterator` | `uranite.iterators.iterator.Iterator` | `has`, `next` | iterator protocol |
| `Droper` | `uranite.memory.droper.Droper` | `drop` | automatic cleanup |

The semantic analyzer checks for these interfaces when resolving binary operators on class types. For example, `a + b` where `a` is a class type checks `a.implementsInterface(qname::Addable)` — if true, the operator desugars to `a.add(b)`.

---

## Assignability Rules

| Assignment | Rule |
|---|---|
| `InterfaceName` → `InterfaceName` | Identity match |
| `ClassName` → `InterfaceName` | Allowed if class implements interface |
| `InterfaceName` → `Object` | Always allowed (universal supertype) |
| `ChildInterface` → `ParentInterface` | Allowed via `extendsInterface()` check |
| `InterfaceName` → `ClassName` | Not allowed (downcast requires explicit cast) |
| `GenericParameter` | Always assignable |

---

## Examples

### Basic Interface Implementation

```uranite
from uranite.io.console import puts

interface Describable:
    public function describe( self ) -> Void

class Circle implements Describable:

    public F64 radius

    public function Circle( self, F64 radius ) -> Void:
        self.radius = radius

    public function describe( self ) -> Void:
        puts( "Circle with radius ", self.radius )

public function main() -> I32:
    Circle c = new Circle( 5.0 )
    c.describe()
    return 0
```

`Circle` implements `Describable` by providing a `describe()` method. The compiler validates the signature matches at semantic analysis time.

### Multiple Interfaces

```uranite
from uranite.io.console import puts

interface Describable:
    public function describe( self ) -> Void

interface Measurable:
    public function measure( self ) -> I64

class Square implements Describable, Measurable:

    public I64 side

    public function Square( self, I64 side ) -> Void:
        self.side = side

    public function describe( self ) -> Void:
        puts( "Square with side ", self.side )

    public function measure( self ) -> I64:
        return self.side * self.side

class Rectangle extends Shape implements Describable:

    public F64 width
    public F64 height

    public function Rectangle( self, F64 w, F64 h ) -> Void:
        self.kind = "Rectangle"
        self.width = w
        self.height = h

    public function describe( self ) -> Void:
        puts( "Rectangle ", self.width, " x ", self.height )

public function main() -> I32:
    Square sq = new Square( 4 )
    sq.describe()
    puts( "Square area = ", sq.measure() )
    return 0
```

`Square` implements both `Describable` and `Measurable`. The compiler generates an itable for each interface — `_MIR_itable_Square_Describable` with `describe` slot, and `_MIR_itable_Square_Measurable` with `measure` slot. `Rectangle` extends `Shape` and implements `Describable`, combining inheritance with interface compliance.

### Interface Dispatch

```uranite
from uranite.io.console import puts

public interface Greeter:
    public function greet( self ) -> Void;

public class EnglishGreeter implements Greeter:

    public Int id

    public function EnglishGreeter( self, Int id ) -> Void:
        self.id = id

    public function greet( self ) -> Void:
        puts( "Hello, World!" )

public class SpanishGreeter implements Greeter:

    public Int id

    public function SpanishGreeter( self, Int id ) -> Void:
        self.id = id

    public function greet( self ) -> Void:
        puts( "Hola, Mundo!" )

public function callGreet( Greeter g ) -> Void:
    g.greet()

public function main() -> I32:
    Greeter eng = new EnglishGreeter( 1 )
    callGreet( eng )
    Greeter spa = new SpanishGreeter( 2 )
    callGreet( spa )
    return 0
```

`callGreet()` accepts a `Greeter`-typed parameter — an opaque pointer at LLVM level. When `g.greet()` is called, the compiler emits itable dispatch: load the vtable pointer from the receiver's first field, index into it at the `greet` slot (index 0 in `Greeter.methodOrder`), load the function pointer, and call it. `EnglishGreeter` dispatches to `EnglishGreeter.greet`, `SpanishGreeter` dispatches to `SpanishGreeter.greet` — same call site, different implementations, resolved at runtime through the itable pointer stored at construction time.
