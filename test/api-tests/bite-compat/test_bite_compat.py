"""
BITE / BITE2 compatibility tests.

Test execution order (enforced by definition order in this file):

  Phase 1   — BITE tests (BITE2 binary is a full BITE superset)
  Phase 2a  — pre-patch: submitCTX must revert ("bite2Patch not enabled")
  Activation — block until bite2PatchTimestamp is reached
  Phase 2b  — post-patch: full BITE2 tests including CTX mining
"""

import json
import logging
import os
import shlex
import subprocess
import time
from pathlib import Path
from typing import Optional

import pytest
from eth_account import Account
from web3 import Web3

logger = logging.getLogger("bite-compat.tests")

SUITE_DIR = Path(__file__).resolve().parent
REPO_ROOT = SUITE_DIR.parent.parent.parent

BITE_MAGIC_ADDRESS = Web3.to_checksum_address(
    "0x42495445204d452049274d20454e435259505444"
)
BITE_CIPHERTEXT_MIN_LEN = 276  # BITE_ENCRYPTED_AES_KEY_LEN + BITE_TE_RANDOM_LEN + ADDRESS_SIZE

TS_ENCRYPT_TRANSFER_SCRIPT = SUITE_DIR / "scripts" / "make_transaction_bite.ts"
TS_SIMPLE_SECRET_SCRIPT    = SUITE_DIR / "scripts" / "make_transaction_bite2.ts"


# ---------------------------------------------------------------------------
# Shared utilities
# ---------------------------------------------------------------------------

def _parse_int(v, default: int) -> int:
    if v is None:
        return default
    if isinstance(v, int):
        return v
    if isinstance(v, str):
        return int(v, 0)
    return default


def _wait_for_new_block(w3: Web3, from_block: int, timeout_s: int) -> Optional[int]:
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


def _wait_for_committees(w3: Web3, timeout_s: int) -> Optional[list]:
    """Poll bite_getCommitteesInfo until nodeGroups is populated."""
    deadline = time.time() + timeout_s
    while time.time() < deadline:
        try:
            resp = w3.provider.make_request("bite_getCommitteesInfo", [])
            if "error" not in resp:
                committees = resp.get("result")
                if isinstance(committees, list) and committees:
                    return committees
        except Exception:
            pass
        time.sleep(3)
    return None


def _make_rpc_call(w3: Web3, method: str, params: list) -> tuple[bool, object, str]:
    try:
        resp = w3.provider.make_request(method, params)
        if "error" in resp:
            return False, None, f"RPC returned error: {resp['error']}"
        return True, resp.get("result"), ""
    except Exception as e:
        return False, None, f"Exception calling {method}: {e}"


def _run_ts_script(
    script_path: Path,
    bite_cfg: dict,
    timeout_s: int,
    private_key: Optional[str] = None,
) -> dict:
    """
    Run a TypeScript helper script and return the parsed JSON payload dict.
    Calls ``pytest.fail`` on any error.
    """
    if not script_path.is_file():
        pytest.fail(f"TS script not found: {script_path}")

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
        pytest.fail(
            f"TS runtime not found: {cmd[0]!r}. "
            "Set [bite_compat].bite_ts_command (e.g. 'bun run')."
        )
    except subprocess.TimeoutExpired:
        pytest.fail("TS subprocess timed out")

    if cp.returncode != 0:
        pytest.fail(
            f"TS subprocess failed (rc={cp.returncode}). "
            f"stdout={cp.stdout[-500:]!r} stderr={cp.stderr[-500:]!r}"
        )

    for line in reversed((cp.stdout or "").splitlines()):
        line = line.strip()
        if not line:
            continue
        try:
            payload = json.loads(line)
            if isinstance(payload, dict):
                return payload
        except Exception:
            continue

    pytest.fail(f"TS output contained no JSON payload: {cp.stdout[-500:]!r}")


def _wait_for_bite2_patch_active(w3: Web3, patch_ts: int, timeout_s: int) -> bool:
    """Return True once 2 consecutive new blocks have timestamp >= patch_ts."""
    deadline = time.time() + timeout_s
    blocks_above = 0
    last_seen_bn = -1
    logger.info(
        "Waiting for bite2Patch: need 2 blocks with timestamp >= %d (%.0fs from now)",
        patch_ts,
        max(0.0, patch_ts - time.time()),
    )
    while time.time() < deadline:
        try:
            bn = w3.eth.block_number
            if bn != last_seen_bn:
                block_ts = w3.eth.get_block(bn)["timestamp"]
                last_seen_bn = bn
                if block_ts >= patch_ts:
                    blocks_above += 1
                    logger.info(
                        "Block %d ts=%d >= patch_ts=%d (%d/2)",
                        bn, block_ts, patch_ts, blocks_above,
                    )
                    if blocks_above >= 2:
                        return True
                else:
                    blocks_above = 0
                    logger.info(
                        "Block %d ts=%d < patch_ts=%d (wait ~%.0fs)",
                        bn, block_ts, patch_ts, patch_ts - block_ts,
                    )
        except Exception:
            pass
        time.sleep(2)
    return False


# ---------------------------------------------------------------------------
# Node health check — verified before any phase tests
# ---------------------------------------------------------------------------

def test_node_rpc_up(w3: Web3):
    """Verify the BITE2 node RPC is reachable (fixture already waited for it)."""
    bn = w3.eth.block_number
    assert bn >= 0, "RPC is not responding"
    logger.info("BITE2 node RPC is up at block %d", bn)


# ---------------------------------------------------------------------------
# Phase 1 — BITE tests  (BITE2 binary is a full BITE superset)
# ---------------------------------------------------------------------------

def test_phase1_block_production(w3: Web3, timeouts: dict):
    start = w3.eth.block_number
    timeout = timeouts.get("block_produce", 60)
    new_bn = _wait_for_new_block(w3, start, timeout)
    assert new_bn is not None, f"No new block within {timeout}s (stuck at block {start})"
    logger.info("block %d produced (was %d)", new_bn, start)


def test_phase1_regular_transfer(w3: Web3, private_key: str, timeouts: dict):
    account   = Account.from_key(private_key)
    recipient = Account.create().address
    timeout   = timeouts.get("tx_mine", 60)
    tx = {
        "from":     account.address,
        "chainId":  w3.eth.chain_id,
        "nonce":    w3.eth.get_transaction_count(account.address),
        "to":       recipient,
        "value":    1,
        "gas":      21000,
        "gasPrice": w3.eth.gas_price,
    }
    estimated = w3.eth.estimate_gas(tx)
    assert estimated <= tx["gas"], f"estimateGas={estimated} exceeds gas limit={tx['gas']}"

    signed   = Account.sign_transaction(tx, private_key)
    tx_hash  = w3.eth.send_raw_transaction(signed.raw_transaction)
    receipt  = _wait_for_tx(w3, tx_hash, timeout)
    assert receipt is not None, f"Tx {tx_hash.hex()} not mined within {timeout}s"
    assert receipt["status"] == 1, f"Tx {tx_hash.hex()} reverted (status=0)"
    logger.info("transfer %s mined in block %d", tx_hash.hex(), receipt["blockNumber"])


def test_phase1_committees_info(w3: Web3, timeouts: dict):
    timeout    = timeouts.get("tx_mine", 60)
    committees = _wait_for_committees(w3, timeout)
    assert committees is not None, (
        f"bite_getCommitteesInfo returned no committees within {timeout}s"
    )
    first = committees[0]
    assert "epochId" in first, f"Missing epochId in committee entry: {first!r}"
    logger.info("%d committee(s), epochId=%s", len(committees), first["epochId"])


def test_phase1_bite_tx_positive(w3: Web3, private_key: str, bite_cfg: dict, timeouts: dict):
    timeout = timeouts.get("tx_mine", 60)
    payload = _run_ts_script(TS_ENCRYPT_TRANSFER_SCRIPT, bite_cfg, timeout)

    encrypted_tx = payload.get("encryptedTx")
    assert isinstance(encrypted_tx, dict), f"TS JSON missing encryptedTx: {payload!r}"
    data_hex = encrypted_tx.get("data")
    assert isinstance(data_hex, str) and data_hex, f"encryptedTx.data invalid: {data_hex!r}"
    data_raw = bytes.fromhex(data_hex[2:] if data_hex.startswith("0x") else data_hex)

    account = Account.from_key(private_key)
    tx = {
        "chainId":  w3.eth.chain_id,
        "nonce":    w3.eth.get_transaction_count(account.address),
        "to":       Web3.to_checksum_address(encrypted_tx.get("to", BITE_MAGIC_ADDRESS)),
        "value":    _parse_int(encrypted_tx.get("value"), 0),
        "gas":      _parse_int(encrypted_tx.get("gas_limit"), 800_000),
        "gasPrice": w3.eth.gas_price,
        "data":     data_raw,
    }
    signed  = Account.sign_transaction(tx, private_key)
    tx_hash = w3.eth.send_raw_transaction(signed.raw_transaction)
    receipt = _wait_for_tx(w3, tx_hash, timeout)
    assert receipt is not None, f"BITE tx {tx_hash.hex()} not mined within {timeout}s"
    assert receipt["status"] == 1, f"BITE tx {tx_hash.hex()} reverted (status=0)"
    logger.info("BITE tx %s mined in block %d", tx_hash.hex(), receipt["blockNumber"])


def test_phase1_bite_tx_rejection(w3: Web3, private_key: str, timeouts: dict):
    try:
        import rlp  # type: ignore[import]
    except ImportError:
        pytest.skip("rlp package not installed (pip install rlp)")

    timeout    = timeouts.get("tx_mine", 60)
    committees = _wait_for_committees(w3, timeout)
    assert committees is not None, "Could not get epoch info from bite_getCommitteesInfo"
    epoch_id = int(committees[0]["epochId"])

    account   = Account.from_key(private_key)
    bite_data = rlp.encode([epoch_id, b"\x00" * 32])  # ciphertext too short
    tx = {
        "chainId":  w3.eth.chain_id,
        "nonce":    w3.eth.get_transaction_count(account.address),
        "to":       BITE_MAGIC_ADDRESS,
        "value":    0,
        "gas":      800_000,
        "gasPrice": w3.eth.gas_price,
        "data":     bite_data,
    }
    signed = Account.sign_transaction(tx, private_key)
    try:
        tx_hash = w3.eth.send_raw_transaction(signed.raw_transaction)
        receipt = _wait_for_tx(w3, tx_hash, min(timeout, 15))
        if receipt is None:
            logger.info("BITE tx with short ciphertext not mined (rejected as expected)")
            return
        assert receipt["status"] == 0, (
            "BITE tx with short ciphertext was ACCEPTED (status=1) — "
            "validation may be disabled"
        )
        logger.info("BITE tx with short ciphertext reverted (rejected as expected)")
    except Exception as send_err:
        err_msg = str(send_err).lower()
        assert any(k in err_msg for k in ("bite", "invalid", "too short", "reject", "error")), (
            f"Unexpected send error: {send_err}"
        )
        logger.info("BITE tx with short ciphertext rejected at RPC level: %s", send_err)


# ---------------------------------------------------------------------------
# Phase 2a — pre-patch BITE2
# ---------------------------------------------------------------------------

def test_phase2a_submit_ctx_pre_patch_revert(
    w3: Web3, private_key: str, bite_cfg: dict, timeouts: dict
):
    """submitCTX must revert with 'bite2Patch not enabled' before patch activation."""
    timeout = timeouts.get("tx_mine", 180)
    payload = _run_ts_script(TS_SIMPLE_SECRET_SCRIPT, bite_cfg, timeout, private_key)

    tx_hash = payload.get("txHash")
    assert isinstance(tx_hash, str) and tx_hash, f"TS JSON missing txHash: {payload!r}"

    receipt = _wait_for_tx(w3, tx_hash, timeout)
    assert receipt is not None, f"submitCTX pre-patch tx {tx_hash} not mined within {timeout}s"
    assert receipt["status"] == 0, (
        f"submitCTX tx {tx_hash} succeeded (status=1) before bite2Patch activation — "
        f"expected revert (block {receipt['blockNumber']})"
    )
    logger.info(
        "submitCTX correctly reverted before patch activation (block %d)",
        receipt["blockNumber"],
    )


# ---------------------------------------------------------------------------
# Patch activation — wait for bite2PatchTimestamp to be reached
# ---------------------------------------------------------------------------

def test_patch_activation(w3: Web3, bite2_patch_ts: int, timeouts: dict):
    remaining     = max(0, bite2_patch_ts - int(time.time()))
    wait_timeout  = remaining + timeouts.get("block_produce", 60) + 60
    active = _wait_for_bite2_patch_active(w3, bite2_patch_ts, wait_timeout)
    assert active, (
        f"bite2PatchTimestamp {bite2_patch_ts} not reached after {wait_timeout}s"
    )
    logger.info("bite2Patch is now active (patch_ts=%d)", bite2_patch_ts)


# ---------------------------------------------------------------------------
# Phase 2b — post-patch BITE2
# ---------------------------------------------------------------------------

def test_phase2b_block_production(w3: Web3, timeouts: dict):
    start   = w3.eth.block_number
    timeout = timeouts.get("block_produce", 60)
    new_bn  = _wait_for_new_block(w3, start, timeout)
    assert new_bn is not None, (
        f"No new block after patch activation within {timeout}s (stuck at {start})"
    )
    logger.info("block %d produced after patch (was %d)", new_bn, start)


def test_phase2b_regular_transfer(w3: Web3, private_key: str, timeouts: dict):
    account   = Account.from_key(private_key)
    recipient = Account.create().address
    timeout   = timeouts.get("tx_mine", 60)
    tx = {
        "chainId":  w3.eth.chain_id,
        "nonce":    w3.eth.get_transaction_count(account.address),
        "to":       recipient,
        "value":    1,
        "gas":      21000,
        "gasPrice": w3.eth.gas_price,
    }
    signed  = Account.sign_transaction(tx, private_key)
    tx_hash = w3.eth.send_raw_transaction(signed.raw_transaction)
    receipt = _wait_for_tx(w3, tx_hash, timeout)
    assert receipt is not None, f"Transfer tx {tx_hash.hex()} not mined within {timeout}s"
    assert receipt["status"] == 1, f"Transfer tx {tx_hash.hex()} reverted (status=0)"
    logger.info("transfer %s mined in block %d", tx_hash.hex(), receipt["blockNumber"])


def test_phase2b_committees_info(w3: Web3, timeouts: dict):
    timeout    = timeouts.get("tx_mine", 60)
    committees = _wait_for_committees(w3, timeout)
    assert committees is not None, (
        f"bite_getCommitteesInfo returned no committees after patch activation within {timeout}s"
    )
    assert "epochId" in committees[0], f"Missing epochId: {committees[0]!r}"
    logger.info("%d committee(s) after patch activation", len(committees))


def test_phase2b_submit_ctx_success(
    w3: Web3, private_key: str, bite_cfg: dict, timeouts: dict
):
    """
    SimpleSecret.revealSecret() triggers submitCTX successfully; the crafted
    CTX transaction must appear in the immediately following block.
    """
    timeout = timeouts.get("tx_mine", 60)
    payload = _run_ts_script(TS_SIMPLE_SECRET_SCRIPT, bite_cfg, timeout, private_key)

    tx_hash = payload.get("txHash")
    assert isinstance(tx_hash, str) and tx_hash, f"TS JSON missing txHash: {payload!r}"

    submit_receipt = _wait_for_tx(w3, tx_hash, timeout)
    assert submit_receipt is not None, f"submitCTX tx {tx_hash} not mined within {timeout}s"
    assert submit_receipt["status"] == 1, f"submitCTX tx {tx_hash} reverted (status=0)"
    submit_block = submit_receipt["blockNumber"]

    ctx_hash = None
    deadline = time.time() + timeout
    while time.time() < deadline:
        rpc_ok, result, _ = _make_rpc_call(w3, "bite_getCraftedCtxs", [tx_hash])
        if rpc_ok and isinstance(result, list) and result:
            ctx_hash = result[0]
            break
        time.sleep(1)

    assert ctx_hash is not None, (
        f"No crafted CTX found for submit tx {tx_hash} within {timeout}s"
    )

    ctx_receipt = _wait_for_tx(w3, ctx_hash, timeout)
    assert ctx_receipt is not None, f"CTX tx {ctx_hash[:12]}… not mined within {timeout}s"
    ctx_block = ctx_receipt["blockNumber"]
    assert ctx_block == submit_block + 1, (
        f"submitCTX mined in block {submit_block}, "
        f"but CTX {ctx_hash[:12]}… mined in block {ctx_block} "
        f"(expected block {submit_block + 1})"
    )
    logger.info(
        "submitCTX in block %d, CTX %s… correctly mined in block %d",
        submit_block, ctx_hash[:12], ctx_block,
    )
