# Arithmetic Operators

Uranite provides six arithmetic operators for numeric computation: addition (`+`), subtraction (`-`), multiplication (`*`), division (`/`), modulo (`%`), and exponentiation (`**`). These operators work natively on primitive integer and floating-point types, extend to OOP wrapper classes via numeric detection through `descriptor::Builtin::numericOopNames`, and support user-defined types through operator interface dispatch (`Addable`, `Subtractable`, `Multipliable`, `Dividable`, `Modulable`). Additionally, `+` is overloaded for string concatenation when the left operand is a `String`. All arithmetic operators are parsed as binary expressions in Pratt-style precedence climbing via `parsePrecedenceExpression`, produce `BinaryExpression` AST nodes, flow through `analyzeBinaryExpression` for type checking, lower to `HIRBinaryOperation`, then to typed MIR instructions (split into `*Integer` and `*Float` variants), and finally emit LLVM IR via `generateArithmetic` with automatic zero-division checks for integer division and modulo, wrapping arithmetic for integer overflow, and IEEE 754 semantics for floating-point operations.

---

## Table of Contents

- [Operator Reference](#operator-reference)
- [Precedence and Associativity](#precedence-and-associativity)
- [Syntax](#syntax)
- [AST Representation — BinaryExpression](#ast-representation--binaryexpression)
- [Parsing](#parsing)
- [Semantic Analysis](#semantic-analysis)
  - [Primitive Numeric Operations](#primitive-numeric-operations)
  - [String Concatenation](#string-concatenation)
  - [Operator Interface Dispatch](#operator-interface-dispatch)
- [Compilation Pipeline](#compilation-pipeline)
  - [HIR Stage — HIRBinaryOperation](#hir-stage--hirbinaryoperation)
  - [MIR Lowering — Typed Instructions](#mir-lowering--typed-instructions)
  - [MIR Codegen — generateArithmetic](#mir-codegen--generatearithmetic)
- [Compound Assignment Operators](#compound-assignment-operators)
  - [AST Representation — AssignStatement](#ast-representation--assignstatement)
  - [Parsing](#compound-assignment-parsing)
  - [MIR Lowering — Load-Compute-Store](#mir-lowering--load-compute-store)
- [Operator Interface Mapping Table](#operator-interface-mapping-table)
- [Division and Modulo Safety](#division-and-modulo-safety)
- [Integer Overflow Behavior](#integer-overflow-behavior)
- [Examples](#examples)

---

## Operator Reference

| Operator | Operation | Token Type | MIR Integer | MIR Float |
|---|---|---|---|---|
| `+` | Addition | `Plus` | `AddInteger` | `AddFloat` |
| `-` | Subtraction | `Minus` | `SubtractInteger` | `SubtractFloat` |
| `*` | Multiplication | `Star` | `MultiplyInteger` | `MultiplyFloat` |
| `/` | Division | `Slash` | `DivideInteger` | `DivideFloat` |
| `%` | Modulo | `Percent` | `ModuloInteger` | N/A |
| `**` | Exponentiation | `Power` | `PowerInteger` | `PowerFloat` |

---

## Precedence and Associativity

Arithmetic operators follow standard mathematical precedence, implemented in `getOperatorPrecedenceValue` at `src/uranite/parser/parser.cpp:111-152`:

| Precedence | Operators | Description |
|---|---|---|
| 12 | `as` | Type cast (highest) |
| 11 | `**` | Exponentiation (right-associative) |
| 10 | `*`, `/`, `%` | Multiplicative |
| 9 | `+`, `-` | Additive |
| 8 | `<<`, `>>` | Shift |
| 7 | `<`, `>`, `<=`, `>=`, `is`, `in`, `instanceof`, `subclassof` | Relational |
| 6 | `==`, `!=` | Equality |
| 5 | `&` | Bitwise AND |
| 4 | `^` | Bitwise XOR |
| 3 | `\|` | Bitwise OR |
| 2 | `and` | Logical AND |
| 1 | `or` | Logical OR (lowest) |

All arithmetic operators are **left-associative** except `**` (exponentiation), which is **right-associative**. This is implemented at `parser.cpp:2189`:

```cpp
int nextMinPrecedence =
    ( kind == token::Type::Power )
    ? precedence : precedence + 1;
```

For left-associative operators, the right operand parses at `precedence + 1`, preventing same-precedence operators from binding rightward. For `**`, the right operand parses at `precedence` (same level), allowing `2 ** 3 ** 4` to parse as `2 ** (3 ** 4)`.

---

## Syntax

```
I64 sum = 10 + 20
I64 difference = 100 - 42
I64 product = 6 * 7
I64 quotient = 100 / 3
I64 remainder = 100 % 7
I64 power = 2 ** 10

F64 floatResult = 3.14 * 2.0
String greeting = "hello" + " world"
```

---

## AST Representation — BinaryExpression

Defined in `src/uranite/ast/node.hpp:1197-1222`:

```cpp
struct BinaryExpression : Expression {

    ExpressionSharedPointer left;
    token::Type operation;
    ExpressionSharedPointer right;

    BinaryExpression(
        token::Type operation,
        ExpressionSharedPointer left,
        ExpressionSharedPointer right,
        const lookup::SourceSharedPointer& source
    ) : Expression( Node::Kind::BinaryExpression,
            source ),
        left( std::move( left ) ),
        operation( operation ),
        right( std::move( right ) ) {
    }

};
```

| Field | Type | Description |
|---|---|---|
| `left` | `ExpressionSharedPointer` | Left operand expression |
| `operation` | `token::Type` | Operator token type (`Plus`, `Minus`, `Star`, `Slash`, `Percent`, `Power`) |
| `right` | `ExpressionSharedPointer` | Right operand expression |

All binary operators (arithmetic, comparison, logical, bitwise) share this single `BinaryExpression` node. The `operation` field distinguishes them.

---

## Parsing

Binary expressions are parsed by `parsePrecedenceExpression` at `src/uranite/parser/parser.cpp:2143-2205` using Pratt precedence climbing:

```cpp
ast::nodes::ExpressionSharedPointer
    Parser::parsePrecedenceExpression(
        int minimumPrecedence ) {
    ast::nodes::ExpressionSharedPointer left =
        this->parseUnaryExpression();
    if( left == nullptr ) {
        return nullptr;
    }
    while( true ) {
        token::Type kind = this->current().type;
        int precedence =
            this->getOperatorPrecedenceValue( kind );
        if( precedence < minimumPrecedence ) {
            break;
        }
        // ... special cases for as/instanceof/subclassof ...
        lookup::SourceSharedPointer source =
            this->current().source;
        this->advance();
        int nextMinPrecedence =
            ( kind == token::Type::Power )
            ? precedence : precedence + 1;
        ast::nodes::ExpressionSharedPointer right =
            this->parsePrecedenceExpression(
                nextMinPrecedence );
        left = std::make_shared<
            ast::nodes::BinaryExpression>(
            kind, left, right, source );
    }
    return left;
}
```

Entry point: `parseExpression` at `parser.cpp:1234` calls `parsePrecedenceExpression(1)`, allowing all precedence levels. The left operand starts with `parseUnaryExpression`, then the loop consumes binary operators whose precedence meets or exceeds `minimumPrecedence`.

---

## Semantic Analysis

The `analyzeBinaryExpression` method at `src/uranite/semantic/analyzer.cpp:1125-1269` handles arithmetic operators in a dedicated case block.

### Primitive Numeric Operations

At `analyzer.cpp:1222-1237`:

```cpp
case token::Type::Minus:
case token::Type::Percent:
case token::Type::Plus:
case token::Type::Power:
case token::Type::Slash:
case token::Type::Star: {
    bool leftIsNumeric = leftSideType->isNumeric() ||
        descriptor::Builtin::numericOopNames.count(
            leftSideType->name ) > 0;
    bool rightIsNumeric = rightSideType->isNumeric() ||
        descriptor::Builtin::numericOopNames.count(
            rightSideType->name ) > 0;
    if( leftIsNumeric && rightIsNumeric ) {
        bool leftIsFloat =
            leftSideType->isFloatingPoint() ||
            descriptor::Builtin::floatOopNames.count(
                leftSideType->name ) > 0;
        bool rightIsFloat =
            rightSideType->isFloatingPoint() ||
            descriptor::Builtin::floatOopNames.count(
                rightSideType->name ) > 0;
        if( leftIsFloat || rightIsFloat ) {
            return this->typeRegistry.getFloat64();
        }
        return this->typeRegistry.getInteger64();
    }
    // ...
}
```

Type promotion rules for primitive arithmetic:

| Left Type | Right Type | Result Type |
|---|---|---|
| Integer | Integer | `I64` |
| Integer | Float | `F64` |
| Float | Integer | `F64` |
| Float | Float | `F64` |

Both primitive types and OOP wrappers are recognized as numeric through `descriptor::Builtin::numericOopNames` (which includes `I8`, `I16`, `I32`, `I64`, `U8`, `U16`, `U32`, `U64`, `F32`, `F64`, `Float`, `Double`, `Byte`) and `descriptor::Builtin::floatOopNames` (which includes `F32`, `F64`, `Float`, `Double`).

### String Concatenation

At `analyzer.cpp:1238-1240`:

```cpp
if( expression.operation == token::Type::Plus &&
    ( leftSideType->kind == Type::Kind::String ||
      leftSideType->qualified ==
        qualname::String ) ) {
    return leftSideType;
}
```

The `+` operator is overloaded for `String` types. When the left operand is `String` (either primitive or OOP wrapper), the result type is `String`. This enables `"hello" + " world"` concatenation.

### Operator Interface Dispatch

At `analyzer.cpp:1241-1257`, if neither operand is a primitive numeric type, the analyzer checks whether the left operand's class implements the appropriate operator interface:

```cpp
if( leftSideType->kind == Type::Kind::Class ||
    leftSideType->kind == Type::Kind::Struct ) {
    ClassTypeSharedPointer leftSideClassType =
        std::dynamic_pointer_cast<ClassType>(
            leftSideType );
    if( leftSideClassType ) {
        for( const qualname::OperatorMapping& mapping :
             qualname::ArithmeticMappings ) {
            if( mapping.tokenType !=
                ( int ) expression.operation )
                continue;
            if( leftSideClassType->implementsInterface(
                    mapping.interfaceQualified ) ) {
                return leftSideType;
            }
            if( leftSideClassType->findMethod(
                    mapping.methodName ) ) {
                // error: has method but no interface
                return leftSideType;
            }
            break;
        }
    }
}
```

Resolution order:
1. Check `implementsInterface` against the interface qualified name from `ArithmeticMappings`.
2. If interface not implemented, check if the class has the method name (e.g., "add" for `+`). If found, emit a diagnostic warning about the missing interface and still allow the operation.
3. If neither, emit error: "invalid operands to binary operator".

---

## Compilation Pipeline

### HIR Stage — HIRBinaryOperation

Defined in `src/uranite/ir/hir.hpp:623-641`:

```cpp
struct HIRBinaryOperation : HIRNode {

    token::Type operatorKind;
    HIRNodeSharedPointer leftOperand;
    HIRNodeSharedPointer rightOperand;

    HIRBinaryOperation(
        token::Type operatorKind,
        HIRNodeSharedPointer leftOperand,
        HIRNodeSharedPointer rightOperand,
        semantic::TypeSharedPointer resolvedType,
        const lookup::SourceSharedPointer& sourceLocation
    ) : HIRNode( HIRNodeKind::BinaryOperation,
            std::move( resolvedType ), sourceLocation ),
        operatorKind( operatorKind ),
        leftOperand( std::move( leftOperand ) ),
        rightOperand( std::move( rightOperand ) ) {
    }

};
```

The HIR node preserves the token-level operator kind and carries the semantically resolved result type.

### MIR Lowering — Typed Instructions

The `lowerBinaryOperation` method at `src/uranite/ir/mir/lowering.cpp:3297-3385` converts `HIRBinaryOperation` to typed MIR instructions:

```cpp
MIRVariableIdentifier MIRLowering::lowerBinaryOperation(
    hir::HIRBinaryOperation& hirBinaryOp ) {
    MIRVariableIdentifier leftVariable =
        this->lowerExpression( hirBinaryOp.leftOperand );
    MIRVariableIdentifier rightVariable =
        this->lowerExpression( hirBinaryOp.rightOperand );
    // ... float detection via resolvedType ...
    MIRInstructionKind instructionKind;
    switch( hirBinaryOp.operatorKind ) {
        case token::Type::Plus:
            instructionKind = isFloatOperation
                ? MIRInstructionKind::AddFloat
                : MIRInstructionKind::AddInteger;
            break;
        case token::Type::Minus:
            instructionKind = isFloatOperation
                ? MIRInstructionKind::SubtractFloat
                : MIRInstructionKind::SubtractInteger;
            break;
        case token::Type::Star:
            instructionKind = isFloatOperation
                ? MIRInstructionKind::MultiplyFloat
                : MIRInstructionKind::MultiplyInteger;
            break;
        case token::Type::Slash:
            instructionKind = isFloatOperation
                ? MIRInstructionKind::DivideFloat
                : MIRInstructionKind::DivideInteger;
            break;
        case token::Type::Percent:
            instructionKind =
                MIRInstructionKind::ModuloInteger;
            break;
        case token::Type::Power:
            instructionKind = isFloatOperation
                ? MIRInstructionKind::PowerFloat
                : MIRInstructionKind::PowerInteger;
            break;
        // ...
    }
    MIRInstruction binaryInstruction( instructionKind );
    binaryInstruction.sourceOperands.push_back(
        leftVariable );
    binaryInstruction.sourceOperands.push_back(
        rightVariable );
    binaryInstruction.operandType =
        hirBinaryOp.resolvedType;
    MIRVariableIdentifier resultVariable =
        this->currentFunction->allocateVariable(
            "_binop", resultType, false );
    binaryInstruction.destinationVariable = resultVariable;
    return this->emitInstruction( binaryInstruction );
}
```

Float detection examines the resolved type from semantic analysis — if either operand or the result type is a float kind or a float OOP wrapper class name, `isFloatOperation` is set to `true` and the `*Float` MIR instruction variant is selected.

Modulo (`%`) always uses `ModuloInteger` — there is no `ModuloFloat` instruction.

### MIR Codegen — generateArithmetic

At `src/uranite/ir/mir/codegen.cpp:1098-1111`, all arithmetic MIR instructions dispatch to `generateArithmetic`:

```cpp
case MIRInstructionKind::AddInteger:
case MIRInstructionKind::SubtractInteger:
case MIRInstructionKind::MultiplyInteger:
case MIRInstructionKind::DivideInteger:
case MIRInstructionKind::ModuloInteger:
case MIRInstructionKind::NegateInteger:
case MIRInstructionKind::PowerInteger:
case MIRInstructionKind::AddFloat:
case MIRInstructionKind::SubtractFloat:
case MIRInstructionKind::MultiplyFloat:
case MIRInstructionKind::DivideFloat:
case MIRInstructionKind::NegateFloat:
case MIRInstructionKind::PowerFloat:
    this->generateArithmetic( instruction );
    break;
```

The `generateArithmetic` method maps each instruction to LLVM IR builder calls:

| MIR Instruction | LLVM Builder Call |
|---|---|
| `AddInteger` | `CreateAdd` (wrapping) |
| `SubtractInteger` | `CreateSub` (wrapping) |
| `MultiplyInteger` | `CreateMul` (wrapping) |
| `DivideInteger` | `CreateSDiv` (signed) + zero check |
| `ModuloInteger` | `CreateSRem` (signed) + zero check |
| `PowerInteger` | Loop-based exponentiation |
| `AddFloat` | `CreateFAdd` |
| `SubtractFloat` | `CreateFSub` |
| `MultiplyFloat` | `CreateFMul` |
| `DivideFloat` | `CreateFDiv` |
| `PowerFloat` | `llvm.pow.f64` intrinsic |
| `NegateInteger` | `CreateNeg` |
| `NegateFloat` | `CreateFNeg` |

Integer operations use wrapping semantics — overflow wraps around without trapping. Float operations follow IEEE 754 rules.

---

## Compound Assignment Operators

### AST Representation — AssignStatement

Defined in `src/uranite/ast/node.hpp:1948-1977`:

```cpp
struct AssignStatement : Statement {

    token::Type operation;
    ExpressionSharedPointer target;
    ExpressionSharedPointer value;

    AssignStatement(
        ExpressionSharedPointer target,
        token::Type operation,
        ExpressionSharedPointer value,
        const lookup::SourceSharedPointer& source
    ) : Statement( Node::Kind::AssignmentStatement,
            source ),
        operation( operation ),
        target( std::move( target ) ),
        value( std::move( value ) ) {
    }

};
```

Compound assignment operators:

| Syntax | Token Type | Desugars To |
|---|---|---|
| `x = y` | `Assignment` | Direct store |
| `x += y` | `PlusAssignment` | `x = x + y` |
| `x -= y` | `MinusAssignment` | `x = x - y` |
| `x *= y` | `StarAssignment` | `x = x * y` |
| `x /= y` | `SlashAssignment` | `x = x / y` |

### Parsing {#compound-assignment-parsing}

At `src/uranite/parser/parser.cpp:2531-2537`:

```cpp
if( this->current().isAssignment() ) {
    lookup::SourceSharedPointer assignmentSource =
        this->current().source;
    token::Type assignmentOperator =
        this->current().type;
    this->advance();
    ast::nodes::ExpressionSharedPointer
        rightHandSideValue = this->parseExpression();
    this->expectNewline( "assignment" );
    return std::make_shared<ast::nodes::AssignStatement>(
        fallbackExpression, assignmentOperator,
        rightHandSideValue, assignmentSource );
}
```

Assignment is not parsed as a binary expression — it is a statement-level construct. After parsing an expression, the parser checks if the next token is an assignment operator via `isAssignment()`. If so, it creates an `AssignStatement`.

Increment (`++`) and decrement (`--`) are desugared at parse time into compound assignments at `parser.cpp:2539-2551`:

```cpp
if( this->check( token::Type::Increment ) ) {
    mutationOperator = token::Type::PlusAssignment;
}
else {
    mutationOperator = token::Type::MinusAssignment;
}
this->advance();
ast::nodes::IntegerLiteralExpressionSharedPointer
    literalOne = std::make_shared<
        ast::nodes::IntegerLiteralExpression>(
        1, "1", mutationSource );
return std::make_shared<ast::nodes::AssignStatement>(
    fallbackExpression, mutationOperator,
    literalOne, mutationSource );
```

So `x++` becomes `x += 1` and `x--` becomes `x -= 1`.

### MIR Lowering — Load-Compute-Store {#mir-lowering--load-compute-store}

At `src/uranite/ir/mir/lowering.cpp:921-1007`, compound assignments desugar to a load-compute-store sequence:

1. **Lower value expression**: Compute the right-hand side to a variable.
2. **Resolve target**: Look up target variable in `variableNameMap` or `globalVariables`.
3. **For compound operators** (`+=`, `-=`, `*=`, `/=`):
   - Emit `LoadVariable` to read current value of target.
   - Select arithmetic instruction kind based on operator and float detection.
   - Emit arithmetic instruction (e.g., `AddInteger`) with loaded target and value as operands.
   - Result variable becomes the new value.
4. **Emit `StoreVariable`**: Write result (or direct value for `=`) to target variable.

```cpp
switch( assignment.assignmentOperator ) {
    case token::Type::PlusAssignment:
        arithmeticKind = isFloatOp
            ? MIRInstructionKind::AddFloat
            : MIRInstructionKind::AddInteger;
        break;
    case token::Type::MinusAssignment:
        arithmeticKind = isFloatOp
            ? MIRInstructionKind::SubtractFloat
            : MIRInstructionKind::SubtractInteger;
        break;
    case token::Type::StarAssignment:
        arithmeticKind = isFloatOp
            ? MIRInstructionKind::MultiplyFloat
            : MIRInstructionKind::MultiplyInteger;
        break;
    case token::Type::SlashAssignment:
        arithmeticKind = isFloatOp
            ? MIRInstructionKind::DivideFloat
            : MIRInstructionKind::DivideInteger;
        break;
}
```

---

## Operator Interface Mapping Table

Defined in `src/uranite/semantic/qualnames.hpp:502-516`:

```cpp
struct OperatorMapping {
    int tokenType;
    const std::string& interfaceQualified;
    const char* methodName;
    const char* interfaceName;
    bool negateResult;
};

inline const OperatorMapping ArithmeticMappings[] = {
    { (int)token::Type::Plus,    Addable::Qualified,
      Addable::methods::Add,     Addable::Name,       false },
    { (int)token::Type::Minus,   Subtractable::Qualified,
      Subtractable::methods::Subtract, Subtractable::Name, false },
    { (int)token::Type::Star,    Multipliable::Qualified,
      Multipliable::methods::Multiply, Multipliable::Name, false },
    { (int)token::Type::Slash,   Dividable::Qualified,
      Dividable::methods::Divide, Dividable::Name,     false },
    { (int)token::Type::Percent, Modulable::Qualified,
      Modulable::methods::Modulo, Modulable::Name,     false },
};
```

| Token | Interface | Method | Example |
|---|---|---|---|
| `+` | `Addable` | `add` | `class Vector implements Addable` |
| `-` | `Subtractable` | `subtract` | `class Vector implements Subtractable` |
| `*` | `Multipliable` | `multiply` | `class Matrix implements Multipliable` |
| `/` | `Dividable` | `divide` | `class Fraction implements Dividable` |
| `%` | `Modulable` | `modulo` | `class BigInt implements Modulable` |

When a class implements the appropriate interface, the operator dispatches to the interface method. At MIR codegen, these dispatch through the interface table (itable) virtual call mechanism.

---

## Division and Modulo Safety

Integer division (`/`) and modulo (`%`) get automatic zero-division checks inserted at MIR codegen. Before emitting `CreateSDiv` or `CreateSRem`, the codegen inserts a comparison of the divisor against zero and branches to a trap or exception if the divisor is zero. This prevents undefined behavior from signed integer division by zero.

Float division follows IEEE 754 — dividing by zero produces `+Inf`, `-Inf`, or `NaN` without trapping.

---

## Integer Overflow Behavior

Integer arithmetic uses LLVM's wrapping instructions (`CreateAdd`, `CreateSub`, `CreateMul`) — overflow wraps around modulo 2^64 without trapping. This matches the behavior of most systems programming languages.

For example, `I64 max = 9223372036854775807` followed by `max + 1` wraps to `-9223372036854775808`.

---

## Examples

### Basic Arithmetic

```
package examples

from uranite.io.console import puts

public function main() -> I32:
    I64 sum = 10 + 20
    I64 difference = 100 - 42
    I64 product = 6 * 7
    I64 quotient = 100 / 3
    I64 remainder = 100 % 7
    I64 power = 2 ** 10

    puts(sum.toString())
    puts(power.toString())

    return 0
```

**Compilation trace**:
1. **Parser**: Each operation parsed as `BinaryExpression(Plus/Minus/Star/Slash/Percent/Power, left, right)`.
2. **Semantic analysis**: Both operands are `I64` (integer literal resolves to `I64`). `isNumeric()` returns `true`. No float operand detected. Result type: `I64`.
3. **MIR lowering**: Each operation becomes `AddInteger`/`SubtractInteger`/`MultiplyInteger`/`DivideInteger`/`ModuloInteger`/`PowerInteger`.
4. **LLVM**: `add i64`, `sub i64`, `mul i64`, `sdiv i64`, `srem i64`, plus power loop.

### Mixed Float Arithmetic

```
package examples

public function main() -> I32:
    F64 result = 3.14 * 2.0 + 1.0 / 3.0
    I64 integer = 42
    F64 mixed = integer + 3.14

    return 0
```

**Compilation trace**:
1. **Semantic analysis**: `3.14 * 2.0` — both `F64`, result `F64`. `1.0 / 3.0` — both `F64`, result `F64`. `F64 + F64` — result `F64`. For `integer + 3.14`, left is `I64`, right is `F64` — float detected, result `F64`.
2. **MIR**: `MultiplyFloat`, `DivideFloat`, `AddFloat` for pure float operations. Mixed `integer + 3.14` becomes `AddFloat` with implicit cast.

### Operator Overloading via Interface

```
package examples

from uranite.io.console import puts

public interface Addable<T>:
    public function add(self, T other) -> T;

public class Vector implements Addable<Vector>:
    public F64 xCoord
    public F64 yCoord

    public function Vector(
        self, F64 xCoord, F64 yCoord) -> Void:
        self.xCoord = xCoord
        self.yCoord = yCoord

    public function add(self, Vector other) -> Vector:
        return new Vector(
            self.xCoord + other.xCoord,
            self.yCoord + other.yCoord)

    public function toString(self) -> String:
        return "(" + self.xCoord.toString() + ", " +
               self.yCoord.toString() + ")"

public function main() -> I32:
    Vector vectorA = new Vector(1.0, 2.0)
    Vector vectorB = new Vector(3.0, 4.0)
    Vector vectorC = vectorA + vectorB
    puts(vectorC.toString())
    return 0
```

**Compilation trace**:
1. **Semantic analysis**: `vectorA + vectorB` — left is `Vector` (class). `ArithmeticMappings` checks `Plus` token, finds `Addable` interface. `Vector.implementsInterface("uranite.language.Addable")` returns `true`. Result type: `Vector`.
2. **MIR lowering**: `lowerBinaryOperation` dispatches to interface method call `Vector.add` via `CallFunction` instruction.
3. **LLVM**: Virtual dispatch through itable lookup for `Addable.add`.

### Compound Assignment

```
package examples

public function main() -> I32:
    I64 counter = 0
    counter += 10
    counter -= 3
    counter *= 2
    counter /= 7

    counter++
    counter--

    return 0
```

**Compilation trace**:
1. **Parser**: `counter += 10` becomes `AssignStatement(target=counter, op=PlusAssignment, value=10)`. `counter++` becomes `AssignStatement(target=counter, op=PlusAssignment, value=1)`.
2. **MIR lowering**: Load-compute-store sequence:
   - `LoadVariable _compound_lhs = counter`
   - `AddInteger _compound_result = _compound_lhs + 10`
   - `StoreVariable counter = _compound_result`
