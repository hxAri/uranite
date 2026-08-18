# None Literal

`None` is a reserved keyword literal representing the explicit absence of a value. It is the only value assignable to optional types (`?T`). `None` cannot be assigned to non-optional types.

---

## Table of Contents

- [None Literal](#none-literal)
  - [Table of Contents](#table-of-contents)
  - [Syntax](#syntax)
  - [Optional Types](#optional-types)
  - [Returning None](#returning-none)
  - [None Comparison](#none-comparison)

---

## Syntax

`None` is PascalCase and case-sensitive. Lowercase `none` is not a valid keyword — the compiler treats it as an ordinary identifier.

```uranite
?String missing = None
```

---

## Optional Types

A variable must have an optional type to hold `None`. The `?T` prefix syntax declares an optional type:

```uranite
?String maybeName = None
?I64 maybeCount = None
?ArrayList<String> maybeList = None
```

Assigning `None` to a non-optional type is a compile-time error:

```uranite
String name = None
```

This fails because `String` is not optional. Use `?String` to allow `None`.

---

## Returning None

Functions returning `?T` can return either a value of type `T` or `None`:

```uranite
public function findUser( String name ) -> ?String:
    if name == "admin":
        return "Administrator"
    return None
```

---

## None Comparison

Use `==` and `!=` to compare against `None`:

```uranite
?String result = findUser( "admin" )
if result != None:
    puts( result )
```

The `is` keyword tests identity and also works with `None`:

```uranite
if result is None:
    puts( "not found" )
```

Both `== None` and `is None` are valid. The `is` keyword tests reference identity, while `==` tests value equality. For `None` checks, both produce the same result.
