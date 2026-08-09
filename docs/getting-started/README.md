# Getting Started

This section walks through everything required to go from a fresh machine to a compiled and running Uranite program. It covers dependency installation across all supported platforms, building the full toolchain from source, understanding the project layout, configuring your editor, and writing and compiling your first program.

---

## Table of Contents

- [Getting Started](#getting-started)
  - [Table of Contents](#table-of-contents)
  - [Toolchain Overview](#toolchain-overview)
  - [Prerequisites](#prerequisites)
    - [Required Dependencies](#required-dependencies)
    - [Auto-Fetched Dependencies](#auto-fetched-dependencies)
  - [Platform Support](#platform-support)
    - [Architectures](#architectures)
    - [Operating Systems](#operating-systems)
  - [Installation](#installation)
    - [Linux (Debian / Ubuntu / Parrot)](#linux-debian--ubuntu--parrot)
    - [Linux (Arch / Manjaro)](#linux-arch--manjaro)
    - [Linux (Fedora / RHEL)](#linux-fedora--rhel)
    - [macOS](#macos)
    - [Windows (WSL2)](#windows-wsl2)
  - [Building from Source](#building-from-source)
    - [Clone and Build](#clone-and-build)
    - [Build Artifacts](#build-artifacts)
    - [System Installation](#system-installation)
  - [Verifying the Build](#verifying-the-build)
  - [Hello World](#hello-world)
    - [Program Structure](#program-structure)
    - [Compiling and Running](#compiling-and-running)
    - [Diagnostic Inspection](#diagnostic-inspection)
  - [Project Layout](#project-layout)
  - [Editor Setup](#editor-setup)
    - [General Settings](#general-settings)
    - [VS Code](#vs-code)
    - [Vim / Neovim](#vim--neovim)
    - [JetBrains (IntelliJ / CLion)](#jetbrains-intellij--clion)
    - [Formatting Integration](#formatting-integration)
  - [What Next](#what-next)
  - [Detailed Guides](#detailed-guides)

---

## Toolchain Overview

Uranite ships as a unified toolchain of four binaries, all built from a single repository and designed to work together out of the box with zero configuration.

| Binary | Purpose |
|---|---|
| `uranite` | Compiler. Transforms `.urn` source files into native executables or object files. Includes a REPL, diagnostic dumpers, cross-compilation support, and integrated GDB launching. |
| `uranite-fmt` | Formatter and linter. Enforces canonical style on `.urn` files. Supports in-place formatting, dry-run checking for CI pipelines, and linter diagnostics for naming violations. |
| `uranite-pkg` | Package manager. Scaffolds new projects with `uranite.yaml` manifests, resolves and downloads dependencies, orchestrates builds with automatic dependency ordering, and manages lockfiles. |
| `uranite-doc` | Documentation generator. Extracts `"""..."""` doccomment blocks from source files and produces Markdown or HTML documentation. Validates doccomment structure (requires "Parameters:", "Returns:", and "Complexity:" sections on public declarations). |

The compiler is the only binary required for writing and running Uranite programs. The formatter, package manager, and documentation generator are development conveniences that become essential as projects grow beyond single-file scripts.

---

## Prerequisites

### Required Dependencies

These must be installed on your system before building. CMake will fail at the configure step if any are missing.

| Dependency | Minimum Version | Purpose |
|---|---|---|
| **LLVM** | 19 | Backend for native code generation and optimization. |
| **CMake** | 3.22 | Build system generator. |
| **C++ Compiler** | GCC 12+ or Clang 15+ | Compiles the Uranite toolchain from source. C++17 support is required. |
| **GNU Make** | Any | Build executor. Ninja works as an alternative (`cmake -G Ninja`). |
| **Git** | Any | Required for cloning the repository. |

### Auto-Fetched Dependencies

These libraries are resolved automatically by CMake. If a compatible version is already installed on your system, the system copy is used. Otherwise, CMake downloads and builds them as part of the Uranite build.

| Library | Version | Purpose |
|---|---|---|
| **fmt** | 11.0.2 | String formatting library used by the toolchain. |
| **spdlog** | 1.14.1 | Logging library. Powers `--verbose` output during compilation. |
| **argparse** | 3.1 | CLI argument parsing for all four toolchain binaries. |
| **GoogleTest** | 1.15.2 | Unit test framework. Only used by the test runner, not the compiler. |

You do not need to install these manually. If you prefer system packages for faster rebuilds, install `libfmt-dev`, `libspdlog-dev`, and `libgtest-dev` (Debian/Ubuntu names) before running CMake.

---

## Platform Support

### Architectures

| Architecture | Status | Notes |
|---|---|---|
| **x86_64** (AMD64) | Primary target | Fully supported and continuously tested. |
| **ARM64** (AArch64) | Cross-compilation | Supported via `--target aarch64-linux-gnu`. Requires appropriate cross-toolchain and sysroot. |

### Operating Systems

| OS | Status | Notes |
|---|---|---|
| **Linux** | Fully supported | Primary development platform. Tested on Ubuntu 22.04+, Debian 12+, Arch Linux, Fedora 38+, and Parrot Security OS. The async runtime uses Linux-specific syscalls and is fully functional on Linux. |
| **macOS** | Supported | Requires LLVM 19 via Homebrew. Xcode Command Line Tools provide the system linker. The async runtime's Linux syscall layer does not function on macOS; async features may be unavailable. |
| **Windows** | Via WSL2 only | Native Windows is not supported. The compiler and standard library depend on POSIX APIs and Linux syscalls. Use WSL2 with any supported Linux distribution for full functionality. |

---

## Installation

### Linux (Debian / Ubuntu / Parrot)

```bash
sudo apt update
sudo apt install -y cmake g++ git make

wget https://apt.llvm.org/llvm.sh
chmod +x llvm.sh
sudo ./llvm.sh 19
sudo apt install -y llvm-19-dev
```

Optional system packages to skip automatic downloads:

```bash
sudo apt install -y libfmt-dev libspdlog-dev libgtest-dev
```

### Linux (Arch / Manjaro)

```bash
sudo pacman -S cmake gcc git make llvm
```

Arch typically ships the latest LLVM release. Verify with `llvm-config --version` that it reports 19.x. If Arch has moved to LLVM 20+, install LLVM 19 from the AUR.

### Linux (Fedora / RHEL)

```bash
sudo dnf install cmake gcc-c++ git make llvm19-devel
```

On RHEL 8/9, enable the CodeReady Builder or EPEL repository for LLVM 19 packages if they are not in the default repositories.

### macOS

```bash
xcode-select --install
brew install cmake llvm@19
```

Homebrew installs LLVM into a keg-only prefix. Add it to your `PATH` so CMake can find it:

```bash
export PATH="/opt/homebrew/opt/llvm@19/bin:$PATH"
```

Add this line to `~/.zshrc` or `~/.bash_profile` to persist across terminal sessions. On Intel Macs, the path is `/usr/local/opt/llvm@19/bin` instead.

### Windows (WSL2)

1. Install WSL2 with Ubuntu from PowerShell:

   ```powershell
   wsl --install -d Ubuntu
   ```

2. Open the Ubuntu terminal and follow the [Linux (Debian / Ubuntu / Parrot)](#linux-debian--ubuntu--parrot) instructions.

WSL2 provides a full Linux kernel. All Uranite features, including the async runtime, function correctly under WSL2.

---

## Building from Source

### Clone and Build

```bash
git clone https://github.com/uranite-lang/uranite.git
cd uranite
cmake -B build -DCMAKE_BUILD_TYPE=Release
make -C build -j$(nproc)
```

The build takes 2-5 minutes depending on hardware. The first build is slower because CMake may fetch and compile dependencies from source.

For debug builds with full symbol information and no optimization:

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug
make -C build -j$(nproc)
```

To use a specific compiler:

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=clang++
```

### Build Artifacts

After a successful build, the `build/` directory contains:

| Artifact | Type | Description |
|---|---|---|
| `build/uranite` | Executable | The compiler |
| `build/uranite-fmt` | Executable | Code formatter and linter |
| `build/uranite-pkg` | Executable | Package manager |
| `build/uranite-doc` | Executable | Documentation generator |
| `build/uranite-tests` | Executable | Unit test runner |
| `build/runtime/` | Directory | Runtime libraries linked automatically into compiled programs |

### System Installation

To install the toolchain binaries system-wide:

```bash
sudo cmake --install build
```

This installs binaries to `/usr/local/bin/` and runtime libraries to `/usr/local/lib/uranite/runtime/` by default. Override the prefix with:

```bash
sudo cmake --install build --prefix /opt/uranite
```

For development, running directly from `build/` is recommended. The compiler auto-discovers the standard library from the source tree's `stdlibs/` directory when run from the repository root.

---

## Verifying the Build

Run the test suite:

```bash
./build/uranite-tests
```

Check the compiler version:

```bash
./build/uranite --version-info
```

---

## Hello World

### Program Structure

Create a file called `hello.urn`:

```uranite
package hello

from uranite.io.console import puts

public function main() -> I32:
    puts( "Hello, World!" )
    return 0
```

Every Uranite program has three structural requirements:

**Package declaration.** The first non-comment line must declare the package name using dot-separated identifiers. The package name establishes the module's namespace for imports. Single-segment names (`package hello`) are valid for standalone scripts. Multi-segment names (`package myorg.myproject.module`) are conventional for library code.

**Entry point.** The program must define a `public function main() -> I32` function. The `public` visibility is required because the linker resolves `main` as an external symbol. The return type must be `I32`, representing the process exit code. Return `0` for success and any non-zero value for failure.

**Indentation-based blocks.** Function bodies, class bodies, control flow branches, and all other block constructs are delimited by a colon (`:`) followed by an indented body. Uranite uses consistent indentation (spaces or tabs, but not mixed within a file) to determine block boundaries. There are no braces and no explicit block-end markers.

### Compiling and Running

Compile to a native executable:

```bash
./build/uranite hello.urn -o hello
./hello
```

Output:

```
Hello, World!
```

Compile and run in one step:

```bash
./build/uranite -r hello.urn
```

The `-r` flag compiles to a temporary executable, runs it immediately, and deletes the temporary file after execution. This is the fastest workflow for development iteration.

### Diagnostic Inspection

The compiler provides several inspection flags for debugging:

```bash
./build/uranite hello.urn --dump-tokens
./build/uranite hello.urn --dump-ast
./build/uranite hello.urn --dump-hir
./build/uranite hello.urn --dump-mir
```

Emit generated IR for manual inspection:

```bash
./build/uranite hello.urn --emit-llvm -o hello.ll
```

Run under GDB for debugging:

```bash
./build/uranite -r hello.urn --gdb
```

Control optimization level:

```bash
./build/uranite hello.urn -O0 -o hello_debug
./build/uranite hello.urn -O3 -o hello_fast
```

---

## Project Layout

A typical Uranite project, whether a single-file script or a multi-module application, follows this layout:

```
myproject/
├── uranite.yaml          # Package manifest (created by uranite-pkg init)
├── uranite.lock           # Dependency lockfile (auto-generated)
├── src/
│   ├── main.urn          # Entry point
│   └── mymodule.urn      # Additional modules
├── tests/
│   └── test_mymodule.urn # Test files
└── build/                # Compiled artifacts (gitignored)
```

For single-file programs, no manifest or directory structure is needed. Write a `.urn` file anywhere and compile it directly:

```bash
./build/uranite myscript.urn -r
```

The package manager scaffolds the full layout:

```bash
uranite-pkg init
```

This creates `uranite.yaml` with a default project configuration. From there, `uranite-pkg build` compiles the project with automatic module resolution, and `uranite-pkg run` builds and executes the entry point.

---

## Editor Setup

Uranite source files use the `.urn` extension. Because the language uses indentation-based blocks, configuring your editor for consistent whitespace handling is important.

### General Settings

Configure your editor with these settings for `.urn` files:

- **Indentation:** Spaces (4 per level) or tabs. Do not mix within a file.
- **Trailing whitespace:** Strip on save.
- **Final newline:** Insert on save.
- **File encoding:** UTF-8.

### VS Code

Add to `.vscode/settings.json`:

```json
{
    "[uranite]": {
        "editor.tabSize": 4,
        "editor.insertSpaces": true,
        "editor.trimAutoWhitespace": true,
        "files.trimTrailingWhitespace": true,
        "files.insertFinalNewline": true
    },
    "files.associations": {
        "*.urn": "python"
    }
}
```

Using Python syntax highlighting for `.urn` files provides reasonable keyword and string highlighting until a dedicated Uranite language extension is available. The indentation-based structure and keyword overlap (`class`, `for`, `if`, `return`, `import`, `from`, `try`, `except`, `finally`, `raise`, `lambda`, `async`, `await`, `pass`, `break`, `continue`, `in`, `not`, `and`, `or`, `is`, `None`, `True`, `False`) make Python mode a practical approximation.

### Vim / Neovim

Add to your configuration:

```vim
autocmd BufNewFile,BufRead *.urn set filetype=python
autocmd BufNewFile,BufRead *.urn set tabstop=4 shiftwidth=4 expandtab
```

### JetBrains (IntelliJ / CLion)

1. Go to **Settings > Editor > File Types**.
2. Select **Python** in the recognized file types list.
3. Add `*.urn` to the registered patterns.

### Formatting Integration

After building the toolchain, integrate `uranite-fmt` as an external formatter:

```bash
uranite-fmt -w src/
```

In CI pipelines, use the check mode to enforce formatting without modifying files:

```bash
uranite-fmt --check src/
```

This returns exit code `0` if all files are already formatted, and `1` if any file needs changes.

---

## What Next

This section has covered installation, building, and your first program. Explore deeper topics through the rest of the documentation:

- **[Language Syntax](../syntax/README.md)** — Complete language reference: types, control flow, generics, pattern matching, async, ownership, and memory management.
- **[Standard Library Guide](../stdlib-guide/README.md)** — Practical guides for collections, I/O, strings, concurrency, memory, testing, and error handling.
- **[Toolchains](../toolchains/README.md)** — Full flag references and usage guides for the compiler, formatter, package manager, and documentation generator.
- **[Compiler Internals](../internals/README.md)** — Contributor guide for developers working on the compiler itself.

---

## Detailed Guides

Each topic in this section has a dedicated page with deeper coverage:

| Guide | Description |
|---|---|
| [Installation](installation.md) | Step-by-step installation instructions with platform-specific troubleshooting and common build failure resolution. |
| [Platform Support](platform-support.md) | Detailed architecture and OS compatibility matrix, cross-compilation setup, and platform-specific limitations. |
| [Hello World](hello-world.md) | Extended walkthrough of your first program with explanations of every language construct used. |
| [Project Structure](project-structure.md) | Conventions for organizing single-file scripts, multi-module applications, and library packages. Package manifest format and module resolution rules. |
| [Editor Setup](editor-setup.md) | Detailed configuration for VS Code, Vim/Neovim, JetBrains IDEs, Emacs, and Sublime Text. Formatter integration and CI pipeline setup. |
