# Project Structure

This guide covers conventions for organizing Uranite projects, from single-file scripts to multi-module applications managed by the package manager. It details the `uranite.yaml` manifest format, the `uranite.lock` lockfile, the standard directory layout, and the module resolution algorithm that translates import statements into filesystem paths.

---

## Table of Contents

- [Project Structure](#project-structure)
  - [Table of Contents](#table-of-contents)
  - [Single-File Programs](#single-file-programs)
  - [Multi-Module Projects](#multi-module-projects)
    - [Standard Directory Layout](#standard-directory-layout)
    - [Scaffolding with uranite-pkg](#scaffolding-with-uranite-pkg)
  - [The Package Manifest (uranite.yaml)](#the-package-manifest-uraniteyaml)
    - [Manifest Schema](#manifest-schema)
    - [Package Identity](#package-identity)
    - [Entry Point](#entry-point)
    - [Package Type](#package-type)
    - [Dependencies](#dependencies)
    - [Development Dependencies](#development-dependencies)
    - [Build Configuration](#build-configuration)
    - [Complete Example](#complete-example)
  - [The Lockfile (uranite.lock)](#the-lockfile-uranitelock)
    - [Lockfile Structure](#lockfile-structure)
    - [Deterministic Resolution](#deterministic-resolution)
  - [Version Constraints](#version-constraints)
  - [Module Resolution](#module-resolution)
    - [Standard Library Imports](#standard-library-imports)
    - [User Module Imports](#user-module-imports)
    - [Include Path Prefix Resolution](#include-path-prefix-resolution)
    - [The Three Candidate Paths](#the-three-candidate-paths)
    - [Resolution Order Summary](#resolution-order-summary)
  - [Standard Library Directory Discovery](#standard-library-directory-discovery)
  - [The __mod__.urn Convention](#the-modurn-convention)
  - [Building and Running with uranite-pkg](#building-and-running-with-uranite-pkg)
    - [Initialize a Project](#initialize-a-project)
    - [Install Dependencies](#install-dependencies)
    - [Build the Project](#build-the-project)
    - [Build and Run](#build-and-run)
    - [Update Dependencies](#update-dependencies)
    - [Regenerate Lockfile](#regenerate-lockfile)
    - [View Dependency Tree](#view-dependency-tree)
    - [Clean Build Artifacts](#clean-build-artifacts)

---

## Single-File Programs

For scripts, prototypes, and learning exercises, no project structure is needed. Write a `.urn` file anywhere on the filesystem and compile it directly:

```uranite
package quicktest

from uranite.io.console import puts

public function main() -> I32:
    puts( "No project structure needed" )
    return 0
```

```bash
./build/uranite quicktest.urn -r
```

Single-file programs require only a `package` declaration and a `public function main() -> I32` entry point. No manifest, no directory hierarchy, no build step beyond the compiler invocation.

Single-file programs can import standard library modules freely. They can also import user-defined modules from the same directory or from directories specified with the `-I` include flag:

```bash
./build/uranite main.urn -I ./libs -o program
```

This adds `./libs` to the module search path, allowing imports like `from mylib.utils import helper` to resolve against files in `./libs/`.

---

## Multi-Module Projects

### Standard Directory Layout

As projects grow beyond a single file, a conventional directory layout keeps code organized:

```
myproject/
├── uranite.yaml              # Package manifest
├── uranite.lock               # Dependency lockfile (auto-generated)
├── src/
│   ├── main.urn              # Entry point (default: src/main.urn)
│   ├── models/
│   │   ├── __mod__.urn       # Module initializer (re-exports)
│   │   ├── user.urn          # Individual module
│   │   └── session.urn       # Individual module
│   ├── services/
│   │   ├── __mod__.urn       # Module initializer
│   │   ├── auth.urn          # Individual module
│   │   └── storage.urn       # Individual module
│   └── utils.urn             # Utility module
├── tests/
│   ├── test-models.urn       # Test files
│   └── test-services.urn     # Test files
├── modules/                   # Downloaded dependencies (managed by uranite-pkg)
└── build/                     # Compiled artifacts (gitignored)
```

This layout is a convention, not a requirement. The compiler does not enforce directory names. What matters is that the `uranite.yaml` manifest points to the correct entry file and that import paths resolve against the project's directory structure.

### Scaffolding with uranite-pkg

The package manager scaffolds a new project with the standard layout:

```bash
mkdir myproject
cd myproject
uranite-pkg init
```

This creates a `uranite.yaml` file in the current directory with default values. It does not create directories or source files; you create those as needed.

---

## The Package Manifest (uranite.yaml)

The `uranite.yaml` file is the project's central configuration. It declares the package identity, entry point, dependencies, and build settings. The package manager reads this file for every `build`, `run`, `install`, and `update` operation.

The manifest uses YAML syntax. It supports string values, inline sequences (`[a, b, c]`), and inline mappings (`{key: value}`). Indentation-based nesting defines sections.

### Manifest Schema

| Field | Type | Default | Required | Description |
|---|---|---|---|---|
| `name` | String | — | Yes | Package name. Must be a valid identifier. |
| `version` | String | — | Yes | Semantic version string (e.g., "1.0.0"). |
| `description` | String | "" | No | One-line package description. |
| `authors` | Sequence | [] | No | List of author names and emails. |
| `license` | String | "" | No | SPDX license identifier (e.g., "MIT", "GPL-3.0"). |
| `entry` | String | "src/main.urn" | No | Path to the entry source file, relative to the project root. |
| `type` | String | "binary" | No | Package type: "binary" or "library". |
| `dependencies` | Mapping | {} | No | Production dependencies with version constraints. |
| `dev-dependencies` | Mapping | {} | No | Development-only dependencies (testing, linting). |
| `build` | Mapping | {} | No | Build configuration overrides. |

### Package Identity

```yaml
name: myproject
version: 1.0.0
description: A web service built with Uranite
authors: ["Alice <alice@example.com>", "Bob <bob@example.com>"]
license: MIT
```

The `name` field is the package's unique identifier in the dependency system. It must be a valid Uranite identifier: starts with a letter or underscore, contains only letters, digits, underscores, and hyphens. Package names starting with "uranite" are reserved for the standard library and official tooling.

The `version` field follows semantic versioning: `MAJOR.MINOR.PATCH` with an optional prerelease label (e.g., "2.0.0-beta.1").

### Entry Point

```yaml
entry: src/main.urn
```

The entry point is the source file that contains `public function main() -> I32`. The path is relative to the project root (the directory containing `uranite.yaml`). The default is "src/main.urn".

For library packages, the entry point is the top-level module file that re-exports the library's public API:

```yaml
type: library
entry: src/lib.urn
```

### Package Type

```yaml
type: binary
```

Two package types are supported:

- **"binary"** — The package compiles to a native executable. Requires a `main` function in the entry file.
- **"library"** — The package is a reusable module intended to be imported by other packages. No `main` function is required. The entry file typically re-exports the library's public API.

### Dependencies

```yaml
dependencies:
  http-client: "^1.2.0"
  json-parser: "~2.0.0"
  crypto-utils: ">=1.0.0, <2.0.0"
```

Each dependency is a key-value pair where the key is the package name and the value is a version constraint string. The resolver evaluates these constraints against available versions to find the highest compatible release.

Dependencies can also specify a source repository for packages not in the default registry:

```yaml
dependencies:
  custom-lib:
    version: "^1.0.0"
    source: "https://github.com/user/custom-lib.git"
```

### Development Dependencies

```yaml
dev-dependencies:
  test-framework: "^3.0.0"
  benchmark-utils: "^1.0.0"
```

Development dependencies are installed during `uranite-pkg install` but are not included when the package is consumed as a dependency by another project. They are typically testing frameworks, benchmark tools, and development utilities.

### Build Configuration

```yaml
build:
  optimization: "3"
  target: "x86_64-linux-gnu"
  output: "build/myproject"
  modules-path: "modules"
  link: ["pthread", "ssl", "crypto"]
  strip: true
  emit-llvm: false
```

| Field | Type | Default | Description |
|---|---|---|---|
| `optimization` | String | "2" | Optimization level: "0", "1", "2", "3", or "fast". Maps to the `-O` flag. |
| `target` | String | "" | Target triple for cross-compilation. Empty uses the host triple. |
| `output` | String | "" | Output binary path. Empty derives from the package name. |
| `modules-path` | String | "modules" | Directory for downloaded dependency sources. |
| `link` | Sequence | [] | External C libraries to link (passed as `-l` flags). |
| `strip` | Boolean | false | Strip debug symbols from the output binary. |
| `emit-llvm` | Boolean | false | Emit LLVM IR instead of a native binary. |

### Complete Example

```yaml
name: sensor-dashboard
version: 2.1.0
description: Real-time sensor data aggregation and visualization
authors: ["hxAri <hxari@proton.me>"]
license: GPL-3.0

entry: src/main.urn
type: binary

dependencies:
  http-server: "^1.4.0"
  json-parser: "^2.0.0"
  websocket: "~1.1.0"

dev-dependencies:
  test-runner: "^3.0.0"

build:
  optimization: "3"
  link: ["pthread", "ssl"]
  strip: true
```

---

## The Lockfile (uranite.lock)

The `uranite.lock` file records the exact resolved versions and integrity hashes for every dependency (direct and transitive). It is generated automatically by `uranite-pkg install` and `uranite-pkg lock`. You should commit this file to version control.

### Lockfile Structure

The lockfile uses a YAML format with one entry per resolved package:

```yaml
lockfile-version: 1

packages:
  http-server:
    version: 1.4.3
    integrity: sha256:a1b2c3d4e5f6...
    source: https://registry.uranite-lang.org/http-server
    dependencies: ["tcp-socket", "http-parser"]

  tcp-socket:
    version: 0.9.1
    integrity: sha256:f6e5d4c3b2a1...
    source: https://registry.uranite-lang.org/tcp-socket
    dependencies: []

  http-parser:
    version: 1.0.2
    integrity: sha256:1a2b3c4d5e6f...
    source: https://registry.uranite-lang.org/http-parser
    dependencies: []
```

Each entry records:

- **version** — The exact version that was resolved (e.g., "1.4.3", not "^1.4.0").
- **integrity** — A cryptographic hash of the source archive for tamper detection.
- **source** — The URL from which the source was downloaded.
- **dependencies** — The list of packages this dependency itself depends on.

### Deterministic Resolution

The lockfile guarantees that every developer, CI pipeline, and deployment environment uses exactly the same dependency versions:

1. **First install** (`uranite-pkg install` with no lockfile): The resolver evaluates all version constraints from `uranite.yaml`, selects the highest compatible version for each dependency, downloads sources, computes integrity hashes, and writes the lockfile.

2. **Subsequent installs** (`uranite-pkg install` with an existing lockfile): The resolver reads the lockfile and downloads the exact versions recorded there, verifying integrity hashes. Version constraints from `uranite.yaml` are not re-evaluated.

3. **Updating** (`uranite-pkg update`): The resolver discards the lockfile and re-evaluates all constraints against the current registry state, potentially selecting newer compatible versions. A new lockfile is written.

4. **Regenerating** (`uranite-pkg lock`): Regenerates the lockfile from the current constraints without downloading packages. Useful for resolving merge conflicts in the lockfile.

---

## Version Constraints

The dependency resolver supports seven constraint operators:

| Operator | Syntax | Meaning |
|---|---|---|
| Exact | `"1.2.3"` | Only version 1.2.3 |
| Caret | `"^1.2.0"` | Compatible with 1.2.0: allows >=1.2.0 and <2.0.0. Permits minor and patch updates. |
| Tilde | `"~1.2.0"` | Close to 1.2.0: allows >=1.2.0 and <1.3.0. Permits only patch updates. |
| Greater than | `">1.0.0"` | Strictly greater than 1.0.0 |
| Greater or equal | `">=1.0.0"` | Greater than or equal to 1.0.0 |
| Less than | `"<2.0.0"` | Strictly less than 2.0.0 |
| Less or equal | `"<=2.0.0"` | Less than or equal to 2.0.0 |

Multiple constraints can be combined with commas for intersection:

```yaml
json-parser: ">=1.5.0, <2.0.0"
```

This accepts any version from 1.5.0 up to (but not including) 2.0.0. The resolver selects the highest version from the available set that satisfies all constraints. If no version satisfies the constraints, the resolver reports an error and aborts.

---

## Module Resolution

When the compiler encounters an import statement, it translates the dot-separated module path into a filesystem path and locates the corresponding `.urn` source file. The resolution algorithm differs between standard library imports and user module imports.

### Standard Library Imports

An import is classified as a standard library import when the first segment of the module path is "uranite".

For the import:

```uranite
from uranite.collection.array-list import ArrayList
```

The compiler processes the path as follows:

1. **Detect stdlib prefix.** The first segment "uranite" identifies this as a standard library import.

2. **Rewrite native segments.** Any occurrence of the "native" segment is replaced with the target architecture identifier (e.g., "x86-64" or "aarch64"). This step only affects imports that include `native` in their path.

3. **Strip the "uranite" prefix.** The leading "uranite" segment is removed, leaving `collection/array-list`.

4. **Build the relative path.** Segments are joined with `/` to form `collection/array-list`.

5. **Locate the standard library directory.** The compiler determines the root of the standard library tree (see [Standard Library Directory Discovery](#standard-library-directory-discovery) below).

6. **Try three candidate paths** against the standard library directory (see [The Three Candidate Paths](#the-three-candidate-paths) below).

For this example, the first candidate `collection/array-list.urn` exists at `stdlibs/collection/array-list.urn`, so it is returned immediately.

### User Module Imports

An import whose first segment is not "uranite" is treated as a user module import.

For the import:

```uranite
from myproject.services.auth import authenticate
```

The compiler searches in this order:

1. **Source directory.** The parent directory of the main source file being compiled. If the compiler was invoked with `./build/uranite src/main.urn`, the search base is `src/`. The compiler looks for `myproject/services/auth.urn` relative to `src/`.

2. **Include paths.** Each directory specified with the `-I` flag is searched in order. If the compiler was invoked with `./build/uranite src/main.urn -I ./libs -I ./vendor`, the compiler searches `./libs/myproject/services/auth.urn` and then `./vendor/myproject/services/auth.urn`.

3. **Include path prefix resolution.** If the direct search fails, the compiler performs a prefix-stripping fallback. It scans each `-I` directory for a `__mod__.urn` file, reads the `package` declaration from that file, and extracts the root package name. If the import path's first segment matches this root package name, the compiler strips the first segment and retries the search from that include path's directory. This allows a dependency at `-I ./modules/http-server` with `package http-server` in its `__mod__.urn` to be imported as `from http-server.client import HttpClient`, resolving to `./modules/http-server/client.urn`.

### Include Path Prefix Resolution

The prefix resolution mechanism enables seamless importing of third-party packages installed by `uranite-pkg`:

1. For each `-I` include path, the compiler checks if `<include-path>/__mod__.urn` exists.
2. If the file exists, the compiler reads the first `package` declaration line.
3. The package name (e.g., "http-server") is extracted and associated with the include path.
4. When resolving an import that starts with "http-server", the compiler:
   - Strips the "http-server" prefix from the import path.
   - Searches the remainder against the associated include path.
   - For example, `from http-server.client import HttpClient` becomes a search for `client.urn` (or `client/Client.urn` or `client/__mod__.urn`) inside the include path directory.

This mapping is cached after the first resolution. Subsequent imports with the same package prefix reuse the cached association without re-reading `__mod__.urn` files.

### The Three Candidate Paths

For any module path that resolves to a relative path like `services/auth`, the compiler tries three filesystem candidates in order:

**Candidate 1: Direct file.**

```
<base>/services/auth.urn
```

Segments are joined with `/` and `.urn` is appended. This is the most common pattern and matches flat module files like `stdlibs/collection/array-list.urn`.

**Candidate 2: Capitalized directory file.**

```
<base>/services/auth/Auth.urn
```

The last segment is capitalized and used as the filename inside a directory named after the module. This pattern supports modules where the directory contains a primary file alongside supporting files. For example, `from myapp.models.user import User` would match `models/user/User.urn`.

**Candidate 3: Module initializer.**

```
<base>/services/auth/__mod__.urn
```

The `__mod__.urn` file is the module initializer pattern. When a directory represents a module, `__mod__.urn` serves as the entry point and typically re-exports the directory's public API. This is the standard pattern used throughout the Uranite standard library: `stdlibs/collection/__mod__.urn` re-exports `ArrayList`, `HashMap`, `HashSet`, and other collection types.

The first candidate that exists on the filesystem is returned. If none of the three candidates exist in any search location, the import fails with a module resolution error.

### Resolution Order Summary

For a standard library import `from uranite.X.Y import Z`:

```
1. Rewrite "native" segments to target architecture
2. Strip "uranite" prefix
3. Find standard library directory (see discovery order below)
4. Try: <stdlibs-dir>/X/Y.urn
5. Try: <stdlibs-dir>/X/Y/Y.urn  (capitalized last segment)
6. Try: <stdlibs-dir>/X/Y/__mod__.urn
```

For a user module import `from A.B.C import Z`:

```
1. Try: <source-dir>/A/B/C.urn
2. Try: <source-dir>/A/B/C/C.urn  (capitalized last segment)
3. Try: <source-dir>/A/B/C/__mod__.urn
4. For each -I <include-path>:
   a. Try: <include-path>/A/B/C.urn
   b. Try: <include-path>/A/B/C/C.urn
   c. Try: <include-path>/A/B/C/__mod__.urn
5. For each -I <include-path> with __mod__.urn containing "package A":
   a. Strip "A" prefix, search for B/C.urn in <include-path>
   b. Strip "A" prefix, search for B/C/C.urn in <include-path>
   c. Strip "A" prefix, search for B/C/__mod__.urn in <include-path>
```

---

## Standard Library Directory Discovery

When the compiler needs to locate the standard library, it searches the following locations in order. The first valid directory found is cached and used for all subsequent standard library imports in the same compilation.

| Priority | Source | Path | Notes |
|---|---|---|---|
| 1 | `-M` flag | User-specified path | `./build/uranite -M /opt/uranite/stdlibs source.urn` |
| 2 | Environment variable | `$URANITE_MODULES_PATH` | Set in shell profile for system-wide override. |
| 3 | Relative to executable | `<exe-dir>/../lib/uranite/stdlibs/` | For installed toolchains where binaries are in `bin/` and modules in `lib/`. |
| 4 | Compile-time constant | Built-in default path | Embedded at build time. Default: `<source-tree>/stdlibs`. |
| 5 | Current working directory | `./stdlibs/` | Fallback for running from the repository root. |
| 6 | Parent traversal | `../stdlibs/`, `../../stdlibs/`, ... | Walks up to 5 parent directories looking for a `stdlibs/` directory. |

A directory is considered valid if it exists on the filesystem. If no valid directory is found after exhausting all six sources, standard library imports fail with a resolution error.

For development builds run from the repository root (`./build/uranite`), priority 4 (the built-in default pointing to `<source-tree>/stdlibs`) is typically the one that resolves. For installed toolchains, priority 3 (relative to the executable) is the expected resolution path.

---

## The __mod__.urn Convention

When a directory represents a module, it contains a `__mod__.urn` file that serves as the module's entry point. This file declares the module's package name and re-exports the public API of the files within the directory.

Example: `stdlibs/collection/__mod__.urn`:

```uranite
package uranite.collection

from uranite.collection.array-list import ArrayList, ArrayListIterator
from uranite.collection.collection import Collection
from uranite.collection.generator import Generator
from uranite.collection.hash-map import HashMap, HashMapIterator
from uranite.collection.hash-set import HashSet, HashSetIterator
from uranite.collection.immutable-sequence import ImmutableSequence
from uranite.collection.map import Map
from uranite.collection.mutable-mapping import MutableMapping
```

This allows users to import from the directory-level module:

```uranite
from uranite.collection import ArrayList, HashMap
```

Instead of importing from individual files:

```uranite
from uranite.collection.array-list import ArrayList
from uranite.collection.hash-map import HashMap
```

Both forms are valid. The `__mod__.urn` re-export pattern is a convenience that aggregates related types under a single import path.

**When to use `__mod__.urn`:**

- When a directory contains multiple related modules that form a logical unit.
- When you want users to import from the directory path rather than individual file paths.
- When a module's public API is spread across multiple implementation files.

**When not to use `__mod__.urn`:**

- For directories that are purely organizational and do not represent a single importable module.
- For leaf modules that consist of a single file (use the direct `.urn` file instead).

---

## Building and Running with uranite-pkg

The package manager orchestrates the full build workflow for projects with a `uranite.yaml` manifest.

### Initialize a Project

```bash
uranite-pkg init
```

Creates a `uranite.yaml` with default values in the current directory.

### Install Dependencies

```bash
uranite-pkg install
```

Reads `uranite.yaml`, resolves all dependency version constraints, downloads package sources into the `modules/` directory, and writes `uranite.lock`. If `uranite.lock` already exists, uses the locked versions instead of re-resolving.

### Build the Project

```bash
uranite-pkg build
```

Invokes the compiler with the entry file from `uranite.yaml`, passes the `modules/` directory as an include path (`-I`), applies build configuration settings (optimization level, target triple, link libraries), and produces the output binary. This is equivalent to:

```bash
./build/uranite src/main.urn \
    -I modules/http-server \
    -I modules/json-parser \
    -O3 \
    -l pthread -l ssl \
    --strip \
    -o build/myproject
```

The package manager constructs this command automatically from the manifest and the resolved dependency tree.

### Build and Run

```bash
uranite-pkg run
```

Builds the project and immediately executes the output binary. Equivalent to `uranite-pkg build` followed by running the output.

### Update Dependencies

```bash
uranite-pkg update
```

Discards the current lockfile, re-evaluates all version constraints against the latest available versions, downloads updated packages, and writes a new lockfile.

### Regenerate Lockfile

```bash
uranite-pkg lock
```

Re-resolves version constraints and writes a new lockfile without downloading packages. Useful for resolving lockfile merge conflicts after a branch merge.

### View Dependency Tree

```bash
uranite-pkg list
```

Prints the resolved dependency tree showing direct and transitive dependencies with their locked versions.

### Clean Build Artifacts

```bash
uranite-pkg clean
```

Removes compiled artifacts from the output directory.
