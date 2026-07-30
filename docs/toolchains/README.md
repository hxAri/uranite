# Toolchains

Usage guides for the official Uranite development tools. Each tool is built alongside the compiler and located in the `build/` directory.

---

## Table of Contents

- [Compiler (uranite)](#compiler-uranite)
  - [Basic Usage](#basic-usage)
  - [Output Modes](#output-modes)
  - [Optimization Levels](#optimization-levels)
  - [Diagnostic Dumps](#diagnostic-dumps)
  - [Linking](#linking)
  - [Cross-Compilation](#cross-compilation)
  - [REPL](#repl)
  - [Debugging](#debugging)
  - [Full Flag Reference](#full-flag-reference)
- [Formatter and Linter (uranite-fmt)](#formatter-and-linter-uranite-fmt)
  - [Formatting Files](#formatting-files)
  - [Format Checking in CI](#format-checking-in-ci)
  - [Linting](#linting)
- [Package Manager (uranite-pkg)](#package-manager-uranite-pkg)
  - [Creating a Project](#creating-a-project)
  - [Managing Dependencies](#managing-dependencies)
  - [Building and Running](#building-and-running)
  - [Subcommand Reference](#subcommand-reference)
- [Documentation Generator (uranite-doc)](#documentation-generator-uranite-doc)
  - [Generating Documentation](#generating-documentation)
  - [Output Formats](#output-formats)
  - [Validation](#validation)
  - [Flag Reference](#flag-reference)

---

## Compiler (uranite)

### Basic Usage

Compile a source file to an executable:

```bash
uranite source.urn -o program
./program
```

Compile and run in one step:

```bash
uranite -r source.urn
```

### Output Modes

| Flag | Output |
|---|---|
| *(default)* | Native executable (compiled + linked) |
| `--emit-llvm` | LLVM IR text (`.ll` file) |
| `--emit-obj` | Object file (`.o` file) |
| `-c, --compile-only` | Compile to object file without linking |

```bash
# Emit LLVM IR for inspection
uranite source.urn --emit-llvm -o output.ll

# Emit object file only
uranite source.urn --emit-obj -o output.o

# Compile without linking (produces .o)
uranite source.urn -c -o output.o
```

### Optimization Levels

| Level | Description |
|---|---|
| `-O0` | No optimization. Fastest compilation, best for debugging. |
| `-O1` | Basic optimizations. Minimal compile-time cost. |
| `-O2` | Balanced optimization. **Default.** |
| `-O3` | Aggressive optimization. May increase binary size. |
| `-Ofast` | Maximum speed. May relax strict IEEE 754 compliance. |

```bash
uranite source.urn -O3 -o fast_program
```

### Diagnostic Dumps

Inspect intermediate representations at each pipeline stage:

```bash
# Lexer token stream
uranite source.urn --dump-tokens

# Abstract syntax tree
uranite source.urn --dump-ast

# High-level IR (preserves loops, match, classes)
uranite source.urn --dump-hir

# Mid-level IR (flattened CFG, linear instructions)
uranite source.urn --dump-mir

# LLVM IR to stdout (without writing a file)
uranite source.urn --dump-ir
```

These flags are invaluable for debugging compiler issues. Combine with `--verbose` for additional pipeline logging:

```bash
uranite source.urn --dump-mir --verbose
```

### Linking

Link with external C libraries using the `-l` flag:

```bash
uranite source.urn -l m -l pthread -o program
```

This passes `-lm` and `-lpthread` to the system linker.

### Cross-Compilation

Target a different architecture with `--target`:

```bash
# Compile for ARM64 Linux
uranite source.urn --target aarch64-linux-gnu -o program_arm64

# Compile for x86_64 Linux (explicit)
uranite source.urn --target x86_64-linux-gnu -o program_x64
```

The target triple follows LLVM conventions: `<arch>-<vendor>-<os>[-<env>]`.

### REPL

Start an interactive read-eval-print loop:

```bash
uranite --repl
```

### Debugging

Run the compiled program under GDB:

```bash
uranite -r source.urn --gdb
```

Use `--no-strip` to preserve debug symbols in the output binary:

```bash
uranite source.urn --no-strip -o program_debug
```

### Full Flag Reference

| Flag | Description |
|---|---|
| `-o, --output <path>` | Output file path |
| `-O, --opt-level <level>` | Optimization level: `0`, `1`, `2`, `3`, `fast` |
| `--emit-llvm` | Emit LLVM IR instead of executable |
| `--emit-obj` | Emit object file instead of executable |
| `-c, --compile-only` | Compile only, do not link |
| `--dump-tokens` | Dump lexer token stream |
| `--dump-ast` | Dump abstract syntax tree |
| `--dump-hir` | Dump high-level IR |
| `--dump-mir` | Dump mid-level IR |
| `--dump-ir` | Dump LLVM IR to stdout |
| `-v, --verbose` | Enable verbose output |
| `--no-strip` | Preserve debug symbols |
| `-l, --link <lib>` | Link with library (repeatable) |
| `-r, --run` | Compile and immediately execute |
| `--gdb` | Run under GDB (requires `-r`) |
| `--repl` | Start interactive REPL |
| `--max-errors <n>` | Stop after N errors (0 = unlimited, 1 = default) |
| `-M, --modules-path <dir>` | Path to standard library modules |
| `-I, --include <dir>` | Additional import search directory (repeatable) |
| `--target <triple>` | Target triple for cross-compilation |
| `--version-info` | Show detailed version information |

---

## Formatter and Linter (uranite-fmt)

`uranite-fmt` formats Uranite source files to canonical style and runs linter diagnostics.

### Formatting Files

Preview formatted output (prints to stdout, does not modify files):

```bash
uranite-fmt source.urn
```

Format and overwrite the file in-place:

```bash
uranite-fmt -w source.urn
```

Format all `.urn` files in a directory:

```bash
uranite-fmt -w src/
```

### Format Checking in CI

Check if files are already formatted without modifying them. Returns exit code `0` if formatted, `1` if not:

```bash
uranite-fmt --check source.urn
uranite-fmt --check src/
```

Use this in CI pipelines to enforce consistent formatting:

```yaml
# GitHub Actions example
- name: Check formatting
  run: uranite-fmt --check src/
```

### Linting

Run linter diagnostics to catch style and correctness issues:

```bash
uranite-fmt --lint source.urn
```

---

## Package Manager (uranite-pkg)

`uranite-pkg` handles project initialization, dependency resolution, and build orchestration.

### Creating a Project

Initialize a new project with a `uranite.yaml` manifest:

```bash
mkdir myproject && cd myproject
uranite-pkg init
```

This creates a `uranite.yaml` file in the current directory.

### Managing Dependencies

Install all dependencies declared in `uranite.yaml`:

```bash
uranite-pkg install
```

Update dependencies to their latest compatible versions:

```bash
uranite-pkg update
```

View the dependency tree:

```bash
uranite-pkg list
```

Regenerate the lockfile without downloading:

```bash
uranite-pkg lock
```

### Building and Running

Build the project with full dependency resolution:

```bash
uranite-pkg build
```

Build and run the project entry file:

```bash
uranite-pkg run
```

Remove build artifacts:

```bash
uranite-pkg clean
```

### Subcommand Reference

| Subcommand | Description |
|---|---|
| `init` | Create a new `uranite.yaml` manifest |
| `install` | Resolve and download all dependencies |
| `update` | Re-resolve to latest compatible versions |
| `lock` | Regenerate `uranite.lock` without downloading |
| `list` | Show dependency tree |
| `build` | Build the project with dependency resolution |
| `run` | Build and execute the project entry file |
| `clean` | Remove build artifacts |

---

## Documentation Generator (uranite-doc)

`uranite-doc` extracts documentation from Uranite source files and generates formatted output. It parses the `"""..."""` doccomment blocks placed after declarations.

### Generating Documentation

Generate Markdown documentation from a source directory:

```bash
uranite-doc src/ -o docs/api/
```

Generate from a single file:

```bash
uranite-doc src/mymodule.urn -o docs/api/
```

### Output Formats

| Format | Flag | Description |
|---|---|---|
| Markdown | `-f markdown` | Default. Generates `.md` files. |
| HTML | `-f html` | Generates browsable HTML pages. |

```bash
# HTML output
uranite-doc src/ -f html -o docs/html/

# Markdown output (default)
uranite-doc src/ -f markdown -o docs/md/
```

### Validation

Validate doccomment structure without generating output:

```bash
uranite-doc src/ --validate-only
```

This checks that all public declarations have properly structured doccomments with `Parameters:`, `Returns:`, and `Complexity:` sections.

Extract the documentation model without rendering:

```bash
uranite-doc src/ --extract-only
```

### Flag Reference

| Flag | Description |
|---|---|
| `-o, --output <dir>` | Output directory (default: `docs`) |
| `-f, --format <fmt>` | Output format: `markdown` or `html` |
| `--project-root <dir>` | Project root for README/LICENSE discovery |
| `--validate-only` | Validate doccomment structure only |
| `--extract-only` | Extract documentation model without rendering |
