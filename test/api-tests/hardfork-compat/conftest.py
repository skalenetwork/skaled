"""
pytest fixtures for the hardfork-compat test suite.

The primary node (5.1.0 binary) is session-scoped: launched once, used by
all workload tests.  The sync node (5.2.0 binary, syncNode=true,
archiveMode=true) is launched on-demand by the final test after all
primary-node transactions have been sent, then compared block-by-block.

Configuration is read from a JSON file whose path is stored in the
HARDFORK_COMPAT_CFG_JSON environment variable.  run.py sets this
automatically when invoked with ``--suite hardfork-compat``.
"""

import json
import logging
import os
import shutil
import signal
import subprocess
import sys
import time
from pathlib import Path
from typing import Optional

import pytest
from web3 import Web3

logger = logging.getLogger("hardfork-compat.conftest")

SUITE_DIR  = Path(__file__).resolve().parent
REPO_ROOT  = SUITE_DIR.parent.parent.parent
FUNC_TESTS = SUITE_DIR.parent

# Add api-tests/ to sys.path so ``from run import ...`` works.
sys.path.insert(0, str(FUNC_TESTS))


# ---------------------------------------------------------------------------
# Config
# ---------------------------------------------------------------------------

@pytest.fixture(scope="session")
def run_cfg() -> dict:
    """Load the merged run.toml config written by run.py to a temp JSON file."""
    cfg_path = os.environ.get("HARDFORK_COMPAT_CFG_JSON", "")
    if cfg_path and Path(cfg_path).is_file():
        with open(cfg_path) as f:
            return json.load(f)
    pytest.skip(
        "HARDFORK_COMPAT_CFG_JSON not set or file missing. "
        "Run via: python3 run.py --suite hardfork-compat"
    )


@pytest.fixture(scope="session")
def hardfork_cfg(run_cfg: dict) -> dict:
    hc = run_cfg.get("hardfork_compat", {})
    if not hc:
        pytest.fail("[hardfork_compat] section missing from config")
    return hc


@pytest.fixture(scope="session")
def private_key(run_cfg: dict) -> str:
    return run_cfg.get("type", {}).get("private_key", "")


@pytest.fixture(scope="session")
def timeouts(hardfork_cfg: dict) -> dict:
    return hardfork_cfg.get("timeouts", {})


@pytest.fixture(scope="session")
def sync_binary(hardfork_cfg: dict) -> Path:
    """Path to the 5.2.0 binary used for the sync node."""
    return _hardfork_binary(
        hardfork_cfg,
        "v520_binary",
        "HARDFORK_COMPAT_V520_BINARY",
        "bin-5-2-0",
    )


# ---------------------------------------------------------------------------
# Node lifecycle helpers
# ---------------------------------------------------------------------------

def _resolve(path_str: str) -> Path:
    p = Path(path_str)
    return p if p.is_absolute() else (REPO_ROOT / p).resolve()


def _hardfork_binary(
    hardfork_cfg: dict, cfg_key: str, env_key: str, default: str,
) -> Path:
    """Resolve a hardfork-compat binary path.

    Environment variables allow local and CI callers to point at externally
    supplied binaries without editing hardfork-compat.toml.
    """
    return _resolve(os.environ.get(env_key) or hardfork_cfg.get(cfg_key, default))


def _resolve_ft(path_str: str) -> Path:
    p = Path(path_str)
    return p if p.is_absolute() else (FUNC_TESTS / p).resolve()


def _wait_for_rpc(w3: Web3, timeout_s: int, label: str) -> None:
    deadline = time.time() + timeout_s
    while time.time() < deadline:
        try:
            bn = w3.eth.block_number
            logger.info("%s RPC UP (block=%d)", label, bn)
            return
        except Exception:
            time.sleep(3)
    raise RuntimeError(f"{label} RPC did not come up within {timeout_s}s")


def _stop_node(proc: subprocess.Popen, log_fd, label: str) -> None:
    if proc.poll() is None:
        logger.info("Stopping %s node (pid=%d) ...", label, proc.pid)
        proc.send_signal(signal.SIGTERM)
        try:
            proc.wait(timeout=20)
        except subprocess.TimeoutExpired:
            logger.warning("Force-killing %s", label)
            proc.kill()
            proc.wait()
    log_fd.close()
    logger.info("%s node stopped.", label)


def _launch_node(
    binary: Path,
    cfg_out: Path,
    http_port: int,
    datadir: Path,
    log_path: Path,
    label: str,
    fresh: bool = True,
) -> tuple[subprocess.Popen, object]:
    """Start a skaled node and return ``(proc, log_fd)``."""
    if fresh:
        if datadir.exists():
            shutil.rmtree(datadir)
        datadir.mkdir(parents=True)

    log_fd = open(log_path, "w")
    cmd = [
        str(binary),
        "--config",  str(cfg_out),
        "--http-port", str(http_port),
        "--ws-port",   str(http_port - 1),
        "--info-http-port", str(http_port + 6),
        "-v", "9",
        "--web3-trace",
        "--enable-debug-behavior-apis",
        "--ipcpath", str(datadir),
        "-d",        str(datadir),
    ]
    logger.info("Launching %s node (http=%d) -> %s", label, http_port, log_path)
    proc = subprocess.Popen(cmd, stdout=log_fd, stderr=subprocess.STDOUT, cwd=REPO_ROOT)
    return proc, log_fd


def make_sync_config(
    primary_cfg_path: Path, sync_cfg_path: Path, sync_http_port: int,
) -> None:
    """Create the 5.2.0 sync config from the primary config.

    syncNode=true + archiveMode=true makes the node replay every block and
    retain historic state, so its recomputed per-block stateRoot can be
    compared against the 5.1.0 primary.
    """
    with open(primary_cfg_path) as f:
        cfg = json.load(f)

    node_info = cfg["skaleConfig"]["nodeInfo"]
    node_info["syncNode"] = True
    node_info["archiveMode"] = True
    node_info["httpRpcPort"] = sync_http_port
    node_info["wsRpcPort"] = sync_http_port - 1
    node_info["basePort"] = sync_http_port - 3
    node_info["infoHttpRpcPort"] = sync_http_port + 6

    # sChain.nodes describes the peer topology -- leave it pointing at the
    # primary node so the sync node knows where to sync from.

    sync_cfg_path.parent.mkdir(parents=True, exist_ok=True)
    with open(sync_cfg_path, "w") as f:
        json.dump(cfg, f, indent=2)
    logger.info(
        "Created 5.2.0 sync config: %s (syncNode=true, archiveMode=true)",
        sync_cfg_path,
    )


# ---------------------------------------------------------------------------
# Primary node (5.1.0, session-scoped)
# ---------------------------------------------------------------------------

@pytest.fixture(scope="session")
def primary_session(run_cfg: dict, hardfork_cfg: dict):
    """
    Render the primary (5.1.0) config, launch the node, wait for RPC,
    and yield ``(w3, cfg_out_path)``.
    """
    from run import (
        configure_single_node_skaled,
        ensure_genesis_balance,
        inject_patches,
        render_template_file,
        set_ulimit,
    )

    priv_key = run_cfg.get("type", {}).get("private_key", "")
    tmpl_ctx = run_cfg.get("skaled", {}).get("template", {}).get("context", {})
    http_port = int(hardfork_cfg.get("primary_http_port", 5334))
    binary  = _hardfork_binary(
        hardfork_cfg,
        "v510_binary",
        "HARDFORK_COMPAT_V510_BINARY",
        "bin-5-1-0",
    )
    datadir = _resolve("test/api-tests/hardfork-compat/datadir-primary")
    cfg_out = _resolve(
        "test/api-tests/hardfork-compat/configs/config-primary.generated.json"
    )
    tmpl = _resolve_ft(
        "hardfork-compat/config-templates/config-template.json.j2"
    )

    if not binary.is_file():
        pytest.fail(
            f"5.1.0 skaled binary not found: {binary}\n"
            "Set HARDFORK_COMPAT_V510_BINARY or [hardfork_compat].v510_binary, "
            "or build with:\n"
            "  git checkout v5.1.0 && cmake -H. -Bbuild-v510 -DCMAKE_BUILD_TYPE=Release "
            "&& cmake --build build-v510 --target skaled -- -j4"
        )

    render_template_file(str(tmpl), str(cfg_out), tmpl_ctx)
    ensure_genesis_balance(str(cfg_out), priv_key)
    configure_single_node_skaled(str(cfg_out))
    inject_patches(str(cfg_out), hardfork_cfg.get("common_patches", {}))

    set_ulimit()
    log_dir = SUITE_DIR / "logs"
    log_dir.mkdir(parents=True, exist_ok=True)

    proc, log_fd = _launch_node(
        binary, cfg_out, http_port, datadir,
        log_dir / "skaled-primary-v510.log", "PRIMARY(5.1.0)",
    )
    w3 = Web3(Web3.HTTPProvider(
        f"http://127.0.0.1:{http_port}", request_kwargs={"timeout": 10}
    ))

    rpc_timeout = hardfork_cfg.get("timeouts", {}).get("rpc_up", 360)
    try:
        _wait_for_rpc(w3, rpc_timeout, "PRIMARY(5.1.0)")
    except RuntimeError as e:
        _stop_node(proc, log_fd, "PRIMARY(5.1.0)")
        pytest.fail(str(e))

    try:
        yield w3, cfg_out
    finally:
        _stop_node(proc, log_fd, "PRIMARY(5.1.0)")


@pytest.fixture(scope="session")
def w3_primary(primary_session) -> Web3:
    return primary_session[0]


@pytest.fixture(scope="session")
def primary_cfg_path(primary_session) -> Path:
    return primary_session[1]
