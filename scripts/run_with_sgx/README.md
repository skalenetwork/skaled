# run_with_sgx

Helper script to launch a single `skaled` node against a running SGX wallet:
it generates ECDSA/BLS keys via SGX, imports them into a config template,
and starts `skaled` with the resulting config.

## Prerequisites

- A running SGX wallet reachable on `127.0.0.1` (default HTTP port `1029`,
  or HTTPS port `1026` with `-HTTPS`). See `utils/spin_default_sgx.sh` if you
  need to spin up a default local SGX wallet container.
- `skaled` already built in `../../build` (this script only **runs** the
  binary, it does not build it). 
  Add `-DHISTORIC_STATE=1` if you plan to run an archive node
  (`-node-type archive`) — this is a compile-time flag and cannot be set by
  this script.

## One-time Environment Setup

This folder uses a local Python virtual environment (`./.venv`) and
`requirements.txt`.

From `scripts/run_with_sgx`:

```bash
# system deps used by sgx.py
sudo apt install swig libudev-dev -y

# python deps
python3 -m venv .venv
source .venv/bin/activate
python3 -m pip install --upgrade pip setuptools wheel
pip install sgx.py==0.10.0.dev0
pip install -r requirements.txt
```

Pip may print a resolver warning about `urllib3` version conflicts after the
install — this is non-fatal and can be ignored as long as the command ends
with `Successfully installed ...`. If you hit `No module named
urllib3.packages.six.moves` at runtime, recreate `.venv` with the same steps
above.

For scripts under `utils/`, run after activating `.venv`:

```bash
python3 utils/prepare_scenario.py ./sample_configs/scenarios/core_historic_local.json
```

## Usage

```bash
bash run.sh [-FAIR] [-HTTPS] [-config <path>] [-node-type <normal|archive>] [-http-port <port>] [-work-dir <path>]
```

### Flags

| Flag | Default | Description |
| --- | --- | --- |
| `-FAIR` | off | Fill FAIR-specific config fields (passed through to `update_config.py -isFair`). |
| `-HTTPS` | off | Talk to the SGX wallet over HTTPS on port `1026` instead of HTTP on port `1029`. Requires `<work-dir>/sgx.crt` and `<work-dir>/sgx.key`. |
| `-config <path>` | `./sample_configs/config.json` | Base config template to fill in with generated keys. For two-node local tests, use `./sample_configs/config_core.json` for core and `./sample_configs/config_historic.json` for historic. |
| `-node-type <normal\|archive>` | `normal` | Sets `skaleConfig.nodeInfo.archiveMode` in the generated config. `archive` requires `skaled` to be built with `-DHISTORIC_STATE=1`. |
| `-http-port <port>` | `1237` | HTTP RPC port passed to `skaled --http-port`. |
| `-work-dir <path>` | `./tmp` | Runtime working directory for generated keys/certs/config/log data (`keys.json`, `updated_config.json`, `data_dir`). |
| `-skip-key-setup` | off | Skip SGX key generation and `update_config.py`; use `-config` as the final config as-is. Required when launching nodes from Phase A prepared configs (see below), otherwise `run.sh` regenerates fresh per-node SGX keys and overwrites the shared key mapping. |

### Examples

Run a normal node with the default sample config:
```bash
bash run.sh
```

Run a normal node with HTTPS SGX wallet and a custom config:
```bash
bash run.sh -HTTPS -config ./sample_configs/my_config.json
```

Run an archive node (requires `skaled` built with `-DHISTORIC_STATE=1`):
```bash
bash run.sh -node-type archive
```

Run an archive node with frequent historic-state DB rotation, useful for
reproducing rotation-related bugs:
```bash
bash run.sh -config ./sample_configs/config_historic.json -node-type archive
```

Run two instances safely from the same repo by isolating runtime state:
```bash
bash run.sh -config ./sample_configs/config_core.json -node-type normal -http-port 1237 -work-dir ./tmp/core
bash run.sh -config ./sample_configs/config_historic.json -node-type archive -http-port 1238 -work-dir ./tmp/historic
```

For a two-node test, open two terminals and run those two commands in parallel.

Warning: this isolates runtime state only (data dirs/ports). Each node
generates independent SGX keys and the placeholder peer key fields in the
config will not match — the two nodes will not form working consensus. For
an actual multi-node test, use the Phase A workflow below.

## Phase A: declarative multi-node preparation

For reproducible multi-node setups, prepare all keys/configs from one scenario
file first. This generates a shared node->key mapping and renders every node
config from that same mapping.

Example scenario file:
- `./sample_configs/scenarios/core_historic_local.json`

Prepare artifacts:
```bash
source .venv/bin/activate
python3 utils/prepare_scenario.py ./sample_configs/scenarios/core_historic_local.json
```

This writes (by default) under:
- `./tmp/prepared/core_historic_local/keys_map.json`
- `./tmp/prepared/core_historic_local/core/updated_config.json`
- `./tmp/prepared/core_historic_local/historic/updated_config.json`

Then run nodes using the prepared configs, with `-skip-key-setup` so `run.sh`
does not regenerate per-node keys and overwrite the shared mapping:
```bash
bash run.sh -skip-key-setup -config ./tmp/prepared/core_historic_local/core/updated_config.json -node-type normal -http-port 1237 -work-dir ./tmp/core
bash run.sh -skip-key-setup -config ./tmp/prepared/core_historic_local/historic/updated_config.json -node-type archive -http-port 1238 -work-dir ./tmp/historic
```

Notes:
- `prepare_scenario.py` only prepares artifacts; it does not launch `skaled`.
- Always pass `-skip-key-setup` when using Phase A prepared configs. Without
  it, `run.sh` generates a fresh SGX key pair and overwrites the shared
  mapping, causing `ECDSA sig did not verify` errors at consensus bootstrap.
- Recommended development workflow: prepare once, then run each node manually
  in its own terminal to see logs in real time.
- Keep `wsRpcPort`, `wssRpcPort`, `httpsRpcPort`, and `infoHttpRpcPort`
  outside `[basePort, basePort+10]` for each node — consensus reserves that
  range internally. The sample configs already do this.
- `Could not connect SGX ZMQ API. Will fallback to HTTP(S)` is non-fatal.

## What the script does

1. Parses/validates the CLI flags above.
2. Checks the SGX wallet is listening on the expected port.
3. Sets up a Python virtualenv (`venv/`) and generates ECDSA/BLS keys via
  `utils/sgx_import.py`, caching them in `<work-dir>/keys.json` (skipped if
  the file already exists).
4. Fills in the base config template with the generated keys via
  `utils/update_config.py`, applying `-node-type`
  as described above, and writes the result to `<work-dir>/updated_config.json`.
5. Runs `../../build/skaled/skaled --config <work-dir>/updated_config.json ...`.

Notes:
- `<work-dir>/keys.json` is cached across runs; delete it to force key
  regeneration.
- Data directory used by the node is `<work-dir>/data_dir` (created if missing).
- Set `skaleConfig.sChain.maxHistoricStateDbSize` directly in your config file
  (for example, `config_historic.json`).

## Forcing historic-state DB rotation

`utils/stress_txs.py` sends a stream of state-changing transactions against a
running node and optionally watches for new rotation pieces on disk:

```bash
source .venv/bin/activate
python3 utils/stress_txs.py \
    --rpc-url http://127.0.0.1:1237 \
    --count 2000 --rate 20 \
    --historic-dir ./tmp/historic/data_dir/historic_state
```

The sender address (`0x907cd0881E50d359bb9Fd120B1A5A143b1C97De6`) must be
funded in the genesis `accounts` section of the config — already done in
`config_core.json`/`config_historic.json`.

Set `skaleConfig.sChain.maxHistoricStateDbSize` to a small value in the config
(already done in `config_historic.json`) so rotation triggers after minimal
traffic.