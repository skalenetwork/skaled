#!/usr/bin/env python3
"""
Functional test runner.

Reads run.toml, sets up the environment, then delegates to the selected
test suite (a subdirectory with a suite.py).

Usage:
  python3 run.py [--config run.toml] [--suite hardfork-support]

Steps:
  1. Setup environment  (launch skaled / anvil per config)
  2. Deploy contracts   (suite.deploy)
  3. Run tests          (suite.run_tests; anvil mode compares outputs)
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

logger = logging.getLogger("func-tests.run")

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


def resolve_path(p: str) -> str:
    """Resolve a path relative to the func-tests directory."""
    pp = Path(p)
    if pp.is_absolute():
        return str(pp)
    return str((FUNC_TESTS_DIR / pp).resolve())


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


# =========================================================================
# Helpers
# =========================================================================

def set_ulimit():
    import resource
    soft, hard = resource.getrlimit(resource.RLIMIT_NOFILE)
    target = min(65535, hard) if hard > 0 else 65535
    try:
        resource.setrlimit(resource.RLIMIT_NOFILE, (target, hard))
        logger.info("ulimit -n set to %d", target)
    except ValueError:
        logger.warning("Could not raise ulimit (soft=%d, hard=%d)", soft, hard)


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

    # Check case-insensitive (addresses may differ in checksum casing)
    addr_lower = addr.lower()
    for existing in accounts:
        if existing.lower() == addr_lower:
            logger.info("Account %s already in genesis of %s", addr, config_path)
            return

    # 1 billion ETH in wei
    balance = "1000000000000000000000000000000"
    accounts[addr] = {"balance": balance}
    cfg["accounts"] = accounts
    _save_skaled_config(config_path, cfg)
    logger.info("Added genesis balance for %s in %s", addr, config_path)


def inject_patches(config_path: str, patches: dict):
    """Inject patch timestamps into a skaled JSON config's sChain block."""
    with open(config_path, "r") as f:
        cfg = json.load(f)

    schain = cfg.get("skaleConfig", {}).get("sChain", {})
    for key, val in patches.items():
        schain[key] = val
        logger.info("  Patched %s = %s in %s", key, val, config_path)

    cfg.setdefault("skaleConfig", {})["sChain"] = schain
    with open(config_path, "w") as f:
        json.dump(cfg, f, indent=2)


def _load_skaled_config(config_path: str) -> dict:
    with open(config_path, "r") as f:
        return json.load(f)


def _save_skaled_config(config_path: str, cfg: dict):
    with open(config_path, "w") as f:
        json.dump(cfg, f, indent=2)


def _update_node_groups(schain: dict, kept_nodes: list[dict]):
    """Rewrite nodeGroups to contain only the kept schain nodes.

    nodeGroups entries are: { "nodeID": [schainIndex, nodeID, pubkey], ... }
    """
    kept_ids = {n["nodeID"] for n in kept_nodes}
    node_groups = schain.get("nodeGroups", {})
    for _gid, group in node_groups.items():
        old_nodes = group.get("nodes", {})
        new_nodes = {}
        new_idx = 0
        for _nid, ndata in old_nodes.items():
            # ndata is [schainIndex, nodeID, pubkey]
            if isinstance(ndata, list) and len(ndata) >= 2:
                node_id = ndata[1]
                if node_id in kept_ids:
                    ndata[0] = new_idx  # update schainIndex
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
    """Rewrite a skaled config to single-node mode.

    Keeps only schain node at *node_index*, updates nodeGroups and
    nodeInfo to match.
    """
    cfg = _load_skaled_config(config_path)
    schain = cfg.get("skaleConfig", {}).get("sChain", {})
    nodes = schain.get("nodes", [])

    if len(nodes) <= 1:
        logger.info("Config %s already single-node, skipping.", config_path)
        return

    if node_index >= len(nodes):
        logger.warning("node_index %d out of range (%d nodes), using 0",
                        node_index, len(nodes))
        node_index = 0

    kept = nodes[node_index]
    kept["schainIndex"] = 1
    schain["nodes"] = [kept]
    _update_node_groups(schain, [kept])

    # Align nodeInfo with the kept node
    node_info = cfg.get("skaleConfig", {}).get("nodeInfo", {})
    node_info["nodeID"] = kept["nodeID"]
    node_info["basePort"] = kept.get("basePort", node_info.get("basePort"))

    cfg["skaleConfig"]["sChain"] = schain
    cfg["skaleConfig"]["nodeInfo"] = node_info
    _save_skaled_config(config_path, cfg)
    logger.info("Configured %s as single-node (nodeID=%s)", config_path, kept["nodeID"])


def configure_two_node_skaled(config_paths: list[str], nodes_cfg: dict[int, dict]):
    """Rewrite skaled configs for two-node mode.

    Ensures both configs have exactly two schain nodes with correct IPs/ports,
    and each config's nodeInfo points to its own node.
    """
    # Load the first config as the template for the schain node list
    cfg0 = _load_skaled_config(config_paths[0])
    schain = cfg0.get("skaleConfig", {}).get("sChain", {})
    schain_nodes = schain.get("nodes", [])

    if len(schain_nodes) < 2:
        logger.warning("Template config has < 2 schain nodes; two-node setup may be incomplete.")
        return

    # Update schain node IPs/ports from run.toml node configs
    for i, snode in enumerate(schain_nodes[:2]):
        if i in nodes_cfg:
            ncfg = nodes_cfg[i]
            snode["ip"] = "127.0.0.1"
            snode["publicIP"] = "127.0.0.1"
            snode["httpRpcPort"] = ncfg["http_port"]
            snode["basePort"] = ncfg["http_port"] - 3  # convention: basePort = http - 3
            snode["schainIndex"] = i + 1

    schain["nodes"] = schain_nodes[:2]
    _update_node_groups(schain, schain_nodes[:2])

    # Write both configs, each with its own nodeInfo
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
    if last_error is None:
        logger.error("TIMEOUT: RPC %s not available", label)
    else:
        logger.error("TIMEOUT: RPC %s not available (last error: %s)", label, last_error)
    return False


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
        self.web3s: dict[str, Web3] = {}     # label -> Web3
        self.rpc_urls: dict[str, str] = {}   # label -> URL

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


def build_skaled(binary_cfg: str) -> str:
    if binary_cfg and os.path.isfile(binary_cfg):
        logger.info("Using pre-built skaled: %s", binary_cfg)
        return binary_cfg

    build_dir = REPO_ROOT / "build"
    skaled_bin = build_dir / "skaled" / "skaled"

    if skaled_bin.is_file():
        logger.info("Found existing build: %s", skaled_bin)
        return str(skaled_bin)

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


def launch_skaled_node(binary: str, ncfg: dict, log_dir: Path) -> Environment.ManagedProcess:
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


def launch_anvil(anvil_cfg: dict, log_dir: Path) -> Environment.ManagedProcess:
    image = anvil_cfg.get("image", "ghcr.io/foundry-rs/foundry:rc-3")
    extra = anvil_cfg.get("extra_args", [])
    container_name = anvil_cfg.get("container_name") or f"skaled-func-tests-anvil-{os.getpid()}"
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

    # Cleanup existing
    if os.path.ismount(datadir):
        subprocess.run(["sudo", "umount", datadir], check=False)
    if os.path.isdir(datadir):
        shutil.rmtree(datadir, ignore_errors=True)
    if os.path.exists(image):
        # Detach any loopback devices for this image
        result = subprocess.run(
            ["losetup", "-j", image], capture_output=True, text=True, check=False,
        )
        for line in (result.stdout or "").strip().splitlines():
            dev = line.split(":")[0]
            subprocess.run(["sudo", "losetup", "-d", dev], check=False)
        os.remove(image)

    # Create sparse image
    with open(image, "wb") as f:
        f.truncate(size_bytes)

    # Setup loopback
    result = subprocess.run(["losetup", "-f"], capture_output=True, text=True, check=True)
    loop_dev = result.stdout.strip()
    subprocess.run(["sudo", "losetup", loop_dev, image], check=True)

    # Format and mount
    subprocess.run(["sudo", "mkfs.btrfs", "-f", loop_dev], check=True)
    os.makedirs(datadir, exist_ok=True)
    subprocess.run(["sudo", "mount", loop_dev, datadir], check=True)
    subprocess.run(["sudo", "chmod", "777", datadir], check=True)
    logger.info("btrfs volume ready: %s -> %s", loop_dev, datadir)


def setup_environment(cfg: dict) -> Environment:
    """Step 1: launch processes according to env_type."""
    env = Environment()
    env_type = cfg["type"]["env_type"]
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

    # Resolve node paths relative to func-tests/
    nodes: dict[int, dict] = {}
    for idx_str, ncfg in nodes_raw.items():
        idx = int(idx_str)
        config_path = resolve_path(ncfg["config"])
        config_template = ncfg.get("config_template", "")
        if config_template:
            template_path = resolve_path(config_template)
            try:
                render_template_file(template_path, config_path, template_context)
            except Exception as e:
                raise RunnerError(f"Failed to render template for node {idx}: {e}") from e

        nodes[idx] = {
            "config": config_path,
            "datadir": resolve_path(ncfg["datadir"]),
            "http_port": ncfg["http_port"],
        }

    # Ensure deployer account has genesis balance
    private_key = cfg["type"].get("private_key", "")
    for idx, ncfg in nodes.items():
        ensure_genesis_balance(ncfg["config"], private_key)

    # Inject patch timestamps
    if patch_ts:
        for idx, ncfg in nodes.items():
            if not apply_to or idx in apply_to:
                inject_patches(ncfg["config"], patch_ts)

    # Configure schain node topology based on env_type
    if env_type in ("single", "anvil"):
        # Only node 0 is used; rewrite its config to single-node mode
        if 0 in nodes:
            configure_single_node_skaled(nodes[0]["config"], node_index=0)
    elif env_type == "two":
        if 0 in nodes and 1 in nodes:
            configure_two_node_skaled(
                [nodes[0]["config"], nodes[1]["config"]], nodes
            )

    # --- Prepare datadirs ---
    use_btrfs = cfg["type"].get("use_btrfs", False)

    # Without btrfs, snapshots are not possible — force interval to 0.
    if not use_btrfs:
        snapshot_override = {"snapshotIntervalSec": 0}
        for idx, ncfg in nodes.items():
            inject_patches(ncfg["config"], snapshot_override)

    need_skaled = env_type in ("single", "two", "anvil")
    if need_skaled:
        indices = {
            "single": [0],
            "two": [0, 1],
            "anvil": [0],
        }[env_type]

        for idx in indices:
            if idx not in nodes:
                raise RunnerError(f"Missing [skaled.nodes.{idx}] in config")
            datadir = nodes[idx]["datadir"]
            if use_btrfs:
                logger.info("Preparing btrfs loopback for node %d ...", idx)
                _prepare_btrfs_datadir(datadir, idx)
            else:
                # Clear datadir so genesis accounts are applied fresh.
                # Skip if restoring from snapshot.
                snapshot_cfg = skaled_cfg.get("snapshot", {})
                if not snapshot_cfg.get("path"):
                    if os.path.isdir(datadir):
                        import shutil
                        logger.info("Clearing datadir %s for fresh genesis", datadir)
                        shutil.rmtree(datadir)
                os.makedirs(datadir, exist_ok=True)
                logger.info("Using plain datadir: %s", datadir)

    # --- Launch skaled node(s) ---
    if need_skaled:
        binary = build_skaled(skaled_cfg.get("binary", ""))
        set_ulimit()

        for idx in indices:
            if idx not in nodes:
                raise RunnerError(f"Missing [skaled.nodes.{idx}] in config")
            ncfg = nodes[idx]
            managed = launch_skaled_node(binary, ncfg, log_dir)
            w3 = make_web3("127.0.0.1", ncfg["http_port"])
            url = f"http://127.0.0.1:{ncfg['http_port']}"
            env.add(f"skaled-{idx}", managed, w3, url)

    # --- Launch anvil ---
    if env_type == "anvil":
        acfg = cfg.get("anvil", {})
        aport = 8545
        managed = launch_anvil(acfg, log_dir)
        w3 = make_web3("127.0.0.1", aport)
        env.add("anvil", managed, w3, f"http://127.0.0.1:{aport}")

    # Wait for all RPCs
    all_up = all(wait_for_rpc(w3, lbl) for lbl, w3 in env.web3s.items())
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

    print(f"\n  Total: {total_pass} passed, {total_fail} failed")
    print("=" * 60)
    return total_fail == 0


def main():
    parser = argparse.ArgumentParser(description="Functional test runner for skaled")
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
    suite_name = args.suite or cfg.get("suite", {}).get("name", "hardfork-support")
    env_type = cfg["type"]["env_type"]

    logger.info("Suite: %s | Environment: %s", suite_name, env_type)

    # Load suite module
    suite = load_suite(suite_name)

    env: Optional[Environment] = None
    hash_checker: Optional[HashChecker] = None

    try:
        # ---- Step 1: Setup environment ----
        logger.info("=" * 60)
        logger.info("STEP 1: Setting up environment (%s)", env_type)
        logger.info("=" * 60)
        env = setup_environment(cfg)

        # ---- Step 4 (background): Hash checker for two-node mode ----
        if env_type == "two":
            hc = cfg.get("hash_check", {})
            hash_checker = HashChecker(
                [env.web3s["skaled-0"], env.web3s["skaled-1"]],
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

        # Summary
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
        logger.info("Cleanup complete.")


if __name__ == "__main__":
    main()
