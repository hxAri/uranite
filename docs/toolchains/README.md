# Toolchains

Usage guides for the official Uranite development tools. Each tool is built alongside the compiler and located in the `build/` directory.

---

## Table of Contents

- [Toolchains](#toolchains)
  - [Table of Contents](#table-of-contents)
  - [Overview](#overview)
  - [Quick Reference](#quick-reference)
  - [Subpages](#subpages)

---

## Overview

The Uranite toolchain includes four tools:

- **`uranite`** — the compiler. Compiles `.urn` source files to native executables through LLVM
- **`uranite-fmt`** — the formatter and linter. Enforces canonical code style and runs diagnostics
- **`uranite-pkg`** — the package manager. Handles project initialization, dependency resolution, and builds
- **`uranite-doc`** — the documentation generator. Extracts doccomments and produces Markdown or HTML output

All tools are built with `cmake` and available in the `build/` directory after compilation.

---

## Quick Reference

| Task | Command |
|---|---|
| Compile to executable | `uranite source.urn -o program` |
| Compile and run | `uranite -r source.urn` |
| Emit LLVM IR | `uranite source.urn --emit-llvm -o output.ll` |
| Dump AST | `uranite source.urn --dump-ast` |
| Format file in-place | `uranite-fmt -w source.urn` |
| Check formatting | `uranite-fmt --check src/` |
| Lint source | `uranite-fmt --lint source.urn` |
| Init project | `uranite-pkg init` |
| Build project | `uranite-pkg build` |
| Run project | `uranite-pkg run` |
| Generate docs | `uranite-doc src/ -o docs/api/` |

---

## Subpages

| Page | Description |
|---|---|
| [Compiler](compiler.md) | Compilation modes, output formats, optimization, cross-compilation, REPL |
| [Compiler Flags Reference](compiler-flags-reference.md) | Complete flag reference for the `uranite` compiler |
| [Formatter](formatter.md) | Code formatting, linting, and CI integration with `uranite-fmt` |
| [Package Manager](package-manager.md) | Project setup, dependencies, and build management with `uranite-pkg` |
| [Documentation Generator](doc-generator.md) | API documentation generation with `uranite-doc` |
