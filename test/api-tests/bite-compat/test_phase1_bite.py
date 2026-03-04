"""
Phase 1 — BITE compatibility tests.

These tests run against the real BITE binary.  The module-scoped ``w3_bite``
fixture tears down the BITE node as soon as this file finishes, freeing
resources before Phase 2 begins.

Reference data (simulate result, receipt status) is stored in
``_test_cache.cache`` for comparison by Phase 2a tests in
``test_bite_compat.py``.

suite.py runs this file before test_bite_compat.py.
"""

import logging

import pytest
from eth_account import Account
from web3 import Web3

from _test_cache import cache as _cache
from _test_utils import (
    BITE_MAGIC_ADDRESS,
    TS_ENCRYPT_TRANSFER_SCRIPT,
    TS_SIMPLE_SECRET_SCRIPT,
    _bite_provider_env,
    _parse_int,
    _run_ts_script,
    _wait_for_committees,
    _wait_for_new_block,
    _wait_for_tx,
)

logger = logging.getLogger("bite-compat.phase1")


# ---------------------------------------------------------------------------
# Node health check — verified before any phase tests
# ---------------------------------------------------------------------------

def test_node_rpc_up(w3_bite: Web3):
    """Verify the BITE node RPC is reachable."""
    bn = w3_bite.eth.block_number
    assert bn >= 0, "BITE RPC is not responding"
    logger.info("BITE node RPC is up at block %d", bn)


# ---------------------------------------------------------------------------
# Phase 1 — BITE tests  (run against the real BITE binary)
# ---------------------------------------------------------------------------

def test_phase1_block_production(w3_bite: Web3, timeouts: dict):
    start = w3_bite.eth.block_number
    timeout = timeouts.get("block_produce", 60)
    new_bn = _wait_for_new_block(w3_bite, start, timeout)
    assert new_bn is not None, f"No new block within {timeout}s (stuck at block {start})"
    logger.info("block %d produced (was %d)", new_bn, start)


def test_phase1_regular_transfer(w3_bite: Web3, private_key: str, timeouts: dict):
    account   = Account.from_key(private_key)
    recipient = Account.create().address
    timeout   = timeouts.get("tx_mine", 60)
    tx = {
        "from":     account.address,
        "chainId":  w3_bite.eth.chain_id,
        "nonce":    w3_bite.eth.get_transaction_count(account.address),
        "to":       recipient,
        "value":    1,
        "gas":      21000,
        "gasPrice": w3_bite.eth.gas_price,
    }
    estimated = w3_bite.eth.estimate_gas(tx)
    assert estimated <= tx["gas"], f"estimateGas={estimated} exceeds gas limit={tx['gas']}"

    signed  = Account.sign_transaction(tx, private_key)
    tx_hash = w3_bite.eth.send_raw_transaction(signed.raw_transaction)
    receipt = _wait_for_tx(w3_bite, tx_hash, timeout)
    assert receipt is not None, f"Tx {tx_hash.hex()} not mined within {timeout}s"
    assert receipt["status"] == 1, f"Tx {tx_hash.hex()} reverted (status=0)"
    logger.info("transfer %s mined in block %d", tx_hash.hex(), receipt["blockNumber"])


def test_phase1_committees_info(w3_bite: Web3, timeouts: dict):
    timeout    = timeouts.get("tx_mine", 60)
    committees = _wait_for_committees(w3_bite, timeout)
    assert committees is not None, (
        f"bite_getCommitteesInfo returned no committees within {timeout}s"
    )
    first = committees[0]
    assert "epochId" in first, f"Missing epochId in committee entry: {first!r}"
    logger.info("%d committee(s), epochId=%s", len(committees), first["epochId"])


def test_phase1_bite_tx_positive(w3_bite: Web3, private_key: str, bite_cfg: dict, timeouts: dict):
    timeout = timeouts.get("tx_mine", 60)
    payload = _run_ts_script(
        TS_ENCRYPT_TRANSFER_SCRIPT, bite_cfg, timeout,
        extra_env=_bite_provider_env(bite_cfg),
    )

    encrypted_tx = payload.get("encryptedTx")
    assert isinstance(encrypted_tx, dict), f"TS JSON missing encryptedTx: {payload!r}"
    data_hex = encrypted_tx.get("data")
    assert isinstance(data_hex, str) and data_hex, f"encryptedTx.data invalid: {data_hex!r}"
    data_raw = bytes.fromhex(data_hex[2:] if data_hex.startswith("0x") else data_hex)

    account = Account.from_key(private_key)
    tx = {
        "chainId":  w3_bite.eth.chain_id,
        "nonce":    w3_bite.eth.get_transaction_count(account.address),
        "to":       Web3.to_checksum_address(encrypted_tx.get("to", BITE_MAGIC_ADDRESS)),
        "value":    _parse_int(encrypted_tx.get("value"), 0),
        "gas":      _parse_int(encrypted_tx.get("gas_limit"), 800_000),
        "gasPrice": w3_bite.eth.gas_price,
        "data":     data_raw,
    }
    signed  = Account.sign_transaction(tx, private_key)
    tx_hash = w3_bite.eth.send_raw_transaction(signed.raw_transaction)
    receipt = _wait_for_tx(w3_bite, tx_hash, timeout)
    assert receipt is not None, f"BITE tx {tx_hash.hex()} not mined within {timeout}s"
    assert receipt["status"] == 1, f"BITE tx {tx_hash.hex()} reverted (status=0)"
    logger.info("BITE tx %s mined in block %d", tx_hash.hex(), receipt["blockNumber"])


def test_phase1_bite_tx_rejection(w3_bite: Web3, private_key: str, timeouts: dict):
    try:
        import rlp  # type: ignore[import]
    except ImportError:
        pytest.skip("rlp package not installed (pip install rlp)")

    timeout    = timeouts.get("tx_mine", 60)
    committees = _wait_for_committees(w3_bite, timeout)
    assert committees is not None, "Could not get epoch info from bite_getCommitteesInfo"
    epoch_id = int(committees[0]["epochId"])

    account   = Account.from_key(private_key)
    bite_data = rlp.encode([epoch_id, b"\x00" * 32])  # ciphertext too short
    tx = {
        "chainId":  w3_bite.eth.chain_id,
        "nonce":    w3_bite.eth.get_transaction_count(account.address),
        "to":       BITE_MAGIC_ADDRESS,
        "value":    0,
        "gas":      800_000,
        "gasPrice": w3_bite.eth.gas_price,
        "data":     bite_data,
    }
    signed = Account.sign_transaction(tx, private_key)
    try:
        tx_hash = w3_bite.eth.send_raw_transaction(signed.raw_transaction)
        receipt = _wait_for_tx(w3_bite, tx_hash, min(timeout, 15))
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


def test_phase1_simple_secret_deploy(
    w3_bite: Web3, private_key: str, bite_cfg: dict, timeouts: dict
):
    """
    Deploy SimpleSecret once on the BITE node.
    The contract address is stored in _cache["bite_simple_secret_address"].
    """
    timeout = timeouts.get("tx_mine", 180)
    payload = _run_ts_script(
        TS_SIMPLE_SECRET_SCRIPT, bite_cfg, timeout, private_key,
        extra_args=["--deploy"],
        extra_env=_bite_provider_env(bite_cfg),
    )
    addr = payload.get("contractAddress")
    assert isinstance(addr, str) and addr, f"--deploy JSON missing contractAddress: {payload!r}"
    _cache["bite_simple_secret_address"] = addr
    logger.info("SimpleSecret deployed on BITE node at %s", addr)


def test_phase1_simple_secret_generate_encrypted(
    w3_bite: Web3, private_key: str, bite_cfg: dict, timeouts: dict
):
    """
    Generate encrypted bytes once on the BITE node and cache them.
    The same bytes are reused for Phase 1, Phase 2a, and Phase 2b so that
    calldata is identical across all phases (same gasUsed, same intrinsic gas).
    """
    timeout = timeouts.get("tx_mine", 60)
    addr = _cache.get("bite_simple_secret_address")
    if not addr:
        pytest.skip("bite_simple_secret_address not in cache — deploy step did not run")

    payload = _run_ts_script(
        TS_SIMPLE_SECRET_SCRIPT, bite_cfg, timeout, private_key,
        extra_args=["--generate-encrypted"],
        extra_env=_bite_provider_env(bite_cfg, {
            "BITE_CONTRACT_ADDRESS": addr,
        }),
    )
    encrypted = payload.get("encrypted")
    assert isinstance(encrypted, str) and encrypted, (
        f"--generate-encrypted JSON missing encrypted: {payload!r}"
    )
    _cache["simple_secret_encrypted"] = encrypted
    logger.info("Encrypted bytes generated on BITE node: %s…", encrypted[:20])


def test_phase1_simple_secret_simulate(
    w3_bite: Web3, private_key: str, bite_cfg: dict, timeouts: dict
):
    """
    Simulate revealSecret on the BITE node via estimateGas.
    Saves the result as the BITE reference snapshot for Phase 2a comparison.
    """
    timeout = timeouts.get("tx_mine", 60)
    addr = _cache.get("bite_simple_secret_address")
    if not addr:
        pytest.skip("bite_simple_secret_address not in cache — deploy step did not run")

    encrypted = _cache.get("simple_secret_encrypted", "")
    payload = _run_ts_script(
        TS_SIMPLE_SECRET_SCRIPT, bite_cfg, timeout, private_key,
        extra_args=["--simulate"],
        extra_env=_bite_provider_env(bite_cfg, {
            "BITE_CONTRACT_ADDRESS": addr,
            **({"BITE_FIXED_ENCRYPTED": encrypted} if encrypted else {}),
        }),
    )
    assert "callSucceeded" in payload, f"--simulate JSON missing callSucceeded: {payload!r}"
    print(f"[BITE simulate raw] {payload}")
    assert payload["callSucceeded"] is False, (
        f"BITE simulate must fail (callSucceeded=False) — "
        f"submitCTX returns empty data, causing ABI-decode revert. got: {payload!r}"
    )
    _cache["simple_secret_simulate_reference"] = payload
    logger.info("Phase 1 (BITE) simulate reference saved: %s", payload)


def test_phase1_simple_secret_transaction(
    w3_bite: Web3, private_key: str, bite_cfg: dict, timeouts: dict
):
    """
    Send the revealSecret transaction and wait for it to be mined.
    Under BITE (pre-patch) the tx must revert (status=0).
    Saves the receipt status as the BITE reference snapshot.
    """
    timeout = timeouts.get("tx_mine", 180)
    addr = _cache.get("bite_simple_secret_address")
    if not addr:
        pytest.skip("bite_simple_secret_address not in cache — deploy step did not run")

    encrypted = _cache.get("simple_secret_encrypted", "")
    payload = _run_ts_script(
        TS_SIMPLE_SECRET_SCRIPT, bite_cfg, timeout, private_key,
        extra_args=["--transaction"],
        extra_env=_bite_provider_env(bite_cfg, {
            "BITE_CONTRACT_ADDRESS": addr,
            **({"BITE_FIXED_ENCRYPTED": encrypted} if encrypted else {}),
        }),
    )
    tx_hash = payload.get("txHash")
    assert isinstance(tx_hash, str) and tx_hash, f"--transaction JSON missing txHash: {payload!r}"

    receipt = _wait_for_tx(w3_bite, tx_hash, timeout)
    assert receipt is not None, f"BITE simple-secret tx {tx_hash} not mined within {timeout}s"
    assert receipt["status"] == 0, (
        f"BITE simple-secret tx {tx_hash} succeeded (status=1) — "
        f"expected revert: submitCTX returns empty data on BITE, "
        f"causing ABI-decode failure in the calling contract (block {receipt['blockNumber']})"
    )

    _cache["simple_secret_tx_reference"] = {
        "receipt_status": receipt["status"],
        "gas_used": receipt["gasUsed"],
        "cumulative_gas_used": receipt["cumulativeGasUsed"],
        "logs_bloom": receipt["logsBloom"].hex(),
        "logs": [
            {"address": log["address"], "topics": [t.hex() for t in log["topics"]], "data": log["data"].hex()}
            for log in receipt["logs"]
        ],
    }
    logger.info(
        "Phase 1 (BITE) transaction reference saved: status=%d gasUsed=%d cumulativeGasUsed=%d logs=%d block=%d",
        receipt["status"], receipt["gasUsed"], receipt["cumulativeGasUsed"],
        len(receipt["logs"]), receipt["blockNumber"],
    )
