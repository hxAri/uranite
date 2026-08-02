# Installation

This guide provides step-by-step instructions for installing the Uranite compiler toolchain from source on every supported platform. It covers dependency installation, repository cloning, building, verification, and troubleshooting common failure scenarios.

Uranite is distributed as source only. There are no prebuilt binaries or system packages. The build process produces five binaries from a single CMake project: the compiler (`uranite`), the formatter (`uranite-fmt`), the package manager (`uranite-pkg`), the documentation generator (`uranite-doc`), and the test runner (`uranite-tests`).

---

## Table of Contents

- [Installation](#installation)
  - [Table of Contents](#table-of-contents)
  - [Dependency Overview](#dependency-overview)
    - [Required System Dependencies](#required-system-dependencies)
    - [Auto-Fetched Dependencies](#auto-fetched-dependencies)
  - [Parrot Security OS](#parrot-security-os)
    - [Step 1: System Update](#step-1-system-update)
    - [Step 2: Install Build Tools](#step-2-install-build-tools)
    - [Step 3: Install LLVM 19](#step-3-install-llvm-19)
    - [Step 4: Install Optional System Libraries](#step-4-install-optional-system-libraries)
    - [Step 5: Build](#step-5-build)
    - [Step 6: Verify](#step-6-verify)
  - [Debian and Ubuntu](#debian-and-ubuntu)
    - [Step 1: Install Build Tools](#step-1-install-build-tools)
    - [Step 2: Install LLVM 19](#step-2-install-llvm-19)
    - [Step 3: Optional System Libraries](#step-3-optional-system-libraries)
    - [Step 4: Build](#step-4-build)
  - [Arch Linux and Manjaro](#arch-linux-and-manjaro)
    - [Step 1: Install Dependencies](#step-1-install-dependencies)
    - [Step 2: Verify LLVM Version](#step-2-verify-llvm-version)
    - [Step 3: Build](#step-3-build)
  - [Fedora and RHEL](#fedora-and-rhel)
    - [Fedora 38+](#fedora-38)
    - [RHEL 8 / RHEL 9](#rhel-8--rhel-9)
    - [Build](#build)
  - [macOS](#macos)
    - [Step 1: Xcode Command Line Tools](#step-1-xcode-command-line-tools)
    - [Step 2: Homebrew Dependencies](#step-2-homebrew-dependencies)
    - [Step 3: Configure PATH](#step-3-configure-path)
    - [Step 4: Build](#step-4-build-1)
    - [macOS Limitations](#macos-limitations)
  - [Windows via WSL2](#windows-via-wsl2)
    - [Step 1: Install WSL2](#step-1-install-wsl2)
    - [Step 2: Install Inside WSL2](#step-2-install-inside-wsl2)
    - [File System Performance](#file-system-performance)
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
    - [CMake Install](#cmake-install)
    - [Manual Copy](#manual-copy)
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

| Dependency | Minimum Version | Verification Command |
|---|---|---|
| LLVM | 19.x | `llvm-config-19 --version` |
| CMake | 3.22 | `cmake --version` |
| C++ Compiler | GCC 12+ or Clang 15+ | `g++ --version` or `clang++ --version` |
| GNU Make or Ninja | Any | `make --version` or `ninja --version` |
| Git | Any | `git --version` |

### Auto-Fetched Dependencies

Four libraries are used by the toolchain. CMake first attempts to find each on the system via `find_package()`. If the system package is missing or incompatible, CMake downloads the exact pinned version from GitHub and builds it automatically.

| Library | Pinned Version | System Package (Debian) |
|---|---|---|
| **fmt** | 11.0.2 | `libfmt-dev` |
| **spdlog** | 1.14.1 | `libspdlog-dev` |
| **argparse** | 3.1 | — (always fetched) |
| **GoogleTest** | 1.15.2 | `libgtest-dev` |

You do not need to install these manually. If you prefer system packages for faster rebuilds, install them before running CMake. The result is deterministic: the same pinned version is always used regardless of what the system provides, unless a compatible system package is found first.

---

## Parrot Security OS

Parrot Security OS is the primary development platform for Uranite. These instructions are tested on Parrot 7.x (codename echo).

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

### Step 4: Install Optional System Libraries

These are optional. If present, CMake uses them instead of downloading from GitHub, speeding up subsequent builds.

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

This provides the system linker, standard C/C++ headers, and `clang++`.

### Step 2: Homebrew Dependencies

```bash
brew install cmake llvm@19
```

### Step 3: Configure PATH

Homebrew installs LLVM into a keg-only prefix to avoid conflicting with Apple's system Clang. CMake cannot find it without adding the Homebrew LLVM to your `PATH`.

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

Native Windows is not supported. The compiler and standard library depend on POSIX APIs and Linux syscalls that do not exist on Windows.

### Step 1: Install WSL2

From an elevated PowerShell prompt:

```powershell
wsl --install -d Ubuntu
```

Restart your machine if prompted. After restart, the Ubuntu terminal opens and asks you to create a Linux user account.

### Step 2: Install Inside WSL2

Once inside the WSL2 Ubuntu terminal, follow the [Debian and Ubuntu](#debian-and-ubuntu) instructions in their entirety. WSL2 provides a real Linux kernel, so all Uranite features function identically to a native Linux installation.

### File System Performance

For best build performance, clone the repository into the WSL2 filesystem (`/home/<user>/`), not into a Windows-mounted path (`/mnt/c/`). Builds from `/mnt/c/` can be 5-10x slower due to filesystem translation overhead.

---

## Building from Source

These instructions apply to all platforms after dependencies are installed.

### Cloning the Repository

```bash
git clone https://github.com/uranite-lang/uranite.git
cd uranite
```

### Configure Step

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
```

For debug builds with full symbol information:

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug
```

To explicitly specify the C++ compiler:

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=clang++
```

### Build Step

```bash
make -C build -j$(nproc)
```

Build time on a 4-core machine: 3-5 minutes for Release, 2-4 minutes for Debug. First build is 1-2 minutes longer if CMake needs to fetch and compile dependencies.

For Ninja instead of Make:

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release -G Ninja
ninja -C build
```

### Build Outputs

| Path | Description |
|---|---|
| `build/uranite` | Compiler |
| `build/uranite-fmt` | Formatter and linter |
| `build/uranite-pkg` | Package manager |
| `build/uranite-doc` | Documentation generator |
| `build/uranite-tests` | Unit test runner |
| `build/runtime/` | Runtime libraries (linked automatically into compiled programs) |

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

All 152 tests must pass. Any failure indicates a build configuration problem or a platform incompatibility.

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

Example output:

```
Build Jul 30 2026 12:00:20
Compiler v1.2.0 | Language v2026.8
GCC/G++ v14.2.0
Signature 1eb4363

Host: x86_64-pc-linux-gnu
LLVM Version: 19.1.7
```

Verify that the LLVM version is 19.x.x and the signature matches your build's Git commit hash.

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

If this prints correctly, the compiler, linker, and standard library are all functioning.

---

## System-Wide Installation

### CMake Install

```bash
sudo cmake --install build
```

Default install paths:

| Artifact | Path |
|---|---|
| Binaries | `/usr/local/bin/` |
| Runtime libraries | `/usr/local/lib/uranite/runtime/` |
| Standard library modules | `/usr/local/lib/uranite/stdlibs/` |

Override the prefix:

```bash
sudo cmake --install build --prefix /opt/uranite
```

With a custom prefix, add the bin directory to your `PATH`:

```bash
export PATH="/opt/uranite/bin:$PATH"
```

### Manual Copy

For development machines:

```bash
sudo cp build/uranite build/uranite-fmt build/uranite-pkg build/uranite-doc /usr/local/bin/
```

This copies only the binaries. The compiler will still locate the standard library from the source tree's `stdlibs/` directory when run from the repository root. To override the module path at runtime:

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

If the binary exists at a non-standard path, add its directory to `PATH`:

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

**Fix:** Upgrade to GCC 12+ or Clang 15+:

```bash
sudo apt install g++-12
cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=g++-12
```

### LLVM Header or Library Mismatch

**Symptom:**

```
fatal error: llvm/IR/Module.h: No such file or directory
```

**Cause:** The LLVM runtime package is installed but the development headers (`llvm-19-dev`) are not.

**Fix:**

```bash
sudo apt install llvm-19-dev
```

If you have multiple LLVM versions installed, ensure `llvm-config-19` is the one CMake finds. Remove older versions or set the `PATH` explicitly.

### FetchContent Download Failures

**Symptom:**

```
Failed to download https://github.com/fmtlib/fmt.git
```

**Cause:** Network connectivity issues, corporate proxy, or GitHub rate limiting.

**Fix Option 1:** Install system packages to bypass downloads entirely:

```bash
sudo apt install -y libfmt-dev libspdlog-dev libgtest-dev
```

**Fix Option 2:** If behind a proxy, configure Git:

```bash
git config --global http.proxy http://proxy.example.com:8080
git config --global https.proxy http://proxy.example.com:8080
```

### Linker Errors for pthread, rt, or dl

**Symptom:**

```
/usr/bin/ld: cannot find -lpthread
/usr/bin/ld: cannot find -lrt
/usr/bin/ld: cannot find -ldl
```

**Cause:** On minimal container images or stripped-down installations, the C library development files may be missing.

**Fix:**

```bash
sudo apt install -y libc6-dev
```

### Out of Memory During Build

**Symptom:** The build process is killed by the OOM killer, or the compiler exits with signal 9 during compilation.

**Fix:** Reduce parallelism:

```bash
make -C build -j2
```

Or build with Debug mode, which uses less memory during compilation:

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug
make -C build -j$(nproc)
```

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

**Fix:** Use `sudo`:

```bash
sudo cmake --install build
```

Or install to a user-writable directory:

```bash
cmake --install build --prefix ~/.local
```

Then ensure `~/.local/bin` is in your `PATH`.
