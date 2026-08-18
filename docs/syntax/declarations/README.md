# Declarations

Declarations introduce named entities into a program: variables, constants, functions, classes, structs, enums, interfaces, traits, and generic type parameters. Every declaration has a name, a type, and a visibility level that determines where it can be accessed.

---

## Table of Contents

- [Declarations](#declarations)
  - [Table of Contents](#table-of-contents)
  - [Overview](#overview)
  - [At a Glance](#at-a-glance)
  - [Subpage Index](#subpage-index)

---

## Overview

Declarations fall into three groups:

**Bindings** associate a name with a value or a constant. Variable declarations (`I64 count = 0`) create mutable local bindings. Constant declarations (`const I64 MAX_SIZE = 512`) create immutable top-level values evaluated at compile time.

**Functions** define callable units of code. Function declarations specify parameters with explicit types, a return type, and an indented body. Uranite supports default parameter values, variadic parameters (`Type args{}`), keyword parameters (`Type kwargs{}`), nested functions, and `extern` declarations for foreign linkage.

**Type definitions** introduce new types. Classes define heap-allocated objects with fields, methods, constructors, and single inheritance via `extends`. Structs define value types with fields. Enums define closed sets of named variants with optional backing values. Interfaces define method contracts that classes implement. Traits provide reusable method implementations mixed into classes with `use`. Generic type parameters (`<T>`) and `where` clause constraints enable parametric polymorphism across all type definition forms.

---

## At a Glance

```uranite
public const I64 MAX_SIZE = 512

public interface Describable:
    public function describe( self ) -> String;

public class Item implements Describable:

    public String label

    public function Item( self, String label ) -> Void:
        self.label = label

    public function describe( self ) -> String:
        return self.label

public enum Color:
    unit Red
    unit Green
    unit Blue

public function greet( String name ) -> String:
    return "hello " + name
```

---

## Subpage Index

| Document | Description |
|---|---|
| [Variable](variable.md) | Typed variable declarations (`Type name = value`), scope rules, mutability with `mut`, and the `volatile` modifier. |
| [Constants](constants.md) | Top-level `const` declarations for compile-time constant values. |
| [Functions](functions.md) | Function declarations, parameters, return types, default values, variadic and keyword parameters, nested functions, and `extern` declarations. |
| [Classes](classes.md) | Class declarations with fields, methods, constructors, visibility modifiers, static members, properties, `readonly`, `final`, `native`, and `virtual` dispatch. |
| [Class Inheritance](class-inheritance.md) | Single inheritance with `extends`, `parent()` constructor delegation, method overriding with `override`, and polymorphic dispatch. |
| [Abstract Classes](abstract-classes.md) | `abstract class` with `abstract function` signatures and concrete methods. |
| [Structs](structs.md) | Value-type struct declarations with fields and value semantics. |
| [Enums](enums.md) | Enum declarations with `unit` variants, `backed` enums with explicit discriminant values, and enum methods. |
| [Interfaces](interfaces.md) | Interface declarations defining method contracts, `implements` on classes, and multiple interface implementation. |
| [Traits](traits.md) | Trait declarations with default method implementations and `use` for mixing into classes. |
| [Generics](generics.md) | Generic type parameters, `where` clause constraints, diamond inference, and generic functions. |
