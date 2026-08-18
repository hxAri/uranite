# Boolean Literals

Uranite provides two keyword literals for truth values: `True` and `False`. Both are reserved keywords with mandatory PascalCase — lowercase `true` and `false` are not valid boolean values.

---

## Table of Contents

- [Boolean Literals](#boolean-literals)
  - [Table of Contents](#table-of-contents)
  - [Syntax and Casing](#syntax-and-casing)
  - [Type](#type)
  - [Boolean in Conditions](#boolean-in-conditions)
  - [Boolean as Return Values](#boolean-as-return-values)
  - [Logical Operators](#logical-operators)
  - [Evaluation Behavior](#evaluation-behavior)

---

## Syntax and Casing

```uranite
Boolean active = True
Boolean disabled = False
```

The casing is mandatory. `True` and `False` are the only accepted forms. Writing `true`, `TRUE`, or any other casing produces a compilation error — the compiler treats them as ordinary identifiers.

---

## Type

Both `True` and `False` have type `Boolean`. This is the only boolean type in Uranite.

```uranite
Boolean flag = True
```

---

## Boolean in Conditions

Uranite requires explicit `Boolean` values in conditions. There is no implicit truthiness — integers, strings, `None`, and other types do not auto-convert to `Boolean`.

```uranite
I64 count = 5
if count > 0:
    puts( "positive" )
```

The condition `count > 0` produces a `Boolean` value. Writing `if count:` is a compilation error because `I64` is not `Boolean`.

---

## Boolean as Return Values

Functions can return `Boolean` to signal success, failure, or a predicate result:

```uranite
public function isPositive( I64 value ) -> Boolean:
    if value > 0:
        return True
    return False
```

---

## Logical Operators

Three keyword operators work on `Boolean` values:

| Operator | Description |
|---|---|
| `and` | Logical conjunction — `True` only when both operands are `True` |
| `or` | Logical disjunction — `True` when at least one operand is `True` |
| `not` | Logical negation — inverts a `Boolean` value |

```uranite
Boolean both = True and False
Boolean either = True or False
Boolean inverted = not True
```

---

## Evaluation Behavior

Both `and` and `or` evaluate both operands unconditionally. There is no short-circuit evaluation. If the right operand has side effects, those side effects always execute regardless of the left operand's value.
