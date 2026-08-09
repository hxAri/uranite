# Instanceof and Subclassof Expressions

Uranite provides two type-checking operators: `instanceof` for runtime instance type checking and `subclassof` for compile-time inheritance relationship verification. Both are binary operators at precedence level 7 (same as comparison operators) and produce `Bool` results. The key architectural distinction: `instanceof` is lowered through the full compilation pipeline — HIR, MIR (`InstanceOfCheck` instruction), and LLVM codegen — producing a compile-time-evaluated boolean constant based on static type information and class hierarchy traversal. `subclassof` is compile-time only — it has an HIR node (`HIRSubclassOf`) but no MIR lowering and no codegen. It operates purely on types (not values) and resolves entirely during semantic analysis. Neither operator generates runtime instructions — `instanceof` evaluates statically at codegen time by walking the class hierarchy in the semantic type system, and `subclassof` resolves at semantic analysis time.

---

## Table of Contents

- [Syntax](#syntax)
- [Operator Precedence](#operator-precedence)
- [Instanceof Expression](#instanceof-expression)
  - [AST Representation — InstanceofExpression](#ast-representation--instanceofexpression)
  - [Parsing](#parsing)
  - [Semantic Analysis](#semantic-analysis)
  - [HIR Representation — HIRInstanceOf](#hir-representation--hirinstanceof)
  - [HIR Lowering](#hir-lowering)
  - [MIR Lowering](#mir-lowering)
  - [LLVM Code Generation](#llvm-code-generation)
    - [Exact Type Match](#exact-type-match)
    - [Class Hierarchy Walk](#class-hierarchy-walk)
    - [Object Fallback](#object-fallback)
    - [Unknown Type Fallback](#unknown-type-fallback)
- [Subclassof Expression](#subclassof-expression)
  - [AST Representation — SubclassofExpression](#ast-representation--subclassofexpression)
  - [Parsing](#parsing-1)
  - [Semantic Analysis](#semantic-analysis-1)
  - [HIR Representation — HIRSubclassOf](#hir-representation--hirsubclassof)
  - [HIR Lowering](#hir-lowering-1)
  - [No MIR Lowering — Compile-Time Only](#no-mir-lowering--compile-time-only)
- [Comparison: instanceof vs subclassof](#comparison-instanceof-vs-subclassof)
- [Examples](#examples)

---

## Syntax

```
expression instanceof Type
Type subclassof Type
```

| Operator | Left Operand | Right Operand | Description |
|---|---|---|---|
| `instanceof` | Value expression | Type | Tests whether value is an instance of type |
| `subclassof` | Type name | Type | Tests whether left type inherits from right type |

```
package examples

from uranite.io.console import puts

public class Animal:
    pass

public class Dog extends Animal:
    pass

public function main() -> I32:
    Dog dog = new Dog()
    if dog instanceof Animal:
        puts("Dog is an Animal")
    if Dog subclassof Animal:
        puts("Dog extends Animal")
    return 0
```

---

## Operator Precedence

Both `instanceof` and `subclassof` share precedence level 7 with comparison operators:

| Precedence | Operators |
|---|---|
| 12 | `as` (cast) |
| 11 | `**` (power) |
| 10 | `*`, `/`, `%` |
| 9 | `+`, `-` |
| 8 | `<<`, `>>` |
| **7** | **`<`, `<=`, `>`, `>=`, `in`, `instanceof`, `is`, `subclassof`** |
| 6 | `==`, `!=` |
| 5 | `&` (bitwise AND) |
| 4 | `^` (bitwise XOR) |
| 3 | `\|` (bitwise OR) |
| 2 | `and` |
| 1 | `or` |

From `src/uranite/parser/parser.cpp:120-127`:

```cpp
case token::Type::GreaterThan:
case token::Type::GreaterThanEqual:
case token::Type::KeywordIn:
case token::Type::KeywordInstanceOf:
case token::Type::KeywordIs:
case token::Type::KeywordSubclassOf:
case token::Type::LessThan:
case token::Type::LessThanEqual:
    return 7;
```

This means type checks bind at the same level as comparisons:
- `x + 1 instanceof I64` parses as `(x + 1) instanceof I64`.
- `x instanceof Dog and y instanceof Cat` parses as `(x instanceof Dog) and (y instanceof Cat)`.

---

## Instanceof Expression

### AST Representation — InstanceofExpression

Defined at `src/uranite/ast/node.hpp:1479-1502`:

```cpp
struct InstanceofExpression : Expression {

    ExpressionSharedPointer object;
    TypeNodeSharedPointer targetType;

    InstanceofExpression(
        ExpressionSharedPointer object,
        TypeNodeSharedPointer targetType,
        const lookup::SourceSharedPointer& source
    ) : Expression(
            Node::Kind::InstanceofExpression, source ),
        object( std::move( object ) ),
        targetType( std::move( targetType ) ) {
    }

};
```

| Field | Type | Description |
|---|---|---|
| `object` | `ExpressionSharedPointer` | Value expression being type-checked |
| `targetType` | `TypeNodeSharedPointer` | Target type to check against |

### Parsing

At `src/uranite/parser/parser.cpp:2161-2166`, inside the precedence-climbing loop:

```cpp
if( kind == token::Type::KeywordInstanceOf ) {
    lookup::SourceSharedPointer source =
        this->current().source;
    this->advance();
    ast::nodes::TypeNodeSharedPointer type =
        this->parseTypeNode();
    left = std::make_shared<ast::nodes::InstanceofExpression>(
        left, type, source );
    continue;
}
```

Like `as`, the right operand is a **type** parsed via `parseTypeNode()`, not an expression. The left operand is the accumulated expression from the precedence-climbing loop.

### Semantic Analysis

At `src/uranite/semantic/analyzer.cpp:2321-2326`:

```cpp
case ast::Node::Kind::InstanceofExpression: {
    ast::nodes::InstanceofExpression& instanceOfExpression =
        static_cast<ast::nodes::InstanceofExpression&>(
            *expression );
    this->analyzeExpression( instanceOfExpression.object );
    this->resolveType( instanceOfExpression.targetType );
    expressionType = this->typeRegistry.getBool();
    break;
}
```

Semantic analysis:
1. Analyze the object expression — ensures it is well-typed.
2. Resolve the target type — ensures it exists in the type registry.
3. Set expression type to `Bool` — instanceof always produces a boolean result.

No validation is performed on whether the type check is sensible (e.g., checking a `String` against `I64` is allowed — it will simply evaluate to `false` at codegen time).

### HIR Representation — HIRInstanceOf

Defined at `src/uranite/ir/hir.hpp:948-962`:

```cpp
struct HIRInstanceOf : HIRNode {

    HIRNodeSharedPointer checkedExpression;
    semantic::TypeSharedPointer checkedType;

    HIRInstanceOf(
        HIRNodeSharedPointer checkedExpression,
        semantic::TypeSharedPointer checkedType,
        const lookup::SourceSharedPointer& sourceLocation
    ) : HIRNode( HIRNodeKind::InstanceOf,
            nullptr, sourceLocation ),
        checkedExpression( std::move( checkedExpression ) ),
        checkedType( std::move( checkedType ) ) {
    }

};
```

| Field | Type | Description |
|---|---|---|
| `checkedExpression` | `HIRNodeSharedPointer` | Lowered object expression |
| `checkedType` | `TypeSharedPointer` | Resolved target type for the check |

Note: `resolvedType` is passed as `nullptr` — the boolean result type is reconstructed during MIR lowering.

### HIR Lowering

At `src/uranite/ir/hir/lowering.cpp:1101-1105`:

```cpp
case ast::Node::Kind::InstanceofExpression: {
    ast::nodes::InstanceofExpression& instanceofExpression =
        static_cast<ast::nodes::InstanceofExpression&>(
            *expression );
    HIRNodeSharedPointer checkedExpression =
        this->lowerExpression( instanceofExpression.object );
    semantic::TypeSharedPointer checkedType =
        this->resolveTypeNode(
            instanceofExpression.targetType );
    return std::make_shared<HIRInstanceOf>(
        std::move( checkedExpression ),
        checkedType, expression->source );
}
```

Straightforward: lower the object expression, resolve the target type from the AST type node to a semantic type, create `HIRInstanceOf`.

### MIR Lowering

At `src/uranite/ir/mir/lowering.cpp:2479-2489`:

```cpp
case hir::HIRNodeKind::InstanceOf: {
    hir::HIRInstanceOf& instanceNode =
        static_cast<hir::HIRInstanceOf&>(
            *hirExpression );
    MIRVariableIdentifier checkedVariable =
        this->lowerExpression(
            instanceNode.checkedExpression );
    MIRInstruction checkInstruction(
        MIRInstructionKind::InstanceOfCheck );
    checkInstruction.sourceOperands.push_back(
        checkedVariable );
    checkInstruction.operandType =
        instanceNode.checkedType;
    checkInstruction.sourceLocation =
        instanceNode.sourceLocation;
    semantic::TypeSharedPointer boolType =
        std::make_shared<semantic::Type>(
            semantic::Type::Kind::Bool,
            semantic::qualname::classes::boolean::Name );
    MIRVariableIdentifier resultVariable =
        this->currentFunction->allocateVariable(
            "_instanceof", boolType, false );
    checkInstruction.destinationVariable = resultVariable;
    return this->emitInstruction( checkInstruction );
}
```

MIR lowering produces a single `InstanceOfCheck` instruction:

| MIR Field | Value | Description |
|---|---|---|
| `instructionKind` | `InstanceOfCheck` | Runtime type check instruction |
| `sourceOperands[0]` | Checked variable | Value being type-checked |
| `operandType` | Target type | Type to check against |
| `destinationVariable` | `_instanceof` | Result variable typed as `Bool` |

### LLVM Code Generation

At `src/uranite/ir/mir/codegen.cpp:1331-1389`, `InstanceOfCheck` is evaluated entirely at compile time using static type information from the MIR variable descriptor table:

```cpp
case MIRInstructionKind::InstanceOfCheck: {
    if( instruction.destinationVariable != 0 ) {
        bool isMatch = false;
        if( instruction.operandType != nullptr &&
            instruction.sourceOperands.empty() == false ) {
            std::string targetTypeName =
                instruction.operandType->name;
            size_t bracketPos = targetTypeName.find( '<' );
            if( bracketPos != std::string::npos ) {
                targetTypeName = targetTypeName.substr(
                    0, bracketPos );
            }
            MIRVariableIdentifier sourceVar =
                instruction.sourceOperands[0];
            if( this->currentMIRFunction
                    ->variableDescriptorTable.count(
                        sourceVar ) > 0 ) {
                MIRVariableDescriptor& descriptor =
                    this->currentMIRFunction
                        ->variableDescriptorTable[sourceVar];
                // ... type checking logic
            }
            else {
                isMatch = true;
            }
        }
        this->setVariableValue(
            instruction.destinationVariable,
            isMatch
                ? llvm::ConstantInt::getTrue(
                      this->llvmContext )
                : llvm::ConstantInt::getFalse(
                      this->llvmContext ) );
    }
    break;
}
```

The result is always a compile-time constant — `ConstantInt::getTrue` or `ConstantInt::getFalse`. No runtime RTTI mechanism exists. The check uses four strategies in order:

#### Exact Type Match

```cpp
std::string sourceTypeName =
    descriptor.variableType->name;
bracketPos = sourceTypeName.find( '<' );
if( bracketPos != std::string::npos ) {
    sourceTypeName = sourceTypeName.substr(
        0, bracketPos );
}
if( sourceTypeName == targetTypeName ) {
    isMatch = true;
}
```

Both source and target type names have generic brackets stripped (`ArrayList<I64>` becomes `ArrayList`). If the base names match exactly, the check succeeds. This handles the trivial case: `dog instanceof Dog` where the static type exactly matches the target.

#### Class Hierarchy Walk

```cpp
else if( descriptor.variableType->kind ==
             semantic::Type::Kind::Class ) {
    semantic::ClassType* classPtr =
        static_cast<semantic::ClassType*>(
            descriptor.variableType.get() );
    semantic::TypeSharedPointer current =
        classPtr->baseClass;
    while( current != nullptr && isMatch == false ) {
        std::string baseName = current->name;
        bracketPos = baseName.find( '<' );
        if( bracketPos != std::string::npos ) {
            baseName = baseName.substr(
                0, bracketPos );
        }
        if( baseName == targetTypeName ) {
            isMatch = true;
        }
        if( current->kind ==
                semantic::Type::Kind::Class ) {
            current =
                static_cast<semantic::ClassType*>(
                    current.get() )->baseClass;
        }
        else {
            break;
        }
    }
}
```

When the exact type does not match and the source is a class type, the codegen walks the `baseClass` chain upward through the inheritance hierarchy. At each level, the base class name (with generic brackets stripped) is compared against the target type name. The walk continues until either a match is found or the root of the hierarchy is reached (`baseClass == nullptr`).

This means `dog instanceof Animal` succeeds when `Dog extends Animal`, because the hierarchy walk finds `Animal` as a base class of `Dog`.

#### Object Fallback

```cpp
if( isMatch == false &&
    ( sourceTypeName ==
          semantic::qualname::classes::object::Name ||
      sourceTypeName == targetTypeName ) ) {
    isMatch = true;
}
```

If the hierarchy walk did not find a match, a final check tests whether the source type is `Object` (the universal base class). Since every class implicitly extends `Object`, `objectRef instanceof AnyClass` always succeeds when the source type is `Object`. The duplicate `sourceTypeName == targetTypeName` check is a safety net that should have been caught by the earlier exact match.

#### Unknown Type Fallback

```cpp
else {
    isMatch = true;
}
```

Two fallback paths default to `true`:
1. When the source variable has no type in the descriptor table (`variableDescriptorTable.count(sourceVar) == 0`): the type is unknown, so the check optimistically returns `true`.
2. When the source variable has a descriptor but `variableType` is null: same — insufficient information defaults to match.

This ensures that `instanceof` never blocks compilation due to missing type information — it degrades gracefully to `true`.

---

## Subclassof Expression

### AST Representation — SubclassofExpression

Defined at `src/uranite/ast/node.hpp:1776-1799`:

```cpp
struct SubclassofExpression : Expression {

    TypeNodeSharedPointer sourceType;
    TypeNodeSharedPointer targetType;

    SubclassofExpression(
        TypeNodeSharedPointer sourceType,
        TypeNodeSharedPointer target,
        const lookup::SourceSharedPointer& source
    ) : Expression(
            Node::Kind::SubclassofExpression, source ),
        sourceType( std::move( sourceType ) ),
        targetType( std::move( target ) ) {
    }

};
```

| Field | Type | Description |
|---|---|---|
| `sourceType` | `TypeNodeSharedPointer` | Child type (left operand) |
| `targetType` | `TypeNodeSharedPointer` | Parent type (right operand) |

Unlike `instanceof` which takes a value expression on the left, `subclassof` takes a **type** on both sides. Both operands are type nodes, not value expressions.

### Parsing

At `src/uranite/parser/parser.cpp:2168-2180`:

```cpp
if( kind == token::Type::KeywordSubclassOf ) {
    lookup::SourceSharedPointer source =
        this->current().source;
    this->advance();
    ast::nodes::TypeNodeSharedPointer type =
        this->parseTypeNode();
    left = std::make_shared<ast::nodes::SubclassofExpression>(
        std::make_shared<ast::nodes::SimpleTypeNode>(
            static_cast<ast::nodes::IdentifierExpression&>(
                *left ).name,
            left->source
        ),
        type,
        source
    );
    continue;
}
```

Key parsing detail: the left operand was already parsed as an expression (since it entered the precedence-climbing loop as an expression). The parser **reinterprets** it: it casts the left expression to `IdentifierExpression`, extracts the `.name`, and wraps it in a new `SimpleTypeNode`. This means the left operand of `subclassof` must be a bare identifier — complex expressions are not allowed.

`Dog subclassof Animal`:
1. `Dog` is initially parsed as `IdentifierExpression("Dog")`.
2. When `subclassof` is encountered, the parser extracts `"Dog"` and creates `SimpleTypeNode("Dog")`.
3. `Animal` is parsed as a type via `parseTypeNode()`.
4. Creates `SubclassofExpression(sourceType=SimpleTypeNode("Dog"), targetType=SimpleTypeNode("Animal"))`.

### Semantic Analysis

At `src/uranite/semantic/analyzer.cpp:2419-2424`:

```cpp
case ast::Node::Kind::SubclassofExpression: {
    ast::nodes::SubclassofExpression& subclassOfExpression =
        static_cast<ast::nodes::SubclassofExpression&>(
            *expression );
    this->resolveType( subclassOfExpression.sourceType );
    this->resolveType( subclassOfExpression.targetType );
    expressionType = this->typeRegistry.getBool();
    break;
}
```

Semantic analysis:
1. Resolve both the source (child) and target (parent) types.
2. Set expression type to `Bool`.

No actual subclass checking is performed at semantic analysis time — both types are simply resolved to ensure they exist. The actual inheritance check could theoretically be performed here (the type registry has the full class hierarchy), but the current implementation defers it.

### HIR Representation — HIRSubclassOf

Defined at `src/uranite/ir/hir.hpp:965-979`:

```cpp
struct HIRSubclassOf : HIRNode {

    semantic::TypeSharedPointer childType;
    semantic::TypeSharedPointer parentType;

    HIRSubclassOf(
        semantic::TypeSharedPointer childType,
        semantic::TypeSharedPointer parentType,
        const lookup::SourceSharedPointer& sourceLocation
    ) : HIRNode( HIRNodeKind::SubclassOf,
            nullptr, sourceLocation ),
        childType( std::move( childType ) ),
        parentType( std::move( parentType ) ) {
    }

};
```

| Field | Type | Description |
|---|---|---|
| `childType` | `TypeSharedPointer` | Resolved child type (left operand) |
| `parentType` | `TypeSharedPointer` | Resolved parent type (right operand) |

Both fields carry resolved semantic types, not expressions. The `resolvedType` is `nullptr` (boolean result type is not set at HIR level).

### HIR Lowering

At `src/uranite/ir/hir/lowering.cpp:1107-1111`:

```cpp
case ast::Node::Kind::SubclassofExpression: {
    ast::nodes::SubclassofExpression& subclassofExpression =
        static_cast<ast::nodes::SubclassofExpression&>(
            *expression );
    semantic::TypeSharedPointer childType =
        this->resolveTypeNode(
            subclassofExpression.sourceType );
    semantic::TypeSharedPointer parentType =
        this->resolveTypeNode(
            subclassofExpression.targetType );
    return std::make_shared<HIRSubclassOf>(
        childType, parentType, expression->source );
}
```

Both type nodes are resolved to semantic types, and an `HIRSubclassOf` node is created.

### No MIR Lowering — Compile-Time Only

`subclassof` has **no case** in the MIR lowering `lowerExpression` switch. When the MIR lowering encounters an `HIRSubclassOf` node, it falls through to the `default:` case, which returns `INVALID_VARIABLE_IDENTIFIER`.

`subclassof` also has **no case** in LLVM codegen — there is no `MIRInstructionKind` for it.

This means `subclassof` expressions are effectively dead code in the current implementation pipeline. The expression type is set to `Bool` during semantic analysis, but no actual value is produced at runtime. If the result of a `subclassof` expression is used in a conditional (`if Dog subclassof Animal:`), the condition variable will be uninitialized, leading to undefined behavior.

This is a known limitation — `subclassof` is designed as a compile-time type-level operation for use in meta-programming contexts (compile-time assertions, conditional compilation), but the runtime evaluation path is not yet implemented. Future implementations could evaluate the class hierarchy at codegen time (similar to `instanceof`) and emit a constant boolean.

---

## Comparison: instanceof vs subclassof

| Feature | `instanceof` | `subclassof` |
|---|---|---|
| Left operand | Value expression | Type name (identifier) |
| Right operand | Type | Type |
| Result type | `Bool` | `Bool` |
| Precedence | 7 | 7 |
| HIR node | `HIRInstanceOf` | `HIRSubclassOf` |
| MIR instruction | `InstanceOfCheck` | None |
| LLVM codegen | Compile-time constant via hierarchy walk | None (not lowered) |
| Runtime instructions | None (constant folded) | None |
| Checks | Value's type against target type | Child type against parent type |
| Operates on | Instance + Type | Type + Type |

Both operators resolve at compile time. The difference: `instanceof` checks whether a specific variable's static type is or inherits from the target type. `subclassof` checks the abstract type relationship without requiring an instance.

---

## Examples

### Basic Instanceof

```
package examples

from uranite.io.console import puts

public class Shape:
    pass

public class Circle extends Shape:
    F64 radius

    public function Circle(self, F64 radius) -> Void:
        self.radius = radius

public function main() -> I32:
    Circle circle = new Circle(5.0)
    if circle instanceof Shape:
        puts("Circle is a Shape")
    if circle instanceof Circle:
        puts("Circle is a Circle")
    return 0
```

Output: both messages printed.

**Compilation trace for `circle instanceof Shape`**:
1. **Parsing**: `circle` parsed as `IdentifierExpression`. `instanceof` at precedence 7. `Shape` parsed via `parseTypeNode()`. Creates `InstanceofExpression(object=circle, targetType=Shape)`.
2. **Semantic analysis**: `circle` analyzed as `Circle` type. `Shape` resolved. Expression type = `Bool`.
3. **HIR lowering**: `HIRInstanceOf(checkedExpression=HIRIdentifier("circle"), checkedType=Shape)`.
4. **MIR lowering**: `InstanceOfCheck` with `sourceOperands[0] = circle_var`, `operandType = Shape`.
5. **LLVM codegen**: `targetTypeName = "Shape"`. Source variable descriptor type = `Circle`. Exact match fails (`"Circle" != "Shape"`). Class hierarchy walk: `Circle.baseClass = Shape`. `"Shape" == "Shape"` matches. `isMatch = true`. Emits `ConstantInt::getTrue`.

### Basic Subclassof

```
package examples

from uranite.io.console import puts

public class Vehicle:
    pass

public class Car extends Vehicle:
    pass

public class Sedan extends Car:
    pass

public function main() -> I32:
    if Car subclassof Vehicle:
        puts("Car extends Vehicle")
    if Sedan subclassof Vehicle:
        puts("Sedan extends Vehicle")
    return 0
```

**Compilation trace**:
1. **Parsing**: `Car` parsed as `IdentifierExpression("Car")`. `subclassof` keyword at precedence 7. Parser extracts `"Car"` from identifier, wraps in `SimpleTypeNode("Car")`. `Vehicle` parsed via `parseTypeNode()`. Creates `SubclassofExpression(sourceType=SimpleTypeNode("Car"), targetType=SimpleTypeNode("Vehicle"))`.
2. **Semantic analysis**: both types resolved. Expression type = `Bool`.
3. **HIR lowering**: `HIRSubclassOf(childType=Car, parentType=Vehicle)` created.
4. **MIR lowering**: no case exists — falls through to default, returns invalid variable.

Note: in current implementation, `subclassof` does not produce a usable boolean value at runtime.

### Instanceof with Deep Hierarchy

```
package examples

from uranite.io.console import puts

public class A:
    pass

public class B extends A:
    pass

public class C extends B:
    pass

public class D extends C:
    pass

public function main() -> I32:
    D obj = new D()
    if obj instanceof A:
        puts("D is an A")
    return 0
```

**Compilation trace for `obj instanceof A`**:
1. **LLVM codegen**: `targetTypeName = "A"`. Source type = `D`.
   - Exact match: `"D" != "A"` — no.
   - Hierarchy walk: `D.baseClass = C`. `"C" != "A"`. `C.baseClass = B`. `"B" != "A"`. `B.baseClass = A`. `"A" == "A"` — match found.
   - `isMatch = true`. Emits `ConstantInt::getTrue`.

The hierarchy walk handles arbitrary depth — it follows the `baseClass` chain until a match is found or the chain ends.

### Instanceof with Generic Types

```
package examples

from uranite.collection.array-list import ArrayList
from uranite.io.console import puts

public function main() -> I32:
    ArrayList<String> names = new ArrayList<String>()
    if names instanceof ArrayList:
        puts("names is an ArrayList")
    return 0
```

**Compilation trace**:
1. **LLVM codegen**: `targetTypeName = "ArrayList"` (no generic brackets in target). Source type name = `"ArrayList<String>"`. Generic bracket stripping: `"ArrayList<String>"` becomes `"ArrayList"`. Exact match: `"ArrayList" == "ArrayList"` — match.
2. `isMatch = true`. Emits `ConstantInt::getTrue`.

Generic type parameters are stripped from both source and target before comparison. `ArrayList<String> instanceof ArrayList` matches because the base type names are equal after stripping.

### Instanceof Negative Case

```
package examples

from uranite.io.console import puts

public class Cat:
    pass

public class Dog:
    pass

public function main() -> I32:
    Cat cat = new Cat()
    if cat instanceof Dog:
        puts("This will not print")
    else:
        puts("Cat is not a Dog")
    return 0
```

**Compilation trace**:
1. **LLVM codegen**: `targetTypeName = "Dog"`. Source type = `Cat`.
   - Exact match: `"Cat" != "Dog"` — no.
   - Hierarchy walk: `Cat` has no base class (or base is `Object`). If `baseClass` is `Object`, `"Object" != "Dog"`, then `Object.baseClass = nullptr` — walk ends.
   - Object fallback: `"Cat" != "Object"` — no.
   - `isMatch = false`. Emits `ConstantInt::getFalse`.

Since the result is a compile-time constant, the LLVM optimizer can eliminate the dead branch entirely — the "if" body is removed from the final binary.

### Instanceof in Conditional Chain

```
package examples

from uranite.io.console import puts

public class Shape:
    pass

public class Circle extends Shape:
    pass

public class Rectangle extends Shape:
    pass

public function processShape(Shape shape) -> Void:
    if shape instanceof Circle:
        puts("Processing circle")
    else:
        if shape instanceof Rectangle:
            puts("Processing rectangle")
        else:
            puts("Unknown shape")

public function main() -> I32:
    Circle circle = new Circle()
    processShape(circle)
    return 0
```

**Compilation trace**: when `processShape` is called with a `Circle` argument, the parameter `shape` has static type `Shape`. At codegen time:
- `shape instanceof Circle`: source type is `Shape`. Target is `Circle`. Exact match: `"Shape" != "Circle"`. Hierarchy walk: `Shape.baseClass` — may be null or `Object`. `Circle` is not a superclass of `Shape`. `isMatch = false`.

This reveals a limitation of static instanceof: when the parameter type is a base class (`Shape`), the static type information does not reflect the runtime concrete type. The check resolves to `false` even though the actual runtime value is a `Circle`. True runtime type identification would require RTTI metadata (type tags or vtable-based checks), which Uranite does not currently implement.
