# Installation

This guide provides exhaustive, step-by-step instructions for installing the Uranite compiler toolchain from source on every supported platform. It covers dependency installation, repository cloning, building, verification, and troubleshooting common failure scenarios.

Uranite is distributed as source only. There are no prebuilt binaries or system packages. The build process produces five binaries from a single CMake project: the compiler (`uranite`), the formatter (`uranite-fmt`), the package manager (`uranite-pkg`), the documentation generator (`uranite-doc`), and the test runner (`uranite-tests`).

---

## Table of Contents

- [Dependency Overview](#dependency-overview)
  - [Required System Dependencies](#required-system-dependencies)
  - [CMake FetchContent Dependencies](#cmake-fetchcontent-dependencies)
  - [How FetchContent Works](#how-fetchcontent-works)
- [Parrot Security OS](#parrot-security-os)
- [Debian and Ubuntu](#debian-and-ubuntu)
- [Arch Linux and Manjaro](#arch-linux-and-manjaro)
- [Fedora and RHEL](#fedora-and-rhel)
- [macOS](#macos)
- [Windows via WSL2](#windows-via-wsl2)
- [Building from Source](#building-from-source)
  - [Cloning the Repository](#cloning-the-repository)
  - [Configure Step](#configure-step)
  - [Build Step](#build-step)
  - [Build Outputs](#build-outputs)
- [Verification](#verification)
  - [Test Suite](#test-suite)
  - [Version Check](#version-check)
  - [Compilation Test](#compilation-test)
- [System-Wide Installation](#system-wide-installation)
- [Troubleshooting Common Build Failures](#troubleshooting-common-build-failures)
  - [llvm-config Not Found](#llvm-config-not-found)
  - [CMake Version Too Old](#cmake-version-too-old)
  - [Missing C++17 Support](#missing-c17-support)
  - [LLVM Header or Library Mismatch](#llvm-header-or-library-mismatch)
  - [FetchContent Download Failures](#fetchcontent-download-failures)
  - [Linker Errors for pthread, rt, or dl](#linker-errors-for-pthread-rt-or-dl)
  - [Out of Memory During Build](#out-of-memory-during-build)
  - [Permission Denied on Install](#permission-denied-on-install)

---

## Dependency Overview

### Required System Dependencies

These must be present before running CMake. The configure step will fail immediately if any are missing.

| Dependency | Minimum Version | Verification Command | Role |
|---|---|---|---|
| LLVM | 19.x | `llvm-config-19 --version` | Code generation backend. Provides LLVM IR generation, optimization passes (`opt`), target code generation (`llc`), and the system linker invocation path. The build system locates LLVM through `llvm-config` or `llvm-config-19`. |
| CMake | 3.22 | `cmake --version` | Build system generator. Version 3.22 is required for `FetchContent_MakeAvailable` and `list(FILTER)` support. |
| C++ Compiler | GCC 12+ or Clang 15+ | `g++ --version` or `clang++ --version` | Compiles the Uranite compiler source. Must support C++17 (`-std=c++17`). The codebase uses `std::optional`, `std::variant`, structured bindings, and `if constexpr`. |
| GNU Make or Ninja | Any | `make --version` or `ninja --version` | Build executor. CMake generates Makefiles by default. Pass `-G Ninja` for Ninja. |
| Git | Any | `git --version` | Clones the repository. Also used at configure time to embed the short commit hash (`git rev-parse --short HEAD`) into the compiler's `--version-info` output. |
| pthread | System | — | POSIX thread support. Linked by `liburanite-thread-runtime.a` and the GoogleTest runner. |
| rt | System | — | POSIX real-time extensions. Linked by `liburanite-ipc-runtime.a` for `shm_open` and `mq_open`. |
| dl | System | — | Dynamic linker interface. Linked by `liburanite-ffi-runtime.a` for `dlopen`, `dlsym`, and `dlclose`. |

### CMake FetchContent Dependencies

Four C++ libraries are used by the compiler and its toolchain. CMake first attempts to find each on the system via `find_package()`. If the system package is missing or an incompatible version is installed, CMake clones the exact pinned version from GitHub and builds it as part of the Uranite build tree.

| Library | Pinned Version | System Package (Debian) | Role |
|---|---|---|---|
| **fmt** | 11.0.2 | `libfmt-dev` | Type-safe string formatting. Used throughout the compiler for error messages, diagnostic output, and IR dumps. Preferred over `std::format` for C++17 compatibility. |
| **spdlog** | 1.14.1 | `libspdlog-dev` | Structured logging with compile-time severity filtering. Powers the `--verbose` flag's pipeline stage logging. Built on top of `fmt`. |
| **argparse** | 3.1 | — | Declarative CLI argument parser. Generates `--help` output and validates flag combinations. No Debian system package exists; always fetched via FetchContent. |
| **GoogleTest** | 1.15.2 | `libgtest-dev` | Unit testing framework. Only linked into `uranite-tests`. Not linked into the compiler, formatter, package manager, or doc generator. |

### How FetchContent Works

When you run `cmake -B build`, CMake evaluates each `find_package()` call in `CMakeLists.txt`. For each library:

1. If `find_package( fmt QUIET )` succeeds (the system has `libfmt-dev` installed and CMake finds its config or module file), that system copy is used. No download occurs.

2. If `find_package()` fails (the package is not installed, or the installed version is incompatible), CMake enters the `FetchContent_Declare` block. This records the Git repository URL and the exact tag to clone.

3. `FetchContent_MakeAvailable()` clones the repository into `build/_deps/<name>-src/`, configures it, and makes its targets available to the Uranite build. The library is compiled as part of `make -C build`.

The result is deterministic: the same pinned version is always used regardless of what the system provides, unless a compatible system package is found first. This eliminates version mismatch issues at the cost of longer first builds.

To force CMake to always use FetchContent (ignoring system packages):

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_DISABLE_FIND_PACKAGE_fmt=ON \
    -DCMAKE_DISABLE_FIND_PACKAGE_spdlog=ON \
    -DCMAKE_DISABLE_FIND_PACKAGE_GTest=ON
```

To force CMake to require system packages (failing if not found instead of falling back to FetchContent), install the system packages and verify with:

```bash
dpkg -l | grep -E "libfmt-dev|libspdlog-dev|libgtest-dev"
```

---

## Parrot Security OS

Parrot Security OS is the primary development platform for Uranite. These instructions are tested on Parrot 7.x (codename echo), which is Debian-based with access to Debian testing repositories.

### Step 1: System Update

```bash
sudo apt update && sudo apt upgrade -y
```

### Step 2: Install Build Tools

```bash
sudo apt install -y cmake g++ git make
```

Verify minimum versions:

```bash
cmake --version
g++ --version
```

CMake must be 3.22 or newer. GCC must be 12 or newer. Parrot 7.3 ships CMake 3.31.6 and GCC 14.2.0, both well above the minimum.

### Step 3: Install LLVM 19

Parrot's repositories include LLVM 19 packages directly:

```bash
sudo apt install -y llvm-19 llvm-19-dev llvm-19-tools llvm-19-runtime
```

Verify the installation:

```bash
llvm-config-19 --version
```

Expected output: `19.1.7` (or any 19.x release).

Verify that the LLVM tools are accessible:

```bash
which llc-19
which opt-19
```

Expected paths: `/usr/bin/llc-19` and `/usr/bin/opt-19`. The CMake build system reads these paths from `llvm-config-19 --bindir` at configure time and embeds them into the compiler binary.

### Step 4: Install Optional System Libraries

These are optional. If present, CMake uses them instead of downloading from GitHub. This speeds up subsequent builds.

```bash
sudo apt install -y libfmt-dev libspdlog-dev libgtest-dev
```

### Step 5: Build

```bash
git clone https://github.com/uranite-lang/uranite.git
cd uranite
cmake -B build -DCMAKE_BUILD_TYPE=Release
make -C build -j$(nproc)
```

### Step 6: Verify

```bash
./build/uranite-tests
./build/uranite --version-info
```

---

## Debian and Ubuntu

Tested on Debian 12 (bookworm) and Ubuntu 22.04+ (jammy and newer). These instructions also apply to any Debian derivative not specifically listed.

### Step 1: Install Build Tools

```bash
sudo apt update
sudo apt install -y cmake g++ git make wget lsb-release software-properties-common gnupg
```

### Step 2: Install LLVM 19

Ubuntu and Debian stable may not include LLVM 19 in their default repositories. Use the official LLVM apt repository:

```bash
wget https://apt.llvm.org/llvm.sh
chmod +x llvm.sh
sudo ./llvm.sh 19
sudo apt install -y llvm-19-dev
```

The `llvm.sh` script adds the official LLVM apt repository for your distribution release, imports the signing key, and installs the specified LLVM version.

Verify:

```bash
llvm-config-19 --version
```

If `llvm-config-19` is not found but `llvm-config` exists, check its version. Some distributions create only the unversioned symlink:

```bash
llvm-config --version
```

If this reports 19.x, the build system will find it automatically.

### Step 3: Optional System Libraries

```bash
sudo apt install -y libfmt-dev libspdlog-dev libgtest-dev
```

### Step 4: Build

```bash
git clone https://github.com/uranite-lang/uranite.git
cd uranite
cmake -B build -DCMAKE_BUILD_TYPE=Release
make -C build -j$(nproc)
```

---

## Arch Linux and Manjaro

Arch Linux uses a rolling release model and typically ships the latest LLVM version. As of mid-2026, the `llvm` package may provide LLVM 19 or newer.

### Step 1: Install Dependencies

```bash
sudo pacman -S cmake gcc git make llvm
```

### Step 2: Verify LLVM Version

```bash
llvm-config --version
```

If this reports 19.x, proceed to the build step. If Arch has moved to LLVM 20+, you need LLVM 19 specifically. Options:

**Option A: Install from AUR**

```bash
yay -S llvm19
```

Then configure CMake to use the versioned binary:

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release -DLLVM_CONFIG=/usr/bin/llvm-config-19
```

**Option B: Build LLVM 19 from source**

This is a last resort. LLVM builds take 30+ minutes and require 16 GB+ of RAM:

```bash
git clone --depth 1 --branch llvmorg-19.1.7 https://github.com/llvm/llvm-project.git
cmake -S llvm-project/llvm -B llvm-build -DCMAKE_BUILD_TYPE=Release -DLLVM_ENABLE_PROJECTS=""
make -C llvm-build -j$(nproc)
sudo make -C llvm-build install
```

### Step 3: Build

```bash
git clone https://github.com/uranite-lang/uranite.git
cd uranite
cmake -B build -DCMAKE_BUILD_TYPE=Release
make -C build -j$(nproc)
```

---

## Fedora and RHEL

### Fedora 38+

```bash
sudo dnf install cmake gcc-c++ git make llvm19-devel
```

If `llvm19-devel` is not available in the default repositories:

```bash
sudo dnf install cmake gcc-c++ git make llvm-devel
llvm-config --version
```

Verify the version is 19.x before proceeding.

### RHEL 8 / RHEL 9

RHEL stable releases may not include LLVM 19. Enable the CodeReady Builder repository:

```bash
sudo subscription-manager repos --enable codeready-builder-for-rhel-9-x86_64-rpms
sudo dnf install cmake gcc-c++ git make
```

Then install LLVM 19 from EPEL or build from source.

### Build

```bash
git clone https://github.com/uranite-lang/uranite.git
cd uranite
cmake -B build -DCMAKE_BUILD_TYPE=Release
make -C build -j$(nproc)
```

---

## macOS

### Step 1: Xcode Command Line Tools

```bash
xcode-select --install
```

This provides the system linker (`ld`), standard C/C++ headers, and `clang++`.

### Step 2: Homebrew Dependencies

```bash
brew install cmake llvm@19
```

### Step 3: Configure PATH

Homebrew installs LLVM into a keg-only prefix to avoid conflicting with Apple's system Clang. CMake cannot find `llvm-config-19` without adding the Homebrew LLVM to your `PATH`.

On Apple Silicon Macs (M1/M2/M3/M4):

```bash
export PATH="/opt/homebrew/opt/llvm@19/bin:$PATH"
```

On Intel Macs:

```bash
export PATH="/usr/local/opt/llvm@19/bin:$PATH"
```

Add the appropriate line to `~/.zshrc` (default macOS shell) to persist across terminal sessions:

```bash
echo 'export PATH="/opt/homebrew/opt/llvm@19/bin:$PATH"' >> ~/.zshrc
source ~/.zshrc
```

Verify:

```bash
llvm-config --version
```

### Step 4: Build

```bash
git clone https://github.com/uranite-lang/uranite.git
cd uranite
cmake -B build -DCMAKE_BUILD_TYPE=Release
make -C build -j$(sysctl -n hw.ncpu)
```

Note: macOS uses `sysctl -n hw.ncpu` instead of `nproc` for the CPU core count. Alternatively, install `coreutils` via Homebrew for `nproc`.

### macOS Limitations

The Uranite async runtime uses Linux-specific syscalls (`epoll_create1`, `timerfd_create`, `eventfd`). These are not available on macOS. Programs that use `async`/`await` or import modules from `uranite.async` will fail to link on macOS. All other language features, including threading, collections, I/O, and error handling, function correctly.

---

## Windows via WSL2

Native Windows is not supported. The compiler and standard library depend on POSIX APIs (`fork`, `pipe`, `mmap`, `pthread_create`) and Linux syscalls that do not exist on Windows.

### Step 1: Install WSL2

From an elevated PowerShell prompt:

```powershell
wsl --install -d Ubuntu
```

Restart your machine if prompted. After restart, the Ubuntu terminal opens and asks you to create a Linux user account.

### Step 2: Install Inside WSL2

Once inside the WSL2 Ubuntu terminal, follow the [Debian and Ubuntu](#debian-and-ubuntu) instructions in their entirety. WSL2 provides a real Linux kernel, so all Uranite features, including the async runtime's raw syscall layer, function identically to a native Linux installation.

### File System Performance

For best build performance, clone the repository into the WSL2 filesystem (`/home/<user>/`), not into a Windows-mounted path (`/mnt/c/`). Windows filesystem access from WSL2 incurs significant I/O overhead due to the 9P protocol translation layer. Builds from `/mnt/c/` can be 5-10x slower than builds from the native ext4 filesystem.

---

## Building from Source

These instructions apply to all platforms after dependencies are installed.

### Cloning the Repository

```bash
git clone https://github.com/uranite-lang/uranite.git
cd uranite
```

The repository contains:

```
uranite/
├── CMakeLists.txt          # Build configuration (271 lines)
├── src/
│   ├── uranite.cpp          # Compiler entry point
│   ├── uranite/             # Compiler source (lexer, parser, semantic, IR, codegen)
│   ├── uranite-fmt/         # Formatter and linter source
│   ├── uranite-pkg/         # Package manager source
│   ├── uranite-doc/         # Documentation generator source
│   ├── uranite-crt/         # C runtime libraries (6 static .c files)
│   └── uranite-test/        # GoogleTest unit tests
├── stdlibs/                 # Standard library (37 modules, pure Uranite)
├── examples/                # Tutorial and example programs
└── scripts/                 # Build and development scripts
```

### Configure Step

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
```

This step:

1. Detects the C and C++ compilers and verifies C++17 support.
2. Locates `llvm-config` or `llvm-config-19` and extracts LLVM's compiler flags, linker flags, library paths, and binary directory.
3. Attempts `find_package()` for `fmt`, `spdlog`, `argparse`, and `GTest`. Downloads missing libraries via FetchContent.
4. Detects Git and extracts the short commit hash for embedding in `--version-info`.
5. Sets the standard library module path to `${CMAKE_SOURCE_DIR}/stdlibs` (development default) and the C runtime directory to `${CMAKE_BINARY_DIR}/runtime`.
6. Defines compiler macros: `_URANITE_COMPILER_VERSION_`, `_URANITE_LANGUAGE_VERSION_`, `_URANITE_GIT_HASH_`, `_URANITE_LLC_`, `_URANITE_OPT_`, `_URANITE_MODULES_DIR_`, `_URANITE_C_RUNTIME_DIR_`.

For debug builds:

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug
```

Debug builds include full debug symbols (`-g`), disable `NDEBUG`, and produce larger, slower binaries suitable for GDB debugging.

To explicitly specify the C++ compiler:

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=clang++
```

To override the standard library install path (for packaged distributions):

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release \
    -DURANITE_MODULES_INSTALL_DIR=/usr/lib/uranite/stdlibs \
    -DURANITE_C_RUNTIME_INSTALL_DIR=/usr/lib/uranite/runtime
```

### Build Step

```bash
make -C build -j$(nproc)
```

This compiles:

1. Six C runtime static libraries from `src/uranite-crt/*.c` (C11 standard) into `build/runtime/`.
2. The compiler library from all `.cpp` files under `src/uranite/` (C++17 standard, approximately 25,000 lines).
3. The compiler executable from `src/uranite.cpp` linked against the compiler library, LLVM, fmt, spdlog, and argparse.
4. The formatter executable from `src/uranite-fmt/` sources linked against the compiler library.
5. The package manager executable from `src/uranite-pkg/` sources linked against the compiler library.
6. The documentation generator executable from `src/uranite-doc/` sources linked against the compiler library and the formatter's comment extractor.
7. The test runner executable from `src/uranite-test/` sources linked against the compiler library and GoogleTest.

Build time on a 4-core machine: 3-5 minutes for Release, 2-4 minutes for Debug. First build is 1-2 minutes longer if FetchContent clones and compiles dependencies.

For Ninja instead of Make:

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release -G Ninja
ninja -C build
```

### Build Outputs

| Path | Type | Description |
|---|---|---|
| `build/uranite` | Executable | Compiler binary |
| `build/uranite-fmt` | Executable | Formatter and linter binary |
| `build/uranite-pkg` | Executable | Package manager binary |
| `build/uranite-doc` | Executable | Documentation generator binary |
| `build/uranite-tests` | Executable | GoogleTest test runner |
| `build/runtime/liburanite-exception-runtime.a` | Static library | Exception handling and shadow stack |
| `build/runtime/liburanite-async-runtime.a` | Static library | Async runtime bridge |
| `build/runtime/liburanite-thread-runtime.a` | Static library | POSIX thread wrapper |
| `build/runtime/liburanite-subprocess-runtime.a` | Static library | Process spawning |
| `build/runtime/liburanite-ipc-runtime.a` | Static library | Inter-process communication |
| `build/runtime/liburanite-ffi-runtime.a` | Static library | Dynamic library loading |

---

## Verification

### Test Suite

```bash
./build/uranite-tests
```

Expected output:

```
[==========] 152 tests from 12 test suites ran.
[  PASSED  ] 152 tests.
```

All 152 tests must pass. Any failure indicates a build configuration problem, a compiler bug, or a platform incompatibility.

Run a specific test suite:

```bash
./build/uranite-tests --gtest_filter="ParserTest.*"
```

Run a single test:

```bash
./build/uranite-tests --gtest_filter="ParserTest.ParsesSimpleFunction"
```

### Version Check

```bash
./build/uranite --version-info
```

Example output on Parrot Security OS 7.3:

```
Build Jul 30 2026 12:00:20
Compiler v1.2.0 | Language v2026.8
GCC/G++ v14.2.0
Signature 1eb4363

Host: x86_64-pc-linux-gnu
LLVM Version: 19.1.7
```

Verify that:

- The compiler version is `1.2.0`.
- The language version is `2026.8`.
- The LLVM version is `19.x.x`.
- The signature matches the Git commit hash at the time of your build (`git rev-parse --short HEAD`).

### Compilation Test

Write a minimal program and compile it:

```bash
cat > /tmp/verify.urn << 'EOF'
package verify

from uranite.io.console import puts

public function main() -> I32:
    puts( "Uranite is working" )
    return 0
EOF

./build/uranite /tmp/verify.urn -o /tmp/verify
/tmp/verify
```

Expected output:

```
Uranite is working
```

If this prints correctly, the compiler, linker, standard library module resolution, and C runtime linking are all functioning.

---

## System-Wide Installation

### CMake Install

```bash
sudo cmake --install build
```

Default install paths:

| Artifact | Path |
|---|---|
| Binaries | `/usr/local/bin/uranite`, `/usr/local/bin/uranite-fmt`, `/usr/local/bin/uranite-pkg`, `/usr/local/bin/uranite-doc` |
| Runtime libraries | `/usr/local/lib/uranite/runtime/` |
| Standard library modules | `/usr/local/lib/uranite/stdlibs/` |

Override the prefix:

```bash
sudo cmake --install build --prefix /opt/uranite
```

With a custom prefix, add `/opt/uranite/bin` to your `PATH`:

```bash
export PATH="/opt/uranite/bin:$PATH"
```

### Manual Copy

For development machines where `cmake --install` feels heavyweight:

```bash
sudo cp build/uranite build/uranite-fmt build/uranite-pkg build/uranite-doc /usr/local/bin/
```

This copies only the binaries. The compiler will still locate the standard library from the source tree's `stdlibs/` directory (the path is compiled into the binary at build time via `_URANITE_MODULES_DIR_`). To override the module path at runtime:

```bash
uranite -M /path/to/stdlibs source.urn -o program
```

---

## Troubleshooting Common Build Failures

### llvm-config Not Found

**Symptom:**

```
CMake Error: Could not find LLVM_CONFIG (missing: llvm-config llvm-config-19)
```

**Cause:** LLVM 19 development headers are not installed, or `llvm-config-19` is not in `PATH`.

**Fix:** Install `llvm-19-dev` (Debian/Ubuntu/Parrot), `llvm-devel` (Fedora), or `llvm` (Arch). If installed but not found, locate it manually:

```bash
find /usr -name "llvm-config*" 2>/dev/null
```

If the binary exists at a non-standard path (e.g., `/usr/lib/llvm-19/bin/llvm-config-19`), either add the directory to `PATH` or pass it explicitly to CMake:

```bash
export PATH="/usr/lib/llvm-19/bin:$PATH"
cmake -B build -DCMAKE_BUILD_TYPE=Release
```

### CMake Version Too Old

**Symptom:**

```
CMake Error at CMakeLists.txt:20 (cmake_minimum_required):
  CMake 3.22 or higher is required.  You are running version 3.16.3
```

**Cause:** The system CMake is from an older distribution release.

**Fix on Debian/Ubuntu:**

```bash
sudo apt remove cmake
pip3 install cmake
```

Or install from Kitware's APT repository:

```bash
wget -O - https://apt.kitware.com/keys/kitware-archive-latest.asc | sudo gpg --dearmor -o /usr/share/keyrings/kitware-archive-keyring.gpg
echo "deb [signed-by=/usr/share/keyrings/kitware-archive-keyring.gpg] https://apt.kitware.com/ubuntu/ $(lsb_release -cs) main" | sudo tee /etc/apt/sources.list.d/kitware.list
sudo apt update
sudo apt install cmake
```

### Missing C++17 Support

**Symptom:**

```
error: 'optional' is not a member of 'std'
```

or

```
error: structured bindings only available with '-std=c++17'
```

**Cause:** The C++ compiler is too old to support C++17, or CMake is not requesting C++17 correctly.

**Fix:** Upgrade to GCC 12+ or Clang 15+:

```bash
sudo apt install g++-12
cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=g++-12
```

Verify that the compiler supports C++17:

```bash
echo '#include <optional>
int main() { std::optional<int> x = 42; return *x; }' | g++ -std=c++17 -x c++ - -o /dev/null
```

If this compiles without errors, C++17 is supported.

### LLVM Header or Library Mismatch

**Symptom:**

```
fatal error: llvm/IR/Module.h: No such file or directory
```

or linker errors referencing undefined LLVM symbols.

**Cause:** The LLVM runtime package is installed but the development headers (`llvm-19-dev`) are not, or `llvm-config` points to a different LLVM version than the installed headers.

**Fix:** Ensure the development package is installed:

```bash
sudo apt install llvm-19-dev
```

Verify that the include path exists:

```bash
llvm-config-19 --includedir
ls $(llvm-config-19 --includedir)/llvm/IR/Module.h
```

If you have multiple LLVM versions installed, ensure `llvm-config-19` is the one CMake finds. Remove older versions or set the `PATH` explicitly.

### FetchContent Download Failures

**Symptom:**

```
Failed to download https://github.com/fmtlib/fmt.git
```

**Cause:** Network connectivity issues, corporate proxy, or GitHub rate limiting.

**Fix Option 1:** Install system packages to bypass FetchContent entirely:

```bash
sudo apt install -y libfmt-dev libspdlog-dev libgtest-dev
```

**Fix Option 2:** If behind a proxy, configure Git:

```bash
git config --global http.proxy http://proxy.example.com:8080
git config --global https.proxy http://proxy.example.com:8080
```

**Fix Option 3:** Pre-populate the FetchContent cache. Clone dependencies manually into the build directory:

```bash
mkdir -p build/_deps
git clone --branch 11.0.2 https://github.com/fmtlib/fmt.git build/_deps/fmt-src
git clone --branch v1.14.1 https://github.com/gabime/spdlog.git build/_deps/spdlog-src
git clone --branch v3.1 https://github.com/p-ranav/argparse.git build/_deps/argparse-src
git clone --branch v1.15.2 https://github.com/google/googletest.git build/_deps/googletest-src
cmake -B build -DCMAKE_BUILD_TYPE=Release \
    -DFETCHCONTENT_SOURCE_DIR_FMT=build/_deps/fmt-src \
    -DFETCHCONTENT_SOURCE_DIR_SPDLOG=build/_deps/spdlog-src \
    -DFETCHCONTENT_SOURCE_DIR_ARGPARSE=build/_deps/argparse-src \
    -DFETCHCONTENT_SOURCE_DIR_GOOGLETEST=build/_deps/googletest-src
```

### Linker Errors for pthread, rt, or dl

**Symptom:**

```
/usr/bin/ld: cannot find -lpthread
/usr/bin/ld: cannot find -lrt
/usr/bin/ld: cannot find -ldl
```

**Cause:** These are system libraries provided by glibc. On minimal container images or stripped-down installations, the development files may be missing.

**Fix:**

```bash
sudo apt install -y libc6-dev
```

On Alpine Linux (musl-based), `rt` and `dl` are part of musl and do not need separate linking. Uranite is not tested on musl; glibc is the supported C library.

### Out of Memory During Build

**Symptom:** The build process is killed by the OOM killer, or `g++` exits with signal 9 during compilation of large files like `ir/mir/codegen.cpp`.

**Cause:** Parallel compilation of multiple large translation units exceeds available RAM. `codegen.cpp` alone can consume 1-2 GB of RAM during compilation with optimizations enabled.

**Fix:** Reduce parallelism:

```bash
make -C build -j2
```

Or build with reduced optimization for the initial build:

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug
make -C build -j$(nproc)
```

Debug builds use less memory during compilation because the optimizer is not running aggressive inlining and loop unrolling passes.

On machines with less than 4 GB of RAM, use `-j1` and consider adding swap space:

```bash
sudo fallocate -l 4G /swapfile
sudo chmod 600 /swapfile
sudo mkswap /swapfile
sudo swapon /swapfile
make -C build -j2
```

### Permission Denied on Install

**Symptom:**

```
CMake Error at cmake_install.cmake: file INSTALL cannot copy file ... Permission denied
```

**Cause:** `cmake --install` writes to `/usr/local/` which requires root privileges.

**Fix:** Use `sudo`:

```bash
sudo cmake --install build
```

Or install to a user-writable directory:

```bash
cmake --install build --prefix ~/.local
```

Then ensure `~/.local/bin` is in your `PATH`.
