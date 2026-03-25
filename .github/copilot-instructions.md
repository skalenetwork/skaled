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
├── CODING_STYLE.md         # Legacy C++ coding style reference (not enforced)
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

Code formatting is enforced by `clang-format-11` with the config in `.clang-format` (based on Chromium style, 100-char column limit). All naming conventions, structure rules, and quality guidelines are described in the **Code Review Guidelines** section below.

### Code Format Check

```bash
# Check formatting (requires a configured build directory):
cd build && make format-check

# Auto-fix formatting:
cd build && make format
```

---

## Logging

Logging uses the SKALE log system (Boost.Log backend, see `libdevcore/Log.h`). Each log site uses a dedicated macro that maps to a fixed verbosity level and channel.

| Macro | Verbosity | Channel | Use |
|-------|-----------|---------|-----|
| `cerror` | 0 (`VerbosityError`) | `error` | Errors that must always be visible |
| `cwarn` | 1 (`VerbosityWarning`) | `warn` | Warnings: non-fatal problems users should see |
| `cnote` | 2 (`VerbosityInfo`) | `info` | Normal operational messages (default visible level) |
| `cdebug` | 3 (`VerbosityDebug`) | `debug` | Developer-level detail, enabled with `-v 3` |
| `ctrace` | 4 (`VerbosityTrace`) | `trace` | High-frequency low-level detail, enabled with `-v 4` |

For a custom channel, use `clog(severity, "channel")`:
```cpp
clog(VerbosityDebug, "mymodule") << "message";
```

**Verbosity guidelines:**
- Level 0 — Reserved for critical messages users must see and can understand
- Level 1 — Non-essential user messages (e.g., startup banners)
- Level 2+ — Anything that may repeat more than once per minute
- Level 3+ — Developer-only details
- Level 4+ — Low-level events (e.g., peer disconnects, timer cancellations)

**Rules:**
- Never use `std::cout` — always use the logging macros
- Do not spam `cnote`/`cwarn` in hot paths; use `cdebug` or `ctrace`
- No ANSI colour codes — logs are consumed by ElasticSearch and similar tools

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
| `-DBITE=1` | `BITE` | BITE transaction handling |
| `-DBITE2=1` | `BITE2` (also sets `BITE`) | BITE2 variant |
| `-DFAIR=1` | `FAIR` (also sets `BITE` and `BITE2`) | FAIR consensus |

When making changes that relate to any of these features, ensure code compiles and tests pass in all relevant matrix variants.

---

## Ethereum Compatibility

The VM is **fully compatible with the Istanbul fork** and all earlier Ethereum forks. Later forks (post-Istanbul) are **partially supported**. The **Cancun** (Dencun) and **Pectra** forks are **not supported**.

The full JSON-RPC API specification for the VM is documented at:
`docs/json-rpc-interface.md` (a versioned reference is published at https://github.com/skalenetwork/skaled/blob/v4.1.0/docs/json-rpc-interface.md)

### EVM Compatibility Overview

| Category | Status | Details |
|----------|--------|---------|
| Istanbul and earlier | **Fully supported** | All opcodes, gas costs, and precompiles behave identically to a canonical Ethereum node |
| Berlin, London (post-Istanbul) | **Partially supported** | Most JSON-RPC methods work; some EIPs from these forks are available via `SchainPatch` activation |
| Shanghai | **Partially supported** | `PUSH0` (EIP-3855) is available via `PushZeroPatch`; other Shanghai EIPs (EIP-3651, EIP-3860, EIP-4895) are not implemented |
| Cancun (Dencun) | **Not supported** | EIP-4844 (blobs), EIP-1153 (transient storage opcodes `TLOAD`/`TSTORE`), and EIP-5656 (`MCOPY`) are not available |
| Pectra and later | **Not supported** | No EIPs from Pectra or later are implemented |

**What "partially supported" means:** The node accepts and processes transactions, and the JSON-RPC API responds correctly for standard queries. However, opcodes and precompiles introduced after Istanbul may not be present, or their gas costs may differ from canonical Ethereum. New functionality is introduced incrementally through the patch system (see [Patches](#patches) in the Code Review section).

### Custom Precompiles

SKALED extends the standard Ethereum precompile set with SKALE-specific contracts. All addresses are configured per-chain in the genesis `accounts` section. The implementations live in `libethereum/Precompiled.cpp`.

**Standard Ethereum precompiles (addresses 0x01–0x08):**

| Address | Name | Standard |
|---------|------|----------|
| `0x01` | `ecrecover` | EIP-1 |
| `0x02` | `sha256` | EIP-1 |
| `0x03` | `ripemd160` | EIP-1 |
| `0x04` | `identity` | EIP-1 |
| `0x05` | `modexp` | EIP-198 |
| `0x06` | `alt_bn128_G1_add` | EIP-196 |
| `0x07` | `alt_bn128_G1_mul` | EIP-196 |
| `0x08` | `alt_bn128_pairing_product` | EIP-197 |

**SKALE File Storage precompiles (addresses 0x0A–0x11):**

These provide on-chain file storage access. Enabled via `RevertableFSPatch`.

| Address | Name | Description |
|---------|------|-------------|
| `0x0A` | `readChunk` | Read a file chunk |
| `0x0B` | `createFile` | Create a new file (restricted access) |
| `0x0C` | `uploadChunk` | Upload a chunk to a file |
| `0x0D` | `getFileSize` | Get the size of a file |
| `0x0E` | `deleteFile` | Delete a file |
| `0x0F` | `createDirectory` | Create a directory |
| `0x10` | `deleteDirectory` | Delete a directory |
| `0x11` | `calculateFileHash` | Compute SHA-256 hash of a file |

**SKALE Config / Utility precompiles:**

| Address | Name | Description |
|---------|------|-------------|
| `0x12` | `logTextMessage` | Emit a log message at a specified verbosity level |
| `0x13` | `getConfigVariableUint256` | Read a uint256 value from the node config (e.g., node IDs) |
| `0x14` | `getConfigVariableAddress` | Read an address value from the node config |
| `0x15` | `getConfigVariableString` | Read a string value from the node config |
| `0x17` | `getConfigPermissionFlag` | Check a permission flag from the node config |

**SKALE Consensus / IMA precompiles:**

| Address | Name | Description |
|---------|------|-------------|
| `0x18` | `getBlockRandom` | Return a deterministic 256-bit random value for the current block (see [docs](https://docs.skale.space/building-applications/random-number-generation/)) |
| `0x19` | `getIMABLSPublicKey` | Return the IMA BLS public key for the current rotation |


### Transaction Types

SKALED supports the following Ethereum transaction types:

| Type | Standard | Notes |
|------|----------|-------|
| Type-0 | Legacy | Fully supported |
| Type-1 | Access List (EIP-2930) | Fully supported |
| Type-2 | EIP-1559 | Supported at the API level (see gas model note below) |

### Gas Price Model

SKALED follows Ethereum's **pre-EIP-1559** gas pricing behavior regardless of transaction type:

- Transaction fee is calculated as: `transactionFee = gasUsed × gasPrice`
- For Type-0 and Type-1 transactions: `gasPrice` is used normally.
- For Type-2 transactions: `maxFeePerGas` is interpreted as `gasPrice`; `maxPriorityFeePerGas` is ignored.

### Known Limitations

- **`eth_getLogs`**: Supports querying at most **2000 blocks** per request.

### Checking EVM JSON API Compatibility

Any change touching EVM execution, gas schedules, opcodes, or transaction handling **must** be validated against the [Ethereum JSON-RPC API specification](https://ethereum.github.io/execution-apis/api-documentation/) and the EVM test suite:

```bash
# Run the JSON test suite (requires jsontests submodule)
cd build/test
./testeth -t GeneralStateTests -- --all
./testeth -t VMTests -- --all
```

- All standard `eth_*` JSON-RPC methods must behave identically to a standard Ethereum node at Istanbul level.
- SKALE-specific extensions use the `skale_*` namespace and must not collide with Ethereum standard methods.
- EVM schedule parameters for each fork are in `libethcore/EVMSchedule.h`; fork activation block numbers are in `libethcore/ChainOperationParams.h`.

---

## Key Functionality Overview

### Snapshot System

Snapshots allow nodes joining a SKALE chain (rotation or recovery) to sync quickly without replaying the entire chain from genesis.

**How it works:**
1. At configurable time intervals, the node creates a **btrfs filesystem snapshot** of its data directory.
2. A hash (snapshot state root) is computed over all snapshot contents and stored alongside as `snapshot_hash.txt`.
3. When a new node needs to join, it collects BLS-signed snapshot hashes from ≥ 2/3+1 existing nodes to agree on a canonical snapshot.
4. The new node downloads the snapshot in chunks via `skale_downloadSnapshotFragment`, verifies the hash, and restores it locally.

**Snapshot interval:**
Configured via `snapshotIntervalSec` in chain parameters. A snapshot is taken whenever a new block's timestamp crosses the next `snapshotIntervalSec` boundary (i.e., `blockTimestamp / snapshotIntervalSec` increments). Setting `snapshotIntervalSec ≤ 0` disables snapshots. An initial snapshot of block 0 is always created at startup.

**What is included in a snapshot:**

Each snapshot is a set of **btrfs read-only subvolume snapshots** of the following directories:

| Volume | Condition |
|--------|-----------|
| Chain DB (`<chainName>/` directory) | Always |
| `filestorage/` | Excluded in FAIR builds |
| `prices_<nodeId>.db` | Always |
| `blocks_<nodeId>.db` | Always |
| `historic_roots/` | Only with `-DHISTORIC_STATE=1` (archive builds) |
| `historic_state/` | Only with `-DHISTORIC_STATE=1` (archive builds) |

**Snapshot state root (hash):**
The snapshot hash is computed using the libsecp256k1 SHA-256 implementation (`secp256k1_sha256_write` / `secp256k1_sha256_finalize`) over the following components in order:
1. The state database (`state` LevelDB under the chain volume)
2. The blocks-and-extras database chunks
3. All file storage contents (file path hash + file content hash for files; directory path hash for directories)
4. The latest gas price value from `prices_<nodeId>.db`

The resulting 256-bit hash is written to `snapshot_hash.txt` inside the snapshot directory. This hash is what nodes sign with BLS to prove agreement on a canonical chain state.

**Data directory layout:**

```
dataDir/
├── snapshots/
│   ├── 0/                    (genesis snapshot, kept permanently)
│   │   ├── <chainVolume>/    (btrfs read-only subvolume)
│   │   ├── filestorage/
│   │   └── snapshot_hash.txt
│   └── <blockN>/             (named by block number)
│       ├── <chainVolume>/
│       ├── filestorage/
│       └── snapshot_hash.txt
├── diffs/                    (incremental btrfs send/receive diffs)
├── <chainVolume>/            (active chain data btrfs subvolume)
├── filestorage/              (active file storage btrfs subvolume)
├── prices_<nodeId>.db/
└── blocks_<nodeId>.db/
```

The node retains the **last 2 snapshots** with valid hashes (plus the genesis block 0 snapshot which is never deleted). Older snapshots are deleted via btrfs subvolume deletion. The **last 2 diffs** are kept independently using the same count policy.

**Key files:**
- `libskale/SnapshotManager.h/.cpp` — snapshot creation, hash computation, storage, cleanup
- `libethereum/SnapshotAgent.h/.cpp` — interval scheduling and hash thread management
- `libskale/SnapshotHashAgent.h/.cpp` — BLS signature collection from peer nodes
- `libweb3jsonrpc/Skale.cpp` — JSON-RPC endpoints

**Snapshot JSON-RPC methods:**

| Method | Purpose |
|--------|---------|
| `skale_getLatestSnapshotBlockNumber` | Get the most recent snapshot block number |
| `skale_getSnapshot` | Prepare a snapshot for download |
| `skale_downloadSnapshotFragment` | Download a chunk of the snapshot |
| `skale_getSnapshotSignature` | Retrieve the BLS threshold signature for a snapshot hash |

**Important:** Snapshots require **btrfs** as the underlying filesystem for the data directory.

---

### Transaction Flow

A transaction follows this path from submission to block finalization:

```
eth_sendRawTransaction (libweb3jsonrpc/Eth.cpp)
  → Client::submitTransaction (libethereum/Client.cpp)
    → TransactionQueue::import (libethereum/TransactionQueue.h)
      → SkaleHost::pendingTransactions (libskale/SkaleHost.cpp)
        → Consensus engine (libconsensus/)
          → Client::importTransactionsAsBlock (libethereum/Client.cpp)
            → Block::executeTransactions (libethereum/Block.cpp)
              → EVM execution (libevm/)
```

**Details:**
1. **Submission**: `eth_sendRawTransaction` decodes and validates the signed transaction, then calls `Client::submitTransaction`.
2. **Queue**: The transaction enters `TransactionQueue`, which maintains a *current* queue (correct nonce) and a *future* queue (future nonce). Default limit is 1024 entries per queue.
3. **Consensus proposal**: The consensus engine calls `SkaleHost::pendingTransactions()` to retrieve transactions ready for inclusion in the next block proposal.
4. **Block execution**: After consensus agrees on a block, `Client::importTransactionsAsBlock()` executes each transaction in the EVM, updates state, and generates receipts.
5. **Finalization**: The new state root is written, the block is appended to the blockchain, and included transactions are removed from the queue.

**BITE mode note:** When compiled with `-DBITE=1`, transactions can carry encrypted payloads. The consensus engine decrypts these fields before passing them to `importTransactionsAsBlock`. The `DecryptedTransactionFieldsMap` carries the plaintext data.

**Key files:**
- `libweb3jsonrpc/Eth.cpp` — JSON-RPC transaction submission
- `libethereum/Client.cpp` — `submitTransaction`, `importTransactionsAsBlock`
- `libethereum/TransactionQueue.h` — pending transaction pool
- `libskale/SkaleHost.cpp` — consensus ↔ client bridge
- `libethereum/Block.cpp` — per-block EVM execution loop

---

### Historic State (Archive Mode)

When built with `-DHISTORIC_STATE=1`, SKALED operates as an **archive node** that stores the complete state trie at every block height. This enables querying past contract state and full transaction tracing.

**What it enables:**
- Query account balances, storage, and code at any historical block.
- Geth-compatible transaction tracing (`debug_traceTransaction`, `debug_traceCall`, etc.).
- Pre-state and post-state inspection for debugging and auditing.

**How it works:**
- `HistoricState` (in `libhistoric/`) maintains a separate database indexed by block number alongside the normal state database.
- On each block, the state trie for that block is committed to the historic DB.
- Trace requests replay the transaction within the historic state to reconstruct execution.

**Historic-only JSON-RPC methods (require `HISTORIC_STATE` build):**

| Method | Purpose |
|--------|---------|
| `debug_traceTransaction` | Full EVM trace for a transaction |
| `debug_traceCall` | Simulate and trace a call at a given block |
| `debug_traceBlockByNumber` | Trace all transactions in a block |
| `debug_traceBlockByHash` | Trace all transactions in a block (by hash) |
| `debug_accountRangeAt` | Enumerate accounts at a block |
| `debug_storageRangeAt` | Enumerate contract storage at a block |

**Supported tracers:** `callTracer`, `4byteTracer`, `prestateTracer`, `noopTracer`, `replayTracer`, `allTracer`

**Key files:**
- `libhistoric/HistoricState.h/.cpp` — core historic state DB
- `libhistoric/AlethExecutive.h/.cpp` — transaction replay with tracing
- `libhistoric/TracePrinter.h/.cpp` — trace output formatting
- `libweb3jsonrpc/Debug.cpp` / `Tracing.cpp` — JSON-RPC trace endpoints

---

## Code Review Guidelines

When reviewing or writing code in this repository, apply these standards (aligned with SKALE consensus project guidelines):

### Patches

> ⚠️ **Critical**: Any code change that alters EVM execution behavior **must** be gated behind a `SchainPatch`. Deploying unguarded EVM behavior changes causes **state root mismatches** between nodes, breaking consensus.

**How patches work:**
- Each behavioral change is represented as a named entry in `SchainPatchEnum` (`libethereum/SchainPatchEnum.h`).
- A concrete patch class derives from `SchainPatch` and exposes `isEnabledInWorkingBlock()`.
- The patch activation timestamp is set per-chain in `chainParams.patchTimestamps`. A patch is inactive until the committed block timestamp passes that value.
- FAIR builds have a static pre-enabled/pre-disabled set for some patches (see `SchainPatch::preEnabledForFAIR`).

**Examples of existing patches:** `EIP1559TransactionsPatch`, `PushZeroPatch`, `ContractStoragePatch`, `StorageDestructionPatch`, `SkipInvalidTransactionsPatch`, `FlexibleDeploymentPatch`.

**When reviewing:**
- If a PR changes gas accounting, opcode semantics, transaction validation, or contract execution — verify the change is wrapped in a `SchainPatch`.
- A patch-free EVM behavior change is a **blocking review issue**.

### Logic and Correctness
- Verify changes are logically correct, complete, and fully satisfy the intended functionality.
- Identify incorrect behaviors, missing edge cases, and violations of expected input/output contracts.
- For EVM/transaction changes: validate against the Ethereum JSON API spec and run the JSON test suite.

### Code Quality
- **Line length**: ≤ 100 characters in source files.
- **Function length**: ≤ 100 lines per function.
- All comments and identifiers must be in **English with correct spelling**.
- Use consistent naming style — no mixing `snake_case` with `camelCase` in the same file.
- Names must be descriptive and searchable (e.g., `userAccountBalance`, not `x` or `data`).

### Function Best Practices
- Each function must perform **one task only**.
- Every function needs: a clear name or preceding comment, input validation (preconditions), and output validation (postconditions).
- Avoid deeply nested logic; prefer early returns.

### Variables and Fields
- All global or class fields must be well-named or have a comment explaining their purpose.
- Avoid single-letter variable names except for loop counters (`i`, `j`).

### Error Handling
- Never ignore errors or silently discard return values.
- Handle all error cases explicitly with conditionals or exceptions.
- Use exceptions only for exceptional, non-recoverable cases — not for normal control flow.
- Prefer exceptions over `assert` for runtime error handling.

### Safety and Concurrency
- Avoid raw pointers unless necessary; always null-check before dereferencing.
- All shared data structures accessed from multiple threads must be protected with mutexes, atomics, or other synchronization primitives.
- Eliminate race conditions and data races.

### Performance
- Use `const` and `const&` wherever applicable to avoid unnecessary copies.
- Prefer `std::move` when transferring ownership.
- Avoid heap allocations and expensive operations inside tight loops or hot paths.
- Cache repeated values or function results locally if reused.

### Resource Management
- Use RAII for managing resources and memory.
- Prefer `std::unique_ptr` / `std::shared_ptr` over raw pointers.
- Ensure deterministic cleanup via destructors or scoped guards.

### Logging
- Do not spam logs; `info`-level logging must be concise.
- Never use `std::cout` or raw output — use the SKALE logging macros (see `libdevcore/Log.h`).
- No ANSI color codes in logs (logs must remain plaintext for ElasticSearch and similar tools).

### Code Hygiene
- No commented-out code.
- No redundant code — factor repeated logic into helper functions.
- Prefer `private` fields and methods; minimize public interface surface.
- Use standard library facilities over custom implementations for common operations.

---

## Documentation

- `README.md` — Project overview and quickstart build instructions
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

- Branching strategy: `release-branch (vX.X.X)` → `develop` → `beta` → `stable`; multiple release branches can exist simultaneously, and the active one is merged into `develop` when ready for release
- Code formatting is enforced in CI via `clang-format-check.yml`
- All 8 CI matrix variants must pass before merging
- Use GitHub Issues for bugs and feature requests
- Community discussions: Discord at https://discord.gg/vvUtWJB
- Issues labeled `help wanted` are newcomer-friendly
