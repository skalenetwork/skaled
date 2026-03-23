# Copilot Instructions for SKALED

## Project Overview

**SKALED** is the SKALE Proof-of-Stake blockchain client — a C++ (C++20) implementation of a fully Ethereum-compatible blockchain node. It supports EVM, Solidity, Metamask, and Truffle, while providing forkless linear chains with subsecond block production, instant finality, and provable Byzantine Fault Tolerant (BFT) security. It is maintained by SKALE Labs and is historically based on Aleth (cpp-ethereum). The current version is **5.0.0**.

Key SKALE-specific features include:
- **SKALE BFT Consensus** (in the `libconsensus` submodule)
- **Forklessness** — linear chain, no block tree
- **Asynchronous consensus** — starts immediately after previous block finalized
- **Feature variants**: HISTORIC_STATE, BITE, BITE2, FAIR (controlled by CMake flags)
- **Snapshot support** for fast node sync
- **JSON-RPC** with both standard Ethereum and SKALE-specific (`skale_*`) methods

---

## Repository Structure

```
skaled/
├── skaled/                 # Main executable (main.cpp ~3000 lines)
├── libdevcore/             # Core utilities (assertions, DB, logging, filesystem)
├── libdevcrypto/           # Cryptographic primitives
├── libethcore/             # Ethereum core types and parameters
├── libevm/                 # Ethereum Virtual Machine
├── libethereum/            # Ethereum state and blockchain logic (75 files)
├── libethashseal/          # Ethash PoW compatibility shim
├── libweb3jsonrpc/         # JSON-RPC server (Ethereum + SKALE methods, 52 files)
├── libp2p/                 # Peer-to-peer networking
├── libskale/               # SKALE-specific functionality (35 files)
├── libskale-interpreter/   # VM instruction interpreter
├── libskutils/             # SKALE utilities
├── libbatched-io/          # Batched database I/O abstraction
├── libhistoric/            # Historic state tracking (enabled with -DHISTORIC_STATE=1)
├── libconsensus/           # SKALE BFT consensus engine (git submodule)
├── evmc/                   # Ethereum VM C interface (git submodule)
├── evmjit/                 # Optional EVM JIT compiler (git submodule)
├── test/                   # Test suite
│   ├── unittests/          # Boost unit tests (544 files, 837+ test cases)
│   ├── integrational/      # Integration tests
│   ├── jsontests/          # JSON test suite (git submodule)
│   └── historicstate/      # Historic state tests (Hardhat + TypeScript)
├── deps/                   # External dependency build scripts
├── docs/                   # Feature documentation
├── cmake/                  # CMake utilities and cable toolchain (git submodule)
├── scripts/                # Build and utility scripts
├── .github/
│   ├── workflows/          # CI/CD GitHub Actions workflows
│   └── actions/            # Custom GitHub Actions (cmake-build, testeth-run)
├── CMakeLists.txt          # Root CMake configuration (C++20, all options)
├── CODING_STYLE.md         # Comprehensive C++ coding standards
├── CHANGELOG.md            # Version history
└── VERSION                 # Current version number
```

---

## Environment Setup

**Officially supported OS: Ubuntu 22.04**

### 1. Clone (submodules are mandatory)

```bash
git clone --recurse-submodules https://github.com/skalenetwork/skaled.git
cd skaled
```

⚠️ **CRITICAL**: If you forget `--recurse-submodules`, the CMake configure step will fail with a fatal error. Fix with:
```bash
git submodule update --init --recursive
```

### 2. Install system packages

```bash
sudo apt update
sudo apt install autoconf build-essential cmake libprocps-dev libtool texinfo wget yasm \
  flex bison btrfs-progs python3 python3-pip gawk git vim doxygen make pkg-config \
  libgnutls28-dev libssl-dev unzip zlib1g-dev libgcrypt20-dev docker.io \
  gcc-11 g++-11 gperf clang-format-11 gnutls-dev nettle-dev \
  libhiredis-dev redis-server google-perftools libgoogle-perftools-dev lcov libv8-dev
```

### 3. Set gcc-11 as default compiler

```bash
sudo update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-11 11
sudo update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-11 11
sudo update-alternatives --install /usr/bin/gcov gcov /usr/bin/gcov-11 11
sudo update-alternatives --install /usr/bin/gcov-dump gcov-dump /usr/bin/gcov-dump-11 11
sudo update-alternatives --install /usr/bin/gcov-tool gcov-tool /usr/bin/gcov-tool-11 11
gcc --version   # should show gcc 11.x
```

### 4. Install latest CMake (snap version, not apt)

The apt CMake is often too old; use snap:

```bash
sudo apt-get purge cmake
sudo snap install cmake --classic
cmake --version   # should show 3.x+
```

### 5. Build external dependencies

```bash
cd deps
# Remove conflicting packages first (as done in CI):
sudo apt-get remove -y libbz2-dev liblz4-dev || true
./build.sh DEBUG=1 PARALLEL_COUNT=$(nproc)
cd ..
```

---

## Building

### Standard Debug build (recommended for development)

```bash
cmake -H. -Bbuild -DCMAKE_BUILD_TYPE=Debug
cmake --build build -- -j$(nproc)
```

### Build only the test suite

```bash
cmake -H. -Bbuild -DCMAKE_BUILD_TYPE=Debug -DTESTS=1
cmake --build build --target testeth -- -j$(nproc)
```

### CMake Build Options

| Flag | Default | Description |
|------|---------|-------------|
| `-DCMAKE_BUILD_TYPE=Debug\|Release\|RelWithDebInfo\|MinSizeRel` | Debug | Build type |
| `-DHISTORIC_STATE=1` | off | Enable historic state tracking |
| `-DBITE=1` | off | Enable BITE transaction variant |
| `-DBITE2=1` | off | Enable BITE2 variant (implies BITE) |
| `-DFAIR=1` | off | Enable FAIR consensus variant (implies BITE + BITE2) |
| `-DCONSENSUS=1` | off | Build full consensus engine |
| `-DTESTS=1` | off | Build test targets |
| `-DTOOLS=1` | off | Build skale-key and skale-vm utilities |
| `-DEVMJIT=1` | off | Enable JIT compiler |
| `-DSKALED_PROFILING=1` | off | Enable profiling (`-pg` flags) |
| `-DSANITIZE=address\|thread\|undefined` | off | Enable sanitizers |
| `-DBUILD_LEVELDB=1` | off | Build LevelDB from source |

### Using ccache (automatically detected)

The build system auto-detects ccache and enables it for faster rebuilds. No manual setup needed.

---

## Running Tests

All tests are in `build/test/testeth`:

```bash
cd build/test

# Run all tests
./testeth -- --all

# Run a specific test suite
./testeth -t SuiteName -- --all

# Run a specific test case
./testeth -t SuiteName/TestCaseName -- --all

# Quick mode
./testeth --express
```

### Test Environment Variables

```bash
export NO_ULIMIT_CHECK=1   # Disable ulimit validation (used in CI)
export NO_NTP_CHECK=1      # Disable NTP sync check (used in CI)
```

### Test Suites of Note

- The suites `jsonrpc`, `customTestSuite`, and `BlockQueueSuite` are excluded from normal CI runs.
- The CI runs testeth at **verbosity level 1** and **verbosity level 4**.
- Historic state tests use Hardhat (TypeScript/Node.js) in `test/historicstate/`.

### Integration / Hardhat Tests

```bash
cd test
npm install
npx hardhat test   # see test/package.json for specific scripts
```

---

## CI/CD

CI is GitHub Actions. Key workflows in `.github/workflows/`:

| Workflow | Trigger | Purpose |
|----------|---------|---------|
| `test.yml` | push (excl. master/beta/stable) | Main matrix build + tests |
| `test-all.yml` | reusable | Comprehensive testing (self-hosted) |
| `clang-format-check.yml` | PR | Code format validation |
| `on_pr.yml` | PR | PR-specific checks |
| `functional-tests.yml` | scheduled | Functional test suite |
| `nightly.yml` | scheduled | Nightly runs |
| `publish.yml` | release | Docker image publication |
| `cla.yml` | PR | CLA check |

### CI Build Matrix (test.yml)

All 8 combinations run in parallel (fail-fast: false):

| Name | CMake Flags |
|------|------------|
| Default | (none) |
| Default-historic | `-DHISTORIC_STATE=1` |
| FAIR | `-DFAIR=1` |
| FAIR-historic | `-DFAIR=1 -DHISTORIC_STATE=1` |
| BITE | `-DBITE=1` |
| BITE-historic | `-DBITE=1 -DHISTORIC_STATE=1` |
| BITE2 | `-DBITE2=1` |
| BITE2-historic | `-DBITE2=1 -DHISTORIC_STATE=1` |

Runs on: `ubuntu-22.04` with GCC 11.

### Custom GitHub Actions

- `.github/actions/cmake-build/action.yml` — Configures and builds a CMake target
- `.github/actions/testeth-run/action.yml` — Runs testeth with configurable verbosity and mode

---

## Code Style and Conventions

All C++ code must comply with **CODING_STYLE.md**. Formatting is enforced by `clang-format-11` with the config in `.clang-format` (based on Chromium style, 100-char column limit).

### Key Rules

**Naming (Golden Rule: Preprocessor = ALL_CAPS; C++ = camelCase)**

| Symbol | Convention | Example |
|--------|-----------|---------|
| Types, templates, enum members | PascalCase | `MyClass`, `MyEnum::Value` |
| Functions, local variables | camelCase | `myFunction()`, `myVar` |
| Preprocessor macros | ALL_CAPS | `MY_MACRO` |
| Function parameters | `_paramName` | `_value` |
| Output-only parameters | `o_paramName` | `o_result` |
| Input/output parameters | `io_paramName` | `io_buffer` |
| `const` variables | `c_name` | `c_maxSize` |
| Global non-const | `g_name` | `g_logger` |
| Static non-const | `s_name` | `s_instance` |
| Class private members | `m_name` | `m_data` |

**Collections:**
- `MyTypes` → `std::vector<MyType>`
- `MyTypeSet` → `std::set<MyType>`
- `MyTypeHash` → `std::unordered_set<MyType>`

**Classes vs Structs:**
- Use `struct` only for all-public, no-virtual-function data aggregates
- Use `class` otherwise; private members prefixed with `m_`
- Property pattern: member `m_foo`, getter `foo()` (or `isFoo()` for bools), setter `setFoo()`

**Other rules:**
- `#pragma once` (not include guards)
- No `using namespace` in headers
- Prefer exceptions over bool/int error return codes
- One declaration per line; declare close to first use
- Non-trivial parameters passed as `const&`
- Doxygen comments with `@` notation on interfaces

### Code Format Check

```bash
# Check formatting (run from repo root):
find . -path ./build -prune -o -path ./deps -prune -o -path ./evmc -prune \
  -o -path ./evmjit -prune -o -path ./libconsensus -prune \
  -o \( -name '*.cpp' -o -name '*.h' \) -print | \
  xargs clang-format-11 --dry-run --Werror

# Auto-fix formatting:
find . ... | xargs clang-format-11 -i
```

---

## Logging

Logging uses the SKALE log system (see `libdevcore/`). Verbosity levels:

| Level | Use |
|-------|-----|
| 0 | Critical user-facing messages |
| 1 | Non-essential user messages |
| 2+ | Repetitive messages |
| 3+ | Developer-level details |
| 4+ | Low-level debug (peer disconnects, timers, etc.) |

---

## Key Files to Know

| File | Purpose |
|------|---------|
| `CMakeLists.txt` | Root build config; all options and targets defined here |
| `skaled/main.cpp` | Main daemon entry point (~3000 lines) |
| `libethereum/Client.h/.cpp` | Core Ethereum client logic |
| `libweb3jsonrpc/Eth.cpp` | Ethereum JSON-RPC method implementations |
| `libweb3jsonrpc/Skale.cpp` | SKALE-specific JSON-RPC methods |
| `libskale/SkaleHost.cpp` | SKALE consensus host integration |
| `libdevcore/Log.h` | Logging macros and verbosity control |
| `libdevcore/Common.h` | Common types and utilities |
| `test/unittests/` | All unit test files |
| `deps/build.sh` | External dependency builder |
| `CODING_STYLE.md` | Full C++ coding standards |
| `CHANGELOG.md` | Version history |
| `docs/` | Feature-level documentation |

---

## Common Errors and Workarounds

### Error: CMake fatal error about missing submodules
**Cause:** Repository cloned without `--recurse-submodules`.  
**Fix:** `git submodule update --init --recursive`

### Error: CMake version too old
**Cause:** System apt cmake is outdated.  
**Fix:**
```bash
sudo apt-get purge cmake
sudo snap install cmake --classic
```

### Error: Dependency build failures (bz2 / lz4 conflicts)
**Cause:** System packages `libbz2-dev` or `liblz4-dev` conflict with deps builds.  
**Fix:** `sudo apt-get remove -y libbz2-dev liblz4-dev`

### Error: Compiler errors with wrong gcc version
**Cause:** System defaulting to gcc other than 11.  
**Fix:** Set gcc-11 as default using `update-alternatives` (see Environment Setup section).

### Error: Build fails because deps not built
**Cause:** `deps/build.sh` has not been run before the main CMake build.  
**Fix:** `cd deps && ./build.sh DEBUG=1 PARALLEL_COUNT=$(nproc) && cd ..`

### Error: `libv8-dev` not found
**Cause:** V8 library not installed; required for binaryen/testeth.  
**Fix:** `sudo apt install libv8-dev`

### Test failures related to time/ulimits in CI-like environments
**Fix:** Set these environment variables before running:
```bash
export NO_ULIMIT_CHECK=1
export NO_NTP_CHECK=1
```

---

## Feature Flags and Conditional Compilation

Many features are gated by preprocessor defines set via CMake options:

| CMake Option | Preprocessor Define | Affected Code |
|-------------|---------------------|---------------|
| `-DHISTORIC_STATE=1` | `HISTORIC_STATE` | `libhistoric/`, historic JSON-RPC methods |
| `-DBITE=1` | `BITEVM` | BITE transaction handling |
| `-DBITE2=1` | `BITEVM2` | BITE2 variant (also sets BITEVM) |
| `-DFAIR=1` | `FAIRVM` | FAIR consensus (also sets BITEVM, BITEVM2) |

When making changes that relate to any of these features, ensure code compiles and tests pass in all relevant matrix variants.

---

## Documentation

- `README.md` — Project overview and quickstart build instructions
- `CODING_STYLE.md` — C++ coding standards (read before writing any code)
- `CHANGELOG.md` — Version history
- `docs/features/json-rpc-interface.md` — JSON-RPC method reference
- `docs/features/snapshots.md` — Snapshot functionality
- `docs/features/bite-transactions.md` — BITE transaction specification
- `docs/features/tracing.md` — Transaction tracing
- `docs/features/threshold-encryption.md` — Threshold encryption
- `docs/features/state-root-calculation.md` — State root computation
- `docs/databases-info.md` — Database schema
- `docs/getting-started/one-node.md` — Single-node setup

---

## Contributing

- All development goes into the `develop` branch
- Read `CODING_STYLE.md` thoroughly before submitting code
- Code formatting is enforced in CI via `clang-format-check.yml`
- All 8 CI matrix variants must pass before merging
- Use GitHub Issues for bugs and feature requests
- Community discussions: Discord at https://discord.gg/vvUtWJB
- Issues labeled `help wanted` are newcomer-friendly
