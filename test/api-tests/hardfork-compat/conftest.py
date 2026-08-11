"""
pytest fixtures for the hardfork-compat test suite.

The primary node starts on the London-capable binary, produces pre-upgrade
blocks, is restarted in-place with the current Paris-capable binary and a
future ParisForkPatch timestamp, then produces both pre-activation and
post-activation blocks.  A current-version sync node (syncNode=true,
archiveMode=true) is launched at the end to replay the whole chain and compare
state roots / block hashes block-by-block.

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

import pytest
from web3 import Web3

logger = logging.getLogger("hardfork-compat.conftest")

SUITE_DIR = Path(__file__).resolve().parent
REPO_ROOT = SUITE_DIR.parent.parent.parent
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
def london_binary(hardfork_cfg: dict) -> Path:
    """Path to the London-capable binary used before the Paris upgrade."""
    return _hardfork_binary(
        hardfork_cfg,
        "london_binary",
        "HARDFORK_COMPAT_LONDON_BINARY",
        "bin-5-2-0",
        legacy_cfg_key="v510_binary",
        legacy_env_key="HARDFORK_COMPAT_V510_BINARY",
    )


@pytest.fixture(scope="session")
def current_binary(hardfork_cfg: dict) -> Path:
    """Path to the current Paris-capable binary used after upgrade and for sync."""
    return _hardfork_binary(
        hardfork_cfg,
        "current_binary",
        "HARDFORK_COMPAT_CURRENT_BINARY",
        "build/skaled/skaled",
        legacy_cfg_key="v520_binary",
        legacy_env_key="HARDFORK_COMPAT_V520_BINARY",
    )


@pytest.fixture(scope="session")
def sync_binary(current_binary: Path) -> Path:
    """Path to the current binary used for the archive sync node."""
    return current_binary


# ---------------------------------------------------------------------------
# Node lifecycle helpers
# ---------------------------------------------------------------------------

def _resolve(path_str: str) -> Path:
    p = Path(path_str)
    return p if p.is_absolute() else (REPO_ROOT / p).resolve()


def _hardfork_binary(
    hardfork_cfg: dict,
    cfg_key: str,
    env_key: str,
    default: str,
    legacy_cfg_key: str | None = None,
    legacy_env_key: str | None = None,
) -> Path:
    """Resolve a hardfork-compat binary path.

    New Paris-specific names are preferred, while the older v510/v520 names
    remain supported so local/CI callers can migrate without a flag day.
    """
    value = os.environ.get(env_key) or hardfork_cfg.get(cfg_key)
    if value is None and legacy_env_key:
        value = os.environ.get(legacy_env_key)
    if value is None and legacy_cfg_key:
        value = hardfork_cfg.get(legacy_cfg_key)
    return _resolve(value or default)


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


def _tail_file(path: Path, max_lines: int = 80) -> str:
    try:
        return "\n".join(path.read_text(errors="replace").splitlines()[-max_lines:])
    except OSError as exc:
        return f"<failed to read {path}: {exc}>"


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
        "--config", str(cfg_out),
        "--http-port", str(http_port),
        "--ws-port", str(http_port - 1),
        "--info-http-port", str(http_port + 6),
        "-v", "9",
        "--web3-trace",
        "--enable-debug-behavior-apis",
        "--ipcpath", str(datadir),
        "-d", str(datadir),
    ]
    logger.info("Launching %s node (http=%d) -> %s", label, http_port, log_path)
    proc = subprocess.Popen(cmd, stdout=log_fd, stderr=subprocess.STDOUT, cwd=REPO_ROOT)
    return proc, log_fd


def make_sync_config(
    primary_cfg_path: Path, sync_cfg_path: Path, sync_http_port: int,
) -> None:
    """Create the current-version archive sync config from the primary config.

    The primary config already contains the resolved Paris timestamp by the time
    this is called.  Do not re-resolve relative timestamps here, otherwise the
    sync node could replay with a different activation point.
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
        "Created current sync config: %s (syncNode=true, archiveMode=true)",
        sync_cfg_path,
    )


class ManagedPrimaryNode:
    def __init__(
        self,
        *,
        w3: Web3,
        cfg_path: Path,
        datadir: Path,
        http_port: int,
        proc: subprocess.Popen,
        log_fd,
        label: str,
    ):
        self.w3 = w3
        self.cfg_path = cfg_path
        self.datadir = datadir
        self.http_port = http_port
        self.proc = proc
        self.log_fd = log_fd
        self.label = label
        self.upgraded = False

    def stop(self) -> None:
        if self.proc is not None and self.log_fd is not None:
            _stop_node(self.proc, self.log_fd, self.label)
            self.proc = None
            self.log_fd = None

    def upgrade_to_current(
        self, *, binary: Path, patches: dict, timeout_s: int,
    ) -> None:
        """Restart the primary in-place on the current binary with Paris patches."""
        if self.upgraded:
            logger.info("Primary node is already upgraded to current binary")
            return

        from run import inject_patches, set_ulimit

        self.stop()
        inject_patches(str(self.cfg_path), patches)
        set_ulimit()

        log_dir = SUITE_DIR / "logs"
        log_dir.mkdir(parents=True, exist_ok=True)
        self.label = "PRIMARY(current)"
        log_path = log_dir / "skaled-primary-current.log"
        self.proc, self.log_fd = _launch_node(
            binary,
            self.cfg_path,
            self.http_port,
            self.datadir,
            log_path,
            self.label,
            fresh=False,
        )
        try:
            _wait_for_rpc(self.w3, timeout_s, self.label)
        except RuntimeError as exc:
            raise RuntimeError(f"{exc}\n--- {log_path} tail ---\n{_tail_file(log_path)}") from exc
        self.upgraded = True


# ---------------------------------------------------------------------------
# Primary node (London first, then current in-place)
# ---------------------------------------------------------------------------

@pytest.fixture(scope="session")
def primary_session(run_cfg: dict, hardfork_cfg: dict, london_binary: Path):
    """
    Render the primary config, launch the London binary, wait for RPC,
    and yield a managed node object that tests can upgrade in-place.
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
    datadir = _resolve("test/api-tests/hardfork-compat/datadir-primary")
    cfg_out = _resolve(
        "test/api-tests/hardfork-compat/configs/config-primary.generated.json"
    )
    tmpl = _resolve_ft(
        "hardfork-compat/config-templates/config-template.json.j2"
    )

    if not london_binary.is_file():
        pytest.fail(
            f"London skaled binary not found: {london_binary}\n"
            "Set HARDFORK_COMPAT_LONDON_BINARY or [hardfork_compat].london_binary."
        )

    render_template_file(str(tmpl), str(cfg_out), tmpl_ctx)
    ensure_genesis_balance(str(cfg_out), priv_key)
    configure_single_node_skaled(str(cfg_out))
    inject_patches(str(cfg_out), hardfork_cfg.get("common_patches", {}))

    set_ulimit()
    log_dir = SUITE_DIR / "logs"
    log_dir.mkdir(parents=True, exist_ok=True)

    log_path = log_dir / "skaled-primary-london.log"
    proc, log_fd = _launch_node(
        london_binary, cfg_out, http_port, datadir,
        log_path, "PRIMARY(London)",
    )
    w3 = Web3(Web3.HTTPProvider(
        f"http://127.0.0.1:{http_port}", request_kwargs={"timeout": 10}
    ))

    rpc_timeout = hardfork_cfg.get("timeouts", {}).get("rpc_up", 360)
    try:
        _wait_for_rpc(w3, rpc_timeout, "PRIMARY(London)")
    except RuntimeError as e:
        _stop_node(proc, log_fd, "PRIMARY(London)")
        pytest.fail(f"{e}\n--- {log_path} tail ---\n{_tail_file(log_path)}")

    node = ManagedPrimaryNode(
        w3=w3,
        cfg_path=cfg_out,
        datadir=datadir,
        http_port=http_port,
        proc=proc,
        log_fd=log_fd,
        label="PRIMARY(London)",
    )

    try:
        yield node
    finally:
        node.stop()


@pytest.fixture(scope="session")
def primary_node(primary_session: ManagedPrimaryNode) -> ManagedPrimaryNode:
    return primary_session


@pytest.fixture(scope="session")
def w3_primary(primary_session: ManagedPrimaryNode) -> Web3:
    return primary_session.w3


@pytest.fixture(scope="session")
def primary_cfg_path(primary_session: ManagedPrimaryNode) -> Path:
    return primary_session.cfg_path


@pytest.fixture(scope="session")
def workload_state() -> dict:
    """Mutable cross-test state for deployed contracts and activation timestamp."""
    return {}
