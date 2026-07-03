"""
BITE / BITE2 compatibility tests — Phase 2a / 2b.

Test execution order (enforced by definition order in this file):

  Phase 2a  — pre-patch BITE2: behaviour must match BITE (revert via ABI-decode failure)
  Activation — block until bite2PatchTimestamp is reached
  Phase 2b  — post-patch: full BITE2 tests including CTX mining

Pre-patch BITE2 precompileds (submitCTX, encryptTE, encryptECIES) return
{true, {}} when bite2Patch is not yet active, making them indistinguishable
from an unregistered precompile call (same as BITE).

Phase 1 tests (BITE node) live in test_phase1_bite.py and run first.
Reference data written there is read here via _test_cache.cache.

suite.py ensures test_phase1_bite.py runs before this file.
"""

import logging
import time

import pytest
from eth_account import Account
from web3 import Web3

from _test_cache import cache as _cache
from _test_utils import (
    TS_SIMPLE_SECRET_SCRIPT,
    _bite_provider_env,
    _make_rpc_call,
    _run_ts_script,
    _wait_for_bite2_patch_active,
    _wait_for_committees,
    _wait_for_new_block,
    _wait_for_tx,
)

logger = logging.getLogger("bite-compat.tests")


# ---------------------------------------------------------------------------
# Node health check — BITE2 node starts here (after BITE has shut down)
# ---------------------------------------------------------------------------

def test_node_rpc_up(w3: Web3):
    """Verify the BITE2 node RPC is reachable."""
    bn = w3.eth.block_number
    assert bn >= 0, "BITE2 RPC is not responding"
    logger.info("BITE2 node RPC is up at block %d", bn)


def test_current_block_random_patch_activation(
    w3: Web3, current_block_random_patch_ts: int, timeouts: dict
):
    """Wait for currentBlockRandomPatch to activate before Phase 2a begins.

    This patch makes resolveRandomBlockNumber return the current block ID,
    ensuring the reencryption random written by BlockConsensusAgent for the
    current block is the one read in resetEncryptionStateForBlock.
    """
    remaining    = max(0, current_block_random_patch_ts - int(time.time()))
    wait_timeout = remaining + timeouts.get("block_produce", 60) + 60
    active = _wait_for_bite2_patch_active(w3, current_block_random_patch_ts, wait_timeout)
    assert active, (
        f"currentBlockRandomPatchTimestamp {current_block_random_patch_ts} "
        f"not reached after {wait_timeout}s"
    )
    logger.info("currentBlockRandomPatch is now active (patch_ts=%d)", current_block_random_patch_ts)


# ---------------------------------------------------------------------------
# Phase 2a — pre-patch BITE2
# ---------------------------------------------------------------------------

def test_phase2a_simple_secret_simulate(
    w3: Web3, private_key: str, bite_cfg: dict, timeouts: dict
):
    """
    Simulate revealSecret pre-patch on the BITE2 node via estimateGas.
    The contract was deployed on the BITE node (same chain — BITE2 continues
    from the same datadir).  Pre-patch BITE2 routes BITE2-only precompile
    addresses through the unregistered-address path (same as BITE), so
    estimateGas must fail (callSucceeded=False), matching BITE behaviour.
    """
    timeout = timeouts.get("tx_mine", 60)
    addr = _cache.get("bite_simple_secret_address")
    if not addr:
        pytest.skip("bite_simple_secret_address not in cache — Phase 1 deploy did not run")

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
    print(f"[BITE2 pre-patch simulate raw] {payload}")
    assert payload["callSucceeded"] is False, (
        f"Pre-patch BITE2 simulate must fail (callSucceeded=False) — "
        f"submitCTX returns empty data, causing ABI-decode revert. got: {payload!r}"
    )

    ref = _cache.get("simple_secret_simulate_reference")
    if ref is None:
        logger.warning(
            "No Phase 1 (BITE) simulate reference in cache — skipping BITE/BITE2 comparison"
        )
    else:
        assert payload["callSucceeded"] == ref["callSucceeded"], (
            f"Pre-patch BITE2 simulate callSucceeded={payload['callSucceeded']} differs from "
            f"BITE reference callSucceeded={ref['callSucceeded']}"
        )
        logger.info(
            "Pre-patch BITE2 simulate matches BITE reference (callSucceeded=%s)",
            payload["callSucceeded"],
        )


def test_phase2a_simple_secret_transaction(
    w3: Web3, private_key: str, bite_cfg: dict, timeouts: dict
):
    """
    Send revealSecret pre-patch on the BITE2 node using the identical transaction
    data from Phase 1 (same contract, same encrypted bytes, same chain).
    Pre-patch BITE2 routes BITE2-only precompile addresses through the
    unregistered-address path, so the outer transaction reverts (status=0),
    matching BITE.  Receipt fields (gasUsed, logsBloom) must equal the BITE reference.
    """
    timeout = timeouts.get("tx_mine", 180)
    addr = _cache.get("bite_simple_secret_address")
    if not addr:
        pytest.skip("bite_simple_secret_address not in cache — Phase 1 deploy did not run")

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

    receipt = _wait_for_tx(w3, tx_hash, timeout)
    assert receipt is not None, f"submitCTX pre-patch tx {tx_hash} not mined within {timeout}s"
    assert receipt["status"] == 0, (
        f"Pre-patch BITE2 tx {tx_hash} succeeded (status=1) — "
        f"expected revert to match BITE behaviour (block {receipt['blockNumber']})"
    )
    logger.info(
        "Pre-patch BITE2 submitCTX reverted as expected — matches BITE (block %d)",
        receipt["blockNumber"],
    )

    ref = _cache.get("simple_secret_tx_reference")
    if ref is None:
        logger.warning(
            "No Phase 1 (BITE) transaction reference in cache — skipping BITE/BITE2 comparison"
        )
    else:
        assert receipt["status"] == ref["receipt_status"], (
            f"Pre-patch BITE2 receipt status ({receipt['status']}) differs from "
            f"BITE reference ({ref['receipt_status']})"
        )
        actual_logs = [
            {"address": log["address"], "topics": [t.hex() for t in log["topics"]], "data": log["data"].hex()}
            for log in receipt["logs"]
        ]
        assert actual_logs == ref["logs"], (
            f"Pre-patch BITE2 logs differ from BITE reference:\n"
            f"  BITE2: {actual_logs}\n"
            f"  BITE:  {ref['logs']}"
        )
        if "gas_used" in ref:
            assert receipt["gasUsed"] == ref["gas_used"], (
                f"Pre-patch BITE2 gasUsed ({receipt['gasUsed']}) differs from "
                f"BITE reference ({ref['gas_used']}) — BITE2 pre-patch precompileds should "
                f"fall through to the unregistered-address path (same gas as BITE)"
            )
            logger.info(
                "gasUsed matches BITE reference — BITE: %d  BITE2 pre-patch: %d",
                ref["gas_used"], receipt["gasUsed"],
            )
        if "cumulative_gas_used" in ref:
            assert receipt["cumulativeGasUsed"] == ref["cumulative_gas_used"], (
                f"Pre-patch BITE2 cumulativeGasUsed ({receipt['cumulativeGasUsed']}) differs from "
                f"BITE reference ({ref['cumulative_gas_used']})"
            )
        if "logs_bloom" in ref:
            actual_bloom = receipt["logsBloom"].hex()
            assert actual_bloom == ref["logs_bloom"], (
                f"Pre-patch BITE2 logsBloom differs from BITE reference:\n"
                f"  BITE2: {actual_bloom}\n"
                f"  BITE:  {ref['logs_bloom']}"
            )
        logger.info(
            "Pre-patch BITE2 transaction matches BITE reference (status=%d gasUsed=%d cumulativeGasUsed=%d logs=%d)",
            receipt["status"], receipt["gasUsed"], receipt["cumulativeGasUsed"], len(receipt["logs"]),
        )


# ---------------------------------------------------------------------------
# Patch activation — wait for bite2PatchTimestamp to be reached
# ---------------------------------------------------------------------------

def test_patch_activation(w3: Web3, bite2_patch_ts: int, timeouts: dict):
    remaining    = max(0, bite2_patch_ts - int(time.time()))
    wait_timeout = remaining + timeouts.get("block_produce", 60) + 60
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


def test_phase2b_simple_secret_simulate(
    w3: Web3, private_key: str, bite_cfg: dict, timeouts: dict
):
    """
    Simulate revealSecret post-patch on the BITE2 node via estimateGas.
    After bite2Patch activation the call must succeed (callSucceeded=True).
    Uses the same contract address and encrypted bytes as Phase 1 / Phase 2a.
    """
    timeout = timeouts.get("tx_mine", 60)
    addr = _cache.get("bite_simple_secret_address")
    if not addr:
        pytest.skip("bite_simple_secret_address not in cache — Phase 1 deploy did not run")

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
    print(f"[BITE2 post-patch simulate raw] {payload}")
    assert payload["callSucceeded"] is True, (
        f"Post-patch simulate failed (callSucceeded=False): revertReason={payload.get('revertReason')!r}"
    )
    logger.info("Post-patch BITE2 simulate succeeded (gasEstimate=%s)", payload.get("gasEstimate"))


def test_phase2b_submit_ctx_success(
    w3: Web3, private_key: str, bite_cfg: dict, timeouts: dict
):
    """
    SimpleSecret.revealSecret() triggers submitCTX successfully; the crafted
    CTX transaction must appear in the immediately following block.
    Uses the same contract address and encrypted bytes as Phase 1 / Phase 2a.
    """
    timeout = timeouts.get("tx_mine", 60)
    addr = _cache.get("bite_simple_secret_address")
    if not addr:
        pytest.skip("bite_simple_secret_address not in cache — Phase 1 deploy did not run")

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
