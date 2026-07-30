# Platform Support

Uranite is a Linux-first language. Its standard library, async runtime, and memory allocator are built directly on Linux kernel syscalls using inline assembly, with no abstraction layer, no libc wrappers, and no POSIX compatibility shims. This page documents the supported architectures and operating systems, explains the constraints that make Linux the primary target, and provides detailed cross-compilation instructions.

---

## Table of Contents

- [Architecture Support](#architecture-support)
  - [x86_64 (AMD64) — Primary Target](#x86_64-amd64--primary-target)
  - [AArch64 (ARM64) — Cross-Compilation Target](#aarch64-arm64--cross-compilation-target)
  - [RISC-V 64 and ARM32 — Experimental](#risc-v-64-and-arm32--experimental)
- [Operating System Support](#operating-system-support)
  - [Linux — First-Class Citizen](#linux--first-class-citizen)
  - [macOS — Partial Support](#macos--partial-support)
  - [Windows — WSL2 Only](#windows--wsl2-only)
- [Why Linux Is the Primary Platform](#why-linux-is-the-primary-platform)
  - [Raw Syscall Architecture](#raw-syscall-architecture)
  - [Architecture-Specific Syscall Tables](#architecture-specific-syscall-tables)
  - [The Native Segment Rewriting System](#the-native-segment-rewriting-system)
  - [Inline Assembly Per Architecture](#inline-assembly-per-architecture)
- [Cross-Compilation](#cross-compilation)
  - [How Cross-Compilation Works](#how-cross-compilation-works)
  - [The --target Flag](#the---target-flag)
  - [LLVM Target Triples](#llvm-target-triples)
  - [Cross-Compiling to AArch64](#cross-compiling-to-aarch64)
  - [Sysroot and Cross-Toolchain Requirements](#sysroot-and-cross-toolchain-requirements)
  - [Verifying Cross-Compiled Binaries](#verifying-cross-compiled-binaries)
- [Platform Limitation Matrix](#platform-limitation-matrix)

---

## Architecture Support

### x86_64 (AMD64) — Primary Target

x86_64 is the primary development and deployment architecture. All compiler development, CI testing, and standard library validation runs on x86_64 Linux. The Uranite compiler itself is developed on Parrot Security OS 7.3 running on x86_64 hardware.

The x86_64 syscall layer (`stdlibs/os/arch/x86-64/syscall.urn`) uses the `syscall` instruction with the standard Linux x86_64 ABI:

- Syscall number in `RAX`
- Arguments in `RDI`, `RSI`, `RDX`, `R10`, `R8`, `R9`
- Return value in `RAX`
- `RCX` and `R11` are clobbered by the kernel

Syscall numbers follow the x86_64 Linux syscall table: `SYS_READ = 0`, `SYS_WRITE = 1`, `SYS_OPEN = 2`, `SYS_CLOSE = 3`, `SYS_MMAP = 9`.

### AArch64 (ARM64) — Cross-Compilation Target

AArch64 is supported via cross-compilation using the `--target aarch64-linux-gnu` flag. The standard library includes a complete AArch64 syscall layer (`stdlibs/os/arch/aarch64/syscall.urn`) that mirrors the x86_64 API surface but uses the `svc #0` instruction with the AArch64 Linux ABI:

- Syscall number in `X8`
- Arguments in `X0` through `X5`
- Return value in `X0`

AArch64 uses the generic Linux syscall table, which differs significantly from x86_64. Legacy syscalls like `open`, `stat`, and `mkdir` do not exist on AArch64; the syscall layer translates these to their `*at` equivalents (`openat`, `newfstatat`, `mkdirat`) using `AT_FDCWD` as the directory file descriptor. Syscall numbers differ between the two architectures: `SYS_READ = 63` (versus 0 on x86_64), `SYS_WRITE = 64` (versus 1), `SYS_MMAP = 222` (versus 9).

### RISC-V 64 and ARM32 — Experimental

The compiler's `targetArchSegment()` function recognizes `riscv64` and `arm`/`armeb` triples and maps them to architecture directory segments. However, no standard library syscall implementations exist for these architectures. Programs that do not use raw syscalls, the async runtime, or architecture-specific inline assembly can target these architectures through LLVM's cross-compilation backend, but standard library modules that import from `uranite.os.arch.native.*` will fail to resolve.

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

**Kernel requirement:** Linux 4.5 or newer. The async runtime uses `epoll_create1`, `timerfd_create`, and `eventfd`, all of which are available in kernels 2.6.27+. The 4.5 minimum accounts for the `EPOLL_CLOEXEC` flag behavior and `renameat2` usage in the filesystem layer.

### macOS — Partial Support

macOS is supported for building and running the compiler toolchain itself and for compiling Uranite programs that do not depend on Linux-specific features.

**What works on macOS:**

- Building the Uranite compiler, formatter, package manager, and documentation generator from source.
- Compiling and running Uranite programs that use collections, strings, I/O (console and file), error handling, classes, generics, pattern matching, and threading via the POSIX thread runtime.
- Running the compiler's unit test suite.
- Emitting LLVM IR and object files.

**What does not work on macOS:**

- The `async`/`await` runtime. The async scheduler is built on `epoll_create1`, `timerfd_create`, and `eventfd`, which are Linux-specific syscalls with no macOS equivalents. `kqueue` would be the macOS analog, but no `kqueue` backend exists. Programs that import `uranite.async.*` or declare `async function` will fail at link time.
- Any standard library module that imports from `uranite.os.arch.native.*`. The architecture-specific syscall layers use Linux syscall numbers and the `syscall`/`svc` instruction, neither of which is valid on macOS where syscalls go through the Mach/XNU interface.
- The memory allocator (`uranite.memory.allocator`), which calls `SYS_MMAP` and `SYS_MUNMAP` via raw inline assembly.
- The coroutine and fiber runtimes, which use architecture-specific context switching via inline assembly.
- The IPC runtime (`liburanite-ipc-runtime.a`), which links against `librt` for POSIX shared memory, not available on macOS in the same form.

**Summary:** macOS is a viable platform for learning Uranite's syntax and type system, but not for production deployment of programs that use concurrency, raw memory management, or OS-level primitives.

### Windows — WSL2 Only

Native Windows is not supported. Uranite's standard library and runtime depend on:

- **POSIX process APIs:** `fork`, `exec`, `pipe`, `waitpid` — used by the subprocess runtime and the compiler's linker invocation.
- **POSIX threading:** `pthread_create`, `pthread_join`, `pthread_mutex_lock` — used by the thread runtime.
- **Linux syscalls:** `epoll_create1`, `timerfd_create`, `eventfd`, `mmap`, `munmap`, `read`, `write`, `open`, `close` — used directly via inline assembly throughout the standard library.
- **POSIX IPC:** `shm_open`, `mq_open` — used by the IPC runtime.
- **ELF binary format:** The compiler produces ELF object files via `llc` and links them with `cc` (the system C compiler). Windows PE/COFF output is not supported.

None of these APIs exist natively on Windows.

**WSL2 provides full compatibility.** Windows Subsystem for Linux 2 runs a real Linux kernel (not a translation layer like WSL1). Programs compiled under WSL2 execute as native Linux ELF binaries with full access to Linux syscalls, epoll, timerfd, and all other kernel interfaces. The Uranite async runtime, coroutine context switching, raw syscall layer, and all standard library modules function identically under WSL2 and native Linux.

WSL1 is explicitly unsupported. WSL1 translates Linux syscalls to Windows NT kernel calls, and this translation layer does not support epoll, timerfd, or eventfd. Programs using the async runtime will crash or hang under WSL1.

---

## Why Linux Is the Primary Platform

### Raw Syscall Architecture

Uranite's standard library modules bypass the C runtime entirely. Instead of calling libc wrappers like `read()`, `write()`, or `mmap()`, the standard library invokes Linux syscalls directly through inline assembly. This design eliminates the C runtime as a dependency, reduces binary size, and gives the standard library precise control over error handling and register usage.

Every syscall in Uranite follows this pattern: a pure Uranite function wraps an `asm` block that loads the syscall number and arguments into the correct registers, executes the syscall instruction, and captures the return value. The `SyscallResult` wrapper type encodes the kernel's return value for error checking.

The x86_64 syscall path uses the `syscall` instruction:

```uranite
public function syscall3( I64 number, I64 arg1, I64 arg2, I64 arg3 ) -> SyscallResult:
    I64 result = 0
    asm "syscall" : output( "={rax}" result ) : input( "{rax}" number, "{rdi}" arg1, "{rsi}" arg2, "{rdx}" arg3 ) : clobber( "rcx", "r11", "memory" )
    return new SyscallResult( result )
```

The AArch64 syscall path uses the `svc #0` instruction:

```uranite
public function syscall3( I64 number, I64 arg1, I64 arg2, I64 arg3 ) -> SyscallResult:
    I64 result = 0
    asm "svc #0" : output( "={x0}" result ) : input( "{x8}" number, "{x0}" arg1, "{x1}" arg2, "{x2}" arg3 ) : clobber( "memory" )
    return new SyscallResult( result )
```

Both functions have the same Uranite signature. User code imports from `uranite.os.arch.native.syscall` and gets the correct implementation for the target architecture automatically.

### Architecture-Specific Syscall Tables

Linux assigns different syscall numbers to the same operation on different architectures. The x86_64 table descends from the original i386 table with extensions; the AArch64 table uses the "generic" Linux syscall table that omits legacy calls.

| Operation | x86_64 Number | AArch64 Number |
|---|---|---|
| `read` | 0 | 63 |
| `write` | 1 | 64 |
| `open` | 2 | — (use `openat` = 56) |
| `close` | 3 | 57 |
| `mmap` | 9 | 222 |
| `pipe2` | 293 | 59 |

The AArch64 syscall layer handles the missing legacy calls transparently. When user code calls an `open()` wrapper, the AArch64 implementation redirects to `openat` with `AT_FDCWD` (-100) as the directory file descriptor, making the path resolve relative to the current working directory, which matches the behavior of the legacy `open` call.

### The Native Segment Rewriting System

The compiler driver contains a module resolution mechanism that rewrites the reserved `native` path segment to the target architecture's directory name at import resolution time. When a standard library module imports:

```uranite
from uranite.os.arch.native.syscall import SYS_WRITE, SYS_READ
```

The compiler's `resolveModulePath()` method detects the `native` segment and replaces it with the active architecture identifier:

- On x86_64 (or when `--target` resolves to x86_64): `native` becomes `x86-64`
- On AArch64 (or when `--target` resolves to aarch64): `native` becomes `aarch64`
- On RISC-V 64: `native` becomes `riscv64`
- On ARM32: `native` becomes `arm`

The architecture identifier is derived from the LLVM target triple via `targetArchSegment()`, which parses the triple using `llvm::Triple` and maps the `llvm::Triple::ArchType` enum to the corresponding standard library directory name.

This means a single import statement in user code or standard library modules resolves to the correct architecture-specific implementation without conditional compilation, preprocessor macros, or build-time code generation. The same source file compiles correctly for x86_64 and AArch64 without modification.

### Inline Assembly Per Architecture

Beyond syscall invocation, architecture-specific inline assembly appears in:

- **Context switching** (`stdlibs/os/arch/*/context.urn`): Saves and restores register state for coroutine and fiber scheduling. x86_64 saves `RBX`, `RBP`, `R12`-`R15`, and `RSP`. AArch64 saves `X19`-`X30` and `SP`.
- **CPU identification** (`stdlibs/os/arch/*/cpu.urn`): Reads processor identification registers (`CPUID` on x86_64, system registers on AArch64).
- **Memory barriers** (`stdlibs/os/arch/*/memory.urn`): Architecture-specific fence instructions for memory ordering.
- **Port I/O** (`stdlibs/os/arch/x86-64/port.urn`): `in`/`out` instructions for x86 hardware port access. No AArch64 equivalent; ARM uses memory-mapped I/O.

Each architecture directory provides the same public API surface. Application-level code never imports from architecture-specific paths directly; it imports from `uranite.os.arch.native.*` and the compiler rewrites the path.

---

## Cross-Compilation

### How Cross-Compilation Works

Uranite's cross-compilation is built on LLVM's multi-target infrastructure. The compilation pipeline works as follows:

1. **Frontend stages** (lexer, parser, semantic analysis, borrow checking, HIR, MIR) are architecture-independent. They produce the same MIR regardless of the target.

2. **MIR codegen** emits LLVM IR. The `MIRCodegen::generate()` method sets the LLVM module's target triple to either the host triple (from `llvm::sys::getDefaultTargetTriple()`) or the user-specified triple (from `--target`). LLVM IR is largely target-independent, but the data layout string and target triple are embedded in the module.

3. **LLVM optimization** (`opt`) runs target-independent and target-specific optimization passes on the IR.

4. **LLVM code generation** (`llc`) lowers the IR to architecture-specific machine code. When `--target` is specified, `llc` receives `-mtriple=<triple>` and generates object code for the target architecture.

5. **Linking.** When cross-compiling, the compiler invokes `<triple>-gcc` (e.g., `aarch64-linux-gnu-gcc`) instead of the host `cc`. This cross-compiler links the object file against the target's C runtime and system libraries.

6. **Module resolution.** The `native` segment in standard library import paths is resolved using the target triple, not the host triple. Cross-compiling to AArch64 from an x86_64 host causes `uranite.os.arch.native.syscall` to resolve to `stdlibs/os/arch/aarch64/syscall.urn`, pulling in AArch64 syscall numbers and the `svc #0` inline assembly.

### The --target Flag

```bash
./build/uranite source.urn --target aarch64-linux-gnu -o program_arm64
```

The `--target` flag accepts an LLVM target triple and affects three stages:

1. **MIR codegen:** The LLVM module's target triple is set to the specified value, which controls data layout (pointer size, alignment, endianness) and instruction selection.
2. **llc invocation:** The `-mtriple` flag is passed to `llc`, overriding its default host triple.
3. **Linker invocation:** The host `cc` is replaced with `<triple>-gcc` (e.g., `aarch64-linux-gnu-gcc`).

### LLVM Target Triples

A target triple follows the format `<arch>-<vendor>-<os>[-<environment>]`. Common triples for Uranite:

| Triple | Architecture | OS | Use Case |
|---|---|---|---|
| `x86_64-pc-linux-gnu` | x86_64 | Linux (glibc) | Default on x86_64 Linux hosts |
| `aarch64-linux-gnu` | ARM64 | Linux (glibc) | Cross-compile for ARM64 Linux |
| `aarch64-linux-musl` | ARM64 | Linux (musl) | Cross-compile for musl-based ARM64 |
| `riscv64-linux-gnu` | RISC-V 64 | Linux (glibc) | Experimental RISC-V target |
| `arm-linux-gnueabihf` | ARM32 | Linux (glibc, hard float) | Experimental ARM32 target |

The compiler does not validate whether the specified triple corresponds to a supported architecture. Invalid triples are caught by `llc` at the code generation stage with an error like `unable to get target for '<triple>'`.

### Cross-Compiling to AArch64

This is the most common and best-tested cross-compilation scenario.

**Step 1: Install the cross-toolchain.**

On Debian/Ubuntu/Parrot:

```bash
sudo apt install -y gcc-aarch64-linux-gnu g++-aarch64-linux-gnu
```

This provides `aarch64-linux-gnu-gcc` and `aarch64-linux-gnu-g++`, which the Uranite compiler invokes for linking.

**Step 2: Compile.**

```bash
./build/uranite source.urn --target aarch64-linux-gnu -o program_arm64
```

**Step 3: Run on target hardware or emulator.**

The output binary is an AArch64 ELF executable. It cannot run on x86_64 directly. Options:

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

QEMU user-mode translates AArch64 instructions to x86_64 at runtime. Performance is 5-20x slower than native execution, but it is sufficient for functional testing.

### Sysroot and Cross-Toolchain Requirements

Cross-compilation requires two components beyond the Uranite compiler:

**1. Cross-compiler.** The Uranite compiler invokes `<triple>-gcc` for linking. This must be installed and in `PATH`. On Debian-based systems, the `gcc-aarch64-linux-gnu` package provides this. On Arch, install `aarch64-linux-gnu-gcc` from the AUR or community repository.

**2. Target sysroot.** The cross-compiler needs access to the target architecture's C library headers and shared libraries for linking. The Debian cross-compiler packages include a minimal sysroot. For custom sysroots, set the `--sysroot` flag via the linker:

```bash
export LDFLAGS="--sysroot=/path/to/aarch64-sysroot"
./build/uranite source.urn --target aarch64-linux-gnu -o program_arm64
```

Uranite programs that use only the pure Uranite standard library (no FFI, no `-l` flags) require minimal sysroot contents: just `libc.so`, `libpthread.so`, `librt.so`, `libdl.so`, `crt1.o`, `crti.o`, and `crtn.o`.

### Verifying Cross-Compiled Binaries

Check the binary's architecture with `file`:

```bash
file program_arm64
```

Expected output:

```
program_arm64: ELF 64-bit LSB executable, ARM aarch64, version 1 (SYSV), dynamically linked, ...
```

Inspect the ELF header with `readelf`:

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
| Diagnostic dumps (--dump-*) | Yes | — | Yes | Yes | No |
| Unit test suite (uranite-tests) | Yes | — | Yes | Yes | No |

**Legend:**
- **Yes** — Fully functional and tested.
- **No** — Not supported and will not work.
- **Cross-compile** — Not directly buildable on the target; cross-compile from a supported host.
- **Possible** — Technically feasible but untested; may require manual toolchain configuration.
- **Yes (partial)** — Core language features work; syscall-dependent features do not.
