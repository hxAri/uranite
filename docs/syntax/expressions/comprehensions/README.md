# Comprehensions

Comprehensions build collections from inline iteration expressions. A single comprehension replaces the pattern of creating an empty collection, looping over a source, and appending each transformed element.

---

## Table of Contents

- [Comprehensions](#comprehensions)
  - [Table of Contents](#table-of-contents)
  - [Overview](#overview)
  - [Syntax](#syntax)
  - [At a Glance](#at-a-glance)
  - [Subpage Index](#subpage-index)

---

## Overview

Uranite supports three comprehension forms, each producing a different collection type:

**List comprehensions** use square brackets and produce an `ArrayList<T>`. The expression `[index * index for I64 index in 0..5]` creates an `ArrayList<I64>` containing five squared values.

**Set comprehensions** use curly braces and produce a `HashSet<T>`. The expression `{index * 2 for I64 index in 0..5}` creates a `HashSet<I64>` of five doubled values with automatic deduplication.

**Map comprehensions** use curly braces with a key-value pair separated by a colon and produce a `HashMap<K, V>`. The expression `{index: index * 2 for I64 index in 0..3}` creates a `HashMap<I64, I64>` mapping each key to its doubled value.

All three forms support an optional `if` filter clause that selects only elements satisfying a Boolean condition.

---

## Syntax

Every comprehension follows the same structure:

```
[body for Type variable in iterable]
[body for Type variable in iterable if condition]

{body for Type variable in iterable}
{key: value for Type variable in iterable}
```

The loop variable type annotation is optional when the compiler can infer the element type from the iterable.

---

## At a Glance

```uranite
ArrayList<I64> squares = [index * index for I64 index in 0..5]

ArrayList<I64> evens = [index for I64 index in 0..10 if index % 2 == 0]

HashSet<I64> unique = {index * 2 for I64 index in 0..5}

HashMap<I64, I64> doubled = {index: index * 2 for I64 index in 0..3}
```

---

## Subpage Index

| Document | Description |
|---|---|
| [List Comprehensions](list-comprehensions.md) | Square-bracket comprehensions producing `ArrayList<T>`, with filtering and nested iteration. |
| [Map Comprehensions](map-comprehensions.md) | Curly-brace key-value comprehensions producing `HashMap<K, V>`. |
| [Set Comprehensions](set-comprehensions.md) | Curly-brace comprehensions producing `HashSet<T>` with automatic deduplication. |
