# Unary Operators

Uranite provides eight prefix unary operators: arithmetic negation (`-`), logical NOT (`not`/`!`), bitwise NOT (`~`), reference (`&`), dereference (`*`), address-of (`addressof`), move (`move`), and increment/decrement (`++`/`--`). All unary expressions are parsed by `parseUnaryExpression` before binary precedence climbing begins, giving them effectively higher precedence than any binary operator. They produce `UnaryExpression` AST nodes carrying the operator token type, operand expression, and a prefix flag. At MIR level, each operator maps to a specific instruction kind (`NegateInteger`/`NegateFloat`, `LogicalNot`, `BitwiseNot`, etc.), with arithmetic negation supporting Negatable interface dispatch for class types. Increment and decrement desugar to a copy-add/subtract-store sequence at MIR level, returning the old value for postfix and new value for prefix.

---

## Table of Contents

- [Operator Reference](#operator-reference)
- [AST Representation — UnaryExpression](#ast-representation--unaryexpression)
- [Parsing](#parsing)
- [Arithmetic Negation — Minus](#arithmetic-negation--minus)
  - [Semantic Analysis](#negation-semantic-analysis)
  - [MIR Lowering — Negatable Interface Dispatch](#mir-lowering--negatable-interface-dispatch)
  - [MIR Codegen — NegateInteger / NegateFloat](#mir-codegen--negateinteger--negatefloat)
- [Logical NOT — not / !](#logical-not--not--)
- [Bitwise NOT — ~](#bitwise-not--)
- [Reference — &](#reference--)
- [Dereference — *](#dereference--)
- [Address-of — addressof](#address-of--addressof)
- [Move — move](#move--move)
- [Increment / Decrement — ++ / --](#increment--decrement------)
  - [Semantic Analysis](#increment-semantic-analysis)
  - [MIR Lowering — Copy-Compute-Store](#mir-lowering--copy-compute-store)
  - [Prefix vs Postfix](#prefix-vs-postfix)
- [Examples](#examples)

---

## Operator Reference

| Operator | Operation | Token Type | MIR Instruction |
|---|---|---|---|
| `-` | Arithmetic negation | `Minus` | `NegateInteger` / `NegateFloat` |
| `not` | Logical NOT | `KeywordNot` | `LogicalNot` |
| `!` | Logical NOT (alias) | `Bang` | `LogicalNot` |
| `~` | Bitwise NOT | `Tilde` | `BitwiseNot` |
| `&` | Reference (borrow) | `Ampersand` | `TakeReference` |
| `*` | Dereference | `Star` | Dereference/unwrap |
| `addressof` | Raw address | `KeywordAddressof` | Address extraction |
| `move` | Ownership transfer | `KeywordMove` | Move semantics |
| `++` | Increment | `Increment` | AddInteger/AddFloat + StoreVariable |
| `--` | Decrement | `Decrement` | SubtractInteger/SubtractFloat + StoreVariable |

---

## AST Representation — UnaryExpression

Defined in `src/uranite/ast/node.hpp:1888-1917`:

```cpp
struct UnaryExpression : Expression {

    bool isPrefix;
    ExpressionSharedPointer operand;
    token::Type operation;

    UnaryExpression(
        token::Type operation,
        ExpressionSharedPointer operand,
        bool prefix,
        const lookup::SourceSharedPointer& source
    ) : Expression( Node::Kind::UnaryExpression,
            source ),
        isPrefix( prefix ),
        operand( std::move( operand ) ),
        operation( operation ) {
    }

};
```

| Field | Type | Description |
|---|---|---|
| `isPrefix` | `bool` | `true` for prefix operators, `false` for postfix |
| `operand` | `ExpressionSharedPointer` | Expression being operated on |
| `operation` | `token::Type` | Operator token type |

All unary operators in Uranite parse as prefix (`isPrefix = true`), except postfix `++`/`--` which are parsed separately at statement level (desugared to compound assignment — see [Arithmetic Operators](arithmetic-operators.md#compound-assignment-operators)).

---

## Parsing

All prefix unary operators are parsed by `parseUnaryExpression` at `src/uranite/parser/parser.cpp:2878-2921`:

```cpp
ast::nodes::ExpressionSharedPointer
    Parser::parseUnaryExpression() {
    lookup::SourceSharedPointer unarySource =
        this->current().source;
    switch( this->current().type ) {
        case token::Type::Ampersand: {
            this->advance();
            bool isMutable =
                this->match( token::Type::KeywordMutable );
            ( void ) isMutable;
            return std::make_shared<
                ast::nodes::UnaryExpression>(
                token::Type::Ampersand,
                this->parseUnaryExpression(),
                true, unarySource );
        }
        case token::Type::Bang:
        case token::Type::KeywordNot: {
            token::Type negationOp = this->current().type;
            this->advance();
            return std::make_shared<
                ast::nodes::UnaryExpression>(
                negationOp,
                this->parseUnaryExpression(),
                true, unarySource );
        }
        case token::Type::KeywordAddressof: {
            this->advance();
            return std::make_shared<
                ast::nodes::UnaryExpression>(
                token::Type::KeywordAddressof,
                this->parseUnaryExpression(),
                true, unarySource );
        }
        case token::Type::KeywordMove: {
            this->advance();
            return std::make_shared<
                ast::nodes::UnaryExpression>(
                token::Type::KeywordMove,
                this->parseUnaryExpression(),
                true, unarySource );
        }
        case token::Type::Minus: {
            this->advance();
            return std::make_shared<
                ast::nodes::UnaryExpression>(
                token::Type::Minus,
                this->parseUnaryExpression(),
                true, unarySource );
        }
        case token::Type::Star: {
            this->advance();
            return std::make_shared<
                ast::nodes::UnaryExpression>(
                token::Type::Star,
                this->parseUnaryExpression(),
                true, unarySource );
        }
        case token::Type::Tilde: {
            this->advance();
            return std::make_shared<
                ast::nodes::UnaryExpression>(
                token::Type::Tilde,
                this->parseUnaryExpression(),
                true, unarySource );
        }
        case token::Type::Increment:
        case token::Type::Decrement: {
            token::Type operation = this->current().type;
            this->advance();
            return std::make_shared<
                ast::nodes::UnaryExpression>(
                operation,
                this->parseUnaryExpression(),
                true, unarySource );
        }
        default:
            return this->parsePostfixExpression();
    }
}
```

Key details:
- All unary operators are **right-recursive** — `parseUnaryExpression` calls itself for the operand, enabling chaining: `-~x`, `not not flag`, `**ptr`.
- `&` optionally accepts `mutable` keyword (`&mutable x`), though this is currently parsed but not used (`(void) isMutable`).
- If no unary prefix is found, falls through to `parsePostfixExpression()`.

---

## Arithmetic Negation — Minus

### Semantic Analysis {#negation-semantic-analysis}

At `src/uranite/semantic/analyzer.cpp:3952-3971`:

```cpp
case token::Type::Minus: {
    if( operandType->isNumeric() ||
        descriptor::Builtin::numericOopNames.count(
            operandType->name ) > 0 ) {
        return operandType;
    }
    if( operandType->kind == Type::Kind::Class ||
        operandType->kind == Type::Kind::Struct ) {
        ClassTypeSharedPointer classType =
            std::dynamic_pointer_cast<ClassType>(
                operandType );
        if( classType ) {
            if( classType->implementsInterface(
                    qualname::Negatable ) ) {
                return operandType;
            }
            if( classType->findMethod(
                    qualname::interfaces::negatable
                        ::methods::Negate ) ) {
                // warning: has "negate" but no Negatable
                return operandType;
            }
        }
    }
    this->diagnostic.error( expression.source,
        fmt::format(
            "cannot negate type \"{}\"",
            operandType->toString() ) );
    return this->typeRegistry.getError();
}
```

Resolution order:
1. **Primitive/OOP numeric**: `isNumeric()` or listed in `numericOopNames`. Result: same type.
2. **Negatable interface**: Class/struct implementing `Negatable`. Result: same type. Dispatches to `negate` method.
3. **Method-without-interface**: Has `negate` method but no `Negatable` interface — emit warning, still allow.
4. **Error**: Non-numeric, non-Negatable type.

### MIR Lowering — Negatable Interface Dispatch

At `src/uranite/ir/mir/lowering.cpp:3390-3410`, before emitting primitive negate instructions, the lowering checks for Negatable interface:

```cpp
if( operandType != nullptr &&
    operandType->kind == semantic::Type::Kind::Class &&
    hirUnaryOp.operatorKind == token::Type::Minus ) {
    semantic::ClassTypeSharedPointer operandClassType =
        std::static_pointer_cast<semantic::ClassType>(
            operandType );
    if( operandClassType->implementsInterface(
            semantic::qualname::Negatable ) ) {
        std::string qualifiedMethodName =
            operandTypeName + ".negate";
        MIRInstruction methodCall(
            MIRInstructionKind::CallFunction );
        methodCall.calledFunctionQualifiedName =
            qualifiedMethodName;
        methodCall.sourceOperands.push_back(
            operandVariable );
        methodCall.operandType =
            hirUnaryOp.resolvedType;
        MIRVariableIdentifier callResult =
            this->currentFunction->allocateVariable(
                "_op_neg", hirUnaryOp.resolvedType,
                false );
        methodCall.destinationVariable = callResult;
        return this->emitInstruction( methodCall );
    }
}
```

For classes implementing `Negatable`, `-obj` becomes a `CallFunction` to `TypeName.negate(self)`. Generic type parameters are stripped from the type name before method qualification.

For primitive types, the lowering falls through to the standard instruction mapping:

```cpp
case token::Type::Minus: {
    bool isFloatNegate =
        hirUnaryOp.resolvedType != nullptr &&
        ( hirUnaryOp.resolvedType->kind ==
            semantic::Type::Kind::Float ||
          // ... float OOP wrapper checks ...
        );
    instructionKind = isFloatNegate
        ? MIRInstructionKind::NegateFloat
        : MIRInstructionKind::NegateInteger;
    break;
}
```

### MIR Codegen — NegateInteger / NegateFloat

At `src/uranite/ir/mir/codegen.cpp:1926-1956`, `generateArithmetic` handles negation:

```cpp
if( instruction.instructionKind ==
    MIRInstructionKind::NegateInteger ||
    instruction.instructionKind ==
    MIRInstructionKind::NegateFloat ) {
    llvm::Value* operand = this->loadVariableValue(
        instruction.sourceOperands[0] );
    llvm::Value* result = nullptr;
    if( operand->getType()->isFloatingPointTy() ) {
        if( llvm::ConstantFP* constFP =
                llvm::dyn_cast<llvm::ConstantFP>(
                    operand ) ) {
            llvm::APFloat negated =
                constFP->getValueAPF();
            negated.changeSign();
            result = llvm::ConstantFP::get(
                this->llvmContext, negated );
        }
        else {
            result = this->irBuilder.CreateFNeg(
                operand, "neg.f" );
        }
    }
    else if( operand->getType()->isIntegerTy() ) {
        result = this->irBuilder.CreateNeg(
            operand, "neg.i" );
    }
    else if( operand->getType()->isPointerTy() ) {
        llvm::Value* asInt =
            this->irBuilder.CreatePtrToInt(
                operand,
                llvm::Type::getInt64Ty(
                    this->llvmContext ),
                "neg.ptoi" );
        result = this->irBuilder.CreateNeg(
            asInt, "neg.i" );
    }
    this->setVariableValue(
        instruction.destinationVariable, result );
    return;
}
```

| Operand Type | LLVM Operation | Details |
|---|---|---|
| Float (constant) | `APFloat::changeSign` | Compile-time constant folding |
| Float (runtime) | `CreateFNeg` | IEEE 754 negation |
| Integer | `CreateNeg` | Two's complement: `0 - value` |
| Pointer | `PtrToInt` + `CreateNeg` | Convert to i64, then negate |

---

## Logical NOT — not / !

See [Logical Operators](logical-operators.md) for full specification. Summary:

- Semantic: operand must be `Boolean` or assignable to `Boolean` (analyzer.cpp:3973-3981).
- MIR: `LogicalNot` instruction.
- Codegen: `CreateNot` for `i1`, `ICmpEQ(val, 0)` for wider integers, `ICmpEQ(val, null)` for pointers.

---

## Bitwise NOT — ~

See [Bitwise Operators](bitwise-operators.md) for full specification. Summary:

- Semantic: operand must pass `isIntegral()` (analyzer.cpp:3982-3988).
- MIR: `BitwiseNot` instruction.
- Codegen: `PtrToInt` if pointer, then `CreateNot` (XOR with all-ones).

---

## Reference — &

Creates a reference to a value.

At `src/uranite/semantic/analyzer.cpp:3992-3993`:

```cpp
case token::Type::Ampersand: {
    return this->typeRegistry.makeReference(
        operandType );
}
```

`&value` wraps the operand type in `ReferenceType`. Result type is `&T` where `T` is the operand type. The parser also accepts `&mutable value` syntax (parsed but not yet semantically distinct).

---

## Dereference — *

Unwraps a pointer or reference to access the inner value.

At `src/uranite/semantic/analyzer.cpp:3995-4008`:

```cpp
case token::Type::Star: {
    if( operandType->kind == Type::Kind::Pointer ) {
        if( this->currentScope->isInsideUnsafe()
            == false ) {
            this->diagnostic.error( expression.source,
                "dereferencing raw pointer requires "
                "\"unsafe\" block" );
        }
        return std::static_pointer_cast<PointerType>(
            operandType )->inner;
    }
    if( operandType->kind == Type::Kind::Reference ) {
        return std::static_pointer_cast<ReferenceType>(
            operandType )->inner;
    }
    this->diagnostic.error( expression.source,
        fmt::format(
            "cannot dereference type \"{}\"",
            operandType->toString() ) );
    return this->typeRegistry.getError();
}
```

| Operand Type | Result | Requirement |
|---|---|---|
| `Pointer<T>` | `T` | Must be inside `unsafe` block |
| `Reference<T>` | `T` | No restriction |
| Other | Error | Cannot dereference |

Pointer dereferencing is unsafe — accessing arbitrary memory through a raw pointer requires an explicit `unsafe` block. Reference dereferencing is always safe.

---

## Address-of — addressof

Extracts the raw memory address of a value as an integer.

At `src/uranite/semantic/analyzer.cpp:3989-3991`:

```cpp
case token::Type::KeywordAddressof: {
    return this->typeRegistry.lookupType(
        qualname::classes::i64::Name );
}
```

`addressof value` returns an `I64` representing the raw memory address. This is an unsafe operation used for low-level memory manipulation and FFI.

---

## Move — move

Explicitly transfers ownership of a value.

```
String original = "hello"
String moved = move original
```

The `move` keyword signals to the borrow checker and ownership system that the value should be moved (ownership transferred) rather than copied or borrowed. After a move, the original variable is no longer valid.

Semantic analysis passes through to the operand type — `move x` has the same type as `x`.

---

## Increment / Decrement — ++ / --

### Semantic Analysis {#increment-semantic-analysis}

At `src/uranite/semantic/analyzer.cpp:4009-4019`:

```cpp
case token::Type::Increment:
case token::Type::Decrement: {
    if( operandType->isNumeric() == false &&
        operandType->isIntegral() == false ) {
        if( descriptor::Builtin::numericOopNames
                .count( operandType->name ) == 0 ) {
            this->diagnostic.error(
                expression.source, fmt::format(
                "increment/decrement requires numeric "
                "type, got \"{}\"",
                operandType->toString() ) );
            return this->typeRegistry.getError();
        }
    }
    return operandType;
}
```

Operand must be numeric (primitive or OOP wrapper). Result type matches operand type.

### MIR Lowering — Copy-Compute-Store

At `src/uranite/ir/mir/lowering.cpp:3429-3488`, prefix `++`/`--` desugar to a multi-step sequence:

1. **Copy old value**: `CopyValue _incold = operand` — preserves original value for postfix return.
2. **Load constant one**: `ConstantInteger _incone = 1`.
3. **Compute new value**: `AddInteger _incnew = operand + _incone` (for `++`) or `SubtractInteger _incnew = operand - _incone` (for `--`). Float variants use `AddFloat`/`SubtractFloat`.
4. **Store back**: `StoreVariable operand = _incnew` — writes new value back to the variable's storage location (looked up via `variableNameMap`).
5. **Return value**: Prefix returns `_incnew` (new value). Postfix returns `_incold` (old value).

```cpp
return hirUnaryOp.isPrefixOperator
    ? newValue : oldValue;
```

Float detection follows the same pattern as other arithmetic operations — checks `Type::Kind::Float` and float OOP wrapper class names (`Float`, `Double`, `F32`, `F64`).

### Prefix vs Postfix

| Form | Syntax | Returns | MIR Sequence |
|---|---|---|---|
| Prefix | `++x` | New value (after increment) | Copy, add, store, return new |
| Postfix | `x++` | Old value (before increment) | Copy, add, store, return old |

Note: Postfix `++`/`--` at statement level (as standalone statements, e.g., `counter++`) are parsed separately by the statement parser and desugared to compound assignment `AssignStatement(target, PlusAssignment, 1)` — see [Arithmetic Operators](arithmetic-operators.md#compound-assignment-operators). The `UnaryExpression` path handles prefix forms and expression-context postfix forms.

---

## Examples

### Arithmetic Negation

```
package examples

from uranite.io.console import puts

public function main() -> I32:
    I64 positive = 42
    I64 negative = -positive
    puts(negative.toString())

    F64 pi = 3.14159
    F64 negPi = -pi
    puts(negPi.toString())

    return 0
```

**Compilation trace**:
1. **Semantic analysis**: `-positive` — `I64` is numeric, result `I64`. `-pi` — `F64` is numeric, result `F64`.
2. **MIR**: `NegateInteger` for integer, `NegateFloat` for float.
3. **LLVM**: `CreateNeg` emits `sub i64 0, %positive`. `CreateFNeg` emits `fneg double %pi`.

### Negatable Interface

```
package examples

from uranite.io.console import puts

public interface Negatable<T>:
    public function negate(self) -> T;

public class Temperature implements Negatable<Temperature>:
    public F64 degrees

    public function Temperature(self, F64 degrees) -> Void:
        self.degrees = degrees

    public function negate(self) -> Temperature:
        return new Temperature(-self.degrees)

    public function toString(self) -> String:
        return self.degrees.toString() + " degrees"

public function main() -> I32:
    Temperature hot = new Temperature(100.0)
    Temperature cold = -hot
    puts(cold.toString())
    return 0
```

**Compilation trace**:
1. **Semantic analysis**: `-hot` — `Temperature` implements `Negatable`, result `Temperature`.
2. **MIR lowering**: Dispatches to `CallFunction("Temperature.negate")` with `hot` as self argument.

### Reference and Dereference

```
package examples

public function main() -> I32:
    I64 value = 42
    &I64 reference = &value
    I64 dereferenced = *reference

    return 0
```

**Compilation trace**:
1. **Semantic analysis**: `&value` wraps `I64` in `ReferenceType`. Result: `&I64`. `*reference` unwraps `ReferenceType<I64>`. Result: `I64`.

### Pointer Dereference (Unsafe)

```
package examples

public function main() -> I32:
    I64 value = 42
    I64 address = addressof value

    unsafe:
        I64 recovered = *(address as *I64)

    return 0
```

`addressof value` returns `I64` address. Pointer dereference requires `unsafe` block. Outside `unsafe`, compiler emits: "dereferencing raw pointer requires \"unsafe\" block".

### Increment and Decrement

```
package examples

from uranite.io.console import puts

public function main() -> I32:
    I64 counter = 10

    I64 beforeIncrement = counter
    ++counter
    puts(counter.toString())

    counter--
    puts(counter.toString())

    return 0
```

**Compilation trace**:
1. **MIR lowering for `++counter`**:
   - `CopyValue _incold = counter`
   - `ConstantInteger _incone = 1`
   - `AddInteger _incnew = counter + _incone`
   - `StoreVariable counter = _incnew`
   - Returns `_incnew` (prefix — new value)
2. **Statement-level `counter--`**: Desugared by parser to `AssignStatement(counter, MinusAssignment, 1)`. MIR uses load-compute-store pattern.
