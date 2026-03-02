"""
BITE / BITE2 compatibility test suite.

Runs a single BITE2 binary with bite2PatchTimestamp set ~101 seconds into the
future, verifying the patch gating and eventual activation.

Entry points required by run.py:
  deploy(cfg, env)      — no-op; this suite manages its own node lifecycle.
  run_tests(cfg, env)   — runs all phases and returns results.

Phase 1 — BITE tests (BITE2 is a BITE superset):
  * Node health and block production
  * Regular (non-encrypted) ETH transfer
  * bite_getCommitteesInfo RPC returns epoch data
  * BITE-format transaction with valid ciphertext shape is mined
  * BITE-format transaction with invalid ciphertext is properly rejected

Phase 2a — pre-patch BITE2 (bite2PatchTimestamp not yet reached):
  * SimpleSecret.revealSecret() triggers submitCTX precompile,
    which must REVERT with "bite2Patch not enabled" (status=0 expected).

Wait — block timestamps reach bite2PatchTimestamp (2 consecutive blocks).

Phase 2b — post-patch BITE2 (patch active):
  * Block production continues
  * Regular ETH transfer still works
  * bite_getCommitteesInfo still works
  * debug_getPendingBITE2Transactions returns a list
  * SimpleSecret.revealSecret() executes submitCTX successfully,
    and the crafted CTX transaction is mined in the next block
"""

import json
import logging
import os
import re
import shlex
import shutil
import signal
import subprocess
import sys
import time
from pathlib import Path
from typing import Optional

from eth_account import Account
from web3 import Web3

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))
from result import TestResult

logger = logging.getLogger("bite-compat.suite")

SUITE_DIR  = Path(__file__).resolve().parent
REPO_ROOT  = SUITE_DIR.parent.parent.parent
FUNC_TESTS = SUITE_DIR.parent

# BITE magic address (from libconsensus/bite/Constants.h: BITE_ADDRESS_AS_STRING)
BITE_MAGIC_ADDRESS = Web3.to_checksum_address(
    "0x42495445204d452049274d20454e435259505444"
)

# TS helper scripts are fixed by suite layout.
TS_ENCRYPT_TRANSFER_SCRIPT = SUITE_DIR / "scripts" / "make_transaction_bite.ts"
TS_SIMPLE_SECRET_SCRIPT = SUITE_DIR / "scripts" / "make_transaction_bite2.ts"

# Constants
BITE_CIPHERTEXT_MIN_LEN = 276  # BITE_ENCRYPTED_AES_KEY_LEN + BITE_TE_RANDOM_LEN + ADDRESS_SIZE
BITE2_PATCH_DELAY_SECONDS = 101  # Delay before bite2Patch activation in tests


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

def _resolve(path_str: str) -> Path:
    """Resolve a path that is relative to the repo root."""
    p = Path(path_str)
    if p.is_absolute():
        return p
    return (REPO_ROOT / p).resolve()


def _resolve_ft(path_str: str) -> Path:
    """Resolve a path that is relative to the api-tests directory."""
    p = Path(path_str)
    if p.is_absolute():
        return p
    return (FUNC_TESTS / p).resolve()


def _render_template(template_path: Path, output_path: Path, context: dict):
    """Simple {{ var }} template renderer (same logic as run.py)."""
    pat = re.compile(r"{{\s*([a-zA-Z_][a-zA-Z0-9_]*)\s*}}")
    text = template_path.read_text()
    missing = sorted(m.group(1) for m in pat.finditer(text) if m.group(1) not in context)
    if missing:
        raise ValueError(f"Template {template_path} missing vars: {missing}")
    rendered = pat.sub(lambda m: str(context[m.group(1)]), text)
    output_path.parent.mkdir(parents=True, exist_ok=True)
    output_path.write_text(rendered)
    logger.info("Rendered %s -> %s", template_path.name, output_path)


def _modify_json_config(config_path: Path, modifier_func) -> None:
    """Load JSON config, apply modifier function, and save back."""
    cfg = json.loads(config_path.read_text())
    modifier_func(cfg)
    config_path.write_text(json.dumps(cfg, indent=2))


def _apply_sgx_config(config_path: Path, sgx_context: dict) -> None:
    """Patch a rendered skaled JSON config with SGX key values."""
    def modifier(cfg):
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

    _modify_json_config(config_path, modifier)
    logger.info("Applied SGX keys to %s", config_path.name)


def _inject_patches(config_path: Path, patches: dict) -> None:
    def modifier(cfg):
        schain = cfg.setdefault("skaleConfig", {}).setdefault("sChain", {})
        for k in patches.get("delete", []):
            removed = schain.pop(k, None)
            if removed is not None:
                logger.info("  Deleted sChain key %s from %s", k, config_path.name)
        for k, v in patches.items():
            if k == "delete":
                continue
            resolved = _resolve_patch_value(v)
            schain[k] = resolved
            logger.info("  Patched %s = %s (from %s) in %s", k, resolved, v, config_path.name)

    _modify_json_config(config_path, modifier)


def _resolve_patch_value(raw) -> int | str:
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


def _ensure_genesis_balance(config_path: Path, private_key: str) -> None:
    if not private_key:
        return
    addr = Account.from_key(private_key).address

    def modifier(cfg):
        accounts = cfg.setdefault("accounts", {})
        if not any(k.lower() == addr.lower() for k in accounts):
            accounts[addr] = {"balance": "1000000000000000000000000000000"}
            logger.info("Added genesis balance for %s", addr)

    _modify_json_config(config_path, modifier)


def _configure_single_node(config_path: Path) -> None:
    """Keep only the first schain node and align nodeInfo."""
    def modifier(cfg):
        schain = cfg.get("skaleConfig", {}).get("sChain", {})
        nodes = schain.get("nodes", [])
        if len(nodes) <= 1:
            return
        kept = nodes[0]
        kept["schainIndex"] = 1
        schain["nodes"] = [kept]

        # Simplify nodeGroups to contain only the kept node
        for _gid, group in schain.get("nodeGroups", {}).items():
            old = group.get("nodes", {})
            new = {}
            for nid, ndata in old.items():
                if isinstance(ndata, list) and len(ndata) >= 2 and ndata[1] == kept["nodeID"]:
                    ndata[0] = 0
                    new[nid] = ndata
                    break
                elif isinstance(ndata, dict) and ndata.get("nodeID") == kept["nodeID"]:
                    ndata["schainIndex"] = 1
                    new[nid] = ndata
                    break
            group["nodes"] = new

        ni = cfg.get("skaleConfig", {}).get("nodeInfo", {})
        ni["nodeID"]   = kept["nodeID"]
        ni["basePort"] = kept.get("basePort", ni.get("basePort"))
        cfg["skaleConfig"]["sChain"]    = schain
        cfg["skaleConfig"]["nodeInfo"]  = ni
        logger.info("Configured %s as single-node (nodeID=%s)", config_path.name, kept["nodeID"])

    _modify_json_config(config_path, modifier)


def _set_ulimit() -> None:
    import resource
    try:
        soft, hard = resource.getrlimit(resource.RLIMIT_NOFILE)
        target = min(65535, hard) if hard > 0 else 65535
        resource.setrlimit(resource.RLIMIT_NOFILE, (target, hard))
    except Exception:
        pass


def _launch_skaled(binary: Path, config: Path, http_port: int,
                   datadir: Path, log_path: Path,
                   sgx_url: Optional[str] = None) -> subprocess.Popen:
    cmd = [
        str(binary),
        "--config",  str(config),
        "--http-port", str(http_port),
        "--ws-port",   str(http_port - 1),
        "-v", "9",
        "--web3-trace",
        "--enable-debug-behavior-apis",
        "--ipcpath", str(datadir),
        "-d", str(datadir),
    ]
    if sgx_url:
        cmd += ["--sgx-url", sgx_url]
    log_path.parent.mkdir(parents=True, exist_ok=True)
    log_fd = open(log_path, "w")
    logger.info("Launching %s (http=%d) -> %s", binary.name, http_port, log_path)
    return subprocess.Popen(cmd, stdout=log_fd, stderr=subprocess.STDOUT, cwd=REPO_ROOT)


def _stop_skaled(proc: subprocess.Popen, label: str) -> None:
    if proc.poll() is not None:
        logger.info("%s already exited (rc=%d)", label, proc.returncode)
        return
    logger.info("Stopping %s (pid=%d) ...", label, proc.pid)
    proc.send_signal(signal.SIGTERM)
    try:
        proc.wait(timeout=20)
        logger.info("%s stopped.", label)
    except subprocess.TimeoutExpired:
        logger.warning("Force-killing %s", label)
        proc.kill()
        proc.wait()


def _wait_for_rpc(w3: Web3, label: str, timeout_s: int) -> bool:
    deadline = time.time() + timeout_s
    while time.time() < deadline:
        try:
            bn = w3.eth.block_number
            logger.info("RPC %s UP (block=%d)", label, bn)
            return True
        except Exception:
            pass
        time.sleep(3)
    logger.error("TIMEOUT: RPC %s not available after %ds", label, timeout_s)
    return False


def _wait_for_new_block(w3: Web3, from_block: int, timeout_s: int) -> Optional[int]:
    """Wait until block_number > from_block, return new block number or None."""
    deadline = time.time() + timeout_s
    while time.time() < deadline:
        try:
            bn = w3.eth.block_number
            if bn > from_block:
                return bn
        except Exception:
            pass
        time.sleep(2)
    return None


def _wait_for_tx(w3: Web3, tx_hash: str | bytes, timeout_s: int) -> Optional[dict]:
    deadline = time.time() + timeout_s
    while time.time() < deadline:
        try:
            receipt = w3.eth.get_transaction_receipt(tx_hash)
            if receipt:
                return receipt
        except Exception:
            pass
        time.sleep(2)
    return None


def _make_result(name: str, passed: bool, message: str, **details) -> TestResult:
    return TestResult(name=name, passed=passed, message=message, details=details)


# ---------------------------------------------------------------------------
# Individual test helpers
# ---------------------------------------------------------------------------

def _parse_int(v, default: int) -> int:
    """Parse an int from int, hex-string, or decimal-string; return default on failure."""
    if v is None:
        return default
    if isinstance(v, int):
        return v
    if isinstance(v, str):
        return int(v, 0)
    return default

def _test_block_production(w3: Web3, label: str, timeout_s: int) -> TestResult:
    name = f"{label}/block-production"
    try:
        start_block = w3.eth.block_number
        new_block = _wait_for_new_block(w3, start_block, timeout_s)
        if new_block is None:
            return _make_result(name, False,
                f"No new block produced within {timeout_s}s (stuck at {start_block})")
        return _make_result(name, True,
            f"Block {new_block} produced (was {start_block})")
    except Exception as e:
        return _make_result(name, False, f"Exception: {e}")


def _test_regular_transfer(w3: Web3, label: str, private_key: str, timeout_s: int) -> TestResult:
    """Dry-run then send a plain ETH transfer and verify it mines."""
    name = f"{label}/regular-transfer"
    try:
        account = Account.from_key(private_key)
        recipient = Account.create().address
        chain_id = w3.eth.chain_id
        nonce = w3.eth.get_transaction_count(account.address)
        tx = {
            "from":     account.address,
            "chainId":  chain_id,
            "nonce":    nonce,
            "to":       recipient,
            "value":    1,
            "gas":      21000,
            "gasPrice": w3.eth.gas_price,
        }
        try:
            estimated_gas = w3.eth.estimate_gas(tx)
            if estimated_gas > tx["gas"]:
                return _make_result(
                    name, False,
                    f"Dry-run estimateGas={estimated_gas} exceeds tx gas={tx['gas']}")
        except Exception as e:
            return _make_result(name, False, f"estimate_gas dry-run failed: {e}")

        signed = Account.sign_transaction(tx, private_key)
        tx_hash = w3.eth.send_raw_transaction(signed.raw_transaction)
        receipt = _wait_for_tx(w3, tx_hash, timeout_s)
        if receipt is None:
            return _make_result(name, False, f"Tx {tx_hash.hex()} not mined within {timeout_s}s")
        if receipt["status"] != 1:
            return _make_result(name, False,
                f"Tx {tx_hash.hex()} mined but reverted (status=0)")
        return _make_result(name, True,
            f"Tx {tx_hash.hex()} mined in block {receipt['blockNumber']}")
    except Exception as e:
        return _make_result(name, False, f"Exception: {e}")


def _wait_for_committees(w3: Web3, timeout_s: int) -> Optional[list]:
    """
    Poll bite_getCommitteesInfo until nodeGroups is populated.

    The node may return a vector::_M_range_check error immediately after
    startup because sChain.nodeGroups is still empty.  Retry until the
    call succeeds and returns a non-empty list, or timeout expires.
    """
    deadline = time.time() + timeout_s
    while time.time() < deadline:
        try:
            resp = w3.provider.make_request("bite_getCommitteesInfo", [])
            if "error" not in resp:
                committees = resp.get("result")
                if isinstance(committees, list) and len(committees) > 0:
                    return committees
        except Exception:
            pass
        time.sleep(3)
    return None


def _test_bite_committees_info(w3: Web3, label: str, timeout_s: int = 60) -> TestResult:
    """Call bite_getCommitteesInfo and verify the response structure."""
    name = f"{label}/bite_getCommitteesInfo"
    try:
        committees = _wait_for_committees(w3, timeout_s)
        if committees is None:
            return _make_result(name, False,
                f"bite_getCommitteesInfo did not return committees within {timeout_s}s "
                f"(node may not have populated nodeGroups yet)")
        first = committees[0]
        if "epochId" not in first:
            return _make_result(name, False,
                f"Missing epochId in committee entry: {first!r}")
        epoch_id = int(first["epochId"])
        return _make_result(name, True,
            f"Got {len(committees)} committee(s), epochId={epoch_id}",
            epoch_id=epoch_id)
    except Exception as e:
        return _make_result(name, False, f"Exception: {e}")


def _test_bite_tx_rejection(w3: Web3, label: str,
                            private_key: str, timeout_s: int) -> TestResult:
    """
    Send a BITE-format transaction whose RLP ciphertext is too short and
    verify the node rejects it (should not appear in a mined block, or the
    send_raw_transaction call itself raises an error).

    BITE transaction format: to=BITE_MAGIC_ADDRESS, data=RLP([epochId, ciphertext])
    Minimum ciphertext size is BITE_CIPHERTEXT_MIN_LEN (≈276 bytes).
    Here we deliberately use a ciphertext that is 32 bytes — too short.
    """
    name = f"{label}/bite-tx-rejection"
    try:
        import rlp  # type: ignore[import]
    except ImportError:
        return _make_result(name, False,
            "rlp package not available; install it with: pip install rlp")

    try:
        # Get current epoch id (needed so the epochId field itself is valid).
        # Retry because nodeGroups may still be empty just after startup.
        committees = _wait_for_committees(w3, timeout_s)
        if committees is None:
            return _make_result(name, False,
                "Could not get epoch info: bite_getCommitteesInfo not ready")
        epoch_id = int(committees[0]["epochId"])

        account = Account.from_key(private_key)
        chain_id = w3.eth.chain_id
        nonce = w3.eth.get_transaction_count(account.address)

        # Ciphertext that is intentionally too short (only 32 zero bytes)
        too_short_ciphertext = b"\x00" * 32
        bite_data = rlp.encode([epoch_id, too_short_ciphertext])

        tx = {
            "chainId":  chain_id,
            "nonce":    nonce,
            "to":       BITE_MAGIC_ADDRESS,
            "value":    0,
            "gas":      800_000,
            "gasPrice": w3.eth.gas_price,
            "data":     bite_data,
        }
        signed = Account.sign_transaction(tx, private_key)

        try:
            tx_hash = w3.eth.send_raw_transaction(signed.raw_transaction)
            # If the node accepted it into the mempool, wait briefly and
            # verify it does NOT get mined (or gets mined with status=0).
            receipt = _wait_for_tx(w3, tx_hash, min(timeout_s, 15))
            if receipt is None:
                # Not mined — acceptable; the node may have queued it but
                # the validation could still reject it during block building.
                return _make_result(name, True,
                    "BITE tx with short ciphertext not mined (rejected as expected)",
                    tx_hash=tx_hash.hex())
            if receipt["status"] == 0:
                return _make_result(name, True,
                    "BITE tx with short ciphertext mined but reverted (rejected as expected)",
                    tx_hash=tx_hash.hex())
            return _make_result(name, False,
                "BITE tx with short ciphertext was ACCEPTED — validation may be disabled",
                tx_hash=tx_hash.hex())

        except Exception as send_err:
            err_msg = str(send_err).lower()
            # Node rejected it at the RPC level (expected)
            if any(k in err_msg for k in ("bite", "invalid", "too short", "reject", "error")):
                return _make_result(name, True,
                    f"BITE tx with short ciphertext rejected at RPC level: {send_err}")
            # Unexpected error
            return _make_result(name, False, f"Unexpected send error: {send_err}")

    except Exception as e:
        return _make_result(name, False, f"Exception: {e}")


def _run_ts_script(
    script_path: Path,
    bite_cfg: dict,
    timeout_s: int,
    private_key: Optional[str] = None,
) -> tuple[bool, Optional[dict], str]:
    """
    Run a TypeScript helper script and return (success, payload, error_msg).

    Returns:
        (True, payload_dict, "") on success
        (False, None, error_message) on failure
    """
    if not script_path.is_file():
        return False, None, f"TS script not found: {script_path}"

    cmd_str = bite_cfg.get("bite_ts_command", "bun run")
    cmd = shlex.split(cmd_str) + [str(script_path)]
    env = os.environ.copy()
    env["BITE_OUTPUT_JSON"] = "1"
    env["BITE_COMPAT_TOML"] = str(SUITE_DIR / "bite-compat.toml")
    if private_key:
        env["BITE_PRIVATE_KEY"] = private_key

    try:
        cp = subprocess.run(
            cmd,
            cwd=REPO_ROOT,
            env=env,
            capture_output=True,
            text=True,
            timeout=max(30, min(timeout_s, 120)),
        )
    except FileNotFoundError:
        return False, None, (
            f"TS runtime command not found: {cmd[0]}. "
            "Set [bite_compat].bite_ts_command to installed runner (e.g. bun run)."
        )
    except subprocess.TimeoutExpired:
        return False, None, "TS subprocess timed out"

    if cp.returncode != 0:
        stderr = (cp.stderr or "").strip()
        stdout = (cp.stdout or "").strip()
        return False, None, (
            f"TS subprocess failed (rc={cp.returncode}). "
            f"stdout={stdout[-500:]!r} stderr={stderr[-500:]!r}"
        )

    # Parse JSON payload from stdout
    payload = None
    for line in reversed((cp.stdout or "").splitlines()):
        line = line.strip()
        if not line:
            continue
        try:
            payload = json.loads(line)
            break
        except Exception:
            continue

    if not isinstance(payload, dict):
        return False, None, f"TS output did not contain JSON payload: {cp.stdout[-500:]!r}"

    return True, payload, ""


def _make_rpc_call(
    w3: Web3, method: str, params: list, test_name: str
) -> tuple[bool, Optional[dict], str]:
    """
    Make an RPC call and return (success, result, error_msg).

    Returns:
        (True, result_dict, "") on success
        (False, None, error_message) on failure
    """
    try:
        resp = w3.provider.make_request(method, params)
        if "error" in resp:
            return False, None, f"RPC returned error: {resp['error']}"
        return True, resp.get("result"), ""
    except Exception as e:
        return False, None, f"Exception calling {method}: {e}"


def _test_bite_tx_via_ts_subprocess(
    w3: Web3, label: str, private_key: str, bite_cfg: dict, timeout_s: int
) -> TestResult:
    """
    Encrypt a transfer-like tx with bite TypeScript package via subprocess,
    then submit encrypted tx with web3.py and verify it is mined.
    """
    name = f"{label}/bite-tx-positive"
    try:
        success, payload, error_msg = _run_ts_script(
            TS_ENCRYPT_TRANSFER_SCRIPT, bite_cfg, timeout_s
        )
        if not success:
            return _make_result(name, False, error_msg)

        encrypted_tx = payload.get("encryptedTx")
        if not isinstance(encrypted_tx, dict):
            return _make_result(
                name, False, f"TS JSON missing encryptedTx object: {payload!r}"
            )

        to_addr = encrypted_tx.get("to", BITE_MAGIC_ADDRESS)
        data_hex = encrypted_tx.get("data")
        if not isinstance(data_hex, str) or not data_hex:
            return _make_result(
                name, False, f"encryptedTx.data is invalid: {data_hex!r}"
            )
        data_raw = bytes.fromhex(data_hex[2:] if data_hex.startswith("0x") else data_hex)

        account = Account.from_key(private_key)
        chain_id = w3.eth.chain_id
        nonce = w3.eth.get_transaction_count(account.address)

        tx = {
            "chainId": chain_id,
            "nonce": nonce,
            "to": Web3.to_checksum_address(to_addr),
            "value": _parse_int(encrypted_tx.get("value"), 0),
            "gas": _parse_int(encrypted_tx.get("gas_limit"), 800_000),
            "gasPrice": w3.eth.gas_price,
            "data": data_raw,
        }
        signed = Account.sign_transaction(tx, private_key)
        tx_hash = w3.eth.send_raw_transaction(signed.raw_transaction)
        receipt = _wait_for_tx(w3, tx_hash, timeout_s)
        if receipt is None:
            return _make_result(
                name, False, f"BITE tx {tx_hash.hex()} not mined within {timeout_s}s"
            )
        if receipt["status"] != 1:
            return _make_result(
                name, False, f"BITE tx {tx_hash.hex()} mined but reverted (status=0)"
            )
        return _make_result(
            name,
            True,
            f"BITE tx {tx_hash.hex()} (TS-encrypted) mined in block {receipt['blockNumber']}",
        )
    except Exception as e:
        return _make_result(name, False, f"Exception: {e}")


def _test_submit_ctx_via_simple_secret(
    w3: Web3,
    label: str,
    private_key: str,
    bite_cfg: dict,
    timeout_s: int,
) -> TestResult:
    """
    Deploy SimpleSecret and call revealSecret(bytes) via the TS helper, then verify:
      1. The submit tx is mined successfully (status=1).
      2. A crafted CTX transaction appears via bite_getCraftedCtxs.
      3. The CTX transaction is mined in the immediately following block.
    """
    name = f"{label}/submitCTX-simple-secret"

    try:
        success, payload, error_msg = _run_ts_script(
            TS_SIMPLE_SECRET_SCRIPT, bite_cfg, timeout_s, private_key
        )
        if not success:
            return _make_result(name, False, error_msg)

        tx_hash = payload.get("txHash")
        if not isinstance(tx_hash, str) or not tx_hash:
            return _make_result(
                name, False, f"TS JSON missing txHash string: {payload!r}"
            )

        submit_receipt = _wait_for_tx(w3, tx_hash, timeout_s)
        if submit_receipt is None:
            return _make_result(
                name, False, f"submitCTX tx {tx_hash} not mined within {timeout_s}s"
            )
        if submit_receipt["status"] != 1:
            return _make_result(
                name, False, f"submitCTX tx {tx_hash} reverted (status=0)"
            )

        submit_block = submit_receipt["blockNumber"]

        # Poll bite_getCraftedCtxs until the CTX hash appears
        ctx_hash = None
        deadline = time.time() + timeout_s
        while time.time() < deadline:
            rpc_ok, result, _ = _make_rpc_call(w3, "bite_getCraftedCtxs", [tx_hash], name)
            if rpc_ok and isinstance(result, list) and len(result) > 0:
                ctx_hash = result[0]
                break
            time.sleep(1)

        if ctx_hash is None:
            return _make_result(
                name, False,
                f"No crafted CTX found for submit tx {tx_hash} within {timeout_s}s"
            )

        ctx_receipt = _wait_for_tx(w3, ctx_hash, timeout_s)
        if ctx_receipt is None:
            return _make_result(
                name, False,
                f"CTX tx {ctx_hash[:12]}… not mined within {timeout_s}s"
            )

        ctx_block = ctx_receipt["blockNumber"]
        expected_ctx_block = submit_block + 1

        if ctx_block != expected_ctx_block:
            return _make_result(
                name, False,
                f"submitCTX mined in block {submit_block}, "
                f"but CTX {ctx_hash[:12]}… mined in block {ctx_block} "
                f"(expected block {expected_ctx_block})"
            )

        return _make_result(
            name, True,
            f"submitCTX mined in block {submit_block}, "
            f"CTX {ctx_hash[:12]}… correctly mined in next block {ctx_block}",
            tx_hash=tx_hash,
            block=submit_block,
            ctx_hash=ctx_hash,
            ctx_block=ctx_block,
            simple_secret=payload.get("simpleSecretAddress"),
        )
    except Exception as e:
        return _make_result(name, False, f"Exception: {e}")


def _test_pending_bite2_txs(w3: Web3, label: str) -> TestResult:
    """debug_getPendingBITE2Transactions should return an empty list initially."""
    name = f"{label}/debug_getPendingBITE2Transactions"
    rpc_ok, pending, error_msg = _make_rpc_call(w3, "debug_getPendingBITE2Transactions", [], name)
    if not rpc_ok:
        return _make_result(name, False, error_msg)
    if not isinstance(pending, list):
        return _make_result(name, False, f"Expected list, got: {type(pending).__name__}")
    return _make_result(name, True, f"Pending BITE2 txs: {len(pending)}", count=len(pending))





def _test_submit_ctx_pre_patch(
    w3: Web3,
    label: str,
    private_key: str,
    bite_cfg: dict,
    timeout_s: int,
) -> TestResult:
    """
    Run TS helper that calls SimpleSecret.revealSecret() BEFORE bite2Patch
    is active.  The submitCTX precompile must revert with
    "bite2Patch not enabled" (status=0 expected).
    """
    name = f"{label}/submitCTX-pre-patch-revert"

    try:
        success, payload, error_msg = _run_ts_script(
            TS_SIMPLE_SECRET_SCRIPT, bite_cfg, timeout_s, private_key
        )
        if not success:
            return _make_result(name, False, error_msg)

        tx_hash = payload.get("txHash")
        if not isinstance(tx_hash, str) or not tx_hash:
            return _make_result(
                name, False, f"TS JSON missing txHash string: {payload!r}"
            )

        receipt = _wait_for_tx(w3, tx_hash, timeout_s)
        if receipt is None:
            return _make_result(
                name, False,
                f"submitCTX pre-patch tx {tx_hash} not mined within {timeout_s}s"
            )

        if receipt["status"] == 0:
            return _make_result(
                name,
                True,
                f"submitCTX tx {tx_hash} correctly reverted before patch activation "
                f"(mined in block {receipt['blockNumber']}) — 'bite2Patch not enabled'",
                tx_hash=tx_hash,
                block=receipt["blockNumber"],
            )
        return _make_result(
            name,
            False,
            f"submitCTX tx {tx_hash} succeeded (status=1) before bite2Patch activation — "
            f"expected revert (mined in block {receipt['blockNumber']})",
            tx_hash=tx_hash,
            block=receipt["blockNumber"],
        )
    except Exception as e:
        return _make_result(name, False, f"Exception: {e}")


def _wait_for_bite2_patch_active(w3: Web3, patch_ts: int, timeout_s: int) -> bool:
    """
    Wait until 2 consecutive newly-seen blocks have timestamp >= patch_ts.
    Returns True if the condition is met within timeout_s seconds.
    """
    deadline = time.time() + timeout_s
    blocks_above = 0
    last_seen_bn = -1
    logger.info(
        "Waiting for bite2Patch activation: need 2 blocks with timestamp >= %d "
        "(%.0f seconds from now)",
        patch_ts,
        max(0.0, patch_ts - time.time()),
    )
    while time.time() < deadline:
        try:
            bn = w3.eth.block_number
            if bn != last_seen_bn:
                block = w3.eth.get_block(bn)
                block_ts = block["timestamp"]
                last_seen_bn = bn
                if block_ts >= patch_ts:
                    blocks_above += 1
                    logger.info(
                        "Block %d timestamp %d >= patch_ts %d (count=%d/2)",
                        bn, block_ts, patch_ts, blocks_above,
                    )
                    if blocks_above >= 2:
                        logger.info("bite2Patch is now active.")
                        return True
                else:
                    blocks_above = 0
                    logger.info(
                        "Block %d timestamp %d < patch_ts %d (wait ~%.0fs)",
                        bn, block_ts, patch_ts, patch_ts - block_ts,
                    )
        except Exception:
            pass
        time.sleep(2)
    logger.error("Timed out waiting for bite2Patch activation after %ds", timeout_s)
    return False


# ---------------------------------------------------------------------------
# Phase runners
# ---------------------------------------------------------------------------

def _run_phase1_bite(
    w3: Web3, private_key: str, timeouts: dict, bite_cfg: dict
) -> list[TestResult]:
    """Run all BITE-phase tests against an already-running node."""
    results = []
    label = "bite-phase"

    logger.info("=== Phase 1: BITE tests ===")

    results.append(_test_block_production(
        w3, label, timeouts.get("block_produce", 60)))

    results.append(_test_regular_transfer(
        w3, label, private_key, timeouts.get("tx_mine", 60)))

    results.append(_test_bite_committees_info(w3, label, timeouts.get("tx_mine", 60)))

    results.append(_test_bite_tx_via_ts_subprocess(
        w3, label, private_key, bite_cfg, timeouts.get("tx_mine", 60)))

    results.append(_test_bite_tx_rejection(
        w3, label, private_key, timeouts.get("tx_mine", 60)))

    return results


def _run_pre_patch_bite2(
    w3: Web3, private_key: str, bite_cfg: dict, timeouts: dict
) -> list[TestResult]:
    """Run pre-patch BITE2 tests: submitCTX must revert before patch activates."""
    results = []
    label = "bite2-pre-patch"
    logger.info("=== Phase 2a: BITE2 pre-patch tests ===")
    results.append(
        _test_submit_ctx_pre_patch(
            w3, label, private_key, bite_cfg, timeouts.get("tx_mine", 180)
        )
    )
    return results


def _run_phase2_bite2(w3: Web3, private_key: str, bite_cfg: dict,
                      timeouts: dict) -> list[TestResult]:
    """Run all BITE2-phase tests against an already-running node."""
    results = []
    label = "bite2-phase"

    logger.info("=== Phase 2b: BITE2 post-patch tests ===")

    results.append(_test_block_production(
        w3, label, timeouts.get("block_produce", 60)))

    results.append(_test_regular_transfer(
        w3, label, private_key, timeouts.get("tx_mine", 60)))

    results.append(_test_bite_committees_info(w3, label, timeouts.get("tx_mine", 60)))

    results.append(_test_pending_bite2_txs(w3, label))

    results.append(
        _test_submit_ctx_via_simple_secret(
            w3,
            label,
            private_key,
            bite_cfg,
            timeouts.get("tx_mine", 60),
        )
    )

    return results


# ---------------------------------------------------------------------------
# Public entry points
# ---------------------------------------------------------------------------

def deploy(cfg: dict, env) -> None:
    """No-op. This suite manages its own node lifecycle in run_tests()."""
    pass


def run_tests(cfg: dict, env) -> dict[str, list[TestResult]]:
    """
    Manage the BITE2 lifecycle with patch gating and return test results.

    Launches a single BITE2 binary with bite2PatchTimestamp set ~101 seconds
    into the future, then:
      Phase 1   — BITE tests (block production, transfers, committees, BITE txs)
      Phase 2a  — pre-patch: SimpleSecret CTX must revert (bite2Patch not enabled)
      Wait      — until 2 blocks with timestamp >= bite2PatchTimestamp
      Phase 2b  — post-patch: full BITE2 tests (CTX success, RPC tracking)

    The function intentionally ignores *env* and uses only [bite_compat] in *cfg*.
    """
    bc = cfg.get("bite_compat", {})
    if not bc:
        msg = "[bite_compat] section missing from run.toml"
        logger.error(msg)
        return {"bite-compat": [TestResult(name="config", passed=False, message=msg)]}

    private_key  = cfg.get("type", {}).get("private_key", "")
    timeouts     = bc.get("timeouts", {})
    patches      = bc.get("patches", {})
    tmpl_ctx     = bc.get("template_context", {})
    use_sgx = bool(bc["use_sgx"]) if "use_sgx" in bc else bool(cfg.get("sgx", {}).get("enabled"))
    sgx_context  = cfg.get("sgx", {}).get("template_context") if use_sgx else None
    sgx_url      = (cfg.get("sgx", {}).get("url")
                    if (use_sgx and cfg.get("sgx", {}).get("enabled")) else None)
    http_port    = int(bc.get("http_port", 4234))
    bite2_binary = _resolve(bc.get("bite2_binary", "build-bite2/skaled/skaled"))
    datadir      = _resolve(bc.get("datadir", "test/api-tests/bite-compat/datadir"))
    bite2_cfg    = _resolve(bc.get("bite2_config",
                                   "test/api-tests/bite-compat/configs/config-bite2.generated.json"))
    bite2_tmpl   = _resolve_ft(bc.get("bite2_config_template",
                                      "bite-compat/config-templates/config-bite2-template.json.j2"))

    log_dir = SUITE_DIR / "logs"
    log_dir.mkdir(parents=True, exist_ok=True)

    all_results: list[TestResult] = []
    proc: Optional[subprocess.Popen] = None

    def abort(msg: str) -> dict[str, list[TestResult]]:
        all_results.append(TestResult(name="setup", passed=False, message=msg))
        return {"bite-compat": all_results}

    def _append_log_tail(log_path: Path, summary_path: Path, mode: str = "a"):
        if not log_path.exists():
            return
        try:
            log_lines = log_path.read_text().splitlines()
            tail = log_lines[-100:] if len(log_lines) > 100 else log_lines
            with summary_path.open(mode) as f:
                f.write("=" * 80 + "\n")
                f.write(f"{log_path.name} (last {len(tail)} lines):\n")
                f.write("=" * 80 + "\n")
                f.write("\n".join(tail) + "\n")
                f.write("=" * 80 + "\n\n")
        except Exception as exc:
            logger.error("Failed to read %s: %s", log_path.name, exc)

    # ------------------------------------------------------------------
    # Validate binary and required files
    # ------------------------------------------------------------------
    if not bite2_binary.is_file():
        return abort(
            f"BITE2 binary not found: {bite2_binary}\n"
            f"Build it with: cmake -H. -Bbuild-bite2 "
            f"-DCMAKE_BUILD_TYPE=Release -DBITE2=1 && "
            f"cmake --build build-bite2 -j4"
        )
    if use_sgx and not sgx_context:
        return abort(
            "bite_compat.use_sgx resolved to true, but SGX key context is missing. "
            "Enable [sgx].enabled in run.toml so run.py generates sgx.template_context, "
            "or set [bite_compat].use_sgx = false."
        )
    required_files = [
        ("BITE2 config template", bite2_tmpl),
        ("BITE tx TS script", TS_ENCRYPT_TRANSFER_SCRIPT),
        ("BITE2 tx TS script", TS_SIMPLE_SECRET_SCRIPT),
        ("Suite config", SUITE_DIR / "bite-compat.toml"),
    ]
    missing_required = [
        f"{lbl}: {path}" for lbl, path in required_files if not path.is_file()
    ]
    if missing_required:
        return abort("Missing required suite files:\n" + "\n".join(missing_required))

    # ------------------------------------------------------------------
    # Compute bite2PatchTimestamp = now + BITE2_PATCH_DELAY_SECONDS
    # ------------------------------------------------------------------
    bite2_patch_ts = int(time.time()) + BITE2_PATCH_DELAY_SECONDS
    logger.info(
        "bite2PatchTimestamp set to %d (now + %ds, activates at %s)",
        bite2_patch_ts,
        BITE2_PATCH_DELAY_SECONDS,
        time.strftime("%H:%M:%S", time.localtime(bite2_patch_ts)),
    )

    # ------------------------------------------------------------------
    # Render BITE2 config, injecting bite2PatchTimestamp
    # ------------------------------------------------------------------
    try:
        _render_template(bite2_tmpl, bite2_cfg, tmpl_ctx)
        if sgx_context:
            _apply_sgx_config(bite2_cfg, sgx_context)
        patches_with_ts = dict(patches)
        patches_with_ts["bite2PatchTimestamp"] = bite2_patch_ts
        _inject_patches(bite2_cfg, patches_with_ts)
        _ensure_genesis_balance(bite2_cfg, private_key)
        _configure_single_node(bite2_cfg)
    except Exception as e:
        return abort(f"Failed to prepare BITE2 config: {e}")

    # ------------------------------------------------------------------
    # Launch BITE2 binary (fresh datadir)
    # ------------------------------------------------------------------
    _set_ulimit()

    if datadir.exists():
        shutil.rmtree(datadir)
    datadir.mkdir(parents=True)

    w3 = Web3(Web3.HTTPProvider(f"http://127.0.0.1:{http_port}",
                                request_kwargs={"timeout": 10}))

    skaled_log  = log_dir / "skaled-bite2.log"
    summary_log = log_dir / "summary.log"

    try:
        proc = _launch_skaled(
            bite2_binary, bite2_cfg, http_port, datadir,
            skaled_log,
            sgx_url=sgx_url,
        )
        rpc_timeout = timeouts.get("rpc_up", 360)

        if not _wait_for_rpc(w3, "BITE2", rpc_timeout):
            all_results.append(TestResult(
                name="bite2/rpc-up", passed=False,
                message=f"BITE2 node RPC did not come up within {rpc_timeout}s"))
            return {"bite-compat": all_results}

        all_results.append(TestResult(
            name="bite2/rpc-up", passed=True,
            message="BITE2 node RPC is up"))

        # ------------------------------------------------------------------
        # Phase 1 — BITE tests (BITE2 is a full superset of BITE)
        # ------------------------------------------------------------------
        phase1_results = _run_phase1_bite(w3, private_key, timeouts, bc)
        all_results.extend(phase1_results)
        if any(not r.passed for r in phase1_results):
            logger.error("=== BITE tests failed. Writing log tail to %s ===", summary_log)
            _append_log_tail(skaled_log, summary_log, mode="w")

        # ------------------------------------------------------------------
        # Phase 2a — pre-patch BITE2: submitCTX must revert
        # ------------------------------------------------------------------
        pre_patch_results = _run_pre_patch_bite2(w3, private_key, bc, timeouts)
        all_results.extend(pre_patch_results)
        if any(not r.passed for r in pre_patch_results):
            logger.error("=== Pre-patch BITE2 tests failed. Writing log tail ===")
            _append_log_tail(skaled_log, summary_log)

        # ------------------------------------------------------------------
        # Wait for bite2PatchTimestamp to be reached (2 consecutive blocks)
        # ------------------------------------------------------------------
        patch_wait_timeout = BITE2_PATCH_DELAY_SECONDS + timeouts.get("block_produce", 60) + 60
        patch_active = _wait_for_bite2_patch_active(w3, bite2_patch_ts, patch_wait_timeout)
        if not patch_active:
            all_results.append(TestResult(
                name="bite2/patch-activation", passed=False,
                message=(
                    f"bite2PatchTimestamp {bite2_patch_ts} not reached in "
                    f"{patch_wait_timeout}s — post-patch tests skipped"
                ),
            ))
            return {"bite-compat": all_results}

        all_results.append(TestResult(
            name="bite2/patch-activation", passed=True,
            message=f"bite2Patch activated (bite2PatchTimestamp={bite2_patch_ts})"))

        # ------------------------------------------------------------------
        # Phase 2b — post-patch BITE2 tests
        # ------------------------------------------------------------------
        phase2_results = _run_phase2_bite2(w3, private_key, bc, timeouts)
        all_results.extend(phase2_results)
        if any(not r.passed for r in phase2_results):
            logger.error("=== Post-patch BITE2 tests failed. Writing log tail ===")
            _append_log_tail(skaled_log, summary_log)

    finally:
        if proc:
            _stop_skaled(proc, "BITE2")

    return {"bite-compat": all_results}
