# Self and Super Expressions

Uranite uses `self` and `super` (keyword `parent`) to reference the current instance and parent class within method bodies. `self` is a first-class expression that resolves to the enclosing class type by walking the scope chain during semantic analysis. `super` is a call-only construct — it has no standalone semantic analysis case and exists solely to delegate constructor calls to the parent class. Both produce minimal AST nodes with no fields beyond source location. At MIR level, `self` becomes a `LoadVariable` instruction referencing the implicit `self` parameter; `super()` becomes a `CallFunction`/`InvokeFunction` targeting the parent class constructor with `self` prepended as the first argument.

---

## Table of Contents

- [Syntax](#syntax)
- [Self Expression](#self-expression)
  - [AST Representation — SelfExpression](#ast-representation--selfexpression)
  - [Self as Function Parameter](#self-as-function-parameter)
  - [Self as Type — parseTypeNode](#self-as-type--parsetypenode)
  - [Parsing](#parsing)
  - [Semantic Analysis](#semantic-analysis)
  - [HIR Representation — HIRSelfReference](#hir-representation--hirselfreference)
  - [HIR Lowering](#hir-lowering)
  - [HIR Type Inference Fallback](#hir-type-inference-fallback)
  - [MIR Lowering — LoadVariable](#mir-lowering--loadvariable)
  - [Self Parameter Registration](#self-parameter-registration)
  - [Self in Method Calls](#self-in-method-calls)
  - [Self in Field Access](#self-in-field-access)
  - [LLVM Code Generation](#llvm-code-generation)
- [Super Expression](#super-expression)
  - [AST Representation — SuperExpression](#ast-representation--superexpression)
  - [Parsing](#parsing-1)
  - [Semantic Analysis — None](#semantic-analysis--none)
  - [HIR Representation — HIRSuperReference](#hir-representation--hirsuperreference)
  - [HIR Lowering](#hir-lowering-1)
  - [MIR Lowering — Constructor Delegation](#mir-lowering--constructor-delegation)
  - [Parent Class Name Tracking](#parent-class-name-tracking)
- [HIR Validation](#hir-validation)
- [Comparison: self vs super](#comparison-self-vs-super)
- [Examples](#examples)

---

## Syntax

```
self
self.fieldName
self.methodName(args)
super(args)
```

| Expression | Description | Valid Context |
|---|---|---|
| `self` | Reference to current instance | Inside instance methods |
| `self.field` | Access instance field | Inside instance methods |
| `self.method()` | Call instance method | Inside instance methods |
| `super(args)` | Delegate to parent constructor | Inside constructors of child classes |

```
package examples

public class Animal:
    String name

    public function Animal(self, String name) -> Void:
        self.name = name

public class Dog extends Animal:
    String breed

    public function Dog(self, String name, String breed) -> Void:
        super(name)
        self.breed = breed
```

---

## Self Expression

### AST Representation — SelfExpression

Defined at `src/uranite/ast/node.hpp:1730-1739`:

```cpp
struct SelfExpression : Expression {

    SelfExpression(
        const lookup::SourceSharedPointer& source
    ) : Expression( Node::Kind::SelfExpression, source ) {
    }

};
```

Minimal node — no fields beyond inherited `source` location. Type information is resolved during semantic analysis and stored in the inherited `semanticType` field.

### Self as Function Parameter

`self` has dual identity in Uranite: it is both an expression (referencing the current instance) and a function parameter (declaring that a method receives an instance pointer).

Function parameters are parsed at `src/uranite/parser/parser.cpp:1440-1456`:

```cpp
ast::nodes::FunctionParameterSharedPointer
Parser::parseFunctionParameter() {
    lookup::SourceSharedPointer source = this->current().source;
    if( this->check( token::Type::KeywordSelf ) ) {
        this->advance();
        auto parameter = std::make_shared<
            ast::nodes::FunctionParameterNode>(
                semantic::qualname::identifier::Self,
                nullptr, source );
        parameter->isSelf = true;
        return parameter;
    }
    if( this->check( token::Type::Ampersand ) &&
        this->peek().type == token::Type::KeywordSelf ) {
        this->advance();
        this->advance();
        auto parameter = std::make_shared<
            ast::nodes::FunctionParameterNode>(
                semantic::qualname::identifier::Self,
                nullptr, source );
        parameter->isSelf = true;
        parameter->isReference = true;
        return parameter;
    }
    // ... normal parameter parsing
}
```

Two forms are supported:

| Form | isSelf | isReference | Description |
|---|---|---|---|
| `self` | `true` | `false` | Value self parameter (moved ownership) |
| `&self` | `true` | `true` | Reference self parameter (borrowed) |

Key details:
- Name is set to `semantic::qualname::identifier::Self` which is `"self"` (lowercase).
- Type is `nullptr` — the self parameter has no explicit type annotation. The type is inferred from the enclosing class.
- The `isSelf` flag on `FunctionParameterNode` (node.hpp:750) marks this parameter for special handling throughout the pipeline.

### Self as Type — parseTypeNode

`Self` (uppercase) is a valid type name in `parseTypeNode` at `src/uranite/parser/parser.cpp:531-533`:

```cpp
else if( this->check( token::Type::KeywordSelf ) ) {
    name = semantic::qualname::identifier::SelfType;
    this->advance();
}
```

`SelfType` is `"Self"` (uppercase). This allows `Self` to appear in return types and field types within class definitions, referring to the enclosing class type. Distinct from `self` (lowercase) which references the instance value.

### Parsing

At `src/uranite/parser/parser.cpp:2275-2277`, in the primary expression dispatch:

```cpp
case token::Type::KeywordSelf: {
    this->advance();
    return std::make_shared<ast::nodes::SelfExpression>(
        source );
}
```

`KeywordSelf` matches the `self` keyword. Produces a `SelfExpression` node with just the source location.

`self` is also listed in the expression-start token set at parser.cpp:178 for lookahead purposes.

### Semantic Analysis

At `src/uranite/semantic/analyzer.cpp:2399-2412`:

```cpp
case ast::Node::Kind::SelfExpression: {
    ScopeSharedPointer currentSearchingScope =
        this->currentScope;
    while( currentSearchingScope ) {
        if( currentSearchingScope->classType ) {
            expressionType =
                currentSearchingScope->classType;
            break;
        }
        currentSearchingScope =
            currentSearchingScope->parent();
    }
    if( expressionType == nullptr ) {
        this->diagnostic.error( expression->source,
            "\"self\" used outside of class context" );
        expressionType = this->typeRegistry.getError();
    }
    break;
}
```

Resolution algorithm:
1. Start at current scope.
2. Walk parent scopes upward.
3. At each scope, check if `classType` is set (meaning we are inside a class definition).
4. If found, `self` resolves to that class type.
5. If no class scope is found (reached top-level), emit error: `"self" used outside of class context`. Expression type falls back to `Error`.

This means `self` is valid in:
- Instance methods (including constructors)
- Nested functions within methods (captures the enclosing class type)
- Lambda expressions within methods

And invalid in:
- Top-level functions
- Module-level code
- Static class methods (if scope has no `classType`)

### HIR Representation — HIRSelfReference

Defined at `src/uranite/ir/hir.hpp:600-609`:

```cpp
struct HIRSelfReference : HIRNode {

    HIRSelfReference(
        semantic::TypeSharedPointer resolvedType,
        const lookup::SourceSharedPointer& sourceLocation
    ) : HIRNode( HIRNodeKind::SelfReference,
            std::move( resolvedType ), sourceLocation ) {
    }

};
```

No additional fields — type information carried in inherited `resolvedType`.

### HIR Lowering

At `src/uranite/ir/hir/lowering.cpp:855-856`:

```cpp
case ast::Node::Kind::SelfExpression: {
    return std::make_shared<HIRSelfReference>(
        expression->semanticType, expression->source );
}
```

Passes the semantic type directly to the HIR node.

### HIR Type Inference Fallback

At `src/uranite/ir/hir/lowering.cpp:64-78`, `inferExpressionType` has a special case when semantic type is missing:

```cpp
if( expression->kind == ast::Node::Kind::SelfExpression ) {
    if( this->currentOwnerClassName.empty() ) {
        return nullptr;
    }
    std::string baseName = this->currentOwnerClassName;
    size_t lastDot = baseName.rfind( '.' );
    if( lastDot != std::string::npos ) {
        baseName = baseName.substr( lastDot + 1 );
    }
    semantic::TypeSharedPointer ownerType =
        this->semanticAnalyzer.types().lookupType(
            baseName );
    if( ownerType == nullptr ) {
        ownerType =
            this->semanticAnalyzer.types().lookupType(
                this->currentOwnerClassName );
    }
    return ownerType;
}
```

If semantic analysis did not set a type on the expression, HIR lowering attempts to infer it from the `currentOwnerClassName` — the fully-qualified name of the class being lowered. Strips the module prefix to get the short name, looks up in type registry, falls back to qualified name lookup.

This fallback ensures `self` always has type information available even when semantic analysis leaves it null (edge cases with forward references or incomplete analysis).

### MIR Lowering — LoadVariable

At `src/uranite/ir/mir/lowering.cpp:2374-2393`:

```cpp
case hir::HIRNodeKind::SelfReference: {
    MIRInstruction loadInstruction(
        MIRInstructionKind::LoadVariable );
    loadInstruction.calledFunctionQualifiedName =
        semantic::qualname::identifier::Self;
    semantic::TypeSharedPointer selfResolvedType =
        hirExpression->resolvedType;
    std::unordered_map<std::string,
        MIRVariableIdentifier>::iterator selfLookup =
            this->variableNameMap.find(
                semantic::qualname::identifier::Self );
    if( selfLookup != this->variableNameMap.end() ) {
        loadInstruction.sourceOperands.push_back(
            selfLookup->second );
        if( selfResolvedType == nullptr ) {
            MIRVariableIdentifier selfSourceId =
                selfLookup->second;
            if( this->currentFunction
                    ->variableDescriptorTable.count(
                        selfSourceId ) > 0 ) {
                selfResolvedType =
                    this->currentFunction
                        ->variableDescriptorTable[
                            selfSourceId].variableType;
            }
        }
    }
    loadInstruction.operandType = selfResolvedType;
    loadInstruction.sourceLocation =
        hirExpression->sourceLocation;
    MIRVariableIdentifier resultVariable =
        this->currentFunction->allocateVariable(
            semantic::qualname::identifier::Self,
            selfResolvedType, false );
    loadInstruction.destinationVariable = resultVariable;
    return this->emitInstruction( loadInstruction );
}
```

MIR lowering for `self`:
1. Create a `LoadVariable` instruction with `calledFunctionQualifiedName = "self"`.
2. Look up `"self"` in the `variableNameMap` to find the MIR variable identifier for the self parameter.
3. If found, add it as source operand. If resolved type is still null, recover type from the variable descriptor table.
4. Allocate a new result variable named `"self"` and emit the load.

Result is a `LoadVariable` instruction that copies the self parameter into a fresh variable.

### Self Parameter Registration

When MIR lowering processes a regular function definition (lowering.cpp:379-388), all parameters including `self` are registered:

```cpp
for( size_t paramIndex = 0;
     paramIndex < hirFunction.parameterDescriptors.size();
     paramIndex++ ) {
    const hir::HIRParameterDescriptor& parameterDescriptor =
        hirFunction.parameterDescriptors[paramIndex];
    MIRVariableIdentifier parameterVariable =
        mirFunction->allocateVariable(
            parameterDescriptor.parameterName,
            parameterDescriptor.parameterType,
            parameterDescriptor.isMutableParameter );
    mirFunction->variableDescriptorTable[parameterVariable]
        .isParameterVariable = true;
    mirFunction->parameterVariableIdentifiers.push_back(
        parameterVariable );
    this->variableNameMap[
        parameterDescriptor.parameterName] = parameterVariable;
    // ... variadic/keyword handling
}
```

The self parameter is registered like any other parameter — it gets a MIR variable identifier, is marked as a parameter variable, and is added to `variableNameMap` under the key `"self"`.

For nested functions (lowering.cpp:679), self parameters are explicitly skipped:

```cpp
if( paramDescriptor.isSelfParameter ) {
    continue;
}
```

Nested functions do not receive a self parameter — they capture outer variables through the closure mechanism instead.

### Self in Method Calls

When lowering a method call where the receiver is `self`, at `src/uranite/ir/mir/lowering.cpp:3645-3646`:

```cpp
if( hirMethodCall.receiverObject->nodeKind ==
        hir::HIRNodeKind::SelfReference ) {
    ownerClassName = this->currentClassName;
}
```

The MIR lowering recognizes that a `SelfReference` receiver means "call a method on this class" and resolves the owner class name from `this->currentClassName` — the fully-qualified name of the class currently being lowered.

This enables method resolution: `self.doSomething()` resolves to `CurrentClass.doSomething` at the MIR level, which codegen then looks up in the function resolution map.

### Self in Field Access

When lowering a field access expression, `self.fieldName` flows through `lowerFieldAccess` at `src/uranite/ir/mir/lowering.cpp:3816-3849`:

1. The object expression (`self`) is lowered via `lowerExpression`, which hits the `SelfReference` case and emits a `LoadVariable` for the self parameter.
2. The field index is resolved from the `typeLayoutTable` using the object type name.
3. A `ComputeFieldAddress` instruction is emitted with the self variable as the source operand, the field name, and the resolved field index.

At codegen level, `ComputeFieldAddress` translates to an LLVM `GEP` (GetElementPtr) instruction that computes the memory address of the field within the object struct, followed by a load.

### LLVM Code Generation

`LoadVariable` at `src/uranite/ir/mir/codegen.cpp:1551-1639` handles the self load:

1. If `sourceOperands` is empty and `calledFunctionQualifiedName` starts with `@`, loads a global variable.
2. If `sourceOperands` is empty and name matches a function, converts to function pointer.
3. Otherwise, loads the value from the source variable — for `self`, this retrieves the LLVM value representing the instance pointer from the variable value table.

The self parameter arrives as an LLVM function argument (pointer type for class instances). The `LoadVariable` instruction either directly propagates the value or performs an `alloca`-based load if the source was stored to stack.

Concrete class propagation (codegen.cpp:1619-1634) also applies: if the source self variable has a concrete class mapping, the destination variable inherits it — preserving type information through self loads for later virtual dispatch resolution.

---

## Super Expression

### AST Representation — SuperExpression

Defined at `src/uranite/ast/node.hpp:1804-1813`:

```cpp
struct SuperExpression : Expression {

    SuperExpression(
        const lookup::SourceSharedPointer& source
    ) : Expression( Node::Kind::SuperExpression, source ) {
    }

};
```

Like `SelfExpression`, a minimal node with only source location.

### Parsing

At `src/uranite/parser/parser.cpp:2271-2273`:

```cpp
case token::Type::KeywordParent: {
    this->advance();
    return std::make_shared<ast::nodes::SuperExpression>(
        source );
}
```

The keyword is `parent` in the token type enum (`KeywordParent`), but the language keyword is `super`. The lexer maps the `super` keyword text to the `KeywordParent` token type.

`KeywordParent` is also listed in the expression-start token set at parser.cpp:177.

### Semantic Analysis — None

`SuperExpression` has **no case** in the semantic analyzer's `analyzeExpression` switch. It falls through to the default case without type resolution.

This is by design — `super` is only meaningful as the callee of a call expression (`super(args)`). When a call expression has a `SuperExpression` as its callee, the call analysis path handles the dispatch. Standalone `super` without a call (e.g., assigning `super` to a variable) produces no type and no error at semantic analysis time — it will fail at MIR lowering when no parent class context is found.

### HIR Representation — HIRSuperReference

Defined at `src/uranite/ir/hir.hpp:611-620`:

```cpp
struct HIRSuperReference : HIRNode {

    HIRSuperReference(
        semantic::TypeSharedPointer resolvedType,
        const lookup::SourceSharedPointer& sourceLocation
    ) : HIRNode( HIRNodeKind::SuperReference,
            std::move( resolvedType ), sourceLocation ) {
    }

};
```

Carries `resolvedType` from semantic analysis. Since `super` has no semantic case, `resolvedType` will typically be `nullptr`.

### HIR Lowering

At `src/uranite/ir/hir/lowering.cpp:858-859`:

```cpp
case ast::Node::Kind::SuperExpression: {
    return std::make_shared<HIRSuperReference>(
        expression->semanticType, expression->source );
}
```

Passes through whatever semantic type was set (typically `nullptr`).

### MIR Lowering — Constructor Delegation

`super` is handled exclusively in the function call lowering path at `src/uranite/ir/mir/lowering.cpp:3519-3532`:

```cpp
else if( hirCall.calleeExpression->nodeKind ==
             hir::HIRNodeKind::SuperReference ) {
    if( this->currentParentClassName.empty() == false ) {
        std::string parentShortName =
            this->currentParentClassName;
        size_t lastDotPos =
            parentShortName.rfind( '.' );
        if( lastDotPos != std::string::npos ) {
            parentShortName = parentShortName.substr(
                lastDotPos + 1 );
        }
        callInstruction.calledFunctionQualifiedName =
            this->currentParentClassName + "."
                + parentShortName;
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

Step-by-step:
1. Check if the callee is a `SuperReference`.
2. Verify `currentParentClassName` is non-empty (we are inside a class with a parent).
3. Extract the short name from the parent class qualified name. Example: `"mypackage.Animal"` becomes `"Animal"`.
4. Build the constructor function name: `parentQualified + "." + parentShortName`. Example: `"mypackage.Animal.Animal"`.
5. Look up `"self"` in the variable name map and **prepend** it to the argument list — the parent constructor receives the current instance as its first argument.

This means `super(name)` in a `Dog` constructor generates a call to `mypackage.Animal.Animal(self, name)` — the parent constructor initializes its fields on the same object pointer.

If `currentParentClassName` is empty (no parent class), the call instruction has no qualified name and will produce no codegen output — effectively a no-op.

### Parent Class Name Tracking

The MIR lowering tracks `currentParentClassName` through class definition lowering at `src/uranite/ir/mir/lowering.cpp:730-742`:

```cpp
void MIRLowering::lowerClassDefinition(
        hir::HIRClassDefinition& hirClass ) {
    std::string savedClassName = this->currentClassName;
    std::string savedParentClassName =
        this->currentParentClassName;
    this->currentClassName =
        hirClass.classQualifiedName.empty() == false
            ? hirClass.classQualifiedName
            : hirClass.className;
    this->currentParentClassName =
        hirClass.parentClassQualifiedName;
    for( auto& methodDefinition : hirClass.methodDefinitions ) {
        if( methodDefinition != nullptr ) {
            this->lowerFunctionDefinition( *methodDefinition );
        }
    }
    this->currentClassName = savedClassName;
    this->currentParentClassName = savedParentClassName;
}
```

Both `currentClassName` and `currentParentClassName` are saved and restored around each class definition, enabling correct resolution even with nested classes. `parentClassQualifiedName` comes from the HIR class definition, which captures the `extends` declaration from the AST.

For structs and enums, `currentParentClassName` is set to empty string — `super()` inside struct/enum methods produces a no-op call.

---

## HIR Validation

At `src/uranite/ir/hir/validator.cpp:524-525`:

```cpp
case HIRNodeKind::SelfReference:
case HIRNodeKind::SuperReference:
```

Both fall through to a shared `break` with other leaf nodes. No validation is performed — they are accepted as valid HIR nodes without additional checks.

---

## Comparison: self vs super

| Feature | `self` | `super` |
|---|---|---|
| AST node | `SelfExpression` | `SuperExpression` |
| Parser keyword | `KeywordSelf` | `KeywordParent` |
| Semantic analysis | Scope-chain walk to find `classType` | No case — unresolved |
| Result type | Enclosing class type | `nullptr` (no resolution) |
| Error on misuse | `"self" used outside of class context"` | Silent — no error at semantic time |
| HIR node | `HIRSelfReference` with resolved type | `HIRSuperReference` with null type |
| MIR lowering | `LoadVariable` for `"self"` parameter | Only in call context — builds parent constructor call |
| Standalone use | Valid — loads self pointer | Invalid — produces no useful MIR |
| Self parameter prepend | Not applicable | Prepends `self` as first argument |
| Class tracking | `currentClassName` | `currentParentClassName` |
| Valid context | Inside any instance method | Inside constructors of child classes |

---

## Examples

### Basic Self Usage — Field Access and Method Call

```
package examples

from uranite.io.console import puts

public class Counter:
    I64 count

    public function Counter(self, I64 initial) -> Void:
        self.count = initial

    public function increment(self) -> Void:
        self.count = self.count + 1

    public function display(self) -> Void:
        puts("Counter active")

    public function incrementAndDisplay(self) -> Void:
        self.increment()
        self.display()

public function main() -> I32:
    Counter counter = new Counter(0)
    counter.incrementAndDisplay()
    return 0
```

**Compilation trace for `self.count = self.count + 1`**:
1. **Parsing**: `self` parsed as `SelfExpression`. `.count` parsed as member access. `self.count + 1` parsed as binary expression.
2. **Semantic analysis**: `self` resolves to `Counter` type via scope-chain walk. `count` field resolved on `Counter` as `I64`.
3. **HIR lowering**: `HIRSelfReference(type=Counter)` for both `self` occurrences. Field accesses become `HIRFieldAccess` nodes.
4. **MIR lowering**: Two `LoadVariable` instructions for `self`. Two `ComputeFieldAddress` instructions for `.count`. One `AddInteger` for `+ 1`. One `StoreField` for assignment.
5. **LLVM codegen**: `self` loaded as pointer. GEP to `count` field offset. Load i64, add 1, store back.

### Super Constructor Delegation

```
package examples

from uranite.io.console import puts

public class Shape:
    String name

    public function Shape(self, String name) -> Void:
        self.name = name

public class Circle extends Shape:
    F64 radius

    public function Circle(self, String name, F64 radius) -> Void:
        super(name)
        self.radius = radius

public function main() -> I32:
    Circle circle = new Circle("circle", 5.0)
    return 0
```

**Compilation trace for `super(name)`**:
1. **Parsing**: `super` (token `KeywordParent`) parsed as `SuperExpression`. `(name)` parsed as call arguments. Combined into `CallExpression(callee=SuperExpression, args=[name])`.
2. **Semantic analysis**: Call expression analyzed — arguments analyzed. `SuperExpression` callee has no type case — no type resolution for callee itself. Arguments typed normally.
3. **HIR lowering**: `HIRFunctionCall(callee=HIRSuperReference(null), args=[HIRIdentifier("name")])`.
4. **MIR lowering**: Callee is `SuperReference`. `currentParentClassName = "examples.Shape"`. Short name = `"Shape"`. Function qualified name = `"examples.Shape.Shape"`. Self variable looked up from `variableNameMap["self"]` and prepended to arguments. Emits `CallFunction(calledFunction="examples.Shape.Shape", args=[self, name])`.
5. **LLVM codegen**: Resolves `examples.Shape.Shape` function. Passes current `self` pointer and `name` string. Parent constructor writes to `self.name` field using the same object pointer.

### Reference Self Parameter

```
package examples

public class Buffer:
    I64 size

    public function Buffer(self, I64 size) -> Void:
        self.size = size

    public function resize(&self, I64 newSize) -> Void:
        self.size = newSize
```

**Compilation trace for `&self`**:
1. **Parsing**: `&` token followed by `self` keyword. Parser creates `FunctionParameterNode(name="self", type=null)` with `isSelf = true` and `isReference = true`.
2. The `isReference` flag indicates borrowed access — the method does not take ownership of the instance. At MIR level, the parameter is still registered as a pointer — the borrow checker uses the reference flag to enforce borrow rules.

### Multi-level Inheritance

```
package examples

from uranite.io.console import puts

public class A:
    I64 valueA

    public function A(self, I64 valueA) -> Void:
        self.valueA = valueA

public class B extends A:
    I64 valueB

    public function B(self, I64 valueA, I64 valueB) -> Void:
        super(valueA)
        self.valueB = valueB

public class C extends B:
    I64 valueC

    public function C(self, I64 valueA, I64 valueB, I64 valueC) -> Void:
        super(valueA, valueB)
        self.valueC = valueC

public function main() -> I32:
    C obj = new C(1, 2, 3)
    return 0
```

**Compilation trace**:
- `C.C` constructor: `super(valueA, valueB)` generates call to `B.B(self, valueA, valueB)` — `currentParentClassName` is the qualified name for `B`.
- `B.B` constructor: `super(valueA)` generates call to `A.A(self, valueA)` — `currentParentClassName` is the qualified name for `A`.
- `A.A` constructor: no `super()` call — `A` has no parent class. Sets `self.valueA` directly.
- All three constructors operate on the same `self` pointer — the object allocated once in `new C(1, 2, 3)`.

### Self in Nested Method Context

```
package examples

from uranite.io.console import puts

public class Processor:
    I64 state

    public function Processor(self, I64 state) -> Void:
        self.state = state

    public function process(self) -> I64:
        I64 current = self.state
        self.state = current + 1
        return current
```

**Compilation trace for `self.state` inside `process`**:
1. **MIR lowering**: `SelfReference` triggers `LoadVariable` lookup in `variableNameMap["self"]`. Finds the MIR variable ID for the self parameter registered during function definition lowering.
2. **Field access**: `ComputeFieldAddress` instruction with self variable as source, `"state"` as field name. Field index resolved from `typeLayoutTable["Processor"]`.
3. **LLVM codegen**: Self is function argument 0 (pointer to Processor struct). GEP computes offset to `state` field. Load i64 from that address.
