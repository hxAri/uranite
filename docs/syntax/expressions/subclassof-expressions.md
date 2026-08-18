# Subclassof Expressions

The `subclassof` operator checks whether one type inherits from another, operating on types rather than values.

---

## Table of Contents

- [Subclassof Expressions](#subclassof-expressions)
  - [Table of Contents](#table-of-contents)
  - [Syntax](#syntax)
  - [Current Limitation](#current-limitation)
  - [Operator Precedence](#operator-precedence)

---

## Syntax

```
Type subclassof ParentType
```

Both sides are type names, not value expressions. The intended result is a `Boolean` — `True` if the left type inherits from the right type, `False` otherwise.

```uranite
if Dog subclassof Animal:
    puts( "Dog inherits from Animal" )
```

Unlike `instanceof`, which requires a value on the left side, `subclassof` operates purely on types. This is designed for checking inheritance relationships without needing to construct an instance.

---

## Current Limitation

`subclassof` compiles without error but does not produce a usable result at runtime. Using it in a condition leads to undefined behavior where neither the true nor false branch may execute as expected.

Use `instanceof` with a value instead until this is resolved:

```uranite
Dog dog = new Dog()
if dog instanceof Animal:
    puts( "Dog inherits from Animal" )
```

---

## Operator Precedence

`subclassof` shares precedence level 7 with comparison operators and `instanceof`:

| Precedence | Operators |
|---|---|
| 12 (highest) | `as` |
| 11 | `**` |
| 10 | `*`, `/`, `%` |
| 9 | `+`, `-` |
| 8 | `<<`, `>>` |
| 7 | `<`, `<=`, `>`, `>=`, `in`, `instanceof`, `is`, `subclassof` |
| 6 | `==`, `!=` |
| 5 | `&` |
| 4 | `^` |
| 3 | `\|` |
| 2 | `and` |
| 1 (lowest) | `or` |
