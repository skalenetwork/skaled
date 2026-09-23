# run_with_sgx

Launch a local `skaled` node using a running SGX wallet. The script generates
node keys, injects them into a config template, and starts `skaled`.

## Prerequisites

- Build `skaled` in `../../build`.
- Run an SGX wallet on `127.0.0.1:1029` (HTTP), or on port `1026` for HTTPS.
  See `utils/spin_default_sgx.sh` for a local wallet container.
- Have Python 3 and permission to install the first-run system dependencies.

For archive mode, build `skaled` with `-DHISTORIC_STATE=1`.

## Run

From this directory:

```bash
bash run.sh
```

On first run, the script creates `.venv` and installs its Python
dependencies. Generated keys, the rendered config, and node data are kept in
`./tmp`.

```bash
bash run.sh [-FAIR] [-HTTPS] [-config <path>] \
  [-node-type <normal|archive>] [-http-port <port>] \
  [-work-dir <path>] [-skip-key-setup]
```

| Flag | Default | Purpose |
| --- | --- | --- |
| `-config <path>` | `./sample_configs/config_core.json` | Config template. |
| `-node-type <normal\|archive>` | `normal` | Set archive mode in the rendered config. |
| `-http-port <port>` | `1237` | HTTP RPC port. |
| `-work-dir <path>` | `./tmp` | Generated keys, config, and node data. |
| `-HTTPS` | off | Connect to the SGX wallet over HTTPS; requires `sgx.crt` and `sgx.key` in the work directory. |
| `-FAIR` | off | Populate FAIR-specific config fields. |
| `-skip-key-setup` | off | Use `-config` as-is; do not generate keys or render a config. |

## Common commands

```bash
# Default core node
bash run.sh

# Archive node
bash run.sh -config ./sample_configs/config_historic.json -node-type archive

# Use a separate runtime directory and RPC port
bash run.sh -work-dir ./tmp/node-1 -http-port 1238
```

For reproducible multi-node configurations, prepare a scenario first:

```bash
source .venv/bin/activate
python3 utils/prepare_scenario.py ./sample_configs/scenarios/core_historic_local.json
```

Then launch each generated config with `-skip-key-setup`; this preserves the
shared key mapping created by the scenario.

Run these in separate terminals:

```bash
# Core node
bash run.sh -skip-key-setup \
  -config ./tmp/prepared/core_historic_local/core/updated_config.json \
  -node-type normal -http-port 1237 -work-dir ./tmp/core

# Historic node
bash run.sh -skip-key-setup \
  -config ./tmp/prepared/core_historic_local/historic/updated_config.json \
  -node-type archive -http-port 1248 -work-dir ./tmp/historic
```
