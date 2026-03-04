"""
pytest fixtures for the BITE/BITE2 compatibility test suite.

Two nodes are launched during the session:
  - BITE node  (``w3_bite``) — module-scoped; torn down after test_phase1_bite.py
  - BITE2 node (``w3``)      — session-scoped; used by Phase 2a / 2b tests

The BITE node uses a module-scoped fixture so it is shut down as soon as
Phase 1 (test_phase1_bite.py) finishes, freeing resources before Phase 2
starts.  suite.py passes the two files in the correct order to pytest.

Configuration is read from a JSON file whose path is stored in the
BITE_COMPAT_CFG_JSON environment variable.  run.py sets this automatically
when invoked with ``--suite bite-compat``; for direct pytest invocation set
the variable manually:

    export BITE_COMPAT_CFG_JSON=/path/to/run.toml.json
    pytest test/api-tests/bite-compat/test_phase1_bite.py \\
           test/api-tests/bite-compat/test_bite_compat.py -v
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

logger = logging.getLogger("bite-compat.conftest")

SUITE_DIR  = Path(__file__).resolve().parent
REPO_ROOT  = SUITE_DIR.parent.parent.parent
FUNC_TESTS = SUITE_DIR.parent

# Add api-tests/ to sys.path so ``from run import ...`` works.
sys.path.insert(0, str(FUNC_TESTS))

BITE2_PATCH_DELAY_SECONDS = 101


# ---------------------------------------------------------------------------
# Config
# ---------------------------------------------------------------------------

@pytest.fixture(scope="session")
def run_cfg() -> dict:
    """Load the merged run.toml config written by run.py to a temp JSON file."""
    cfg_path = os.environ.get("BITE_COMPAT_CFG_JSON", "")
    if cfg_path and Path(cfg_path).is_file():
        with open(cfg_path) as f:
            return json.load(f)
    pytest.skip(
        "BITE_COMPAT_CFG_JSON not set or file missing. "
        "Run via: python3 run.py --suite bite-compat"
    )


@pytest.fixture(scope="session")
def bite_cfg(run_cfg: dict) -> dict:
    bc = run_cfg.get("bite_compat", {})
    if not bc:
        pytest.fail("[bite_compat] section missing from config")
    return bc


@pytest.fixture(scope="session")
def private_key(run_cfg: dict) -> str:
    return run_cfg.get("type", {}).get("private_key", "")


@pytest.fixture(scope="session")
def timeouts(bite_cfg: dict) -> dict:
    return bite_cfg.get("timeouts", {})


# ---------------------------------------------------------------------------
# Node lifecycle helpers
# ---------------------------------------------------------------------------

def _resolve(path_str: str) -> Path:
    p = Path(path_str)
    return p if p.is_absolute() else (REPO_ROOT / p).resolve()


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
    sgx_url: Optional[str],
    label: str,
) -> tuple[subprocess.Popen, object]:
    """Start a skaled node and return ``(proc, log_fd)``."""
    if datadir.exists():
        shutil.rmtree(datadir)
    datadir.mkdir(parents=True)

    log_fd = open(log_path, "w")
    cmd = [
        str(binary),
        "--config",  str(cfg_out),
        "--http-port", str(http_port),
        "--ws-port",   str(http_port - 1),
        "-v", "9",
        "--web3-trace",
        "--enable-debug-behavior-apis",
        "--ipcpath", str(datadir),
        "-d",        str(datadir),
    ]
    if sgx_url:
        cmd += ["--sgx-url", sgx_url]

    logger.info("Launching %s node (http=%d) -> %s", label, http_port, log_path)
    proc = subprocess.Popen(cmd, stdout=log_fd, stderr=subprocess.STDOUT, cwd=REPO_ROOT)
    return proc, log_fd


# ---------------------------------------------------------------------------
# BITE node (Phase 1)
# ---------------------------------------------------------------------------

@pytest.fixture(scope="module")
def skaled_bite_session(run_cfg: dict, bite_cfg: dict):
    """
    Render the BITE config, launch the BITE node, wait for RPC, and yield
    the ``Web3`` instance.  The node is stopped during teardown.
    """
    from run import (
        apply_sgx_config,
        configure_single_node_skaled,
        ensure_genesis_balance,
        inject_patches,
        render_template_file,
        set_ulimit,
    )

    priv_key = run_cfg.get("type", {}).get("private_key", "")
    patches  = bite_cfg.get("patches", {})
    tmpl_ctx = bite_cfg.get("template_context", {})
    use_sgx  = (
        bool(bite_cfg["use_sgx"]) if "use_sgx" in bite_cfg
        else bool(run_cfg.get("sgx", {}).get("enabled"))
    )
    sgx_ctx = run_cfg.get("sgx", {}).get("template_context") if use_sgx else None
    sgx_url = (
        run_cfg.get("sgx", {}).get("url")
        if use_sgx and run_cfg.get("sgx", {}).get("enabled")
        else None
    )

    http_port   = int(bite_cfg.get("bite_http_port", 4232))
    bite_binary = _resolve(bite_cfg.get("bite_binary", "build_bite/skaled/skaled"))
    datadir     = _resolve(bite_cfg.get("bite_datadir", "test/api-tests/bite-compat/datadir-bite"))
    cfg_out     = _resolve(bite_cfg.get(
        "bite_config",
        "test/api-tests/bite-compat/configs/config-bite.generated.json",
    ))
    tmpl = _resolve_ft(bite_cfg.get(
        "bite_config_template",
        "bite-compat/config-templates/config-bite-template.json.j2",
    ))

    if not bite_binary.is_file():
        pytest.fail(
            f"BITE binary not found: {bite_binary}\n"
            "Build with: cmake -H. -Bbuild-bite -DCMAKE_BUILD_TYPE=Release -DBITE=1 "
            "&& cmake --build build_bite -j4"
        )

    render_template_file(str(tmpl), str(cfg_out), tmpl_ctx)
    if sgx_ctx:
        apply_sgx_config(str(cfg_out), sgx_ctx)
    # bite2PatchTimestamp must never be injected into the BITE node config
    bite_patches = {k: v for k, v in patches.items() if k != "bite2PatchTimestamp"}
    inject_patches(str(cfg_out), bite_patches)
    ensure_genesis_balance(str(cfg_out), priv_key)
    configure_single_node_skaled(str(cfg_out))

    set_ulimit()
    log_dir = SUITE_DIR / "logs"
    log_dir.mkdir(parents=True, exist_ok=True)

    proc, log_fd = _launch_node(
        bite_binary, cfg_out, http_port, datadir,
        log_dir / "skaled-bite.log", sgx_url, "BITE",
    )
    w3_bite = Web3(Web3.HTTPProvider(
        f"http://127.0.0.1:{http_port}", request_kwargs={"timeout": 10}
    ))

    rpc_timeout = bite_cfg.get("timeouts", {}).get("rpc_up", 360)
    try:
        _wait_for_rpc(w3_bite, rpc_timeout, "BITE")
    except RuntimeError as e:
        _stop_node(proc, log_fd, "BITE")
        pytest.fail(str(e))

    try:
        yield w3_bite
    finally:
        _stop_node(proc, log_fd, "BITE")


@pytest.fixture(scope="module")
def w3_bite(skaled_bite_session) -> Web3:
    return skaled_bite_session


# ---------------------------------------------------------------------------
# BITE2 node (Phase 2a / 2b)
# ---------------------------------------------------------------------------

@pytest.fixture(scope="session")
def skaled_session(run_cfg: dict, bite_cfg: dict):
    """
    Render the BITE2 config, launch the BITE2 node, wait for RPC, and
    yield ``(w3, bite2_patch_ts)``.  The node is stopped during teardown.
    """
    from run import (
        apply_sgx_config,
        configure_single_node_skaled,
        ensure_genesis_balance,
        inject_patches,
        render_template_file,
        set_ulimit,
    )

    priv_key = run_cfg.get("type", {}).get("private_key", "")
    patches  = bite_cfg.get("patches", {})
    tmpl_ctx = bite_cfg.get("template_context", {})
    use_sgx  = (
        bool(bite_cfg["use_sgx"]) if "use_sgx" in bite_cfg
        else bool(run_cfg.get("sgx", {}).get("enabled"))
    )
    sgx_ctx = run_cfg.get("sgx", {}).get("template_context") if use_sgx else None
    sgx_url = (
        run_cfg.get("sgx", {}).get("url")
        if use_sgx and run_cfg.get("sgx", {}).get("enabled")
        else None
    )

    http_port    = int(bite_cfg.get("http_port", 4234))
    bite2_binary = _resolve(bite_cfg.get("bite2_binary", "build_bite2/skaled/skaled"))
    datadir      = _resolve(bite_cfg.get("datadir", "test/api-tests/bite-compat/datadir"))
    cfg_out      = _resolve(bite_cfg.get(
        "bite2_config",
        "test/api-tests/bite-compat/configs/config-bite2.generated.json",
    ))
    tmpl = _resolve_ft(bite_cfg.get(
        "bite2_config_template",
        "bite-compat/config-templates/config-bite2-template.json.j2",
    ))

    if not bite2_binary.is_file():
        pytest.fail(
            f"BITE2 binary not found: {bite2_binary}\n"
            "Build with: cmake -H. -Bbuild-bite2 -DCMAKE_BUILD_TYPE=Release -DBITE2=1 "
            "&& cmake --build build_bite2 -j4"
        )

    bite2_patch_ts = int(time.time()) + BITE2_PATCH_DELAY_SECONDS
    logger.info(
        "bite2PatchTimestamp = %d (now+%ds, activates at %s)",
        bite2_patch_ts,
        BITE2_PATCH_DELAY_SECONDS,
        time.strftime("%H:%M:%S", time.localtime(bite2_patch_ts)),
    )

    render_template_file(str(tmpl), str(cfg_out), tmpl_ctx)
    if sgx_ctx:
        apply_sgx_config(str(cfg_out), sgx_ctx)
    inject_patches(str(cfg_out), {**patches, "bite2PatchTimestamp": bite2_patch_ts})
    ensure_genesis_balance(str(cfg_out), priv_key)
    configure_single_node_skaled(str(cfg_out))

    set_ulimit()
    log_dir = SUITE_DIR / "logs"
    log_dir.mkdir(parents=True, exist_ok=True)

    proc, log_fd = _launch_node(
        bite2_binary, cfg_out, http_port, datadir,
        log_dir / "skaled-bite2.log", sgx_url, "BITE2",
    )
    w3 = Web3(Web3.HTTPProvider(
        f"http://127.0.0.1:{http_port}", request_kwargs={"timeout": 10}
    ))

    rpc_timeout = bite_cfg.get("timeouts", {}).get("rpc_up", 360)
    try:
        _wait_for_rpc(w3, rpc_timeout, "BITE2")
    except RuntimeError as e:
        _stop_node(proc, log_fd, "BITE2")
        pytest.fail(str(e))

    try:
        yield w3, bite2_patch_ts
    finally:
        _stop_node(proc, log_fd, "BITE2")


@pytest.fixture(scope="session")
def w3(skaled_session) -> Web3:
    return skaled_session[0]


@pytest.fixture(scope="session")
def bite2_patch_ts(skaled_session) -> int:
    return skaled_session[1]
