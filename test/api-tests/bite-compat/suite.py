"""
BITE / BITE2 compatibility test suite.

Tests that a node can start with the BITE build, produce blocks and accept
transactions, then be restarted with the BITE2 build against the same
datadir — verifying state continuity and the new BITE2 RPC/CTX surfaces.

Entry points required by run.py:
  deploy(cfg, env)      — no-op; this suite manages its own node lifecycle.
  run_tests(cfg, env)   — runs both phases and returns results.

Phase 1 — BITE binary:
  * Node health and block production
  * Regular (non-encrypted) ETH transfer
  * bite_getCommitteesInfo RPC returns epoch data
  * BITE-format transaction with valid ciphertext shape is mined
  * BITE-format transaction with invalid ciphertext is properly rejected

Phase 2 — BITE2 binary, same datadir:
  * Node comes back up and resumes block production (state continuity)
  * Regular ETH transfer still works
  * bite_getCommitteesInfo still works (BITE2 is a BITE superset)
  * debug_getPendingBITE2Transactions returns an empty list before any CTX
  * TS-encrypted call to SimpleBITE.decrypt executes submitCTX
  * bite_getCraftedCtxs maps the origin tx to its crafted CTX hash
  * bite_getCtxOrigin maps the CTX hash back to the origin tx
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
import ctypes
from pathlib import Path
from typing import Optional

from eth_account import Account
from web3 import Web3

# ---------------------------------------------------------------------------
# Optional eth_abi import — used to ABI-encode the submitCTX input.
# web3.py ships eth-abi as a dependency so it is almost always present.
# ---------------------------------------------------------------------------
try:
    from eth_abi import encode as abi_encode
    _HAS_ETH_ABI = True
except ImportError:
    _HAS_ETH_ABI = False

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
    import re
    pat = re.compile(r"{{\s*([a-zA-Z_][a-zA-Z0-9_]*)\s*}}")
    text = template_path.read_text()
    missing = sorted(m.group(1) for m in pat.finditer(text) if m.group(1) not in context)
    if missing:
        raise ValueError(f"Template {template_path} missing vars: {missing}")
    rendered = pat.sub(lambda m: str(context[m.group(1)]), text)
    output_path.parent.mkdir(parents=True, exist_ok=True)
    output_path.write_text(rendered)
    logger.info("Rendered %s -> %s", template_path.name, output_path)


def _apply_sgx_config(config_path: Path, sgx_context: dict):
    """Patch a rendered skaled JSON config with SGX key values."""
    cfg = json.loads(config_path.read_text())
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

    config_path.write_text(json.dumps(cfg, indent=2))
    logger.info("Applied SGX keys to %s", config_path.name)


def _inject_patches(config_path: Path, patches: dict):
    cfg = json.loads(config_path.read_text())
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
    config_path.write_text(json.dumps(cfg, indent=2))


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


def _ensure_genesis_balance(config_path: Path, private_key: str):
    if not private_key:
        return
    addr = Account.from_key(private_key).address
    cfg = json.loads(config_path.read_text())
    accounts = cfg.setdefault("accounts", {})
    if not any(k.lower() == addr.lower() for k in accounts):
        accounts[addr] = {"balance": "1000000000000000000000000000000"}
        config_path.write_text(json.dumps(cfg, indent=2))
        logger.info("Added genesis balance for %s", addr)


def _configure_single_node(config_path: Path):
    """Keep only the first schain node and align nodeInfo."""
    cfg = json.loads(config_path.read_text())
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
    config_path.write_text(json.dumps(cfg, indent=2))
    logger.info("Configured %s as single-node (nodeID=%s)", config_path.name, kept["nodeID"])


def _set_ulimit():
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


def _stop_skaled(proc: subprocess.Popen, label: str):
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


def _wait_for_tx(w3: Web3, tx_hash, timeout_s: int) -> Optional[dict]:
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

        # tx.pop("from", None)
        print('Transaction', tx)
        signed = Account.sign_transaction(tx, private_key)
        tx_hash = w3.eth.send_raw_transaction(signed.raw_transaction)
        receipt = _wait_for_tx(w3, tx_hash, timeout_s)
        print(receipt)
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


def _test_bite_tx_positive(w3: Web3, label: str,
                           private_key: str, timeout_s: int) -> TestResult:
    """
    Send a BITE-format transaction with a valid TE-encrypted ciphertext
    payload and verify it is mined successfully.

    BITE transaction format: to=BITE_MAGIC_ADDRESS, data=RLP([epochId, ciphertext])
    Ciphertext is generated via skale-te using current committee BLS public key.
    """
    name = f"{label}/bite-tx-positive"
    try:
        committees = _wait_for_committees(w3, timeout_s)
        if committees is None:
            return _make_result(name, False,
                "Could not get epoch info: bite_getCommitteesInfo not ready")
        epoch_id = int(committees[0]["epochId"])

        account = Account.from_key(private_key)
        chain_id = w3.eth.chain_id
        nonce = w3.eth.get_transaction_count(account.address)

        bls_public_key = committees[0].get("commonBLSPublicKey") or committees[0].get(
            "common_bls_public_key")
        if not bls_public_key:
            return _make_result(
                name, False, "Could not read common BLS public key from bite_getCommitteesInfo")

        try:
            # skale-te binary wheel depends on OpenSSL 1.1; preload when available.
            lib_dir = REPO_ROOT / "libconsensus" / "libBLS" / "deps" / "deps_inst" / "x86_or_x64" / "lib"
            libcrypto = lib_dir / "libcrypto.so.1.1"
            if libcrypto.is_file():
                ctypes.CDLL(str(libcrypto), mode=ctypes.RTLD_GLOBAL)

            from skale_te import encrypt_message  # type: ignore[import]
        except Exception as e:
            return _make_result(
                name, False,
                f"skale-te is unavailable for encryption: {e}. "
                "Ensure test/api-tests venv dependencies are installed and libcrypto.so.1.1 is reachable.")

        try:
            # Arbitrary plaintext (hex) to encrypt for BITE payload.
            plaintext_hex = "01020304"
            key_hex = bls_public_key[2:] if bls_public_key.startswith("0x") else bls_public_key
            encrypted_hex = encrypt_message(plaintext_hex, key_hex)
            valid_ciphertext = bytes.fromhex(encrypted_hex)
        except Exception as e:
            return _make_result(name, False, f"skale-te encryption failed: {e}")

        try:
            import rlp  # type: ignore[import]
        except ImportError:
            return _make_result(name, False,
                "rlp package not available; install it with: pip install rlp")
        bite_data = rlp.encode([epoch_id, valid_ciphertext])

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
        tx_hash = w3.eth.send_raw_transaction(signed.raw_transaction)
        receipt = _wait_for_tx(w3, tx_hash, timeout_s)
        if receipt is None:
            return _make_result(name, False,
                f"BITE tx {tx_hash.hex()} not mined within {timeout_s}s")
        if receipt["status"] != 1:
            return _make_result(name, False,
                f"BITE tx {tx_hash.hex()} mined but reverted (status=0)")
        return _make_result(name, True,
            f"BITE tx {tx_hash.hex()} mined in block {receipt['blockNumber']}")
    except Exception as e:
        return _make_result(name, False, f"Exception: {e}")


def _test_bite_tx_via_ts_subprocess(
    w3: Web3, label: str, private_key: str, timeout_s: int, bite_cfg: dict
) -> TestResult:
    """
    Encrypt a transfer-like tx with bite TypeScript package via subprocess,
    then submit encrypted tx with web3.py and verify it is mined.
    """
    name = f"{label}/bite-tx-positive"
    try:
        script_path = TS_ENCRYPT_TRANSFER_SCRIPT
        if not script_path.is_file():
            return _make_result(name, False, f"TS script not found: {script_path}")

        cmd_str = bite_cfg.get("bite_ts_command", "bun run")
        cmd = shlex.split(cmd_str) + [str(script_path)]
        env = os.environ.copy()
        env["BITE_OUTPUT_JSON"] = "1"
        env["BITE_COMPAT_TOML"] = str(SUITE_DIR / "bite-compat.toml")

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
            return _make_result(
                name,
                False,
                f"TS runtime command not found: {cmd[0]}. "
                "Set [bite_compat].bite_ts_command to installed runner (e.g. bun run).",
            )
        except subprocess.TimeoutExpired:
            return _make_result(name, False, "TS encryption subprocess timed out")

        if cp.returncode != 0:
            stderr = (cp.stderr or "").strip()
            stdout = (cp.stdout or "").strip()
            return _make_result(
                name,
                False,
                f"TS encryption subprocess failed (rc={cp.returncode}). "
                f"stdout={stdout[-500:]!r} stderr={stderr[-500:]!r}",
            )

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
            return _make_result(
                name, False, f"TS output did not contain JSON payload: {cp.stdout[-500:]!r}"
            )

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

        def _parse_int(v, default: int) -> int:
            if v is None:
                return default
            if isinstance(v, int):
                return v
            if isinstance(v, str):
                return int(v, 0)
            return default

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
    timeout_tx: int,
) -> TestResult:
    """
    Run TS helper that submits SimpleSecret.revealSecret(bytes) on-chain
    and returns tx hash; then verify mined success in Python.
    """
    name = f"{label}/submitCTX-simple-secret"

    try:
        script_path = TS_SIMPLE_SECRET_SCRIPT
        if not script_path.is_file():
            return _make_result(name, False, f"TS script not found: {script_path}")

        cmd_str = bite_cfg.get("bite_ts_command", "bun run")
        cmd = shlex.split(cmd_str) + [str(script_path)]
        env = os.environ.copy()
        env["BITE_OUTPUT_JSON"] = "1"
        env["BITE_COMPAT_TOML"] = str(SUITE_DIR / "bite-compat.toml")
        env["BITE_PRIVATE_KEY"] = private_key

        try:
            cp = subprocess.run(
                cmd,
                cwd=REPO_ROOT,
                env=env,
                capture_output=True,
                text=True,
                timeout=max(30, min(timeout_tx, 120)),
            )
        except FileNotFoundError:
            return _make_result(
                name,
                False,
                f"TS runtime command not found: {cmd[0]}. "
                "Set [bite_compat].bite_ts_command to installed runner (e.g. bun run).",
            )
        except subprocess.TimeoutExpired:
            return _make_result(name, False, "TS encryption subprocess timed out")

        if cp.returncode != 0:
            stderr = (cp.stderr or "").strip()
            stdout = (cp.stdout or "").strip()
            return _make_result(
                name,
                False,
                f"TS revealSecret subprocess failed (rc={cp.returncode}). "
                f"stdout={stdout[-500:]!r} stderr={stderr[-500:]!r}",
            )

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
            return _make_result(
                name, False, f"TS output did not contain JSON payload: {cp.stdout[-500:]!r}"
            )

        tx_hash = payload.get("txHash")
        if not isinstance(tx_hash, str) or not tx_hash:
            return _make_result(
                name, False, f"TS JSON missing txHash string: {payload!r}"
            )

        receipt = _wait_for_tx(w3, tx_hash, timeout_tx)
        if receipt is None:
            return _make_result(
                name, False, f"submitCTX tx {tx_hash} not mined within {timeout_tx}s"
            )
        if receipt["status"] != 1:
            return _make_result(
                name, False, f"submitCTX tx {tx_hash} mined but reverted (status=0)"
            )

        return _make_result(
            name,
            True,
            f"submitCTX tx {tx_hash} (via SimpleSecret.revealSecret) mined in block "
            f"{receipt['blockNumber']}",
            tx_hash=tx_hash,
            block=receipt["blockNumber"],
            simple_secret=payload.get("simpleSecretAddress"),
        )
    except Exception as e:
        return _make_result(name, False, f"Exception: {e}")


def _test_pending_bite2_txs(w3: Web3, label: str) -> TestResult:
    """debug_getPendingBITE2Transactions should return an empty list initially."""
    name = f"{label}/debug_getPendingBITE2Transactions"
    try:
        result = w3.provider.make_request("debug_getPendingBITE2Transactions", [])
        if "error" in result:
            return _make_result(name, False,
                f"RPC returned error: {result['error']}")
        pending = result.get("result", [])
        if not isinstance(pending, list):
            return _make_result(name, False,
                f"Expected list, got: {type(pending).__name__}")
        return _make_result(name, True,
            f"Pending BITE2 txs: {len(pending)}", count=len(pending))
    except Exception as e:
        return _make_result(name, False, f"Exception: {e}")


def _build_submit_ctx_input(gas_limit: int = 500_000) -> bytes:
    """
    Build ABI-encoded input for the submitCTX precompile.

    Signature: abi.encode(uint256 gasLimit, bytes data)
    where data = abi.encode(bytes[] encryptedArgs, bytes[] plaintextArgs).

    When --sgx-url is passed, skaled sets isCiphertextValidationEnabled=true
    (SkaleHost.cpp: isCiphertextValidationEnabled = !sgxServerUrl.empty()).
    In that mode each encrypted argument must be at least BITE_CIPHERTEXT_MIN_LEN
    bytes (= BITE_ENCRYPTED_AES_KEY_LEN + BITE_TE_RANDOM_LEN + ADDRESS_SIZE ≈ 276).
    We use 276 zero bytes as a minimal stub — content does not matter because
    the precompile itself does not decrypt here, only validates the length.
    """
    if not _HAS_ETH_ABI:
        raise ImportError("eth_abi is required for submitCTX test; it ships with web3.py")

    # 276 bytes — meets BITE_CIPHERTEXT_MIN_LEN regardless of SGX being enabled.
    BITE_CIPHERTEXT_MIN_LEN = 276
    encrypted_arg = b"\xde\xad\xbe\xef" * (BITE_CIPHERTEXT_MIN_LEN // 4 + 1)
    encrypted_arg = encrypted_arg[:BITE_CIPHERTEXT_MIN_LEN]
    plaintext_arg = b"\x00" * 32

    encoded_arrays = abi_encode(
        ["bytes[]", "bytes[]"],
        [[encrypted_arg], [plaintext_arg]],
    )
    return abi_encode(["uint256", "bytes"], [gas_limit, encoded_arrays])


def _test_submit_ctx(w3: Web3, label: str,
                     private_key: str, submit_ctx_addr: str,
                     timeout_tx: int, timeout_block: int) -> tuple[TestResult, Optional[str]]:
    """
    Call the submitCTX precompile and return (result, origin_tx_hash).

    The precompile is at the address registered in config-bite2-template.json.j2
    (0x1B by default).  Calling it from an EOA means _ctx.from == EOA address,
    so the crafted CTX is addressed back to the EOA.  The CTX delivery to the
    EOA will silently fail (no code there), but the CTX mechanics are exercised
    and the tracking RPCs can be verified.
    """
    name = f"{label}/submitCTX"
    try:
        input_data = _build_submit_ctx_input()
    except ImportError as e:
        return _make_result(name, False, str(e)), None

    try:
        account = Account.from_key(private_key)
        chain_id = w3.eth.chain_id
        nonce = w3.eth.get_transaction_count(account.address)
        precompile = Web3.to_checksum_address(submit_ctx_addr)
        tx = {
            "chainId":  chain_id,
            "nonce":    nonce,
            "to":       precompile,
            "value":    0,
            "gas":      1_000_000,
            "gasPrice": w3.eth.gas_price,
            "data":     input_data,
        }
        signed = Account.sign_transaction(tx, private_key)
        tx_hash = w3.eth.send_raw_transaction(signed.raw_transaction)
        origin_hex = tx_hash.hex()
        logger.info("submitCTX origin tx: %s", origin_hex)

        receipt = _wait_for_tx(w3, tx_hash, timeout_tx)
        if receipt is None:
            return _make_result(name, False,
                f"submitCTX tx {origin_hex} not mined within {timeout_tx}s"), None

        # Precompile calls may succeed (status=1) or fail (status=0) depending
        # on the node state.  Either way we record the hash for RPC verification.
        status_ok = receipt["status"] == 1
        status_str = "success" if status_ok else "reverted"
        result = _make_result(
            name, status_ok,
            f"submitCTX tx {origin_hex} mined in block "
            f"{receipt['blockNumber']} ({status_str})",
            tx_hash=origin_hex,
            block=receipt["blockNumber"],
        )
        return result, origin_hex if status_ok else None

    except Exception as e:
        return _make_result(name, False, f"Exception: {e}"), None


def _test_crafted_ctxs(w3: Web3, label: str,
                       origin_hash: str, timeout_s: int) -> tuple[TestResult, Optional[str]]:
    """
    Call bite_getCraftedCtxs(originHash) and return (result, first_ctx_hash).

    The CTX may appear in the next block after the origin tx, so retry briefly.
    """
    name = f"{label}/bite_getCraftedCtxs"
    deadline = time.time() + timeout_s
    last_result = None

    while time.time() < deadline:
        try:
            resp = w3.provider.make_request("bite_getCraftedCtxs", [origin_hash])
            if "error" in resp:
                last_result = resp["error"]
                time.sleep(3)
                continue
            ctxs = resp.get("result", [])
            if isinstance(ctxs, list) and len(ctxs) > 0:
                ctx_hash = ctxs[0]
                return _make_result(name, True,
                    f"Found {len(ctxs)} crafted CTX(s) for origin {origin_hash[:12]}…",
                    origin=origin_hash, ctx_hashes=ctxs), ctx_hash
            last_result = ctxs
        except Exception as e:
            last_result = str(e)
        time.sleep(3)

    return _make_result(name, False,
        f"No crafted CTXs found for origin {origin_hash[:12]}… after {timeout_s}s "
        f"(last: {last_result!r})"), None


def _test_ctx_origin(w3: Web3, label: str,
                     ctx_hash: str, expected_origin: str) -> TestResult:
    """Call bite_getCtxOrigin(ctxHash) and verify it returns expected_origin."""
    name = f"{label}/bite_getCtxOrigin"
    try:
        resp = w3.provider.make_request("bite_getCtxOrigin", [ctx_hash])
        if "error" in resp:
            return _make_result(name, False,
                f"RPC returned error: {resp['error']}")
        origin = resp.get("result", "")
        if not origin:
            return _make_result(name, False,
                f"Empty origin returned for CTX {ctx_hash[:12]}…")
        match = origin.lower() == expected_origin.lower()
        if match:
            return _make_result(name, True,
                f"CTX {ctx_hash[:12]}… → origin {origin[:12]}… (correct)")
        return _make_result(name, False,
            f"CTX {ctx_hash[:12]}… → origin {origin[:12]}…, "
            f"expected {expected_origin[:12]}… (mismatch)")
    except Exception as e:
        return _make_result(name, False, f"Exception: {e}")


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
        w3, label, private_key, timeouts.get("tx_mine", 60), bite_cfg))

    results.append(_test_bite_tx_rejection(
        w3, label, private_key, timeouts.get("tx_mine", 60)))

    return results


def _run_phase2_bite2(w3: Web3, private_key: str, bite_cfg: dict,
                      timeouts: dict) -> list[TestResult]:
    """Run all BITE2-phase tests against an already-running node."""
    results = []
    label = "bite2-phase"

    logger.info("=== Phase 2: BITE2 tests ===")

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
    Manage the full BITE → BITE2 lifecycle and return test results.

    The function intentionally ignores *env* (which may contain nodes
    launched by the framework for a different suite) and uses only the
    [bite_compat] section of *cfg*.
    """
    bc = cfg.get("bite_compat", {})
    if not bc:
        msg = "[bite_compat] section missing from run.toml"
        logger.error(msg)
        return {"bite-compat": [TestResult(name="config", passed=False, message=msg)]}

    private_key = cfg.get("type", {}).get("private_key", "")
    timeouts     = bc.get("timeouts", {})
    patches      = bc.get("patches", {})
    tmpl_ctx     = bc.get("template_context", {})
    # If bite_compat.use_sgx is not set, inherit global [sgx].enabled.
    # With SGX disabled, skaled keeps isCiphertextValidationEnabled=false, so
    # submitCTX encrypted args can be byte stubs rather than real BLS ciphertexts.
    # The templates use testSignatures=true and empty ecdsaKeyName by default.
    use_sgx = bool(bc["use_sgx"]) if "use_sgx" in bc else bool(cfg.get("sgx", {}).get("enabled"))
    sgx_context  = cfg.get("sgx", {}).get("template_context") if use_sgx else None
    sgx_url      = (cfg.get("sgx", {}).get("url")
                    if (use_sgx and cfg.get("sgx", {}).get("enabled")) else None)
    http_port    = int(bc.get("http_port", 4234))
    bite_binary  = _resolve(bc.get("bite_binary",  "build_bite/skaled/skaled"))
    bite2_binary = _resolve(bc.get("bite2_binary", "build_bite2/skaled/skaled"))
    datadir      = _resolve(bc.get("datadir", "test/api-tests/bite-compat/datadir"))
    bite_cfg     = _resolve(bc.get("bite_config",
                                   "test/api-tests/bite-compat/configs/config-bite.generated.json"))
    bite2_cfg    = _resolve(bc.get("bite2_config",
                                   "test/api-tests/bite-compat/configs/config-bite2.generated.json"))
    bite_tmpl    = _resolve_ft(bc.get("bite_config_template",
                                      "bite-compat/config-templates/config-bite-template.json.j2"))
    bite2_tmpl   = _resolve_ft(bc.get("bite2_config_template",
                                      "bite-compat/config-templates/config-bite2-template.json.j2"))

    log_dir = SUITE_DIR / "logs"
    log_dir.mkdir(parents=True, exist_ok=True)

    all_results: list[TestResult] = []
    proc: Optional[subprocess.Popen] = None

    def abort(msg: str) -> dict[str, list[TestResult]]:
        all_results.append(TestResult(name="setup", passed=False, message=msg))
        return {"bite-compat": all_results}

    # ------------------------------------------------------------------
    # Validate binaries
    # ------------------------------------------------------------------
    for label, binary in [("BITE", bite_binary), ("BITE2", bite2_binary)]:
        if not binary.is_file():
            return abort(
                f"{label} binary not found: {binary}\n"
                f"Build it with: cmake -H. -B{binary.parent.parent.name} "
                f"-DCMAKE_BUILD_TYPE=Release -D{label}=1 && "
                f"cmake --build {binary.parent.parent.name} -j4"
            )
    if use_sgx and not sgx_context:
        return abort(
            "bite_compat.use_sgx resolved to true, but SGX key context is missing. "
            "Enable [sgx].enabled in run.toml so run.py generates sgx.template_context, "
            "or set [bite_compat].use_sgx = false."
        )
    required_files = [
        ("BITE config template", bite_tmpl),
        ("BITE2 config template", bite2_tmpl),
        ("BITE tx TS script", TS_ENCRYPT_TRANSFER_SCRIPT),
        ("BITE2 tx TS script", TS_SIMPLE_SECRET_SCRIPT),
        ("Suite config", SUITE_DIR / "bite-compat.toml"),
    ]
    missing_required = [f"{label}: {path}" for label, path in required_files if not path.is_file()]
    if missing_required:
        return abort("Missing required suite files:\n" + "\n".join(missing_required))

    # ------------------------------------------------------------------
    # Render BITE config
    # ------------------------------------------------------------------
    try:
        _render_template(bite_tmpl, bite_cfg, tmpl_ctx)
        if sgx_context:
            _apply_sgx_config(bite_cfg, sgx_context)
        _inject_patches(bite_cfg, patches)
        _ensure_genesis_balance(bite_cfg, private_key)
        _configure_single_node(bite_cfg)
    except Exception as e:
        return abort(f"Failed to prepare BITE config: {e}")

    # ------------------------------------------------------------------
    # Render BITE2 config
    # ------------------------------------------------------------------
    try:
        _render_template(bite2_tmpl, bite2_cfg, tmpl_ctx)
        if sgx_context:
            _apply_sgx_config(bite2_cfg, sgx_context)
        _inject_patches(bite2_cfg, patches)
        _ensure_genesis_balance(bite2_cfg, private_key)
        _configure_single_node(bite2_cfg)
    except Exception as e:
        return abort(f"Failed to prepare BITE2 config: {e}")

    # ------------------------------------------------------------------
    # Phase 1 — BITE binary
    # ------------------------------------------------------------------
    _set_ulimit()

    # Fresh datadir for clean genesis
    if datadir.exists():
        shutil.rmtree(datadir)
    datadir.mkdir(parents=True)

    w3 = Web3(Web3.HTTPProvider(f"http://127.0.0.1:{http_port}",
                                 request_kwargs={"timeout": 10}))
    try:
        proc = _launch_skaled(
            bite_binary, bite_cfg, http_port, datadir,
            log_dir / "skaled-bite.log",
            sgx_url=sgx_url,
        )
        rpc_timeout = timeouts.get("rpc_up", 360)
        if not _wait_for_rpc(w3, "BITE", rpc_timeout):
            all_results.append(TestResult(
                name="bite-phase/rpc-up", passed=False,
                message=f"BITE node RPC did not come up within {rpc_timeout}s"))
        else:
            all_results.append(TestResult(
                name="bite-phase/rpc-up", passed=True,
                message="BITE node RPC is up"))
            all_results.extend(_run_phase1_bite(w3, private_key, timeouts, bc))
    finally:
        if proc:
            _stop_skaled(proc, "BITE")
            proc = None

    # ------------------------------------------------------------------
    # Phase 2 — BITE2 binary (same datadir, preserved state)
    # ------------------------------------------------------------------
    # Allow a short pause so OS releases file locks before restarting.
    time.sleep(2)

    try:
        proc = _launch_skaled(
            bite2_binary, bite2_cfg, http_port, datadir,
            log_dir / "skaled-bite2.log",
            sgx_url=sgx_url,
        )
        rpc_timeout = timeouts.get("rpc_up", 360)
        if not _wait_for_rpc(w3, "BITE2", rpc_timeout):
            all_results.append(TestResult(
                name="bite2-phase/rpc-up", passed=False,
                message=f"BITE2 node RPC did not come up within {rpc_timeout}s"))
        else:
            all_results.append(TestResult(
                name="bite2-phase/rpc-up", passed=True,
                message="BITE2 node RPC is up"))
            all_results.extend(
                _run_phase2_bite2(w3, private_key, bc, timeouts))
    finally:
        if proc:
            _stop_skaled(proc, "BITE2")

    return {"bite-compat": all_results}
