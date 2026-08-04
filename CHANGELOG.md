# Changelog

All notable changes to this project will be documented in this file.

## [5.2.0] - 2026-07-22 (beta)

### Breaking Changes

| Patch Name | Description |
| --- | --- |
| BerlinForkPatch | Enables Berlin fork changes (EIP-2718, EIP-2930, EIP-2929, EIP-2565). |

### Added

- Berlin fork support, including a fix for EIP-2929 replay state-root/DB-usage recovery and a
  nonce-overflow fix in EIP-2929 access-list handling ([#2434](https://github.com/skalenetwork/skaled/pull/2434), [#2482](https://github.com/skalenetwork/skaled/pull/2482), [#2494](https://github.com/skalenetwork/skaled/pull/2494))

### Changed

- Update consensus dependency ([#2507](https://github.com/skalenetwork/skaled/pull/2507))

### Fixed

- Fix mismatch in receipts ([#2486](https://github.com/skalenetwork/skaled/pull/2486))
- Fix silent tx drop when CTQ is full ([#2475](https://github.com/skalenetwork/skaled/pull/2475))

## [5.1.0] - 2026-07-03 (beta)

BITE2 confidential transactions.

### Breaking Changes

| Patch Name | Description |
| --- | --- |
| Bite2Patch | Gates BITE2 features (CTX precompiled contracts and CTX transaction detection) behind a timestamp so nodes activate them synchronously. |
| ContractCreationReadOnlyPatch | Fixes readOnly handling during contract creation. |

### Added

- Merge BITE2 into BITE and migrate transaction import/execution to the unified implementation ([#2350](https://github.com/skalenetwork/skaled/pull/2350), [#2423](https://github.com/skalenetwork/skaled/pull/2423), [#2435](https://github.com/skalenetwork/skaled/pull/2435))
- Add BITE2 precompiled contracts for ECIES/TE encryption, including an encryption counter and
  related fixes ([#2351](https://github.com/skalenetwork/skaled/pull/2351), [#2355](https://github.com/skalenetwork/skaled/pull/2355), [#2369](https://github.com/skalenetwork/skaled/pull/2369), [#2387](https://github.com/skalenetwork/skaled/pull/2387))
- CTX (confidential transaction) lifecycle: origin binding, gas-limit exclusion from block gas
  limit, ciphertext/AAD validation, queue initialization, and related fixes ([#2379](https://github.com/skalenetwork/skaled/pull/2379), [#2396](https://github.com/skalenetwork/skaled/pull/2396), [#2401](https://github.com/skalenetwork/skaled/pull/2401), [#2413](https://github.com/skalenetwork/skaled/pull/2413), [#2418](https://github.com/skalenetwork/skaled/pull/2418), [#2436](https://github.com/skalenetwork/skaled/pull/2436), [#2447](https://github.com/skalenetwork/skaled/pull/2447), [#2457](https://github.com/skalenetwork/skaled/pull/2457), [#2460](https://github.com/skalenetwork/skaled/pull/2460), [#2463](https://github.com/skalenetwork/skaled/pull/2463), [#2464](https://github.com/skalenetwork/skaled/pull/2464))
- Reencryption randomness per block; reject onDecrypt transactions submitted directly through
  JSON-RPC ([#2416](https://github.com/skalenetwork/skaled/pull/2416), [#2426](https://github.com/skalenetwork/skaled/pull/2426))
- Patch-guard BITE2-specific block DB entries ([#2428](https://github.com/skalenetwork/skaled/pull/2428))
- Support float percentiles for eth_feeHistory ([#2451](https://github.com/skalenetwork/skaled/pull/2451))

### Fixed

- Fix readOnly in contract creation ([#2443](https://github.com/skalenetwork/skaled/pull/2443))
- Fix init of previous block on historic call ([#2471](https://github.com/skalenetwork/skaled/pull/2471))

## [5.0.0] - 2026-04-22

FAIR network support; C++20 migration; consensus performance improvements.

### Breaking Changes

| Patch Name | Description |
| --- | --- |
| SingleStateCommitPerBlockPatch | Commits state to the database only once per block. |
| CurrentBlockRandomPatch | Uses the current block (instead of a prior one) in getBlockRandom. |
| GroupIndexInitPatch | Fixes group-index initialization on startup when a node restarts right after a rotation timestamp. |
| DisableSelfDestructPatch | Disables the SELFDESTRUCT opcode for FAIR builds. |

### Added

- FAIR network build support: Dencun opcode handling, pre-FAIR config compatibility, sync-mode
  detection, a constant gas-price agent, FAIR-specific patch infrastructure, on-the-fly committee
  rotation with BITE-encrypted payload, 2-node-group config support, and disabling/replacing
  subsystems incompatible with FAIR builds (self-destruct, Oracle, filestorage, IMA, PoW,
  ConfigController, contractStorageLimit, restricted access, remaining precompiled contracts, legacy
  pre-EIP-155 transactions)
  ([#2173](https://github.com/skalenetwork/skaled/pull/2173), [#2174](https://github.com/skalenetwork/skaled/pull/2174), [#2176](https://github.com/skalenetwork/skaled/pull/2176), [#2177](https://github.com/skalenetwork/skaled/pull/2177), [#2184](https://github.com/skalenetwork/skaled/pull/2184), [#2187](https://github.com/skalenetwork/skaled/pull/2187), [#2188](https://github.com/skalenetwork/skaled/pull/2188), [#2189](https://github.com/skalenetwork/skaled/pull/2189), [#2191](https://github.com/skalenetwork/skaled/pull/2191), [#2210](https://github.com/skalenetwork/skaled/pull/2210), [#2215](https://github.com/skalenetwork/skaled/pull/2215), [#2222](https://github.com/skalenetwork/skaled/pull/2222), [#2224](https://github.com/skalenetwork/skaled/pull/2224), [#2225](https://github.com/skalenetwork/skaled/pull/2225), [#2227](https://github.com/skalenetwork/skaled/pull/2227), [#2231](https://github.com/skalenetwork/skaled/pull/2231), [#2232](https://github.com/skalenetwork/skaled/pull/2232), [#2238](https://github.com/skalenetwork/skaled/pull/2238), [#2242](https://github.com/skalenetwork/skaled/pull/2242), [#2246](https://github.com/skalenetwork/skaled/pull/2246))
- Block rewards: reward wallet support, delayed rewards, split rewards to the staking contract,
  reward-wallet address via node groups, and rewards for the consensus round winner ([#2155](https://github.com/skalenetwork/skaled/pull/2155), [#2249](https://github.com/skalenetwork/skaled/pull/2249), [#2254](https://github.com/skalenetwork/skaled/pull/2254), [#2257](https://github.com/skalenetwork/skaled/pull/2257), [#2275](https://github.com/skalenetwork/skaled/pull/2275))
- BITE transaction support: common public key API, encrypted `to` field, gas charging for
  valid/invalid transactions, transaction validation, encrypted/decrypted tx hash tracking, RPC
  methods, and performance improvements ([#2133](https://github.com/skalenetwork/skaled/pull/2133), [#2134](https://github.com/skalenetwork/skaled/pull/2134), [#2149](https://github.com/skalenetwork/skaled/pull/2149), [#2154](https://github.com/skalenetwork/skaled/pull/2154), [#2158](https://github.com/skalenetwork/skaled/pull/2158), [#2171](https://github.com/skalenetwork/skaled/pull/2171), [#2175](https://github.com/skalenetwork/skaled/pull/2175), [#2213](https://github.com/skalenetwork/skaled/pull/2213), [#2330](https://github.com/skalenetwork/skaled/pull/2330))
- Begin BITE2 groundwork: create CTX (confidential transactions) and generate accounts for BITE2
  transactions ([#2321](https://github.com/skalenetwork/skaled/pull/2321), [#2325](https://github.com/skalenetwork/skaled/pull/2325))
- Set max/min gas price via config ([#2314](https://github.com/skalenetwork/skaled/pull/2314), [#2318](https://github.com/skalenetwork/skaled/pull/2318), [#2425](https://github.com/skalenetwork/skaled/pull/2425))

### Changed

- Significantly speed up eth_getLogs request performance ([#2287](https://github.com/skalenetwork/skaled/pull/2287))
- Consensus dependency updates ([#2167](https://github.com/skalenetwork/skaled/pull/2167), [#2272](https://github.com/skalenetwork/skaled/pull/2272), [#2279](https://github.com/skalenetwork/skaled/pull/2279), [#2307](https://github.com/skalenetwork/skaled/pull/2307))
- Move to C++20 ([#2185](https://github.com/skalenetwork/skaled/pull/2185))
- Increase write buffer size ([#2363](https://github.com/skalenetwork/skaled/pull/2363))
- CI/build improvements: parallel build & test pipeline, GitHub-hosted publish runners, and
  Copilot instructions setup ([#2393](https://github.com/skalenetwork/skaled/pull/2393), [#2411](https://github.com/skalenetwork/skaled/pull/2411), [#2453](https://github.com/skalenetwork/skaled/pull/2453))

### Fixed

- Fix finalization in consensus ([#2253](https://github.com/skalenetwork/skaled/pull/2253), [#2373](https://github.com/skalenetwork/skaled/pull/2373), [#2392](https://github.com/skalenetwork/skaled/pull/2392))
- Fix block random ([#2283](https://github.com/skalenetwork/skaled/pull/2283), [#2320](https://github.com/skalenetwork/skaled/pull/2320), [#2323](https://github.com/skalenetwork/skaled/pull/2323))
- Fix historic-state execution for RNG precompiled ([#2292](https://github.com/skalenetwork/skaled/pull/2292), [#2315](https://github.com/skalenetwork/skaled/pull/2315))
- Fix eth_call for contract deployment ([#2398](https://github.com/skalenetwork/skaled/pull/2398))
- Fix readonly contract deployment ([#2452](https://github.com/skalenetwork/skaled/pull/2452))
- Fix memory leak in HashSnapshot tests ([#2391](https://github.com/skalenetwork/skaled/pull/2391))
- Fix memory usage ([#2201](https://github.com/skalenetwork/skaled/pull/2201))
- Fix heap use-after-free on exit ([#2365](https://github.com/skalenetwork/skaled/pull/2365))
- Remove partial receipts at the start of the next block ([#2308](https://github.com/skalenetwork/skaled/pull/2308))
- Fix epoch increment during catchup ([#2310](https://github.com/skalenetwork/skaled/pull/2310))
- No balance transfer after self-destruct ([#2302](https://github.com/skalenetwork/skaled/pull/2302))
- Fix getBlockRandom determinism ([#2270](https://github.com/skalenetwork/skaled/pull/2270))
- Fix schain node comparison ([#2289](https://github.com/skalenetwork/skaled/pull/2289))
- Fix wrong group index on start ([#2282](https://github.com/skalenetwork/skaled/pull/2282))
- Fix libbls init for snapshot downloading ([#2424](https://github.com/skalenetwork/skaled/pull/2424))
- Snapshot fixes: zero-snapshot download, snapshot hash calculation, download only if datadir is
  empty, and NODE-CLI snapshot-downloading fix ([#2244](https://github.com/skalenetwork/skaled/pull/2244), [#2245](https://github.com/skalenetwork/skaled/pull/2245), [#2260](https://github.com/skalenetwork/skaled/pull/2260), [#2265](https://github.com/skalenetwork/skaled/pull/2265))

## [4.0.2] - 2025-10-13

### Added

- DB rotation in consensus for archive mode ([#2281](https://github.com/skalenetwork/skaled/pull/2281))
- Optional historic container build ([#2266](https://github.com/skalenetwork/skaled/pull/2266))

## [4.0.1] - 2025-05-06

### Fixed

- Fix unDDoS crash under load with added mutex ([#2159](https://github.com/skalenetwork/skaled/pull/2159))
- Protect block with mutex against parallel eth_call requests ([#2160](https://github.com/skalenetwork/skaled/pull/2160))

## [4.0.0] - 2025-04-09

Historic-state rotation; state root fixes.

### Breaking Changes

| Patch Name | Description |
| --- | --- |
| ClearPartialReceiptsPatch | Stops saving partial receipts after a block is executed. |
| InvalidTransactionFormatPatch | Fixes the transaction-constructor check so maxFeePerGas cannot be less than maxPriorityFeePerGas. |

### Added

- Historic state rotation ([#2063](https://github.com/skalenetwork/skaled/pull/2063))

### Changed

- Logging improvements: broadcast logs, SGX logs, LevelDB snapshot logging with DB path, and
  general logging enhancements ([#2095](https://github.com/skalenetwork/skaled/pull/2095), [#2096](https://github.com/skalenetwork/skaled/pull/2096), [#2101](https://github.com/skalenetwork/skaled/pull/2101), [#2131](https://github.com/skalenetwork/skaled/pull/2131))
- Remove debug symbols from release build ([#2072](https://github.com/skalenetwork/skaled/pull/2072))

### Fixed

- Fix handling of block 0 in getBlockByHash/getBlockByNumber and legacy receipts ([#2113](https://github.com/skalenetwork/skaled/pull/2113), [#2139](https://github.com/skalenetwork/skaled/pull/2139))
- Don't close snapshot while in use ([#2142](https://github.com/skalenetwork/skaled/pull/2142))
- Fix invalid transaction in block ([#2125](https://github.com/skalenetwork/skaled/pull/2125))
- Remove old transactions from queue ([#2115](https://github.com/skalenetwork/skaled/pull/2115))
- Fix state root mismatch error on 4.0.0 ([#2107](https://github.com/skalenetwork/skaled/pull/2107))
- Add JSON-RPC version to error responses ([#2064](https://github.com/skalenetwork/skaled/pull/2064))

## [3.21.0] - 2025-01-30

### Changed

- Migrate to Ubuntu 22 and remove the Hunter package manager ([#2044](https://github.com/skalenetwork/skaled/pull/2044), [#2058](https://github.com/skalenetwork/skaled/pull/2058), [#2061](https://github.com/skalenetwork/skaled/pull/2061))
- Update GitHub Actions ([#2055](https://github.com/skalenetwork/skaled/pull/2055))

### Fixed

- Fix catchup timeout ([#2062](https://github.com/skalenetwork/skaled/pull/2062))

## [3.20.1] - 2024-11-28

### Fixed

- Revert ZMQ fix (performance regression) ([#2048](https://github.com/skalenetwork/skaled/pull/2048))

## [3.20.0] - 2024-11-06

### Added

- Archive node snapshot support, including a fix for catchup snapshot priority on archive
  nodes ([#1937](https://github.com/skalenetwork/skaled/pull/1937), [#2032](https://github.com/skalenetwork/skaled/pull/2032))
- Tracing API server ([#1940](https://github.com/skalenetwork/skaled/pull/1940))
- Enable state root check for sync/indexer/archive nodes ([#2022](https://github.com/skalenetwork/skaled/pull/2022))
- Release build with separate debug info, including a revert and reapply ([#1969](https://github.com/skalenetwork/skaled/pull/1969), [#2013](https://github.com/skalenetwork/skaled/pull/2013), [#2014](https://github.com/skalenetwork/skaled/pull/2014))

### Fixed

- Fix ZMQ assert ([#1948](https://github.com/skalenetwork/skaled/pull/1948))
- Fix SIGSEGV in rollback ([#1947](https://github.com/skalenetwork/skaled/pull/1947))
- Fix skale-vm and test.ClientBase build ([#1968](https://github.com/skalenetwork/skaled/pull/1968))

## [3.19.3] - 2024-09-27

### Breaking Changes

| Patch Name | Description |
| --- | --- |
| ExternalGasPatch | Fixes externalGas calculation. |

### Fixed

- Fix externalGas calculation ([#1999](https://github.com/skalenetwork/skaled/pull/1999))

## [3.19.2] - 2024-09-17

### Fixed

- Fix mismatched `v` in transaction signature ([#1983](https://github.com/skalenetwork/skaled/pull/1983))
- Fix max fee per gas in JSON-RPC representation ([#1985](https://github.com/skalenetwork/skaled/pull/1985))
- Fix chainId in estimateGas call ([#1984](https://github.com/skalenetwork/skaled/pull/1984))

## [3.19.1] - 2024-08-30

### Added

- Add `data` field for reverted calls and estimateGas ([#1960](https://github.com/skalenetwork/skaled/pull/1960))
- API documentation ([#1927](https://github.com/skalenetwork/skaled/pull/1927))

### Fixed

- Fix missing chainId for 1559 transactions ([#1961](https://github.com/skalenetwork/skaled/pull/1961))

## [3.19.0] - 2024-07-11

EIP-1559 support.

### Breaking Changes

| Patch Name | Description |
| --- | --- |
| EIP1559TransactionsPatch | Enables EIP-1559 (type-2) transaction support. |
| VerifyBlsSyncPatch | Enables BLS signature verification for sync nodes. |
| FlexibleDeploymentPatch | Passes both transaction origin and sender to the ConfigController contract. |
| FastConsensusPatch | Enables consensus performance improvements. |

### Added

- EIP-1559 support, including follow-up refinements ([#1869](https://github.com/skalenetwork/skaled/pull/1869), [#1872](https://github.com/skalenetwork/skaled/pull/1872))
- Support `input` field in eth_call, eth_estimateGas ([#1852](https://github.com/skalenetwork/skaled/pull/1852))
- Support blockHash parameter ([#1884](https://github.com/skalenetwork/skaled/pull/1884))
- Colorful/uncolored log output option ([#1880](https://github.com/skalenetwork/skaled/pull/1880))

### Changed

- Consensus improvements ([#1888](https://github.com/skalenetwork/skaled/pull/1888))
- Patch architecture improvements ([#1868](https://github.com/skalenetwork/skaled/pull/1868))
- More consensus catchup logs ([#1918](https://github.com/skalenetwork/skaled/pull/1918))
- ZMQ broadcast high-water-mark limit tuning, including a revert and reapply ([#1849](https://github.com/skalenetwork/skaled/pull/1849), [#1920](https://github.com/skalenetwork/skaled/pull/1920))

### Fixed

- Fix block-by-number error ([#1923](https://github.com/skalenetwork/skaled/pull/1923))
- Fix eth_feeHistory blockCount ([#1914](https://github.com/skalenetwork/skaled/pull/1914))
- Fix BLS sync config ([#1891](https://github.com/skalenetwork/skaled/pull/1891), [#1894](https://github.com/skalenetwork/skaled/pull/1894))

## [3.18.2] - 2024-05-27

### Changed

- Code style / hash improvements ([#1905](https://github.com/skalenetwork/skaled/pull/1905))

## [3.18.1] - 2024-05-14

Transaction tracing support.

### Added

- Add transaction tracing (eth_trace) support: initial implementation, multi-transaction-in-block
  tracing, geth/skaled trace-output consistency fixes, revert-reason and tracer-params fixes, and
  re-enabling the debug API ([#1734](https://github.com/skalenetwork/skaled/pull/1734), [#1783](https://github.com/skalenetwork/skaled/pull/1783), [#1804](https://github.com/skalenetwork/skaled/pull/1804), [#1809](https://github.com/skalenetwork/skaled/pull/1809), [#1831](https://github.com/skalenetwork/skaled/pull/1831), [#1864](https://github.com/skalenetwork/skaled/pull/1864))

### Changed

- More logs for archive node ([#1895](https://github.com/skalenetwork/skaled/pull/1895))

### Fixed

- Historic node: reset DB ([#1854](https://github.com/skalenetwork/skaled/pull/1854))
- checkoutExternalGas fix ([#1867](https://github.com/skalenetwork/skaled/pull/1867))

## [3.18.0] - 2024-02-17

### Breaking Changes

| Patch Name | Description |
| --- | --- |
| PushZeroPatch | Enables the PUSH0 opcode. |

### Added

- FTQ RPC call ([#1776](https://github.com/skalenetwork/skaled/pull/1776))
- Push-zero opcode support ([#1709](https://github.com/skalenetwork/skaled/pull/1709))

### Changed

- Adopt precompiled Oracle ([#1713](https://github.com/skalenetwork/skaled/pull/1713))
- Sync node broadcast transactions ([#1651](https://github.com/skalenetwork/skaled/pull/1651))
- Update consensus for DB usage call fix ([#1685](https://github.com/skalenetwork/skaled/pull/1685))

### Fixed

- Fix zero-snapshot download/upload handling ([#1784](https://github.com/skalenetwork/skaled/pull/1784), [#1790](https://github.com/skalenetwork/skaled/pull/1790))
- Change snapshot hash computation and fix related slow catchup ([#1770](https://github.com/skalenetwork/skaled/pull/1770), [#1787](https://github.com/skalenetwork/skaled/pull/1787))
- Reload rotation timestamp on each isExitTime request ([#1757](https://github.com/skalenetwork/skaled/pull/1757))
- Fix call on historic block ([#1756](https://github.com/skalenetwork/skaled/pull/1756))
- Fix eth_syncing ([#1742](https://github.com/skalenetwork/skaled/pull/1742))
- Cleanup shared space on unsuccessful snapshot download ([#1740](https://github.com/skalenetwork/skaled/pull/1740))
- Fix net_version ([#1717](https://github.com/skalenetwork/skaled/pull/1717))
- Fix SIGTERM handling at exit ([#1695](https://github.com/skalenetwork/skaled/pull/1695))
- Fix microprofile build ([#1690](https://github.com/skalenetwork/skaled/pull/1690))

## [3.17.1] - 2023-11-07

### Fixed

- Fix transaction duplicates ([#1718](https://github.com/skalenetwork/skaled/pull/1718))
- Fix PoW handling ([#1715](https://github.com/skalenetwork/skaled/pull/1715))

## [3.17.0] - 2023-10-20

### Added

- Add signature to getTransaction calls ([#1534](https://github.com/skalenetwork/skaled/pull/1534))
- Add stats to selfdestruct ([#1673](https://github.com/skalenetwork/skaled/pull/1673))
- Add integration tests ([#1483](https://github.com/skalenetwork/skaled/pull/1483))

### Changed

- Threading rules improvements ([#1543](https://github.com/skalenetwork/skaled/pull/1543))
- Block finalization logging improvements ([#1548](https://github.com/skalenetwork/skaled/pull/1548))
- Transaction queue improvements: limit queue size in bytes, sort transactions under lock, fix
  handling of 0 gas-price transactions, and move queue-size logging to info level
  ([#1533](https://github.com/skalenetwork/skaled/pull/1533), [#1599](https://github.com/skalenetwork/skaled/pull/1599), [#1615](https://github.com/skalenetwork/skaled/pull/1615), [#1616](https://github.com/skalenetwork/skaled/pull/1616), [#1705](https://github.com/skalenetwork/skaled/pull/1705))
- Remove unneeded skaled parts for release 2.2 ([#1521](https://github.com/skalenetwork/skaled/pull/1521))
- Clean up build warnings ([#1516](https://github.com/skalenetwork/skaled/pull/1516))

### Fixed

- Snapshot handling: 0-block snapshot download and old-chain fixes, RAM-growth fix in the
  snapshot thread, snapshot init on start, and killing the snapshot-sending monitoring thread
  ([#1528](https://github.com/skalenetwork/skaled/pull/1528), [#1569](https://github.com/skalenetwork/skaled/pull/1569), [#1589](https://github.com/skalenetwork/skaled/pull/1589), [#1591](https://github.com/skalenetwork/skaled/pull/1591), [#1676](https://github.com/skalenetwork/skaled/pull/1676))
- Fix consensus/catchup stalls under load: slow data destruction, skaled stuck fix, and slow
  catchup fix ([#1546](https://github.com/skalenetwork/skaled/pull/1546), [#1566](https://github.com/skalenetwork/skaled/pull/1566), [#1571](https://github.com/skalenetwork/skaled/pull/1571), [#1585](https://github.com/skalenetwork/skaled/pull/1585), [#1593](https://github.com/skalenetwork/skaled/pull/1593), [#1630](https://github.com/skalenetwork/skaled/pull/1630))
- Fix diffs directory missing after cleanup ([#1541](https://github.com/skalenetwork/skaled/pull/1541))
- Fix DB destructor ([#1524](https://github.com/skalenetwork/skaled/pull/1524))
- Fix config validation ([#1556](https://github.com/skalenetwork/skaled/pull/1556))
- Remove SNB ([#1605](https://github.com/skalenetwork/skaled/pull/1605))
- Fix cmake build ([#1643](https://github.com/skalenetwork/skaled/pull/1643))
- Use 21k gas for gasEstimateStep when gas used is less ([#1639](https://github.com/skalenetwork/skaled/pull/1639))
- Fix SIGSEGV in skale_stats queue ([#1635](https://github.com/skalenetwork/skaled/pull/1635))
- Exit 0 on internal exit reason ([#1649](https://github.com/skalenetwork/skaled/pull/1649))
