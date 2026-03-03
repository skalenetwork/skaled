#!/usr/bin/env python3
"""
Functional test runner.

Reads run.toml, sets up the environment, then delegates to the selected
test suite (a subdirectory with a suite.py).

Usage:
  python3 run.py [--config run.toml] [--suite bite-compat]

Steps:
  1. Setup environment  (suite manages its own lifecycle for delegated suites)
  2. Deploy contracts   (suite.deploy)
  3. Run tests          (suite.run_tests)
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
import time
import tomllib
from pathlib import Path
from typing import Any, Optional, TextIO

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
    """Poll the SGX wallet TCP port until it accepts connections.

    Uses a raw socket connect rather than HTTP so it works for gRPC
    (HTTP/2) servers that don't speak plain HTTP/1.1.
    """
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

    logger.error(
        "SGX wallet log tail (%s, last %d lines):",
        log_path,
        len(tail),
    )
    for line in tail:
        logger.error("[sgxwallet] %s", line.rstrip("\n"))


def generate_sgx_keys(sgx_url: str, keys_file: Path) -> dict:
    """Generate ECDSA and BLS keys in the SGX wallet.

    Delegates to scripts/run_with_sgx/utils/sgx_import.py.
    Keys are always regenerated because the SGX wallet container is
    ephemeral (--rm) and loses all stored keys on every restart.
    The generated keys are saved to *keys_file* for reference.
    Returns a dict of sgx_* template variables ready for config rendering.
    """
    sgx_import = REPO_ROOT / "scripts" / "run_with_sgx" / "utils" / "sgx_import.py"
    if not sgx_import.is_file():
        raise RunnerError(f"sgx_import.py not found: {sgx_import}")
    logger.info("Generating SGX keys via %s ...", sgx_import)
    result = subprocess.run(
        [sys.executable, str(sgx_import), "--sgx-url", sgx_url],
        cwd=str(sgx_import.parent.parent),  # scripts/run_with_sgx/
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
    """Patch a rendered skaled JSON config with SGX key values.

    Sets ecdsaKeyName, wallets.ima, nodeGroups BLS/ECDSA keys, and
    blsPublicKey fields in sChain.nodes from *sgx_context*.
    """
    with open(config_path) as f:
        cfg = json.load(f)

    node_info = cfg["skaleConfig"]["nodeInfo"]
    schain    = cfg["skaleConfig"]["sChain"]

    node_info["ecdsaKeyName"] = sgx_context["sgx_ecdsa_key_name"]
    # testSignatures=true causes skaled to skip reading ecdsaKeyName from config.
    # Must be false when using a real SGX wallet.
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


def setup_sgx(cfg: dict, _log_dir: Path) -> "Optional[Environment.ManagedProcess]":
    """Launch the SGX wallet container and generate keys.

    Writes the generated sgx_* vars into cfg["sgx"]["template_context"] so
    suite.py can use them for config rendering.
    Returns the ManagedProcess for the container so the caller can clean it up.
    """
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
        # Clean up the container before raising so main() doesn't receive a
        # partially-started process it cannot know about.
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
                    ndata[0] = new_idx + 1  # update schainIndex (1-based)
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


# =========================================================================
# Environment management (Step 1)
# =========================================================================

class Environment:
    """Holds launched processes."""

    @dataclass
    class ManagedProcess:
        proc: subprocess.Popen
        log_fd: Optional[TextIO] = None
        docker_container_name: Optional[str] = None

    def __init__(self):
        self.processes: list[Environment.ManagedProcess] = []

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
    """Step 1: suite manages its own node lifecycle (delegated mode)."""
    if env_type == "none":
        logger.warning("env_type='none' is deprecated; use env_type='delegated'")
    logger.info("env_type='%s': suite manages its own node lifecycle.", env_type)
    return Environment()


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
    parser = argparse.ArgumentParser(description="API test runner for skaled")
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

    log_dir = FUNC_TESTS_DIR / "logs"
    log_dir.mkdir(exist_ok=True)

    try:
        # ---- Step 0: Launch SGX wallet (optional) ----
        launch_sgx = bool(cfg.get("sgx", {}).get("enabled"))
        # Allow the suite's own config section to override the global SGX flag.
        # Convention: a suite named "foo-bar" may set use_sgx under [foo_bar].
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

        success = print_summary(all_results)
        sys.exit(0 if success else 1)

    except RunnerError as e:
        logger.error("%s", e)
        sys.exit(1)
    except KeyboardInterrupt:
        logger.info("Interrupted by user.")
    finally:
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
