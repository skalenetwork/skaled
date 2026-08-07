#!/usr/bin/env python3
"""
API/functional test runner.

Reads run.toml, sets up the environment, then delegates to the selected
test suite (a subdirectory with a suite.py).

Usage:
  python3 run.py [--config run.toml] [--suite bite-compat]
  python3 run.py [--config run.toml] [--suite hardfork-support]

Steps:
  0. Launch SGX wallet        (optional — suites that need SGX)
  1. Setup environment        (launch skaled(s) and/or anvil, or delegate to suite)
  2. Deploy contracts         (suite.deploy)
  3. Run tests                (suite.run_tests; anvil mode compares outputs)
  4. Background hash comparison (two-node skaled mode)
"""

import argparse
from dataclasses import dataclass
import importlib.util
import json
import logging
import os
import re
import signal
import subprocess
import sys
import threading
import time
import tomllib
from pathlib import Path
from typing import Any, Optional, TextIO

from web3 import Web3
from result import TestResult

logger = logging.getLogger("api-tests.run")

FUNC_TESTS_DIR = Path(__file__).resolve().parent
REPO_ROOT = FUNC_TESTS_DIR.parent.parent


class RunnerError(RuntimeError):
    """Raised for recoverable runner failures handled in main()."""


# =========================================================================
# Configuration
# =========================================================================

def load_config(path: str) -> dict:
    with open(path, "rb") as f:
        return tomllib.load(f)


def _merge_dicts(base: dict, override: dict) -> dict:
    """Deep-merge *override* into *base*, returning a new dict.

    Dicts are merged recursively; all other value types are replaced.
    """
    result = dict(base)
    for key, val in override.items():
        if key in result and isinstance(result[key], dict) and isinstance(val, dict):
            result[key] = _merge_dicts(result[key], val)
        else:
            result[key] = val
    return result


def load_suite_config(suite_name: str) -> dict:
    """Load per-suite config from <suite-dir>/<suite-name>.toml if it exists."""
    suite_toml = FUNC_TESTS_DIR / suite_name / (suite_name + ".toml")
    if suite_toml.is_file():
        logger.info("Loading suite config: %s", suite_toml)
        with open(suite_toml, "rb") as f:
            return tomllib.load(f)
    return {}


# =========================================================================
# SGX wallet management
# =========================================================================

def _wait_for_sgx_port(url: str, timeout_s: int = 360) -> bool:
    """Poll the SGX wallet TCP port until it accepts connections."""
    import socket
    from urllib.parse import urlparse
    parsed = urlparse(url)
    host = parsed.hostname or "127.0.0.1"
    port = parsed.port or 1029
    deadline = time.time() + timeout_s
    logger.info("Waiting for SGX wallet TCP port %s:%d ...", host, port)
    while time.time() < deadline:
        try:
            s = socket.create_connection((host, port), timeout=3)
            s.close()
            logger.info("SGX wallet port %d is open", port)
            return True
        except Exception:
            pass
        time.sleep(3)
    logger.error("SGX wallet not available after %ds", timeout_s)
    return False


def _log_sgx_wallet_tail(log_path: Path, max_lines: int = 200):
    """Print the last SGX wallet log lines to help diagnose startup timeouts."""
    if not log_path.is_file():
        logger.error("SGX wallet log file not found: %s", log_path)
        return
    try:
        with open(log_path, "r", errors="replace") as f:
            lines = f.readlines()
    except Exception as exc:
        logger.error("Failed reading SGX wallet log file %s: %s", log_path, exc)
        return
    tail = lines[-max_lines:]
    if not tail:
        logger.error("SGX wallet log file is empty: %s", log_path)
        return
    logger.error("SGX wallet log tail (%s, last %d lines):", log_path, len(tail))
    for line in tail:
        logger.error("[sgxwallet] %s", line.rstrip("\n"))


def generate_sgx_keys(sgx_url: str, keys_file: Path) -> dict:
    """Generate ECDSA and BLS keys in the SGX wallet."""
    sgx_import = REPO_ROOT / "scripts" / "run_with_sgx" / "utils" / "sgx_import.py"
    if not sgx_import.is_file():
        raise RunnerError(f"sgx_import.py not found: {sgx_import}")
    logger.info("Generating SGX keys via %s ...", sgx_import)
    result = subprocess.run(
        [sys.executable, str(sgx_import), "--sgx-url", sgx_url],
        cwd=str(sgx_import.parent.parent),
        capture_output=True,
        text=True,
    )
    if result.returncode != 0:
        raise RunnerError(
            f"sgx_import.py failed (rc={result.returncode}):\n"
            f"stdout: {result.stdout}\nstderr: {result.stderr}"
        )
    generated = sgx_import.parent.parent / "tmp" / "keys.json"
    if not generated.is_file():
        raise RunnerError(f"sgx_import.py ran but keys not found: {generated}")
    with open(generated) as f:
        keys = json.load(f)
    keys_file.parent.mkdir(parents=True, exist_ok=True)
    with open(keys_file, "w") as f:
        json.dump(keys, f, indent=2)
    logger.info("SGX keys saved to %s", keys_file)

    entry = keys[0]
    pub_key = entry["bls"]["public_key"]
    ecdsa_pub = entry["ecdsa"]["public_key"]
    if not ecdsa_pub.startswith("0x"):
        ecdsa_pub = "0x" + ecdsa_pub
    return {
        "sgx_ecdsa_key_name":   entry["ecdsa"]["name"],
        "sgx_ecdsa_public_key": ecdsa_pub,
        "sgx_bls_key_name":     entry["bls"]["name"],
        "sgx_bls_public_key_0": pub_key[0],
        "sgx_bls_public_key_1": pub_key[1],
        "sgx_bls_public_key_2": pub_key[2],
        "sgx_bls_public_key_3": pub_key[3],
    }


def apply_sgx_config(config_path: str, sgx_context: dict):
    """Patch a rendered skaled JSON config with SGX key values."""
    with open(config_path) as f:
        cfg = json.load(f)

    node_info = cfg["skaleConfig"]["nodeInfo"]
    schain    = cfg["skaleConfig"]["sChain"]

    node_info["ecdsaKeyName"] = sgx_context["sgx_ecdsa_key_name"]
    node_info["testSignatures"] = False

    ima = node_info.setdefault("wallets", {}).setdefault("ima", {})
    ima["keyShareName"] = sgx_context["sgx_bls_key_name"]
    for i in range(4):
        ima[f"commonBLSPublicKey{i}"] = sgx_context[f"sgx_bls_public_key_{i}"]
        ima[f"BLSPublicKey{i}"]       = sgx_context[f"sgx_bls_public_key_{i}"]

    for node in schain.get("nodes", []):
        node["publicKey"] = sgx_context["sgx_ecdsa_public_key"]
        for i in range(4):
            node[f"blsPublicKey{i}"] = sgx_context[f"sgx_bls_public_key_{i}"]

    for group in schain.get("nodeGroups", {}).values():
        for ndata in group.get("nodes", {}).values():
            if isinstance(ndata, list) and len(ndata) >= 3:
                ndata[2] = sgx_context["sgx_ecdsa_public_key"]
        bls = group.get("bls_public_key", {})
        for i in range(4):
            bls[f"blsPublicKey{i}"] = sgx_context[f"sgx_bls_public_key_{i}"]

    with open(config_path, "w") as f:
        json.dump(cfg, f, indent=2)
    logger.info("Applied SGX keys to %s", config_path)


def setup_sgx(cfg: dict, _log_dir: Path) -> "Environment.ManagedProcess":
    """Launch the SGX wallet container and generate keys."""
    sgx_cfg = cfg.get("sgx", {})
    url            = sgx_cfg.get("url", "http://127.0.0.1:1029")
    image          = sgx_cfg.get("image", "skalenetwork/sgxwallet_sim:e548b375cae741af8fd11db54d6925c27a947af9")
    container_name = sgx_cfg.get("container_name", "skaled-api-tests-sgx")
    startup_timeout_sec = int(sgx_cfg.get("startup_timeout_sec", 360))
    artifacts_dir  = Path(resolve_repo_path(
        sgx_cfg.get("artifacts_dir", "test/api-tests/sgx-artifacts")
    ))
    keys_file_cfg = sgx_cfg.get("keys_file", "")
    keys_file = (
        Path(resolve_repo_path(keys_file_cfg))
        if keys_file_cfg
        else artifacts_dir / "sgx-keys.json"
    )

    artifacts_dir.mkdir(parents=True, exist_ok=True)
    log_file = artifacts_dir / f"{container_name}.log"
    cmd = [
        "docker", "run", "--rm",
        "--name", container_name,
        "--network", "host",
        image, "-s", "-y", "-Tn",
    ]
    logger.info("Launching SGX wallet '%s' -> %s", container_name, log_file)
    log_fd = open(log_file, "w")
    proc = subprocess.Popen(cmd, stdout=log_fd, stderr=subprocess.STDOUT)
    managed = Environment.ManagedProcess(
        proc=proc,
        log_fd=log_fd,
        docker_container_name=container_name,
    )

    if not _wait_for_sgx_port(url, startup_timeout_sec):
        log_fd.flush()
        _log_sgx_wallet_tail(log_file)
        proc.send_signal(signal.SIGTERM)
        try:
            proc.wait(timeout=15)
        except subprocess.TimeoutExpired:
            proc.kill()
            proc.wait()
        subprocess.run(
            ["docker", "rm", "-f", container_name],
            check=False, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
        )
        log_fd.close()
        raise RunnerError(f"SGX wallet at {url} did not come up in {startup_timeout_sec}s")

    context = generate_sgx_keys(url, keys_file)
    cfg.setdefault("sgx", {})["template_context"] = context
    logger.info("SGX ready — template context: %s", list(context.keys()))
    return managed


# =========================================================================
# Helpers
# =========================================================================

def resolve_repo_path(p: str) -> str:
    """Resolve a path relative to the repo root."""
    pp = Path(p)
    if pp.is_absolute():
        return str(pp)
    return str((REPO_ROOT / pp).resolve())


_TEMPLATE_VAR_RE = re.compile(r"{{\s*([a-zA-Z_][a-zA-Z0-9_]*)\s*}}")


def render_template_file(template_path: str, output_path: str, context: dict[str, Any]):
    """Render a simple Jinja-like template file by replacing {{ var }} tokens."""
    with open(template_path, "r") as f:
        template = f.read()

    required = {m.group(1) for m in _TEMPLATE_VAR_RE.finditer(template)}
    missing = sorted(v for v in required if v not in context)
    if missing:
        raise ValueError(
            f"Template {template_path} has undefined variables: {', '.join(missing)}"
        )

    rendered = _TEMPLATE_VAR_RE.sub(lambda m: str(context[m.group(1)]), template)
    out_path = Path(output_path)
    out_path.parent.mkdir(parents=True, exist_ok=True)
    with open(out_path, "w") as f:
        f.write(rendered)
    logger.info("Rendered config template %s -> %s", template_path, output_path)


def set_ulimit():
    import resource
    try:
        soft, hard = resource.getrlimit(resource.RLIMIT_NOFILE)
        target = min(65535, hard) if hard > 0 else 65535
        resource.setrlimit(resource.RLIMIT_NOFILE, (target, hard))
        logger.info("ulimit -n set to %d", target)
    except Exception:
        logger.warning("Could not set ulimit")


def ensure_genesis_balance(config_path: str, private_key: str):
    """Ensure the account derived from *private_key* has balance in genesis."""
    if not private_key:
        logger.debug("No private_key provided, skipping genesis balance.")
        return

    try:
        from eth_account import Account
        addr = Account.from_key(private_key).address
    except Exception as e:
        logger.warning("Could not derive address from private_key: %s", e)
        return

    logger.info("Ensuring genesis balance for %s in %s", addr, config_path)
    cfg = _load_skaled_config(config_path)
    accounts = cfg.get("accounts", {})

    addr_lower = addr.lower()
    for existing in accounts:
        if existing.lower() == addr_lower:
            logger.info("Account %s already in genesis of %s", addr, config_path)
            return

    balance = "1000000000000000000000000000000"
    accounts[addr] = {"balance": balance}
    cfg["accounts"] = accounts
    _save_skaled_config(config_path, cfg)
    logger.info("Added genesis balance for %s in %s", addr, config_path)


def inject_patches(config_path: str, patches: dict):
    """Inject patch timestamps into a skaled JSON config's sChain block.

    Special key "delete" (list of strings) removes those keys from sChain.
    All other keys are set, with values resolved via _resolve_patch_value.
    """
    with open(config_path, "r") as f:
        cfg = json.load(f)

    schain = cfg.setdefault("skaleConfig", {}).setdefault("sChain", {})
    for key in patches.get("delete", []):
        removed = schain.pop(key, None)
        if removed is not None:
            logger.info("  Deleted sChain key %s from %s", key, config_path)
    for key, val in patches.items():
        if key == "delete":
            continue
        resolved = _resolve_patch_value(val)
        schain[key] = resolved
        logger.info("  Patched %s = %s (from %s) in %s", key, resolved, val, config_path)

    with open(config_path, "w") as f:
        json.dump(cfg, f, indent=2)


def _resolve_patch_value(raw):
    """Resolve timestamp shorthand expressions in patch values.

    Supported forms:
      - integer (kept as-is)
      - numeric string, e.g. "-1", "1712314800"
      - "now"
      - "now+<n>" / "now-<n>" (seconds)
      - "now+<n><unit>" / "now-<n><unit>", unit in {s,m,h,d}
    """
    if isinstance(raw, int):
        return raw
    if isinstance(raw, float) and raw.is_integer():
        return int(raw)
    if not isinstance(raw, str):
        return raw

    value = raw.strip().lower()
    if re.fullmatch(r"[+-]?\d+", value):
        return int(value)

    m = re.fullmatch(r"now(?:\s*([+-])\s*(\d+)\s*([smhd]?))?", value)
    if not m:
        return raw

    sign, amount_raw, unit = m.groups()
    now_ts = int(time.time())
    if amount_raw is None:
        return now_ts

    amount = int(amount_raw)
    unit_mul = {"": 1, "s": 1, "m": 60, "h": 3600, "d": 86400}[unit]
    delta = amount * unit_mul
    return now_ts + delta if sign == "+" else now_ts - delta


def _load_skaled_config(config_path: str) -> dict:
    with open(config_path, "r") as f:
        return json.load(f)


def _save_skaled_config(config_path: str, cfg: dict):
    with open(config_path, "w") as f:
        json.dump(cfg, f, indent=2)


def _update_node_groups(schain: dict, kept_nodes: list[dict]):
    """Rewrite nodeGroups to contain only the kept schain nodes."""
    kept_ids = {n["nodeID"] for n in kept_nodes}
    node_groups = schain.get("nodeGroups", {})
    for _gid, group in node_groups.items():
        old_nodes = group.get("nodes", {})
        new_nodes = {}
        new_idx = 0
        for _nid, ndata in old_nodes.items():
            if isinstance(ndata, list) and len(ndata) >= 2:
                node_id = ndata[1]
                if node_id in kept_ids:
                    ndata[0] = new_idx + 1  # schainIndex is 1-based
                    new_nodes[str(node_id)] = ndata
                    new_idx += 1
            elif isinstance(ndata, dict):
                node_id = ndata.get("nodeID")
                if node_id in kept_ids:
                    ndata["schainIndex"] = new_idx + 1
                    new_nodes[str(node_id)] = ndata
                    new_idx += 1
        group["nodes"] = new_nodes
    return node_groups


def configure_single_node_skaled(config_path: str, node_index: int = 0):
    """Rewrite a skaled config to single-node mode."""
    cfg = _load_skaled_config(config_path)
    schain = cfg.get("skaleConfig", {}).get("sChain", {})
    nodes = schain.get("nodes", [])

    if len(nodes) <= 1:
        logger.info("Config %s already single-node, skipping.", config_path)
        return

    if node_index >= len(nodes):
        logger.warning("node_index %d out of range (%d nodes), using 0", node_index, len(nodes))
        node_index = 0

    kept = nodes[node_index]
    kept["schainIndex"] = 1
    schain["nodes"] = [kept]
    _update_node_groups(schain, [kept])

    node_info = cfg.get("skaleConfig", {}).get("nodeInfo", {})
    node_info["nodeID"] = kept["nodeID"]
    node_info["basePort"] = kept.get("basePort", node_info.get("basePort"))

    cfg["skaleConfig"]["sChain"] = schain
    cfg["skaleConfig"]["nodeInfo"] = node_info
    _save_skaled_config(config_path, cfg)
    logger.info("Configured %s as single-node (nodeID=%s)", config_path, kept["nodeID"])


def configure_two_node_skaled(config_paths: list[str], nodes_cfg: dict[int, dict]):
    """Rewrite skaled configs for two-node mode."""
    cfg0 = _load_skaled_config(config_paths[0])
    schain = cfg0.get("skaleConfig", {}).get("sChain", {})
    schain_nodes = schain.get("nodes", [])

    if len(schain_nodes) < 2:
        logger.warning("Template config has < 2 schain nodes; two-node setup may be incomplete.")
        return

    for i, snode in enumerate(schain_nodes[:2]):
        if i in nodes_cfg:
            ncfg = nodes_cfg[i]
            snode["ip"] = "127.0.0.1"
            snode["publicIP"] = "127.0.0.1"
            snode["httpRpcPort"] = ncfg["http_port"]
            snode["basePort"] = ncfg["http_port"] - 3
            snode["schainIndex"] = i + 1

    schain["nodes"] = schain_nodes[:2]
    _update_node_groups(schain, schain_nodes[:2])

    for i, config_path in enumerate(config_paths):
        cfg = _load_skaled_config(config_path)
        cfg["skaleConfig"]["sChain"] = schain

        node_info = cfg.get("skaleConfig", {}).get("nodeInfo", {})
        node_info["nodeID"] = schain_nodes[i]["nodeID"]
        node_info["basePort"] = schain_nodes[i].get("basePort", node_info.get("basePort"))
        cfg["skaleConfig"]["nodeInfo"] = node_info

        _save_skaled_config(config_path, cfg)
        logger.info("Configured %s for two-node mode (nodeID=%s)",
                    config_path, schain_nodes[i]["nodeID"])


def make_web3(host: str, port: int) -> Web3:
    return Web3(Web3.HTTPProvider(
        f"http://{host}:{port}", request_kwargs={"timeout": 10}
    ))


def wait_for_rpc(w3: Web3, label: str, timeout_s: int = 120) -> bool:
    deadline = time.time() + timeout_s
    last_error: Optional[Exception] = None
    logger.info("Waiting for RPC (%s) ...", label)
    while time.time() < deadline:
        try:
            bn = w3.eth.block_number
            logger.info("RPC %s UP (block=%d)", label, bn)
            return True
        except Exception as e:
            last_error = e
        time.sleep(5)
    logger.error("TIMEOUT: RPC %s not available (last error: %s)", label, last_error)
    return False


def build_skaled_binary(binary_cfg: str, build_enabled: bool = True) -> str:
    if binary_cfg and os.path.isfile(binary_cfg):
        logger.info("Using pre-built skaled: %s", binary_cfg)
        return binary_cfg

    build_dir = REPO_ROOT / "build"
    skaled_bin = build_dir / "skaled" / "skaled"

    if skaled_bin.is_file():
        logger.info("Found existing build: %s", skaled_bin)
        return str(skaled_bin)

    if not build_enabled:
        raise RunnerError(
            "skaled build is disabled, and no pre-built binary was found. "
            "Set [skaled].binary to an existing binary path or enable build."
        )

    logger.info("Building skaled from source ...")
    subprocess.run(
        ["cmake", "-H.", f"-B{build_dir}", "-DCMAKE_BUILD_TYPE=Release"],
        cwd=REPO_ROOT, check=True,
    )
    subprocess.run(
        ["cmake", "--build", str(build_dir), "--target", "skaled", "--", "-j4"],
        cwd=REPO_ROOT, check=True,
    )
    if not skaled_bin.is_file():
        raise RunnerError(f"Build failed: no binary at {skaled_bin}")
    return str(skaled_bin)


def launch_skaled_node(binary: str, ncfg: dict, log_dir: Path) -> "Environment.ManagedProcess":
    http_port = ncfg["http_port"]
    cmd = [
        binary,
        "--config", ncfg["config"],
        "--http-port", str(http_port),
        "--ws-port", str(http_port - 1),
        "-v", "9",
        "--web3-trace",
        "--enable-debug-behavior-apis",
        "-d", ncfg["datadir"],
    ]
    log_file = log_dir / f"skaled-{http_port}.log"
    logger.info("Launching skaled (http=%d) -> %s", http_port, log_file)
    log_fd = open(log_file, "w")
    proc = subprocess.Popen(cmd, stdout=log_fd, stderr=subprocess.STDOUT, cwd=REPO_ROOT)
    return Environment.ManagedProcess(proc=proc, log_fd=log_fd)


def launch_anvil(anvil_cfg: dict, log_dir: Path) -> "Environment.ManagedProcess":
    image = anvil_cfg.get("image", "ghcr.io/foundry-rs/foundry:rc-3")
    extra = anvil_cfg.get("extra_args", [])
    container_name = anvil_cfg.get("container_name") or f"skaled-api-tests-anvil-{os.getpid()}"
    log_file = log_dir / f"{container_name}.log"
    cmd = [
        "docker", "run", "--rm",
        "--name", container_name,
        "--network", "host",
        image, "anvil",
        "--host", "0.0.0.0",
    ] + extra
    logger.info("Launching anvil container '%s' -> %s", container_name, log_file)
    log_fd = open(log_file, "w")
    proc = subprocess.Popen(cmd, stdout=log_fd, stderr=subprocess.STDOUT)
    return Environment.ManagedProcess(
        proc=proc,
        log_fd=log_fd,
        docker_container_name=container_name,
    )


def _prepare_btrfs_datadir(datadir: str, node_idx: int):
    """Create a btrfs loopback volume for one node's datadir."""
    import shutil

    image = f"{datadir}.img"
    size_bytes = 3 * 1024 * 1024 * 1024  # 3 GB

    if os.path.ismount(datadir):
        subprocess.run(["sudo", "umount", datadir], check=False)
    if os.path.isdir(datadir):
        shutil.rmtree(datadir, ignore_errors=True)
    if os.path.exists(image):
        result = subprocess.run(
            ["losetup", "-j", image], capture_output=True, text=True, check=False,
        )
        for line in (result.stdout or "").strip().splitlines():
            dev = line.split(":")[0]
            subprocess.run(["sudo", "losetup", "-d", dev], check=False)
        os.remove(image)

    with open(image, "wb") as f:
        f.truncate(size_bytes)

    result = subprocess.run(["losetup", "-f"], capture_output=True, text=True, check=True)
    loop_dev = result.stdout.strip()
    subprocess.run(["sudo", "losetup", loop_dev, image], check=True)
    subprocess.run(["sudo", "mkfs.btrfs", "-f", loop_dev], check=True)
    os.makedirs(datadir, exist_ok=True)
    subprocess.run(["sudo", "mount", loop_dev, datadir], check=True)
    subprocess.run(["sudo", "chmod", "777", datadir], check=True)
    logger.info("btrfs volume ready: %s -> %s", loop_dev, datadir)


# =========================================================================
# Environment management (Step 1)
# =========================================================================

class Environment:
    """Holds launched processes and web3 connections."""

    @dataclass
    class ManagedProcess:
        proc: subprocess.Popen
        log_fd: Optional[TextIO] = None
        docker_container_name: Optional[str] = None

    def __init__(self):
        self.processes: list[Environment.ManagedProcess] = []
        self.web3s: dict[str, Web3] = {}
        self.rpc_urls: dict[str, str] = {}

    def add(self, label: str, managed: ManagedProcess, w3: Web3, url: str):
        self.processes.append(managed)
        self.web3s[label] = w3
        self.rpc_urls[label] = url

    def cleanup(self):
        for managed in self.processes:
            proc = managed.proc
            if proc.poll() is None:
                logger.info("Terminating pid=%d", proc.pid)
                proc.send_signal(signal.SIGTERM)
        for managed in self.processes:
            proc = managed.proc
            try:
                proc.wait(timeout=15)
            except subprocess.TimeoutExpired:
                logger.warning("Force-killing pid=%d", proc.pid)
                proc.kill()
                proc.wait()
            container_name = managed.docker_container_name
            if container_name:
                logger.info("Removing docker container '%s' ...", container_name)
                result = subprocess.run(
                    ["docker", "rm", "-f", container_name],
                    check=False,
                    stdout=subprocess.PIPE,
                    stderr=subprocess.STDOUT,
                    text=True,
                )
                if result.returncode != 0:
                    logger.warning(
                        "docker rm -f %s failed (rc=%d): %s",
                        container_name,
                        result.returncode,
                        (result.stdout or "").strip(),
                    )
            if managed.log_fd:
                managed.log_fd.close()


def setup_environment(cfg: dict, env_type: str) -> Environment:
    """Step 1: launch processes according to env_type.

    "delegated" / "none" — suite manages its own node lifecycle.
    "single"              — one skaled node.
    "two"                 — two connected skaled nodes.
    "anvil"               — skaled + anvil side-by-side.
    """
    if env_type in ("none", "delegated"):
        if env_type == "none":
            logger.warning("env_type='none' is deprecated; use env_type='delegated'")
        logger.info("env_type='%s': suite manages its own node lifecycle.", env_type)
        return Environment()

    env = Environment()
    log_dir = FUNC_TESTS_DIR / "logs"
    log_dir.mkdir(exist_ok=True)

    skaled_cfg = cfg.get("skaled", {})
    template_cfg = skaled_cfg.get("template", {})
    template_context = template_cfg.get("context", {})
    if not isinstance(template_context, dict):
        raise RunnerError("[skaled.template.context] must be a table/object")

    nodes_raw = skaled_cfg.get("nodes", {})
    patches_cfg = skaled_cfg.get("patches", {})
    patch_ts = patches_cfg.get("timestamps", {})
    apply_to = patches_cfg.get("apply_to", [])

    # Resolve node paths relative to repo root
    nodes: dict[int, dict] = {}
    for idx_str, ncfg in nodes_raw.items():
        idx = int(idx_str)
        config_path = resolve_repo_path(ncfg["config"]) if ncfg.get("config") else ""
        config_template = ncfg.get("config_template", "")
        if config_template and config_path:
            template_path = resolve_repo_path(config_template)
            try:
                render_template_file(template_path, config_path, template_context)
            except Exception as e:
                raise RunnerError(f"Failed to render template for node {idx}: {e}") from e

        nodes[idx] = {
            "config": config_path,
            "datadir": resolve_repo_path(ncfg["datadir"]) if ncfg.get("datadir") else "",
            "http_port": ncfg["http_port"],
        }

    # Ensure deployer account has genesis balance
    private_key = cfg.get("type", {}).get("private_key", "")
    for idx, ncfg in nodes.items():
        if ncfg["config"]:
            ensure_genesis_balance(ncfg["config"], private_key)

    # Inject patch timestamps
    if patch_ts:
        for idx, ncfg in nodes.items():
            if ncfg["config"] and (not apply_to or idx in apply_to):
                inject_patches(ncfg["config"], patch_ts)

    # Configure schain node topology based on env_type
    if env_type in ("single", "anvil"):
        if 0 in nodes and nodes[0]["config"]:
            configure_single_node_skaled(nodes[0]["config"], node_index=0)
    elif env_type == "two":
        if 0 in nodes and 1 in nodes:
            configure_two_node_skaled(
                [nodes[0]["config"], nodes[1]["config"]], nodes
            )

    # Prepare datadirs
    use_btrfs = cfg.get("type", {}).get("use_btrfs", False)
    if not use_btrfs:
        snapshot_override = {"snapshotIntervalSec": 0}
        for idx, ncfg in nodes.items():
            if ncfg["config"]:
                inject_patches(ncfg["config"], snapshot_override)

    indices_map: dict[str, list[int]] = {"single": [0], "two": [0, 1], "anvil": [0]}
    indices = indices_map.get(env_type, [0])

    for idx in indices:
        if idx not in nodes:
            raise RunnerError(f"Missing [skaled.nodes.{idx}] in config")
        datadir = nodes[idx]["datadir"]
        if use_btrfs:
            logger.info("Preparing btrfs loopback for node %d ...", idx)
            _prepare_btrfs_datadir(datadir, idx)
        else:
            snapshot_cfg = skaled_cfg.get("snapshot", {})
            if not snapshot_cfg.get("path"):
                if os.path.isdir(datadir):
                    import shutil
                    logger.info("Clearing datadir %s for fresh genesis", datadir)
                    shutil.rmtree(datadir)
            os.makedirs(datadir, exist_ok=True)
            logger.info("Using plain datadir: %s", datadir)

    binary = build_skaled_binary(
        skaled_cfg.get("binary", ""),
        skaled_cfg.get("build", True),
    )
    set_ulimit()

    for idx in indices:
        ncfg = nodes[idx]
        managed = launch_skaled_node(binary, ncfg, log_dir)
        w3 = make_web3("127.0.0.1", ncfg["http_port"])
        url = f"http://127.0.0.1:{ncfg['http_port']}"
        env.add(f"skaled-{idx}", managed, w3, url)

    if env_type == "anvil":
        acfg = cfg.get("anvil", {})
        aport = acfg.get("port", 8545)
        managed = launch_anvil(acfg, log_dir)
        w3 = make_web3("127.0.0.1", aport)
        env.add("anvil", managed, w3, f"http://127.0.0.1:{aport}")

    rpc_timeout = cfg.get("type", {}).get("rpc_up_timeout", 120)
    all_up = all(wait_for_rpc(w3, lbl, rpc_timeout) for lbl, w3 in env.web3s.items())
    if not all_up:
        env.cleanup()
        raise RunnerError("Not all RPC endpoints came up.")

    logger.info("All endpoints UP:")
    for lbl, url in env.rpc_urls.items():
        logger.info("  %s -> %s", lbl, url)

    return env


# =========================================================================
# Block-hash comparison (Step 4 — background thread)
# =========================================================================

class HashChecker(threading.Thread):
    """Periodically compares block hashes across two skaled nodes."""

    def __init__(self, w3_list: list[Web3], interval: int, timeout: int):
        super().__init__(daemon=True)
        self.w3s = w3_list
        self.interval = interval
        self.timeout = timeout
        self.next_block = 0
        self.mismatches = 0
        self.running = True

    def stop(self):
        self.running = False

    def run(self):
        last_progress = time.time()
        while self.running:
            try:
                heads = [w3.eth.block_number for w3 in self.w3s]
                latest = min(heads)
                prev = self.next_block

                for bn in range(self.next_block, latest + 1):
                    hashes = [w3.eth.get_block(bn)["hash"].hex() for w3 in self.w3s]
                    if len(set(hashes)) != 1:
                        self.mismatches += 1
                        logger.error("HASH MISMATCH block %d: %s", bn, hashes)
                self.next_block = latest + 1

                if self.next_block > prev:
                    last_progress = time.time()
                elif time.time() - last_progress > self.timeout:
                    logger.error("No new blocks for %ds — hash checker stopping", self.timeout)
                    return
            except Exception as e:
                logger.warning("Hash check error: %s", e)

            time.sleep(self.interval)


# =========================================================================
# Suite loading
# =========================================================================

def load_suite(suite_name: str):
    """Dynamically import <suite_name>/suite.py and return the module."""
    suite_dir = FUNC_TESTS_DIR / suite_name
    suite_file = suite_dir / "suite.py"
    if not suite_file.is_file():
        raise RunnerError(f"Suite '{suite_name}' not found (expected {suite_file})")

    spec = importlib.util.spec_from_file_location(f"suite_{suite_name}", suite_file)
    if spec is None or spec.loader is None:
        raise RunnerError(f"Failed to load suite module spec: {suite_file}")
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


# =========================================================================
# Anvil output comparison
# =========================================================================

def compare_anvil_results(all_results: dict[str, list[TestResult]]):
    skaled = {r.name: r for r in all_results.get("skaled-0", [])}
    anvil = {r.name: r for r in all_results.get("anvil", [])}

    logger.info("=" * 60)
    logger.info("Anvil comparison:")
    logger.info("=" * 60)

    for name in sorted(set(skaled) | set(anvil)):
        s, a = skaled.get(name), anvil.get(name)
        if s and a:
            match = s.passed == a.passed
            logger.info("  %s: skaled=%s anvil=%s %s",
                        name,
                        "PASS" if s.passed else "FAIL",
                        "PASS" if a.passed else "FAIL",
                        "MATCH" if match else "MISMATCH")
            if not match:
                logger.warning("    skaled: %s", s.message)
                logger.warning("    anvil:  %s", a.message)
        else:
            logger.warning("  %s: missing from %s", name, "anvil" if not a else "skaled")


# =========================================================================
# Main
# =========================================================================

def print_summary(all_results: dict[str, list[TestResult]]) -> bool:
    print("\n" + "=" * 60)
    print("TEST SUMMARY")
    print("=" * 60)

    total_pass = total_fail = 0
    failed_entries: list[tuple[str, TestResult]] = []
    for label in sorted(all_results):
        results = all_results[label]
        print(f"\n  {label}:")
        for r in results:
            tag = "PASS" if r.passed else "FAIL"
            print(f"    {r.name}: {tag} — {r.message}")
            if r.passed:
                total_pass += 1
            else:
                total_fail += 1
                failed_entries.append((label, r))

    print(f"\n  Total: {total_pass} passed, {total_fail} failed")
    print("\n" + "=" * 60)
    print("FAILED TESTS")
    print("=" * 60)
    if not failed_entries:
        print("  None")
    else:
        for label, r in failed_entries:
            print(f"  [{label}] {r.name}")
            print(f"    {r.message}")
    print("=" * 60)
    return total_fail == 0


def main():
    parser = argparse.ArgumentParser(description="API/functional test runner for skaled")
    parser.add_argument("--config", default=str(FUNC_TESTS_DIR / "run.toml"),
                        help="Path to run.toml")
    parser.add_argument("--suite", default=None,
                        help="Override suite name from config")
    args = parser.parse_args()

    logging.basicConfig(
        level=logging.DEBUG,
        format="%(asctime)s [%(levelname)s] %(name)s: %(message)s",
    )
    logging.getLogger("web3").setLevel(logging.WARNING)
    logging.getLogger("urllib3").setLevel(logging.WARNING)

    cfg = load_config(args.config)
    suite_name = args.suite or cfg.get("suite", {}).get("name", "bite-compat")

    # Load and deep-merge per-suite config (overrides base config).
    suite_cfg = load_suite_config(suite_name)
    cfg = _merge_dicts(cfg, suite_cfg)

    env_type = cfg["type"]["env_type"]
    logger.info("Suite: %s | Environment: %s", suite_name, env_type)

    # Load suite module
    suite = load_suite(suite_name)

    env: Optional[Environment] = None
    sgx_managed: Optional[Environment.ManagedProcess] = None
    hash_checker: Optional[HashChecker] = None

    log_dir = FUNC_TESTS_DIR / "logs"
    log_dir.mkdir(exist_ok=True)

    try:
        # ---- Step 0: Launch SGX wallet (optional) ----
        launch_sgx = bool(cfg.get("sgx", {}).get("enabled"))
        suite_section = cfg.get(suite_name.replace("-", "_"), {})
        if "use_sgx" in suite_section:
            launch_sgx = bool(suite_section["use_sgx"])

        if launch_sgx:
            logger.info("=" * 60)
            logger.info("STEP 0: Launching SGX wallet")
            logger.info("=" * 60)
            sgx_managed = setup_sgx(cfg, log_dir)
        else:
            logger.info("STEP 0: SGX wallet launch skipped")

        # ---- Step 1: Setup environment ----
        logger.info("=" * 60)
        logger.info("STEP 1: Setting up environment (%s)", env_type)
        logger.info("=" * 60)
        env = setup_environment(cfg, env_type)

        # ---- Step 4 (background): Hash checker for two-node mode ----
        if env_type == "two" and len(env.web3s) >= 2:
            hc = cfg.get("hash_check", {})
            hash_checker = HashChecker(
                list(env.web3s.values()),
                interval=hc.get("interval_sec", 30),
                timeout=hc.get("no_blocks_timeout_sec", 300),
            )
            hash_checker.start()
            logger.info("Background hash checker started.")

        # ---- Step 2: Deploy contracts ----
        logger.info("=" * 60)
        logger.info("STEP 2: Deploying contracts (%s)", suite_name)
        logger.info("=" * 60)
        suite.deploy(cfg, env)

        # ---- Step 3: Run tests ----
        logger.info("=" * 60)
        logger.info("STEP 3: Running tests (%s)", suite_name)
        logger.info("=" * 60)
        all_results: dict[str, list[TestResult]] = suite.run_tests(cfg, env)

        # Anvil comparison
        if env_type == "anvil":
            compare_anvil_results(all_results)

        success = print_summary(all_results)

        # Hash checker report
        if hash_checker:
            hash_checker.stop()
            hash_checker.join(timeout=5)
            if hash_checker.mismatches > 0:
                logger.error("Hash checker: %d mismatches!", hash_checker.mismatches)
                success = False
            else:
                logger.info("Hash checker: all blocks matched.")

        sys.exit(0 if success else 1)

    except RunnerError as e:
        logger.error("%s", e)
        sys.exit(1)
    except KeyboardInterrupt:
        logger.info("Interrupted by user.")
    finally:
        if hash_checker:
            hash_checker.stop()
        if env:
            env.cleanup()
        if sgx_managed:
            sgx_managed.proc.send_signal(signal.SIGTERM)
            try:
                sgx_managed.proc.wait(timeout=15)
            except subprocess.TimeoutExpired:
                sgx_managed.proc.kill()
                sgx_managed.proc.wait()
            if sgx_managed.docker_container_name:
                subprocess.run(
                    ["docker", "rm", "-f", sgx_managed.docker_container_name],
                    check=False, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                )
            if sgx_managed.log_fd:
                sgx_managed.log_fd.close()
        logger.info("Cleanup complete.")


if __name__ == "__main__":
    main()
