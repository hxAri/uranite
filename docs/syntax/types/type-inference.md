# Type Inference

---

## Table of Contents

- [Type Inference](#type-inference)
  - [Table of Contents](#table-of-contents)
  - [Overview](#overview)
  - [Literal Type Resolution](#literal-type-resolution)
    - [Integer Literals](#integer-literals)
    - [Float Literals](#float-literals)
    - [String Literals](#string-literals)
    - [Boolean Literals](#boolean-literals)
    - [None Literal](#none-literal)
  - [Explicit Type Annotations](#explicit-type-annotations)
    - [Variable Declarations](#variable-declarations)
    - [Function Parameters](#function-parameters)
    - [Return Types](#return-types)
  - [Diamond Constructor Inference](#diamond-constructor-inference)
  - [For-In Loop Variable Typing](#for-in-loop-variable-typing)
    - [Range Iteration](#range-iteration)
    - [Generator Iteration](#generator-iteration)
  - [Lambda Return Type Inference](#lambda-return-type-inference)
    - [Arithmetic Lambdas](#arithmetic-lambdas)
    - [Boolean Lambdas](#boolean-lambdas)
    - [String Lambdas](#string-lambdas)
  - [Method Reference](#method-reference)
    - [Literal Type Rules](#literal-type-rules)
    - [Inference Contexts](#inference-contexts)
  - [Examples](#examples)
    - [Literal Type Resolution Showcase](#literal-type-resolution-showcase)
    - [Diamond Constructor Inference Showcase](#diamond-constructor-inference-showcase)
    - [For-In Loop Typing](#for-in-loop-typing)
    - [Lambda Return Type Inference Showcase](#lambda-return-type-inference-showcase)

---

## Overview

Uranite requires explicit type annotations on all variable declarations and function signatures. There is no `auto` or `var` keyword — every variable must name its type.

Type inference operates in three narrow contexts:

1. **Literal type resolution** — the compiler determines the type of each literal from its form (`42` is `I64`, `3.14` is `F64`, `"hello"` is `String`)
2. **Diamond constructor inference** — `new ArrayList<>()` infers the generic argument from the variable's declared type
3. **Lambda return type inference** — a lambda's return type is inferred from its body expression

Outside these contexts, all types must be stated explicitly.

---

## Literal Type Resolution

Each literal form maps to a specific type. No annotation is needed on the literal itself — the compiler determines the type from the syntax.

### Integer Literals

Integer literals produce `I64` values.

```uranite
I64 count = 42
I64 negative = -10
I64 zero = 0
```

The literals `42`, `-10`, and `0` are all `I64`.

### Float Literals

Float literals with a decimal point produce `F64` values.

```uranite
F64 ratio = 0.75
F64 temperature = 36.6
F64 small = 0.001
```

The decimal point distinguishes float literals from integer literals. `42` is `I64` while `42.0` is `F64`.

### String Literals

Double-quoted text produces `String` values.

```uranite
String greeting = "hello"
String empty = ""
String sentence = "the quick brown fox"
```

### Boolean Literals

`True` and `False` produce `Boolean` values.

```uranite
Boolean active = True
Boolean deleted = False
```

### None Literal

`None` represents the absence of a value. It is used with optional types.

```uranite
?String missing = None
?I64 empty = None
```

`None` is compatible with any optional type.

---

## Explicit Type Annotations

### Variable Declarations

Every variable declaration must include the type name before the variable name.

```uranite
from uranite.io.console import puts

public function main() -> I32:
    I64 count = 42
    String label = "items"
    Boolean active = True
    F64 ratio = 0.75
    puts( count.toString() )
    puts( label )
    return 0
```

Output:

```
42
items
```

There is no way to omit the type. Writing `count = 42` without a type produces a compile-time error.

### Function Parameters

Function parameters require explicit type annotations.

```uranite
public function add( I64 first, I64 second ) -> I64:
    return first + second
```

Each parameter names its type. The compiler does not infer parameter types.

### Return Types

Function return types are declared with the `->` arrow syntax.

```uranite
public function getName() -> String:
    return "Alice"
```

Every function must declare its return type. Omitting the return type is a syntax error.

---

## Diamond Constructor Inference

When constructing a generic class, the type arguments can be omitted using the diamond syntax `<>`. The compiler infers the generic arguments from the variable's declared type.

```uranite
from uranite.io.console import puts
from uranite.collection.array-list import ArrayList

public function main() -> I32:
    ArrayList<String> names = new ArrayList<>()
    names.add( "Alice" )
    names.add( "Bob" )
    puts( names.size().toString() )
    return 0
```

Output:

```
2
```

The variable is declared as `ArrayList<String>`, so `new ArrayList<>()` infers `String` as the generic argument. This is equivalent to `new ArrayList<String>()`.

The diamond syntax works because the left-hand side provides the complete type information. Both forms produce identical results:

```uranite
ArrayList<I64> numbers = new ArrayList<>()
ArrayList<I64> numbers = new ArrayList<I64>()
```

---

## For-In Loop Variable Typing

For-in loop variables require explicit type annotations to ensure method access works correctly.

### Range Iteration

```uranite
from uranite.io.console import puts

public function main() -> I32:
    for I64 index in 0..5:
        puts( index.toString() )
    return 0
```

Output:

```
0
1
2
3
4
```

The type annotation `I64` before `index` ensures the variable has full access to `I64` methods like `toString()`.

### Generator Iteration

```uranite
from uranite.io.console import puts

public function numbers() -> Generator<I64>:
    yield 10
    yield 20
    yield 30

public function main() -> I32:
    for I64 value in numbers():
        puts( value.toString() )
    return 0
```

Output:

```
10
20
30
```

The loop variable type `I64` matches the generator's yield type `Generator<I64>`.

---

## Lambda Return Type Inference

Lambdas do not declare a return type — the compiler infers it from the body expression.

### Arithmetic Lambdas

```uranite
from uranite.io.console import puts

public function main() -> I32:
    Callable<I64, <I64>> doubler = lambda I64 value: value * 2
    I64 result = doubler( 21 )
    puts( result.toString() )
    return 0
```

Output:

```
42
```

The body `value * 2` produces an `I64`, so the lambda's return type is inferred as `I64`. The `Callable<I64, <I64>>` type on the variable confirms the return type.

### Boolean Lambdas

```uranite
from uranite.io.console import puts

public function main() -> I32:
    Callable<Boolean, <I64>> isEven = lambda I64 value: value % 2 == 0
    Boolean check = isEven( 42 )
    puts( check.toString() )
    return 0
```

Output:

```
True
```

The body `value % 2 == 0` is a comparison producing `Boolean`, so the return type is inferred as `Boolean`.

### String Lambdas

```uranite
from uranite.io.console import puts

public function main() -> I32:
    Callable<String, <String>> greeter = lambda String name: "Hello " + name
    String result = greeter( "World" )
    puts( result )
    return 0
```

Output:

```
Hello World
```

The body `"Hello " + name` is a string concatenation producing `String`, so the return type is inferred as `String`.

---

## Method Reference

### Literal Type Rules

| Literal Form | Resolved Type |
|---|---|
| `42`, `-10`, `0` | `I64` |
| `3.14`, `0.5` | `F64` |
| `"hello"`, `""` | `String` |
| `'a'` | `Char` |
| `True`, `False` | `Boolean` |
| `None` | Compatible with any `?T` |

### Inference Contexts

| Context | How It Works |
|---|---|
| Literal in variable | Type determined by literal form |
| Diamond constructor `<>` | Generic argument inferred from declared variable type |
| For-in loop variable | Must use explicit type annotation for method access |
| Lambda body | Return type inferred from body expression |
| Function parameter | Must be explicitly annotated |
| Function return type | Must be explicitly declared |

---

## Examples

### Literal Type Resolution Showcase

```uranite
from uranite.io.console import puts

public function main() -> I32:
    I64 count = 42
    String label = "items"
    Boolean active = True
    F64 ratio = 0.75

    puts( count.toString() )
    puts( label )
    puts( active.toString() )
    puts( ratio.toString() )
    return 0
```

Output:

```
42
items
True
0.75
```

### Diamond Constructor Inference Showcase

```uranite
from uranite.io.console import puts
from uranite.collection.array-list import ArrayList

public function main() -> I32:
    ArrayList<String> names = new ArrayList<>()
    names.add( "Alice" )
    names.add( "Bob" )
    names.add( "Charlie" )

    puts( names.size().toString() )

    ArrayList<I64> scores = new ArrayList<>()
    scores.add( 100 )
    scores.add( 85 )

    puts( scores.size().toString() )
    return 0
```

Output:

```
3
2
```

### For-In Loop Typing

```uranite
from uranite.io.console import puts

public function fibonacci() -> Generator<I64>:
    I64 alpha = 0
    I64 beta = 1
    yield alpha
    yield beta
    I64 next1 = alpha + beta
    yield next1
    I64 next2 = beta + next1
    yield next2
    I64 next3 = next1 + next2
    yield next3

public function main() -> I32:
    for I64 value in fibonacci():
        puts( value.toString() )

    for I64 index in 0..5:
        puts( index.toString() )
    return 0
```

Output:

```
0
1
1
2
3
0
1
2
3
4
```

### Lambda Return Type Inference Showcase

```uranite
from uranite.io.console import puts

public function applyTransform( Callable<I64, <I64>> transform, I64 value ) -> I64:
    return transform( value )

public function main() -> I32:
    Callable<I64, <I64>> doubler = lambda I64 value: value * 2
    Callable<I64, <I64>> tripler = lambda I64 value: value * 3
    Callable<Boolean, <I64>> isEven = lambda I64 value: value % 2 == 0

    I64 doubled = applyTransform( doubler, 15 )
    puts( doubled.toString() )

    I64 tripled = applyTransform( tripler, 10 )
    puts( tripled.toString() )

    Boolean evenCheck = isEven( 42 )
    puts( evenCheck.toString() )

    Boolean oddCheck = isEven( 7 )
    puts( oddCheck.toString() )
    return 0
```

Output:

```
30
30
True
False
```
