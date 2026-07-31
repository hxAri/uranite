# Range Expressions

Uranite provides two range operators: exclusive range (`..`) and inclusive range (`...`). A range expression `start..end` or `start...end` produces a `RangeExpression` AST node carrying start/end sub-expressions and an `inclusive` boolean flag. Range expressions are not first-class values — they do not materialize into a runtime collection object. Instead, they are consumed directly by `for-in` loops and comprehensions, where the MIR lowering desugars them into explicit loop control flow: an `AllocateLocal` for the loop variable, a header block with `CompareLessThan` (exclusive) or `CompareLessEqual` (inclusive), a body block, and an update block that increments by 1 via `AddInteger`. The semantic analyzer types range expressions as `Array<I64>`, and for-in loops over ranges infer the loop variable type as `I64`.

---

## Table of Contents

- [Syntax](#syntax)
- [Token Types](#token-types)
- [AST Representation — RangeExpression](#ast-representation--rangeexpression)
- [Parsing](#parsing)
- [Semantic Analysis](#semantic-analysis)
- [HIR Representation](#hir-representation)
- [MIR Lowering — For-In Range Desugaring](#mir-lowering--for-in-range-desugaring)
  - [Block Structure](#block-structure)
  - [Inclusive vs Exclusive](#inclusive-vs-exclusive)
  - [Complete MIR Sequence](#complete-mir-sequence)
- [MIR Lowering — Comprehension Range](#mir-lowering--comprehension-range)
- [Examples](#examples)

---

## Syntax

```
I64 start..end
I64 start...end
```

| Operator | Token Type | Inclusive | Range |
|---|---|---|---|
| `..` | `DoubleDot` | No | `[start, end)` — end excluded |
| `...` | `Ellipsis` | Yes | `[start, end]` — end included |

```
for I64 index in 0..10:
    pass

for I64 value in 1...5:
    pass
```

`0..10` iterates values 0 through 9. `1...5` iterates values 1 through 5.

---

## Token Types

The lexer recognizes both operators at `src/uranite/lexer/lexer.cpp:562-563`:

```cpp
if( this->match( '.' ) ) {
    this->tokens.push_back( token::Token(
        token::Type::Ellipsis, operatorSource, "..." ) );
}
else {
    this->tokens.push_back( token::Token(
        token::Type::DoubleDot, operatorSource, ".." ) );
}
```

When the lexer encounters `.`, it looks ahead: if the next character is also `.`, it reads a second `.`. If a third `.` follows, it emits `Ellipsis` (`...`); otherwise it emits `DoubleDot` (`..`).

Note: `Ellipsis` is also used in extern function declarations for C-style variadic parameters (`extern function printf(String format, ...) -> I32`). Context disambiguates — range vs extern variadic.

---

## AST Representation — RangeExpression

Defined at `src/uranite/ast/node.hpp:1696-1725`:

```cpp
struct RangeExpression : Expression {

    ExpressionSharedPointer end;
    bool inclusive;
    ExpressionSharedPointer start;

    RangeExpression(
        ExpressionSharedPointer start,
        ExpressionSharedPointer end,
        bool inclusive,
        const lookup::SourceSharedPointer& source
    ) : Expression( Node::Kind::RangeExpression, source ),
        end( std::move( end ) ),
        inclusive( inclusive ),
        start( std::move( start ) ) {
    }

};
```

| Field | Type | Description |
|---|---|---|
| `start` | `ExpressionSharedPointer` | Start value (inclusive lower bound) |
| `end` | `ExpressionSharedPointer` | End value (exclusive or inclusive upper bound) |
| `inclusive` | `bool` | `true` for `...` (inclusive), `false` for `..` (exclusive) |

---

## Parsing

Range operators are parsed **after** the precedence-climbing loop completes, at `src/uranite/parser/parser.cpp:2196-2203`:

```cpp
if( this->check( token::Type::DoubleDot ) ||
    this->check( token::Type::Ellipsis ) ) {
    bool inclusive = this->check( token::Type::Ellipsis );
    lookup::SourceSharedPointer source = this->current().source;
    this->advance();
    ast::nodes::ExpressionSharedPointer end =
        parsePrecedenceExpression( 1 );
    left = std::make_shared<ast::nodes::RangeExpression>(
        left, end, inclusive, source );
}
return left;
```

Key details:
- Range operators bind **after** all binary operators — they have effectively the lowest precedence. `1 + 2..5 * 3` parses as `(1 + 2)..(5 * 3)`.
- The right operand is parsed with minimum precedence 1 (above `or`), so range cannot nest without parentheses.
- Range operators are not part of the precedence table (`getOperatorPrecedenceValue` returns -1 for them) — they are handled as a special post-loop check.

---

## Semantic Analysis

At `src/uranite/semantic/analyzer.cpp:2392-2397`:

```cpp
case ast::Node::Kind::RangeExpression: {
    ast::nodes::RangeExpression& rangeExpression =
        static_cast<ast::nodes::RangeExpression&>( *expression );
    this->analyzeExpression( rangeExpression.start );
    this->analyzeExpression( rangeExpression.end );
    expressionType =
        this->typeRegistry.makeArray(
            this->typeRegistry.getInteger64() );
    break;
}
```

Both start and end sub-expressions are analyzed. The result type is `Array<I64>` — the range is typed as an integer array, though it is never materialized as one at runtime.

When a range expression is used as the iterable in a `for-in` loop, the loop variable type is inferred as `I64` (analyzer.cpp:2594-2595):

```cpp
if( isRangeExpression ) {
    variableType = this->typeRegistry.getInteger64();
}
```

---

## HIR Representation

Defined at `src/uranite/ir/hir.hpp:981-999`:

```cpp
struct HIRRangeExpression : HIRNode {

    HIRNodeSharedPointer rangeStart;
    HIRNodeSharedPointer rangeEnd;
    bool isInclusive;

    HIRRangeExpression(
        HIRNodeSharedPointer rangeStart,
        HIRNodeSharedPointer rangeEnd,
        bool isInclusive,
        semantic::TypeSharedPointer resolvedType,
        const lookup::SourceSharedPointer& sourceLocation
    ) : HIRNode( HIRNodeKind::RangeExpression,
            std::move( resolvedType ), sourceLocation ),
        rangeStart( std::move( rangeStart ) ),
        rangeEnd( std::move( rangeEnd ) ),
        isInclusive( isInclusive ) {
    }

};
```

HIR lowering is straightforward — lower start/end sub-expressions, carry `inclusive` flag (hir/lowering.cpp:1113-1123).

---

## MIR Lowering — For-In Range Desugaring

When a `for-in` loop iterates over a `HIRRangeExpression`, the MIR lowering desugars it into an explicit four-block loop structure at `src/uranite/ir/mir/lowering.cpp:1226-1331`.

### Block Structure

```
                    ┌──────────────┐
                    │  [init]      │
                    │  AllocLocal  │
                    │  Store start │
                    └──────┬───────┘
                           │ JumpUnconditional
                    ┌──────▼───────┐
            ┌──────►│ range.header │
            │       │  Load var    │
            │       │  Compare     │
            │       │  Branch      │
            │       └──┬───────┬───┘
            │     true │       │ false
            │   ┌──────▼──┐  ┌─▼────────┐
            │   │range.body│  │range.exit │
            │   │  [body]  │  └───────────┘
            │   └──────┬───┘
            │          │ JumpUnconditional
            │   ┌──────▼──────┐
            │   │range.update │
            │   │  Load var   │
            │   │  Add 1      │
            │   │  Store var  │
            └───┤  Jump header│
                └─────────────┘
```

### Inclusive vs Exclusive

The comparison instruction in the header block depends on the range operator:

```cpp
MIRInstructionKind compareKind = rangeExpression.isInclusive
    ? MIRInstructionKind::CompareLessEqual
    : MIRInstructionKind::CompareLessThan;
```

| Operator | Comparison | Loop continues while |
|---|---|---|
| `..` (exclusive) | `CompareLessThan` | `loopVar < end` |
| `...` (inclusive) | `CompareLessEqual` | `loopVar <= end` |

### Complete MIR Sequence

**Initialization**:
1. `AllocateLocal _loopVar : I64` — allocate loop variable as local (addressable).
2. Register loop variable name in `variableNameMap`.
3. Lower `rangeStart` expression to get start value.
4. `StoreVariable _loopVar = startValue` — initialize loop variable.
5. Lower `rangeEnd` expression to get end value.

**Header block** (`range.header`):
1. `LoadVariable _range_cur = _loopVar` — load current value from local storage.
2. `CompareLessThan _range_cond = _range_cur, endValue` (or `CompareLessEqual` for inclusive).
3. `BranchConditional _range_cond -> range.body, range.exit`.

**Body block** (`range.body`):
1. Lower loop body statements.
2. Execute deferred statements if any.
3. `JumpUnconditional -> range.update`.

**Update block** (`range.update`):
1. `LoadVariable _range_reload = _loopVar` — reload current value.
2. `ConstantInteger _range_step = 1` — step value.
3. `AddInteger _range_next = _range_reload + _range_step` — increment.
4. `StoreVariable _loopVar = _range_next` — store updated value.
5. `JumpUnconditional -> range.header` — loop back.

**Exit block** (`range.exit`): control flow continues after loop.

The `LoopContext` is pushed onto `loopContextStack` with header, exit, and update block identifiers — enabling `break` (jumps to `range.exit`) and `continue` (jumps to `range.update`).

---

## MIR Lowering — Comprehension Range

Range expressions in comprehensions (`[expr for Type x in start..end]`) follow the same desugaring pattern at `src/uranite/ir/mir/lowering.cpp:2950-3019`:

```cpp
bool isRangeIterable =
    comprehension.iterableExpression != nullptr &&
    comprehension.iterableExpression->nodeKind ==
        hir::HIRNodeKind::RangeExpression;
```

When the comprehension iterable is a range:
1. Extract start and end values from `HIRRangeExpression`.
2. Create four blocks: `comp.header`, `comp.body`, `comp.update`, `comp.exit`.
3. Same header/body/update/exit structure as for-in range loops.
4. Inside the body, evaluate the comprehension body expression and accumulate results.
5. Comparison uses `CompareLessEqual` for inclusive (`...`), `CompareLessThan` for exclusive (`..`).

---

## Examples

### Exclusive Range — for-in

```
package examples

from uranite.io.console import puts

public function main() -> I32:
    for I64 index in 0..5:
        puts(index.toString())
    return 0
```

Output: `0`, `1`, `2`, `3`, `4` (5 is excluded).

**Compilation trace**:
1. **Parsing**: `0..5` — `DoubleDot` token. Creates `RangeExpression(start=0, end=5, inclusive=false)`.
2. **Semantic analysis**: Range typed as `Array<I64>`. Loop variable `index` inferred as `I64`.
3. **MIR lowering**: Four blocks generated:
   - Init: `AllocateLocal index`, `StoreVariable index = 0`
   - Header: `LoadVariable _range_cur = index`, `CompareLessThan _range_cond = _range_cur, 5`, branch
   - Body: lower `puts(index.toString())`
   - Update: `AddInteger _range_next = _range_reload + 1`, `StoreVariable index = _range_next`

### Inclusive Range — for-in

```
package examples

from uranite.io.console import puts

public function main() -> I32:
    for I64 value in 1...5:
        puts(value.toString())
    return 0
```

Output: `1`, `2`, `3`, `4`, `5` (5 is included).

**Compilation trace**: Same structure as exclusive, but header uses `CompareLessEqual` instead of `CompareLessThan`. Loop continues while `value <= 5`.

### Range in Comprehension

```
package examples

public function main() -> I32:
    Memory<I64> squares = [index * index for I64 index in 0..5]
    return 0
```

**Compilation trace**:
1. **Parsing**: `0..5` inside comprehension. `RangeExpression(inclusive=false)` as iterable.
2. **MIR lowering**: Four-block loop within comprehension desugaring. Body evaluates `index * index`, stores result via `ComputeIndexAddress` + `StoreVariable` into heap-allocated array.

### Expression Ranges

```
package examples

from uranite.io.console import puts

public function main() -> I32:
    I64 start = 5
    I64 end = 10
    for I64 value in start..end:
        puts(value.toString())
    return 0
```

Start and end can be arbitrary expressions — they are lowered to MIR values before the loop initialization. Variables, function calls, arithmetic expressions, and any expression producing an integer value are valid.

### Range with Break and Continue

```
package examples

from uranite.io.console import puts

public function main() -> I32:
    for I64 index in 0..100:
        if index == 5:
            continue
        if index >= 10:
            break
        puts(index.toString())
    return 0
```

Output: `0`, `1`, `2`, `3`, `4`, `6`, `7`, `8`, `9`.

**Compilation trace**:
- `continue` emits `JumpUnconditional -> range.update` (skips rest of body, increments, re-checks header).
- `break` emits `JumpUnconditional -> range.exit` (exits loop immediately).

Both resolve via the `LoopContext` pushed onto `loopContextStack` during range loop desugaring.

### Exclusive vs Inclusive Comparison

```
package examples

from uranite.io.console import puts

public function main() -> I32:
    puts("Exclusive 0..3:")
    for I64 index in 0..3:
        puts(index.toString())

    puts("Inclusive 0...3:")
    for I64 index in 0...3:
        puts(index.toString())

    return 0
```

Output:
```
Exclusive 0..3:
0
1
2
Inclusive 0...3:
0
1
2
3
```

The only difference in generated MIR: `CompareLessThan` vs `CompareLessEqual` in the header block comparison instruction.
