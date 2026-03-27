# SKALED repository onboarding for Copilot coding agent
Use this file first. It is written to minimize failed commands and unnecessary
searching.

## 1) Repository summary
- SKALED is the SKALE blockchain client (C++), EVM-compatible, with consensus,
  networking, RPC, historic-state, and snapshot support.
- Project type: large C++ monorepo with many internal libraries + git
  submodules.
- Build system: CMake (C++20).
- Main runtime target: Linux **Ubuntu 22.04**.
- Always prefer Ubuntu 22.04 for agent work to match CI behavior.

## 2) High-level layout (start here before searching)
Top-level files and folders most often needed:
- `CMakeLists.txt` - root build orchestration and feature flags.
- `.clang-format` - formatting rules (`ColumnLimit: 100`).
- `.github/PULL_REQUEST_TEMPLATE.md` - required PR sections.
- `.github/workflows/` - CI pipelines.
- `skaled/main.cpp` - main binary entrypoint.
- `libweb3jsonrpc/` - Ethereum/JSON-RPC handlers.
- `libethereum/`, `libevm/`, `libhistoric/`, `libskale/` - core
  runtime subsystems (state/execution and tx flow and historic-state).
- `libconsensus/` - `skale-consensus` submodule used by `skaled` for consensus
  integration.
- `test/` - unit/integration/historic tests, plus `testeth` usage.
- `deps/build.sh` - dependency bootstrap script used by CI.

### Repository folder structure (quick map)
- `.github/` - CI workflows, PR template, and Copilot instructions.
- `cmake/` - CMake modules/toolchain helpers used by root build.
- `deps/` - third-party dependency bootstrap scripts/build artifacts.
- `docs/` - feature and operational documentation.
- `libbatched-io/` - batched database I/O layer.
- `libconsensus/` - `skale-consensus` submodule integration.
- `libdevcore/`, `libdevcrypto/` - core utility/crypto primitives.
- `libethcore/`, `libethereum/`, `libevm/` - EVM/block
  execution and blockchain logic.
- `libhistoric/` - historic state and archive-specific execution/state logic.
- `libskale/` - SKALE runtime, patches, and snapshot orchestration.
- `libweb3jsonrpc/` - JSON-RPC APIs and request handlers.
- `rlp/` - RLP-related tools/components.
- `scripts/` - operational, test, and packaging scripts.
- `skaled/` - main node binary sources.
- `skale-vm/` - auxiliary tool.
- `storage_benchmark/` - storage benchmark target.
- `test/` - unit/integration/historicstate tests and harnesses.
Documentation pointers:
- `README.md` - install/build/test baseline.
- `docs/getting-started/one-node.md` - local run flow with SGX scripts.
- `docs/features/*.md` - subsystem behavior/spec notes.

### Consensus integration note (important)
- `skale-consensus` is brought in via `libconsensus/` submodule and feeds
  finalized chain progress/config into `skaled`.
- If a change crosses consensus boundary (block/tx format, execution rules,
  config semantics), validate both sides of the interface:
  - consensus-side behavior (`libconsensus/`)
  - execution/RPC/state behavior in `skaled` (`libethereum/`, `libskale/`,
    `libweb3jsonrpc/`).

## 3) CI and checks
Primary workflows:
- `.github/workflows/test.yml`
  - Build/test matrix for default + HISTORIC_STATE + FAIR + BITE + BITE2.
  - Uses dependency bootstrap (`deps/build.sh`) then builds `testeth`.
- `.github/workflows/clang-format-check.yml`
  - clang-format lint (configured for version 11 in CI).
- `.github/workflows/issue_check.yml`
  - PR must have linked issue(s).
- `.github/workflows/cla.yml`
  - CLA gate.
- `.github/workflows/test-all.yml` and `nightly.yml`
  - broad suite + historic validations.

## 4) Build / test / run / lint reference (use when task requires)
### Bootstrap
```bash
git submodule update --init --recursive
cd deps
./build.sh DEBUG=1 PARALLEL_COUNT=$(nproc)
cd ..
```
### Configure/build
```bash
CC=gcc-11 CXX=g++-11 cmake -H. -Bbuild -DCMAKE_BUILD_TYPE=Debug
cmake --build build --target testeth -- -j"$(nproc)"
```
### Test
```bash
cd build/test
NO_ULIMIT_CHECK=1 NO_NTP_CHECK=1 ./testeth -- --all --verbosity 4
```
### Run
```bash
# example from docs
build/skaled/skaled --config test/historicstate/configs/basic_config.json
```
### Lint/format
```bash
clang-format --version
clang-format -i <changed-cpp-files>
```

## 5) Coding and PR standards that frequently gate reviews
Apply these by default to new/updated code:
- Max line length: 100.
- Function length: <= 100 lines.
- English only, correct spelling, professional comments.
- Consistent naming style per file; descriptive searchable names.
- Never ignore errors.
- Avoid raw pointers; null-check immediately before dereference when used.
- Protect concurrently accessed data structures.
- Remove duplicate copy-paste logic.
- No commented-out dead code.
- Prefer logging library usage over raw `cout` in normal paths.
- Breaking functional changes must be protected by timestamped chain patches
  to avoid mixed-version state discrepancies.

### PR Requirements
When creating or reviewing a PR, require all of the following:
1. **Description**
   Clearly describe the changes made and affected behavior.
2. **Tests**
   List developer tests run and new/updated tests.
3. **Performance impacts**
   State no impact, or provide measurement evidence.
4. **New patches**
   Document new/updated patch activation details when behavior changes.
5. **Linked issue**
   Link the issue tracked by CI policy.

Any breaking functional change that can alter block execution/state root
across versions must be gated by a patch mechanism (`SchainPatch*`,
`*PatchTimestamp` config).

## 6) Agent behavior rule
Trust this file first. Only perform repository-wide searching when:
1. information here is missing for the current task, or
2. information here is contradicted by current files/CI logs.

## 7) PR review protocol (default when asked to review)
 
When reviewing a PR, prioritize findings over summary.
 
1. Report findings first, sorted by severity: Critical, High, Medium, Low.
3. Focus order: correctness -> consensus determinism -> security -> compatibility -> performance -> style.
4. Do not spend time on style-only nits unless they are CI-gating.
5. If no blocking findings exist, explicitly say so and list residual risks/testing gaps.
