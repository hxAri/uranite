# Identifiers and Naming

Uranite enforces strict naming conventions at both the lexer and linter levels. Identifiers must follow specific character set rules, minimum length requirements, and casing conventions. This document specifies the exact lexical definition of identifiers, the naming rules enforced by the compiler and formatter, and how identifiers interact with the scope system.

---

## Table of Contents

- [Lexical Definition](#lexical-definition)
  - [Character Set Rules](#character-set-rules)
  - [Start Characters](#start-characters)
  - [Continuation Characters](#continuation-characters)
  - [Maximum Length](#maximum-length)
  - [Unicode and Non-ASCII Characters](#unicode-and-non-ascii-characters)
- [The Underscore Policy](#the-underscore-policy)
  - [Lexer Acceptance](#lexer-acceptance)
  - [Linter and Style Rejection](#linter-and-style-rejection)
  - [Why Underscores Are Forbidden](#why-underscores-are-forbidden)
- [Enforced Naming Conventions](#enforced-naming-conventions)
  - [camelCase: Variables, Functions, Methods, Parameters](#camelcase-variables-functions-methods-parameters)
  - [PascalCase: Classes, Structs, Interfaces, Traits, Enums](#pascalcase-classes-structs-interfaces-traits-enums)
  - [UPPER_SNAKE_CASE: Constants](#upper_snake_case-constants)
  - [kebab-case: Packages, Files, Directories](#kebab-case-packages-files-directories)
- [Minimum Name Length Enforcement](#minimum-name-length-enforcement)
  - [The Cryptic Name Rule](#the-cryptic-name-rule)
  - [Allowed Short Names](#allowed-short-names)
  - [Configuration](#configuration)
- [Identifier Resolution and Scoping](#identifier-resolution-and-scoping)
  - [Scope Hierarchy](#scope-hierarchy)
  - [Symbol Lookup](#symbol-lookup)
  - [Redefinition Errors](#redefinition-errors)
  - [Field Shadowing Warnings](#field-shadowing-warnings)
  - [Function Overloading](#function-overloading)
- [Builtin Identifier Shadowing](#builtin-identifier-shadowing)
- [Keyword vs Identifier Boundary](#keyword-vs-identifier-boundary)
- [Examples](#examples)
  - [Valid Identifiers](#valid-identifiers)
  - [Invalid Identifiers](#invalid-identifiers)
  - [Naming Convention Violations](#naming-convention-violations)

---

## Lexical Definition

### Character Set Rules

The lexer recognizes identifiers using two character classes: start characters and continuation characters.

### Start Characters

An identifier must begin with an ASCII letter (`a`-`z`, `A`-`Z`) or an underscore (`_`). The dispatch check in the main tokenization loop is:

```
if character is alphabetic or character is '_':
    call readIdentifierOrKeyword()
```

Digits cannot start an identifier. A token beginning with a digit is dispatched to `readNumber()` instead, which parses numeric literals.

### Continuation Characters

After the start character, an identifier may contain any combination of ASCII letters, digits (`0`-`9`), and underscores. The `readIdentifierOrKeyword()` method accumulates characters using this rule:

```
while not at end and (current is alphanumeric or current is '_'):
    append current to identifierValue
    advance()
```

The accumulation stops at the first character that is not alphanumeric and not an underscore. This means hyphens, dots, spaces, and all other non-alphanumeric characters are identifier terminators.

### Maximum Length

There is no explicit maximum length for identifiers. The lexer accumulates characters into a `std::string` with no bounds check. In practice, identifiers are limited by available memory, which is effectively unlimited for reasonable identifier lengths.

### Unicode and Non-ASCII Characters

Uranite does **not** support non-ASCII characters in identifiers. The lexer's start-character check uses `std::isalpha()`, which operates on single bytes and recognizes only the ASCII range. Multi-byte UTF-8 sequences (accented characters, CJK characters, emoji) fail the `std::isalpha()` test and are rejected as "unexpected character" errors.

This means identifiers like `café`, `größe`, `変数`, and `π` are not valid. All identifiers must use the 26 Latin letters (upper and lower), digits 0-9, and underscore.

---

## The Underscore Policy

### Lexer Acceptance

The lexer **accepts** underscores in identifiers. Both the start-character dispatch (`character == '_'`) and the continuation loop (`this->current() == '_'`) explicitly include underscore. The lexer will successfully tokenize `my_variable`, `_private`, and `__dunder__` as `Identifier` tokens without error.

### Linter and Style Rejection

Despite lexer acceptance, Uranite's coding conventions **strictly forbid** underscores in identifiers. This prohibition is enforced at the style level by `uranite-fmt --lint` and by the language's design philosophy, not by the lexer itself.

The reasoning is simple: Uranite uses `camelCase` for all runtime identifiers and `PascalCase` for type names. Underscores belong to `snake_case`, which is not a valid naming convention in Uranite. The only exception is `UPPER_SNAKE_CASE` for top-level constants, where underscores separate words in all-caps identifiers.

Code that uses underscores in variable or function names will compile, but it violates Uranite's naming standards and will be flagged during code review. The `uranite-fmt` formatter does not automatically rename identifiers (it formats structure, not names), but the linter's cryptic-name rules and the project's style guide enforce descriptive `camelCase` names.

### Why Underscores Are Forbidden

Uranite uses `kebab-case` for file names and package paths (`hash-map.urn`, `array-list.urn`). If identifiers also used underscores, there would be two competing word-separation conventions in the same codebase. By restricting runtime identifiers to `camelCase` and `PascalCase`, and reserving hyphens for filesystem paths, each naming convention occupies a distinct domain with no ambiguity:

| Domain | Convention | Example |
|---|---|---|
| Variables, functions, methods | `camelCase` | `bufferSize`, `computeHash` |
| Types (classes, structs, interfaces) | `PascalCase` | `ArrayList`, `HashMap` |
| Constants | `UPPER_SNAKE_CASE` | `MAX_CAPACITY`, `DEFAULT_PORT` |
| Files, directories, packages | `kebab-case` | `hash-map.urn`, `array-list` |

---

## Enforced Naming Conventions

### camelCase: Variables, Functions, Methods, Parameters

All runtime identifiers — local variables, function names, method names, and parameter names — use `camelCase`. The first word is lowercase; subsequent words are capitalized with no separator:

```uranite
public function calculateTotalPrice( I64 itemCount, F64 unitPrice ) -> F64:
    F64 subtotal = unitPrice * itemCount
    F64 taxRate = 0.08
    F64 totalPrice = subtotal * ( 1.0 + taxRate )
    return totalPrice
```

Every identifier here follows `camelCase`: `calculateTotalPrice`, `itemCount`, `unitPrice`, `subtotal`, `taxRate`, `totalPrice`.

### PascalCase: Classes, Structs, Interfaces, Traits, Enums

Type declarations use `PascalCase`. Every word is capitalized, including the first:

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

### UPPER_SNAKE_CASE: Constants

Top-level constants declared with `const` use `UPPER_SNAKE_CASE`. All letters are uppercase, and words are separated by underscores:

```uranite
public const I64 MAX_BUFFER_SIZE = 65536
public const F64 PI = 3.14159265358979
public const I32 DEFAULT_PORT = 8080
public const String VERSION = "1.0.0"
```

This is the **only** context where underscores appear in Uranite identifiers. Constants are visually distinct from all other identifiers, making it immediately clear that `MAX_BUFFER_SIZE` is an immutable compile-time value, not a variable.

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
```

Import statements reflect this convention:

```uranite
from uranite.collection.array-list import ArrayList
from uranite.collection.hash-map import HashMap
from uranite.io.console import puts
```

Note that the dot-separated module path (`uranite.collection.array-list`) corresponds directly to the filesystem path (`stdlibs/collection/array-list.urn`). The kebab-case file name becomes the final segment of the module path.

---

## Minimum Name Length Enforcement

### The Cryptic Name Rule

The linter enforces a minimum character length for variable and parameter identifiers. Names shorter than the configured threshold trigger a warning diagnostic:

```
warning: [naming/cryptic-variable] variable "x" has a cryptic name; use a descriptive identifier
  --> source.urn:5:5
```

```
warning: [naming/cryptic-parameter] parameter "n" has a cryptic name; use a descriptive identifier
  --> source.urn:3:25
```

Two distinct rule IDs govern this check:

| Rule ID | Target | Default |
|---|---|---|
| `naming/cryptic-variable` | Local variable declarations | Enabled |
| `naming/cryptic-parameter` | Function parameter names | Enabled |

Both rules compare the identifier's character length against `minimumVariableNameLength` (default: 3). Any identifier shorter than 3 characters triggers the warning.

### Allowed Short Names

Six identifiers are exempt from the minimum-length check, regardless of their character count:

| Name | Reason |
|---|---|
| `self` | Instance reference in methods. Always valid. |
| `it` | Conventional lambda parameter for single-argument closures. |
| `id` | Universally understood abbreviation for "identifier". |
| `io` | Standard abbreviation for input/output operations. |
| `ip` | Standard abbreviation for internet protocol address. |
| `ok` | Conventional boolean result name. |

These six names are stored in the `allowedShortNames_` set, which is checked before applying the length rule. If the identifier matches any entry in this set, the check is skipped entirely — no warning is emitted.

### Configuration

The minimum length threshold is configurable via the `LintRuleConfig` struct:

| Field | Type | Default | Description |
|---|---|---|---|
| `enableCrypticVariable` | `bool` | `true` | Enable/disable variable name length checking. |
| `enableCrypticParameter` | `bool` | `true` | Enable/disable parameter name length checking. |
| `minimumVariableNameLength` | `uint32_t` | `3` | Minimum character count before a name is flagged. |

Setting `minimumVariableNameLength` to `1` effectively disables cryptic-name warnings (since all identifiers are at least 1 character). Setting it to `5` or higher enforces very descriptive naming.

---

## Identifier Resolution and Scoping

### Scope Hierarchy

The semantic analyzer manages a tree of nested scopes. Each scope has a kind and an optional parent:

| Scope Kind | Created By |
|---|---|
| `Global` | Top-level module scope. |
| `Module` | Imported module boundary. |
| `Class` | Class body. |
| `Function` | Function body. |
| `Block` | Generic nested block (e.g., `if` body). |
| `Loop` | `for` or `while` body. |
| `Switch` | `match`/`switch` body. |
| `Unsafe` | `unsafe` block. |

Scopes form a parent-child chain. When the analyzer enters a new block, it pushes a new scope with the current scope as parent. When the block ends, it pops back to the parent.

### Symbol Lookup

When the semantic analyzer encounters an identifier, it resolves it through the scope chain using `Scope::lookup()`:

1. Search the current scope's symbol map for the name.
2. If not found, recurse into the parent scope.
3. Continue up the chain until the name is found or the root (Global) scope is reached.
4. If no scope contains the name, resolution fails — the identifier is undeclared.

This parent-chain traversal means inner scopes can access names from any enclosing scope. A variable declared in a function body is visible in all nested blocks (loops, conditionals, unsafe blocks) within that function.

### Redefinition Errors

The `Scope::define()` method prevents duplicate definitions of variables, fields, parameters, types, enum variants, and modules within the same scope. If a name already exists in the current scope for any of these symbol kinds, `define()` returns `false`, and the semantic analyzer emits an error:

```
error: redefinition of variable "counter"
  --> source.urn:6:5
```

This check is **scope-local** — it only prevents duplicates within the same scope. A variable in an inner scope may have the same name as a variable in an outer scope. The inner variable shadows the outer one:

```uranite
public function example() -> Void:
    I64 value = 10
    if True:
        I64 value = 20
        puts( value.toString() )
    puts( value.toString() )
```

The inner `value` (20) shadows the outer `value` (10) within the `if` block. After the block ends, the outer `value` is accessible again. This is valid — it is not a redefinition because the two declarations are in different scopes.

However, declaring the same name twice within the same scope is an error:

```uranite
public function broken() -> Void:
    I64 counter = 0
    I64 counter = 1
```

The second `I64 counter` triggers "redefinition of variable" because both declarations exist in the same function scope.

### Field Shadowing Warnings

When a class declares a field with the same name as an inherited field from a parent class, the semantic analyzer emits a warning (not an error):

```
warning: field "name" in class "Employee" shadows inherited field
  --> source.urn:8:5
```

This is a warning because field shadowing is technically valid — the subclass field takes precedence within the subclass — but it often indicates a design mistake where the developer intended to use the inherited field rather than declare a new one.

### Function Overloading

Functions are the exception to the single-definition rule. Multiple functions with the same name but different parameter type signatures can coexist in the same scope. The `define()` method detects this by comparing parameter counts and types:

```uranite
public function format( I64 value ) -> String:
    return value.toString()

public function format( F64 value ) -> String:
    return value.toString()

public function format( String value ) -> String:
    return value
```

All three `format` functions share the same name but have distinct parameter types. The scope stores them as a vector of symbols under the key "format", and the semantic analyzer resolves calls based on argument types.

If two functions have the same name **and** the same parameter types, the second definition is rejected as a redefinition error.

---

## Builtin Identifier Shadowing

The compiler pre-registers a set of builtin identifiers (type names like "I64", "String", "Boolean", "Object") in the type registry during initialization. These names are not keywords — the lexer produces `Identifier` tokens for them, and they are resolved by the semantic analyzer through the type registry.

Because builtin identifiers are not keywords, they can technically be shadowed by user declarations:

```uranite
public function confusing() -> Void:
    I64 String = 42
    puts( String.toString() )
```

This compiles. The local variable `String` (of type `I64`) shadows the builtin type name `String` within this function scope. After the function, the type name `String` resolves normally again.

While the compiler permits this, it is strongly discouraged. Shadowing a type name with a variable creates code that is difficult to read and maintain. The linter does not currently enforce a rule against builtin shadowing, but project style guides universally prohibit it.

---

## Keyword vs Identifier Boundary

The lexer determines whether a token is a keyword or an identifier through a single hash-map lookup in `readIdentifierOrKeyword()`. The boundary is absolute:

- If the accumulated string matches a `keymaps()` entry, the token is a keyword. No user-defined entity can have that name.
- If the string does not match, the token is an `Identifier`. The semantic analyzer handles all further resolution.

This boundary creates a clean separation. Tokens like `function`, `class`, `return`, and `if` are always keywords and never identifiers. Tokens like `ArrayList`, `hashCode`, and `main` are always identifiers and never keywords — even though the compiler treats "main" specially as the program entry point.

The special treatment of "main" happens in the semantic analyzer and code generator, not in the lexer. The lexer produces `Identifier("main")`, and downstream passes check for this specific identifier string when looking for the entry point function.

---

## Examples

### Valid Identifiers

```uranite
package myapp.services

from uranite.collection.array-list import ArrayList

public const I64 MAX_RETRIES = 3

public class HttpClient:

    private String baseUrl
    private I64 timeoutMillis

    public function HttpClient( self, String baseUrl, I64 timeoutMillis ) -> Void:
        self.baseUrl = baseUrl
        self.timeoutMillis = timeoutMillis

    public function sendRequest( self, String endpoint, String method ) -> String:
        String fullUrl = self.baseUrl + endpoint
        return fullUrl
```

Every identifier follows its domain's convention:

- `camelCase`: `baseUrl`, `timeoutMillis`, `sendRequest`, `endpoint`, `method`, `fullUrl`
- `PascalCase`: `HttpClient`, `String`, `ArrayList`
- `UPPER_SNAKE_CASE`: `MAX_RETRIES`
- `kebab-case`: `myapp.services`, `array-list`

### Invalid Identifiers

The following identifiers are **lexer-level invalid** — the tokenizer rejects them outright:

```
123abc       → starts with digit, dispatched to readNumber()
my-variable  → hyphen terminates identifier at "my", "-" is Minus token
hello world  → space terminates identifier at "hello"
über         → non-ASCII, rejected as unexpected character
```

### Naming Convention Violations

The following identifiers are **lexer-valid** but violate naming conventions and will be flagged by the linter or rejected by code review:

```uranite
I64 x = 10
```

Triggers `naming/cryptic-variable`: "x" is 1 character, below the minimum of 3.

```uranite
public function calc( I64 n ) -> I64:
    return n * 2
```

Triggers `naming/cryptic-parameter`: "n" is 1 character.

```uranite
I64 my_counter = 0
```

Violates the `camelCase` convention. Should be `myCounter`.

```uranite
public class event_handler:
    pass
```

Violates the `PascalCase` convention. Should be `EventHandler`.

```uranite
public function ComputeValue() -> I64:
    return 42
```

Violates the `camelCase` convention for functions. `PascalCase` is reserved for type names. Should be `computeValue`.

Correct versions of all the above:

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
