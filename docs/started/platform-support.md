# Platform Support

Uranite is a Linux-first language. Its standard library, async runtime, and memory allocator are built directly on Linux kernel syscalls using inline assembly, with no abstraction layer and no C runtime dependency. This page documents the supported architectures and operating systems, explains the constraints that make Linux the primary target, and provides cross-compilation instructions.

---

## Table of Contents

- [Platform Support](#platform-support)
  - [Table of Contents](#table-of-contents)
  - [Architecture Support](#architecture-support)
    - [x86\_64 (AMD64) — Primary Target](#x86_64-amd64--primary-target)
    - [AArch64 (ARM64) — Cross-Compilation Target](#aarch64-arm64--cross-compilation-target)
    - [RISC-V 64 and ARM32 — Experimental](#risc-v-64-and-arm32--experimental)
  - [Operating System Support](#operating-system-support)
    - [Linux — First-Class Citizen](#linux--first-class-citizen)
    - [macOS — Partial Support](#macos--partial-support)
    - [Windows — WSL2 Only](#windows--wsl2-only)
  - [Why Linux Is the Primary Platform](#why-linux-is-the-primary-platform)
    - [Raw Syscall Architecture](#raw-syscall-architecture)
    - [Architecture-Specific Syscall Tables](#architecture-specific-syscall-tables)
    - [The Native Import Mechanism](#the-native-import-mechanism)
    - [Inline Assembly Per Architecture](#inline-assembly-per-architecture)
  - [Cross-Compilation](#cross-compilation)
    - [The --target Flag](#the---target-flag)
    - [Target Triples](#target-triples)
    - [Cross-Compiling to AArch64](#cross-compiling-to-aarch64)
    - [Sysroot and Cross-Toolchain Requirements](#sysroot-and-cross-toolchain-requirements)
    - [Verifying Cross-Compiled Binaries](#verifying-cross-compiled-binaries)
  - [Platform Limitation Matrix](#platform-limitation-matrix)

---

## Architecture Support

### x86_64 (AMD64) — Primary Target

x86_64 is the primary development and deployment architecture. All compiler development, CI testing, and standard library validation runs on x86_64 Linux.

The standard library's x86_64 syscall layer (`stdlibs/os/arch/x86-64/syscall.urn`) uses the `syscall` instruction with the standard Linux x86_64 calling convention.

### AArch64 (ARM64) — Cross-Compilation Target

AArch64 is supported via cross-compilation using the `--target aarch64-linux-gnu` flag. The standard library includes a complete AArch64 syscall layer (`stdlibs/os/arch/aarch64/syscall.urn`) that mirrors the x86_64 API surface but uses the `svc #0` instruction with the AArch64 calling convention.

<!-- AArch64 uses a different Linux syscall table than x86_64. Legacy syscalls like `open`, `stat`, and `mkdir` do not exist on AArch64; the syscall layer translates these to their `*at` equivalents (`openat`, `newfstatat`, `mkdirat`) transparently. -->

### RISC-V 64 and ARM32 — Experimental

The compiler recognizes `riscv64` and `arm`/`armeb` target triples. However, no standard library syscall implementations exist for these architectures yet. Programs that do not use raw syscalls, the async runtime, or architecture-specific inline assembly can target these architectures, but standard library modules that import from `uranite.os.arch.native.*` will fail to resolve.

---

## Operating System Support

### Linux — First-Class Citizen

Linux is the only fully supported operating system. Every standard library module, every runtime library, and every compiler feature is tested and validated on Linux.

**Tested distributions:**

| Distribution | Version | Notes |
|---|---|---|
| Parrot Security OS | 7.3 (echo) | Primary development machine. Debian testing-based. |
| Ubuntu | 22.04+ (jammy) | Widely tested. LLVM 19 available via apt.llvm.org. |
| Debian | 12+ (bookworm) | LLVM 19 available via apt.llvm.org or backports. |
| Arch Linux | Rolling | Ships latest LLVM. May require version pinning. |
| Fedora | 38+ | LLVM 19 available in default or updates repositories. |

**Kernel requirement:** Linux 4.5 or newer. The async runtime uses `epoll_create1`, `timerfd_create`, and `eventfd`, all available in earlier kernels, but 4.5 is the minimum for full compatibility with all standard library modules.

### macOS — Partial Support

macOS is supported for building the compiler toolchain and for compiling Uranite programs that do not depend on Linux-specific features.

**What works on macOS:**

- Building the Uranite compiler, formatter, package manager, and documentation generator from source.
- Compiling and running Uranite programs that use collections, strings, I/O (console and file), error handling, classes, generics, pattern matching, and threading.
- Running the compiler's unit test suite.

**What does not work on macOS:**

- The `async`/`await` runtime. The async scheduler is built on `epoll`, `timerfd`, and `eventfd`, which are Linux-specific. Programs that use `async function` or import `uranite.async.*` will fail at link time.
- Any standard library module that imports from `uranite.os.arch.native.*`. The syscall layers use Linux syscall numbers and instructions.
- The memory allocator (`uranite.memory.allocator`), which calls `mmap` and `munmap` via raw inline assembly.
- The coroutine and fiber runtimes, which use architecture-specific context switching.

**Summary:** macOS is a viable platform for learning Uranite's syntax and type system, but not for production deployment of programs that use concurrency, raw memory management, or OS-level primitives.

### Windows — WSL2 Only

Native Windows is not supported. Uranite's standard library and runtime depend on POSIX APIs (`fork`, `pipe`, `pthread_create`) and Linux syscalls that do not exist natively on Windows.

**WSL2 provides full compatibility.** Windows Subsystem for Linux 2 runs a real Linux kernel. Programs compiled under WSL2 execute as native Linux binaries with full access to Linux syscalls and all standard library modules function identically to native Linux.

WSL1 is explicitly unsupported. WSL1 uses a syscall translation layer that does not support epoll, timerfd, or eventfd. Programs using the async runtime will crash or hang under WSL1.

---

## Why Linux Is the Primary Platform

### Raw Syscall Architecture

Uranite's standard library modules bypass the C runtime entirely. Instead of calling C library wrappers like `read()`, `write()`, or `mmap()`, the standard library invokes Linux syscalls directly through inline assembly. This design eliminates the C runtime as a dependency, reduces binary size, and gives the standard library precise control over error handling.

Every syscall in Uranite follows this pattern: a pure Uranite function wraps an `assembly` block that loads the syscall number and arguments into the correct registers, executes the syscall instruction, and captures the return value.

The x86_64 syscall path:

```uranite
public function syscall3( I64 number, I64 arg1, I64 arg2, I64 arg3 ) -> SyscallResult:
    I64 result = 0
    asm "syscall" : output( "={rax}" result ) : input( "{rax}" number, "{rdi}" arg1, "{rsi}" arg2, "{rdx}" arg3 ) : clobber( "rcx", "r11", "memory" )
    return new SyscallResult( result )
```

The AArch64 syscall path:

```uranite
public function syscall3( I64 number, I64 arg1, I64 arg2, I64 arg3 ) -> SyscallResult:
    I64 result = 0
    asm "svc #0" : output( "={x0}" result ) : input( "{x8}" number, "{x0}" arg1, "{x1}" arg2, "{x2}" arg3 ) : clobber( "memory" )
    return new SyscallResult( result )
```

Both functions have the same Uranite signature. User code imports from `uranite.os.arch.native.syscall` and gets the correct implementation for the target architecture automatically.

### Architecture-Specific Syscall Tables

Linux assigns different syscall numbers to the same operation on different architectures:

| Operation | x86_64 Number | AArch64 Number |
|---|---|---|
| `read` | 0 | 63 |
| `write` | 1 | 64 |
| `open` | 2 | — (use `openat` = 56) |
| `close` | 3 | 57 |
| `mmap` | 9 | 222 |
| `pipe2` | 293 | 59 |

The AArch64 syscall layer handles missing legacy calls transparently. When user code calls an `open()` wrapper, the AArch64 implementation redirects to `openat` with `AT_FDCWD` (-100) as the directory file descriptor, matching the behavior of the legacy `open` call.

### The Native Import Mechanism

The compiler provides a module resolution mechanism that rewrites the reserved `native` path segment to the target architecture's directory name at import resolution time. When a standard library module imports:

```uranite
from uranite.os.arch.native.syscall import SYS_WRITE, SYS_READ
```

The `native` segment is replaced with the active architecture identifier:

- On x86_64 (or when `--target` resolves to x86_64): `native` becomes `x86-64`
- On AArch64 (or when `--target` resolves to aarch64): `native` becomes `aarch64`
- On RISC-V 64: `native` becomes `riscv64`
- On ARM32: `native` becomes `arm`

This means a single import statement resolves to the correct architecture-specific implementation without conditional compilation or preprocessor macros. The same source file compiles correctly for x86_64 and AArch64 without modification.

### Inline Assembly Per Architecture

Beyond syscall invocation, architecture-specific inline assembly appears in:

- **Context switching** (`stdlibs/os/arch/*/context.urn`): Saves and restores register state for coroutine and fiber scheduling.
- **CPU identification** (`stdlibs/os/arch/*/cpu.urn`): Reads processor identification registers.
- **Memory barriers** (`stdlibs/os/arch/*/memory.urn`): Architecture-specific fence instructions for memory ordering.
- **Port I/O** (`stdlibs/os/arch/x86-64/port.urn`): x86 hardware port access instructions. No AArch64 equivalent; ARM uses memory-mapped I/O.

Each architecture directory provides the same public API surface. Application-level code never imports from architecture-specific paths directly; it imports from `uranite.os.arch.native.*` and the compiler resolves the path.

---

## Cross-Compilation

### The --target Flag

```bash
./build/uranite source.urn --target aarch64-linux-gnu -o program_arm64
```

The `--target` flag accepts a target triple and instructs the compiler to generate code for the specified architecture instead of the host machine. It also causes the `native` import segment to resolve using the target architecture and the linker to use the target's cross-compiler.

### Target Triples

A target triple follows the format `<arch>-<vendor>-<os>[-<environment>]`. Common triples for Uranite:

| Triple | Architecture | OS | Use Case |
|---|---|---|---|
| `x86_64-pc-linux-gnu` | x86_64 | Linux (glibc) | Default on x86_64 Linux hosts |
| `aarch64-linux-gnu` | ARM64 | Linux (glibc) | Cross-compile for ARM64 Linux |
| `aarch64-linux-musl` | ARM64 | Linux (musl) | Cross-compile for musl-based ARM64 |
| `riscv64-linux-gnu` | RISC-V 64 | Linux (glibc) | Experimental RISC-V target |
| `arm-linux-gnueabihf` | ARM32 | Linux (glibc, hard float) | Experimental ARM32 target |

### Cross-Compiling to AArch64

This is the most common and best-tested cross-compilation scenario.

**Step 1: Install the cross-toolchain.**

On Debian/Ubuntu/Parrot:

```bash
sudo apt install -y gcc-aarch64-linux-gnu g++-aarch64-linux-gnu
```

**Step 2: Compile.**

```bash
./build/uranite source.urn --target aarch64-linux-gnu -o program_arm64
```

**Step 3: Run on target hardware or emulator.**

The output binary is an AArch64 executable. It cannot run on x86_64 directly. Options:

**Option A: Copy to an ARM64 machine** (Raspberry Pi 4/5, AWS Graviton, Apple Silicon Linux VM):

```bash
scp program_arm64 user@arm-host:~/
ssh user@arm-host ./program_arm64
```

**Option B: Run under QEMU user-mode emulation:**

```bash
sudo apt install -y qemu-user-static
qemu-aarch64-static ./program_arm64
```

QEMU user-mode translates AArch64 instructions to x86_64 at runtime. Performance is 5-20x slower than native execution, but sufficient for functional testing.

### Sysroot and Cross-Toolchain Requirements

Cross-compilation requires two components beyond the Uranite compiler:

**1. Cross-compiler.** The Uranite compiler invokes `<triple>-gcc` for linking (e.g., `aarch64-linux-gnu-gcc`). This must be installed and in `PATH`. On Debian-based systems, the `gcc-aarch64-linux-gnu` package provides this.

**2. Target sysroot.** The cross-compiler needs access to the target architecture's C library headers and shared libraries for linking. The Debian cross-compiler packages include a minimal sysroot. For custom sysroots:

```bash
export LDFLAGS="--sysroot=/path/to/aarch64-sysroot"
./build/uranite source.urn --target aarch64-linux-gnu -o program_arm64
```

Uranite programs that use only the pure Uranite standard library (no FFI, no `-l` flags) require minimal sysroot contents: just `libc.so`, `libpthread.so`, `crt1.o`, `crti.o`, and `crtn.o`.

### Verifying Cross-Compiled Binaries

Check the binary's architecture:

```bash
file program_arm64
```

Expected output:

```
program_arm64: ELF 64-bit LSB executable, ARM aarch64, version 1 (SYSV), dynamically linked, ...
```

Inspect the ELF header:

```bash
readelf -h program_arm64
```

Key fields to verify:

```
  Class:                             ELF64
  Machine:                           AArch64
```

If the `Machine` field shows `Advanced Micro Devices X86-64` instead, the `--target` flag was not applied correctly.

---

## Platform Limitation Matrix

| Feature | Linux x86_64 | Linux AArch64 | macOS | Windows (WSL2) | Windows (Native) |
|---|---|---|---|---|---|
| Compiler builds from source | Yes | — | Yes | Yes | No |
| Compile and run programs | Yes | Cross-compile | Yes (partial) | Yes | No |
| Collections, strings, I/O | Yes | Yes | Yes | Yes | No |
| Classes, generics, pattern matching | Yes | Yes | Yes | Yes | No |
| Error handling (try/except/finally) | Yes | Yes | Yes | Yes | No |
| Threading (Mutex, RwLock, Channel) | Yes | Yes | Yes | Yes | No |
| Async/await (Future, epoll runtime) | Yes | Yes | No | Yes | No |
| Coroutines and fibers | Yes | Yes | No | Yes | No |
| Raw syscalls (uranite.os.arch.*) | Yes | Yes | No | Yes | No |
| Memory allocator (mmap-based) | Yes | Yes | No | Yes | No |
| FFI (dlopen/dlsym) | Yes | Yes | Yes | Yes | No |
| IPC (shared memory, message queues) | Yes | Yes | No | Yes | No |
| Subprocess spawning | Yes | Yes | Yes | Yes | No |
| Cross-compilation | Yes | — | Possible | Yes | No |
| REPL | Yes | — | Yes | Yes | No |

**Legend:**
- **Yes** — Fully functional and tested.
- **No** — Not supported and will not work.
- **Cross-compile** — Not directly buildable on the target; cross-compile from a supported host.
- **Possible** — Technically feasible but untested; may require manual toolchain configuration.
- **Yes (partial)** — Core language features work; syscall-dependent features do not.
