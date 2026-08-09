# Identifiers and Naming

Uranite enforces strict naming conventions at both the compiler and linter levels. Identifiers must follow specific character set rules, casing conventions, and minimum length requirements. This document specifies the lexical definition of identifiers, the naming rules enforced by the compiler and formatter, how identifiers interact with the scope system, and every edge case related to naming.

---

## Table of Contents

- [Identifiers and Naming](#identifiers-and-naming)
  - [Table of Contents](#table-of-contents)
  - [Lexical Definition](#lexical-definition)
    - [Character Set Rules](#character-set-rules)
    - [Start Characters](#start-characters)
    - [Continuation Characters](#continuation-characters)
    - [Maximum Length](#maximum-length)
    - [Unicode and Non-ASCII Characters](#unicode-and-non-ascii-characters)
  - [The Underscore Policy](#the-underscore-policy)
    - [Compiler Acceptance](#compiler-acceptance)
    - [Style Rejection](#style-rejection)
    - [Why Underscores Are Forbidden](#why-underscores-are-forbidden)
    - [The One Exception: Constants](#the-one-exception-constants)
  - [Enforced Naming Conventions](#enforced-naming-conventions)
    - [camelCase: Variables, Functions, Methods, Parameters](#camelcase-variables-functions-methods-parameters)
    - [PascalCase: Classes, Structs, Interfaces, Traits, Enums](#pascalcase-classes-structs-interfaces-traits-enums)
    - [Constants: Flexible Naming](#constants-flexible-naming)
    - [kebab-case: Packages, Files, Directories](#kebab-case-packages-files-directories)
    - [Convention Summary Table](#convention-summary-table)
  - [Minimum Name Length Enforcement](#minimum-name-length-enforcement)
    - [The Cryptic Name Rule](#the-cryptic-name-rule)
    - [Allowed Short Names](#allowed-short-names)
    - [Configuring the Minimum Length](#configuring-the-minimum-length)
  - [Identifier Resolution and Scoping](#identifier-resolution-and-scoping)
    - [Scope Hierarchy](#scope-hierarchy)
    - [Name Lookup](#name-lookup)
    - [Variable Shadowing](#variable-shadowing)
    - [Redefinition Errors](#redefinition-errors)
    - [Field Shadowing Warnings](#field-shadowing-warnings)
    - [Function Overloading](#function-overloading)
  - [Builtin Type Name Shadowing](#builtin-type-name-shadowing)
  - [Keywords vs Identifiers](#keywords-vs-identifiers)
  - [Examples](#examples)
    - [Valid Identifiers](#valid-identifiers)
    - [Invalid Identifiers](#invalid-identifiers)
    - [Naming Convention Violations](#naming-convention-violations)
    - [Corrected Versions](#corrected-versions)

---

## Lexical Definition

### Character Set Rules

Identifiers in Uranite are composed of two character classes: start characters and continuation characters. These rules determine what the compiler accepts as a valid identifier token.

### Start Characters

An identifier must begin with an ASCII letter (`a`-`z`, `A`-`Z`) or an underscore (`_`). Tokens that begin with a digit are interpreted as numeric literals, not identifiers. This means `counter` is an identifier, but `2counter` is a number followed by the separate identifier `counter`.

### Continuation Characters

After the start character, an identifier may contain any combination of:

- ASCII letters (`a`-`z`, `A`-`Z`)
- Digits (`0`-`9`)
- Underscores (`_`)

The identifier ends at the first character that does not match this set. Hyphens, dots, spaces, and all other non-alphanumeric characters terminate the identifier:

| Input | Identifier Produced | Remaining |
|---|---|---|
| `bufferSize` | `bufferSize` | (nothing) |
| `count123` | `count123` | (nothing) |
| `my-variable` | `my` | `-variable` (hyphen terminates) |
| `hello world` | `hello` | ` world` (space terminates) |
| `name.length` | `name` | `.length` (dot terminates) |
| `data[0]` | `data` | `[0]` (bracket terminates) |

This means hyphenated names like `my-variable` are never a single identifier. The compiler sees `my`, then a minus operator, then `variable` — three separate tokens.

### Maximum Length

There is no maximum length for identifiers. In practice, identifiers are limited only by available memory. Descriptive multi-word names like `calculateCompoundInterestOverMultiplePeriods` are valid (though excessively long names reduce readability rather than improve it).

### Unicode and Non-ASCII Characters

Uranite does **not** support non-ASCII characters in identifiers. Only the 26 Latin letters (upper and lower case), digits 0-9, and underscore are valid. Multi-byte UTF-8 characters — accented letters, CJK characters, Greek letters, emoji — are rejected as "unexpected character" errors when they appear in identifier positions:

```
error: unexpected character "ü"
  --> source.urn:1:5
```

Identifiers like `café`, `größe`, `変数`, and `π` are not valid. Non-ASCII characters are permitted inside string literals and comments, but never in identifiers.

---

## The Underscore Policy

### Compiler Acceptance

The compiler **accepts** underscores in identifiers at the syntactic level. Names like `my_variable`, `_private`, and `calculate_total` will compile without error. The underscore is a valid character in identifier positions.

### Style Rejection

Despite compiler acceptance, Uranite's coding conventions **strictly forbid** underscores in runtime identifiers. This prohibition is enforced by the linter (`uranite-fmt --lint`) and by the language's design philosophy.

Uranite uses `camelCase` for all runtime identifiers (variables, functions, methods, parameters) and `PascalCase` for type names. Underscores belong to `snake_case`, which is not a valid naming convention in Uranite. Code that uses underscores in variable or function names will compile, but it violates naming standards and will be flagged by the linter.

```uranite
I64 myCounter = 0
```

This is correct. The variable name `myCounter` follows `camelCase`.

```uranite
I64 my_counter = 0
```

This compiles but violates the naming convention. The linter flags it, and the correct form is `myCounter`.

### Why Underscores Are Forbidden

Uranite uses `kebab-case` for file names and package paths (`hash-map.urn`, `array-list.urn`). If identifiers also used underscores, there would be two competing word-separation conventions in the same codebase. By restricting runtime identifiers to `camelCase` and `PascalCase`, and reserving hyphens for filesystem paths, each naming convention occupies a distinct domain with zero ambiguity.

### The One Exception: Constants

Top-level constants declared with `const` are the **only** context where underscores are permitted in Uranite identifiers. Constants are not restricted to a single naming convention — they can use `camelCase`, `PascalCase`, or `UPPER_SNAKE_CASE` depending on context:

```uranite
public const I64 MAX_BUFFER_SIZE = 65536
public const I64 STATE_RUNNING = 1
public const I64 SIGTERM = 15
public const I64 argc = 0
public const NativeScheduler scheduler = new NativeScheduler()
```

`UPPER_SNAKE_CASE` is the most common convention for numeric constants, flags, state codes, and syscall numbers. It creates immediate visual distinction, signaling that the value is immutable. However, constants that represent singleton objects, well-known conventional names (like `argc` and `argv`), or module-level bindings often use `camelCase` instead. Both styles are valid.

---

## Enforced Naming Conventions

### camelCase: Variables, Functions, Methods, Parameters

All runtime identifiers — local variables, function names, method names, and parameter names — use `camelCase`. The first word is entirely lowercase; subsequent words begin with an uppercase letter, with no separator between words:

```uranite
public function calculateTotalPrice( I64 itemCount, Double unitPrice ) -> Double:
    Double subtotal = unitPrice * itemCount
    Double taxRate = 0.08
    Double totalPrice = subtotal * ( 1.0 + taxRate )
    return totalPrice
```

Every identifier follows `camelCase`: `calculateTotalPrice`, `itemCount`, `unitPrice`, `subtotal`, `taxRate`, `totalPrice`. None use underscores, hyphens, or leading capitals.

Acronyms in `camelCase` names follow a specific rule: treat the acronym as a regular word. Capitalize only the first letter when it appears after the start of the name:

| Correct | Incorrect | Reason |
|---|---|---|
| `httpClient` | `hTTPClient` | Acronym treated as word |
| `parseJson` | `parseJSON` | Acronym treated as word |
| `xmlParser` | `XMLParser` | camelCase, not PascalCase |
| `userId` | `userID` | Acronym treated as word |

### PascalCase: Classes, Structs, Interfaces, Traits, Enums

Type declarations use `PascalCase`. Every word begins with an uppercase letter, including the first word:

```uranite
public class EventDispatcher:
    pass

public interface Serializable:
    public function serialize( self ) -> String;

public struct ConnectionConfig:
    public String hostAddress
    public I32 portNumber

public enum HttpMethod:
    unit Get
    unit Post
    unit Put
    unit Delete
```

Type names are always `PascalCase`: `EventDispatcher`, `Serializable`, `ConnectionConfig`, `HttpMethod`. Enum variants also use `PascalCase`: `Get`, `Post`, `Put`, `Delete`.

Acronyms in `PascalCase` follow the same rule — treat the acronym as a word:

| Correct | Incorrect | Reason |
|---|---|---|
| `HttpClient` | `HTTPClient` | Acronym treated as word |
| `JsonParser` | `JSONParser` | Acronym treated as word |
| `XmlDocument` | `XMLDocument` | Acronym treated as word |
| `IoStream` | `IOStream` | Acronym treated as word |

### Constants: Flexible Naming

Constants declared with `const` can use any valid naming convention. The `const` keyword marks a binding as immutable — it does not enforce a particular casing style.

**`UPPER_SNAKE_CASE`** is the most common convention for numeric constants, flags, state codes, signal numbers, and syscall identifiers:

```uranite
public const I64 MAX_BUFFER_SIZE = 65536
public const I64 DEFAULT_PORT = 8080
public const I64 STATE_RUNNING = 1
public const I64 SIGTERM = 15
public const I64 PAGE_SIZE = 4096
```

**`camelCase`** is used for singleton objects, module-level bindings, and well-known conventional names:

```uranite
public const I64 argc = 0
public const ArrayList<String> argv = new ArrayList<String>()
const NativeScheduler scheduler = new NativeScheduler()
```

**Single-word constants** can be either all uppercase or lowercase depending on context:

```uranite
public const Double PI = 3.14159265358979
public const I64 EPSILON = 1
```

Constants are the only identifier category where underscores are permitted. The choice between `UPPER_SNAKE_CASE` and `camelCase` depends on the constant's purpose — numeric flags and configuration values favor uppercase, while object bindings and conventional names favor camelCase.

### kebab-case: Packages, Files, Directories

Package names, file names, and directory names use `kebab-case` — all lowercase with hyphens separating words:

```
stdlibs/
    collection/
        array-list.urn
        hash-map.urn
        hash-set.urn
        mutable-sequence.urn
    language/
        boolean.urn
        string.urn
    io/
        console.urn
        file.urn
```

Import statements reflect this convention. The dot-separated module path corresponds directly to the filesystem structure:

```uranite
from uranite.collection.array-list import ArrayList
from uranite.collection.hash-map import HashMap
from uranite.io.console import puts
```

The module path `uranite.collection.array-list` maps to the file `stdlibs/collection/array-list.urn`. The kebab-case file name becomes the final segment of the module path.

Single-word module names use plain lowercase with no hyphens:

```uranite
from uranite.io.console import puts
from uranite.math.random import nextInt
```

### Convention Summary Table

| Domain | Convention | Example | Underscores |
|---|---|---|---|
| Variables, functions, methods, parameters | `camelCase` | `bufferSize`, `computeHash` | Forbidden |
| Classes, structs, interfaces, traits, enums | `PascalCase` | `ArrayList`, `HashMap` | Forbidden |
| Enum variants | `PascalCase` | `North`, `HttpOk` | Forbidden |
| Constants (numeric/flags) | `UPPER_SNAKE_CASE` | `MAX_CAPACITY`, `STATE_RUNNING` | Permitted (as word separator) |
| Constants (objects/bindings) | `camelCase` | `scheduler`, `argc` | Forbidden |
| Files, directories | `kebab-case` | `hash-map.urn`, `array-list/` | Forbidden |
| Package paths | `kebab-case` (dots as separators) | `uranite.collection.hash-map` | Forbidden |

---

## Minimum Name Length Enforcement

### The Cryptic Name Rule

The linter enforces a minimum character length for variable and parameter names. Names shorter than the configured threshold (default: 3 characters) trigger a warning:

```
warning: [naming/cryptic-variable] variable "x" has a cryptic name; use a descriptive identifier
  --> source.urn:5:5
```

```
warning: [naming/cryptic-parameter] parameter "n" has a cryptic name; use a descriptive identifier
  --> source.urn:3:25
```

Two distinct linter rules govern this check:

| Rule ID | Target | Default State |
|---|---|---|
| `naming/cryptic-variable` | Local variable declarations | Enabled |
| `naming/cryptic-parameter` | Function parameter names | Enabled |

Both rules compare the identifier's character count against the minimum length threshold. Any identifier shorter than 3 characters triggers the warning.

The rationale: single-letter and two-letter names (`i`, `j`, `x`, `n`, `cb`, `fn`) communicate nothing about the value's purpose. Uranite prioritizes readability — every identifier should describe what the value represents:

| Cryptic | Descriptive | What It Represents |
|---|---|---|
| `i` | `index` or `slotIndex` | Position in a sequence |
| `n` | `count` or `elementCount` | Number of items |
| `s` | `source` or `inputText` | Input string data |
| `cb` | `callback` or `onComplete` | Function to call later |
| `buf` | `buffer` or `readBuffer` | Temporary storage area |
| `res` | `result` or `response` | Output of a computation |

### Allowed Short Names

Six identifiers are exempt from the minimum-length check, regardless of their character count:

| Name | Reason |
|---|---|
| `self` | Instance reference in methods. Always required in this exact form. |
| `it` | Conventional lambda parameter for single-argument closures. |
| `id` | Universally understood abbreviation for "identifier". |
| `io` | Standard abbreviation for input/output operations. |
| `ip` | Standard abbreviation for internet protocol address. |
| `ok` | Conventional boolean result name for success/failure. |

These six names are checked before the length rule applies. If an identifier matches any entry in this list, no warning is emitted:

```uranite
public function processUser( I64 id ) -> Boolean:
    Boolean ok = validateId( id )
    return ok
```

Both `id` and `ok` are 2 characters but exempt from the cryptic-name warning because they are in the allowed list.

### Configuring the Minimum Length

The minimum length threshold is configurable through the linter configuration:

| Setting | Default | Description |
|---|---|---|
| `enableCrypticVariable` | `true` | Enable or disable variable name length checking. |
| `enableCrypticParameter` | `true` | Enable or disable parameter name length checking. |
| `minimumVariableNameLength` | `3` | Minimum character count before a name is flagged. |

Setting `minimumVariableNameLength` to `1` effectively disables cryptic-name warnings (since all identifiers are at least 1 character long). Setting it to `5` or higher enforces very descriptive naming, requiring names like `index` instead of `idx`.

---

## Identifier Resolution and Scoping

### Scope Hierarchy

Uranite uses lexical scoping with a hierarchy of nested scopes. Each scope corresponds to a syntactic construct that introduces a new naming context:

| Scope | Created By | Can Access Names From |
|---|---|---|
| Global | Top-level module | (root — no parent) |
| Module | Imported module boundary | Global |
| Class | Class body | Module, Global |
| Function | Function body | Class (if method), Module, Global |
| Block | `if`, `elif`, `else`, `try`, `except`, `finally` body | Function, Class, Module, Global |
| Loop | `for`, `while` body | Function, Class, Module, Global |
| Switch | `switch` body | Function, Class, Module, Global |
| Unsafe | `unsafe` block | Function, Class, Module, Global |

Scopes form a parent-child chain. When the compiler enters a new block, it creates a new scope linked to the enclosing scope. When the block ends, the inner scope is discarded and the enclosing scope resumes. Every name lookup walks up the chain from the current scope toward the global scope.

### Name Lookup

When the compiler encounters an identifier, it resolves the name by searching the scope chain:

1. Search the current (innermost) scope for the name.
2. If not found, search the parent scope.
3. Continue up the chain until the name is found or the global scope is exhausted.
4. If no scope contains the name, the identifier is undeclared and the compiler reports an error.

This chain traversal means inner scopes can access names from any enclosing scope. A variable declared in a function body is visible in all nested blocks — loops, conditionals, unsafe blocks, and switch bodies — within that function:

```uranite
public function process( ArrayList<I64> values ) -> I64:
    I64 total = 0
    for I64 element in values:
        if element > 0:
            total = total + element
    return total
```

The variable `total` is declared in the function scope. The `for` loop body and the `if` body are nested scopes that can both access `total` because it exists in an enclosing scope.

### Variable Shadowing

A variable in an inner scope may have the same name as a variable in an outer scope. The inner variable **shadows** the outer one — within the inner scope, the name refers to the inner variable. After the inner scope ends, the outer variable is accessible again:

```uranite
public function example() -> Void:
    I64 value = 10
    if True:
        I64 value = 20
        puts( value.toString() )
    puts( value.toString() )
```

The first `puts` call prints "20" — the inner `value` shadows the outer one within the `if` block. The second `puts` call prints "10" — after the `if` block ends, the outer `value` is accessible again.

Shadowing is valid because the two declarations exist in different scopes. It is not a redefinition. However, shadowing can make code confusing and is best avoided when possible.

### Redefinition Errors

Declaring the same name twice within the **same** scope is an error:

```uranite
public function broken() -> Void:
    I64 counter = 0
    I64 counter = 1
```

```
error: redefinition of variable "counter"
  --> source.urn:3:5
```

Both declarations of `counter` exist in the same function scope, which is not allowed. This rule applies to variables, fields, parameters, types, enum variants, and modules — any two entities of these kinds cannot share a name within the same scope.

### Field Shadowing Warnings

When a class declares a field with the same name as an inherited field from a parent class, the compiler emits a warning (not an error):

```
warning: field "name" in class "Employee" shadows inherited field
  --> source.urn:8:5
```

```uranite
public class Person:
    protect String name

    public function Person( self, String name ) -> Void:
        self.name = name

public class Employee extends Person:
    private String name

    public function Employee( self, String name ) -> Void:
        parent( name )
        self.name = name
```

The `name` field in `Employee` shadows the inherited `name` field from `Person`. This is technically valid — the subclass field takes precedence within `Employee` — but it often indicates a design mistake. The developer may have intended to use the inherited field rather than declare a new one.

### Function Overloading

Functions are the exception to the single-definition rule. Multiple functions with the same name but different parameter type signatures can coexist in the same scope:

```uranite
public function format( I64 value ) -> String:
    return value.toString()

public function format( Double value ) -> String:
    return value.toString()

public function format( String value ) -> String:
    return value
```

All three `format` functions share the same name but have distinct parameter types (`I64`, `Double`, `String`). The compiler resolves calls based on the types of the arguments provided at the call site:

```uranite
public function main() -> I32:
    puts( format( 42 ) )
    puts( format( 3.14 ) )
    puts( format( "hello" ) )
    return 0
```

Each call resolves to the matching overload based on argument type. If two functions have the same name **and** the same parameter types, the second definition is rejected as a redefinition error.

---

## Builtin Type Name Shadowing

The compiler pre-registers a set of builtin type names (`I64`, `String`, `Boolean`, `Object`, `ArrayList`, etc.) during initialization. These names are identifiers, not keywords. This distinction means they can technically be shadowed by user declarations:

```uranite
public function confusing() -> Void:
    I64 String = 42
    puts( String.toString() )
```

This compiles. The local variable `String` (of type `I64`) shadows the builtin type name `String` within this function scope. After the function, the type name `String` resolves normally again.

While the compiler permits this, it is strongly discouraged. Shadowing a type name with a variable creates code that is difficult to read and maintain. The meaning of `String` shifts from "the string type" to "an integer variable" within the scope, which confuses both human readers and code analysis tools.

The key difference from keywords: you **cannot** shadow a keyword. Writing `I64 class = 5` is always an error because `class` is a reserved keyword. But writing `I64 String = 5` is technically valid because `String` is a builtin identifier, not a keyword. See [Reserved Keywords](reserved-keywords.md) for the full list of reserved words.

---

## Keywords vs Identifiers

The boundary between keywords and identifiers is absolute. When the compiler encounters a word:

- If it exactly matches one of the 78 reserved keywords, the word is a keyword. No user-defined entity can have that name.
- If it does not match any keyword, the word is an identifier. Further resolution happens during semantic analysis.

This boundary is based purely on string matching, not context. The word `function` is always a keyword, even in positions where an identifier might seem to make sense. The word `main` is always an identifier, even though the compiler treats a function named `main` specially as the program entry point.

```uranite
I64 function = 5
```

This is an error. `function` is a keyword and cannot be used as a variable name.

```uranite
public function main() -> I32:
    return 0
```

The name `main` is an identifier. The compiler recognizes it as the entry point because of a specific check during analysis — not because `main` is a keyword. You could declare a variable named `main` in a different scope without error (though it would shadow the entry point function if done carelessly).

Type names like `I64`, `String`, `ArrayList`, `HashMap`, `Boolean`, and `Object` are all identifiers, not keywords. They are resolved during semantic analysis through the type registry. See [Builtin Type Name Shadowing](#builtin-type-name-shadowing) for how this affects naming.

---

## Examples

### Valid Identifiers

A complete example demonstrating all naming conventions working together:

```uranite
package myapp.services

from uranite.collection.array-list import ArrayList

public const I64 MAX_RETRIES = 3
public const I64 TIMEOUT_MILLIS = 5000

public class HttpClient:

    private String baseUrl
    private I64 timeoutMillis

    public function HttpClient( self, String baseUrl, I64 timeoutMillis ) -> Void:
        self.baseUrl = baseUrl
        self.timeoutMillis = timeoutMillis

    public function sendRequest( self, String endpoint, String method ) -> String:
        String fullUrl = self.baseUrl + endpoint
        return fullUrl

    public function retryRequest( self, String endpoint, I64 maxAttempts ) -> String:
        I64 attempt = 0
        while attempt < maxAttempts:
            String result = self.sendRequest( endpoint, "GET" )
            if result.length() > 0:
                return result
            attempt = attempt + 1
        return ""
```

Convention breakdown:

- `camelCase`: `baseUrl`, `timeoutMillis`, `sendRequest`, `endpoint`, `method`, `fullUrl`, `retryRequest`, `maxAttempts`, `attempt`, `result`
- `PascalCase`: `HttpClient`, `String`, `ArrayList`, `I64`
- `UPPER_SNAKE_CASE`: `MAX_RETRIES`, `TIMEOUT_MILLIS`
- `kebab-case`: `myapp.services`, `array-list`

### Invalid Identifiers

The following are **rejected by the compiler** — they are not valid identifier tokens:

| Input | Problem |
|---|---|
| `123abc` | Starts with a digit. Parsed as a number, not an identifier. |
| `my-variable` | Hyphen terminates at `my`. Parsed as identifier `my`, minus operator, identifier `variable`. |
| `hello world` | Space terminates at `hello`. Parsed as two separate identifiers. |
| `über` | Non-ASCII character. Rejected as "unexpected character". |
| `café` | Non-ASCII character. Rejected as "unexpected character". |

### Naming Convention Violations

The following are **accepted by the compiler** but **flagged by the linter** or rejected by code review:

Single-letter variable name (triggers `naming/cryptic-variable`):

```uranite
I64 x = 10
```

The name `x` is 1 character, below the minimum of 3. Use a descriptive name like `counter`, `position`, or `offset`.

Single-letter parameter name (triggers `naming/cryptic-parameter`):

```uranite
public function calculate( I64 n ) -> I64:
    return n * 2
```

The name `n` is 1 character. Use a descriptive name like `factor`, `count`, or `multiplier`.

Underscores in a variable name (violates `camelCase`):

```uranite
I64 my_counter = 0
```

Should be `myCounter`.

Lowercase class name (violates `PascalCase`):

```uranite
public class event_handler:
    pass
```

Should be `EventHandler`.

PascalCase function name (violates `camelCase`):

```uranite
public function ComputeValue() -> I64:
    return 42
```

Function names use `camelCase`, not `PascalCase`. Should be `computeValue`.

### Corrected Versions

All naming convention violations from the previous section, corrected:

```uranite
I64 counter = 10

public function calculate( I64 factor ) -> I64:
    return factor * 2

I64 myCounter = 0

public class EventHandler:
    pass

public function computeValue() -> I64:
    return 42
```
