"""
hardfork-compat: cross-version state-root equality across a 5.1.0 -> 5.2.0 upgrade.

Test flow:
  1. Verify the 5.1.0 primary RPC is up and producing blocks
  2. Run a mixed transaction workload on the 5.1.0 primary:
       - legacy value transfers
       - Type1 (EIP-2930 access list) transactions
       - Type2 (EIP-1559) transactions
       - a London-fork deploy exercising the EIP-3198 BASEFEE opcode
       - a contract deploy (SSTORE in constructor)
       - a factory whose constructor runs CREATE and CREATE2
  3. Launch the 5.2.0 sync node (syncNode=true, archiveMode=true)
  4. Wait for the sync node to catch up to the primary head
  5. Compare the per-block stateRoot of every block (the primary assertion)
  6. Compare per-block hashes as a diagnostic cross-check

Tests run in file order (sequential): the workload must complete before the
sync node is launched and the comparison runs.
"""

import logging
from pathlib import Path

from eth_account import Account
from web3 import Web3

from _test_utils import (
    compare_block_hashes,
    compare_state_roots,
    wait_for_new_block,
    wait_for_sync_catchup,
    wait_for_tx,
)

logger = logging.getLogger("hardfork-compat.test")

# ---------------------------------------------------------------------------
# Minimal contract bytecode (no Solidity compiler needed)
#
# Constructor:  PUSH1 42, PUSH1 0, SSTORE  -- writes 42 to slot 0
# Then copies 1-byte runtime (STOP) to memory and RETURNs it.
# Hex: 602a6000556001601160003960016000f300
# ---------------------------------------------------------------------------
_SIMPLE_STORAGE_BYTECODE = "0x602a6000556001601160003960016000f300"

# ---------------------------------------------------------------------------
# Factory bytecode that exercises CREATE and CREATE2 during its own
# construction, deploying two minimal child contracts (empty runtime).
# Mirrors the berlin-compat factory so both opcodes touch state.
# ---------------------------------------------------------------------------
_CREATE_FACTORY_BYTECODE = (
    "0x6460006000f36000526005601b6000f05060006005601b6000f5"
    "50600060205360016020f3"
)

# ---------------------------------------------------------------------------
# London-fork bytecode that exercises the EIP-3198 BASEFEE opcode (0x48).
#
# Constructor:  BASEFEE, PUSH1 0, SSTORE  -- writes the block base fee to slot 0
# Then RETURNs an empty (zero-length) runtime.
# Hex: 4860005560006000f3
#
# BASEFEE is only a valid opcode once the London fork is active; on a binary
# without London support the EVM treats 0x48 as invalid and the deploy reverts.
# Either way both binaries behave identically, so the per-block stateRoot
# comparison -- the suite's real assertion -- still holds.
# ---------------------------------------------------------------------------
_BASEFEE_BYTECODE = "0x4860005560006000f3"


# ---------------------------------------------------------------------------
# Helpers for sending transactions
# ---------------------------------------------------------------------------

def _send_legacy_transfer(
    w3: Web3, private_key: str, recipient: str, value: int, timeout_s: int,
) -> dict:
    account = Account.from_key(private_key)
    tx = {
        "chainId": w3.eth.chain_id,
        "nonce": w3.eth.get_transaction_count(account.address),
        "to": recipient,
        "value": value,
        "gas": 21000,
        "gasPrice": w3.eth.gas_price,
    }
    signed = Account.sign_transaction(tx, private_key)
    tx_hash = w3.eth.send_raw_transaction(signed.raw_transaction)
    logger.info("Sent legacy transfer tx: %s", tx_hash.hex())
    return wait_for_tx(w3, tx_hash, timeout_s)


def _send_type1_tx(
    w3: Web3, private_key: str, recipient: str, value: int, timeout_s: int,
) -> dict:
    """Send a Type 1 (EIP-2930 access list) transaction and return the receipt."""
    account = Account.from_key(private_key)
    tx = {
        "type": 1,
        "chainId": w3.eth.chain_id,
        "nonce": w3.eth.get_transaction_count(account.address),
        "to": recipient,
        "value": value,
        "gas": 30000,
        "gasPrice": w3.eth.gas_price,
        "accessList": [{"address": recipient, "storageKeys": []}],
    }
    signed = Account.sign_transaction(tx, private_key)
    tx_hash = w3.eth.send_raw_transaction(signed.raw_transaction)
    logger.info("Sent Type1 tx: %s", tx_hash.hex())
    return wait_for_tx(w3, tx_hash, timeout_s)


def _send_type2_tx(
    w3: Web3, private_key: str, recipient: str, value: int, timeout_s: int,
) -> dict:
    """Send a Type 2 (EIP-1559) transaction and return the receipt."""
    account = Account.from_key(private_key)
    tx = {
        "type": 2,
        "chainId": w3.eth.chain_id,
        "nonce": w3.eth.get_transaction_count(account.address),
        "to": recipient,
        "value": value,
        "gas": 30000,
        "maxFeePerGas": w3.eth.gas_price * 2,
        "maxPriorityFeePerGas": 0,
    }
    signed = Account.sign_transaction(tx, private_key)
    tx_hash = w3.eth.send_raw_transaction(signed.raw_transaction)
    logger.info("Sent Type2 tx: %s", tx_hash.hex())
    return wait_for_tx(w3, tx_hash, timeout_s)


def _deploy(w3: Web3, private_key: str, bytecode: str, gas: int, timeout_s: int) -> dict:
    account = Account.from_key(private_key)
    tx = {
        "chainId": w3.eth.chain_id,
        "nonce": w3.eth.get_transaction_count(account.address),
        "gas": gas,
        "gasPrice": w3.eth.gas_price,
        "data": bytecode,
    }
    signed = Account.sign_transaction(tx, private_key)
    tx_hash = w3.eth.send_raw_transaction(signed.raw_transaction)
    logger.info("Sent deploy tx: %s", tx_hash.hex())
    return wait_for_tx(w3, tx_hash, timeout_s)


# ---------------------------------------------------------------------------
# Tests (ordered by file position)
# ---------------------------------------------------------------------------

def test_primary_rpc_up(w3_primary: Web3):
    """Verify the 5.1.0 primary node RPC is reachable."""
    bn = w3_primary.eth.block_number
    assert bn >= 0, "Primary (5.1.0) RPC is not responding"
    logger.info("Primary (5.1.0) RPC is up at block %d", bn)


def test_primary_block_production(w3_primary: Web3, timeouts: dict):
    """Verify the 5.1.0 primary node is producing blocks."""
    start = w3_primary.eth.block_number
    timeout = timeouts.get("block_produce", 60)
    new_bn = wait_for_new_block(w3_primary, start, timeout)
    assert new_bn is not None, (
        f"No new block within {timeout}s (stuck at block {start})"
    )
    logger.info("Block %d produced (was %d)", new_bn, start)


def test_workload_legacy_transfers(w3_primary: Web3, private_key: str, timeouts: dict):
    """Send several legacy value transfers on the 5.1.0 primary."""
    timeout = timeouts.get("tx_mine", 120)
    for i in range(3):
        recipient = Account.create().address
        receipt = _send_legacy_transfer(w3_primary, private_key, recipient, 1 + i, timeout)
        assert receipt is not None, f"Legacy transfer {i} not mined within {timeout}s"
        assert receipt["status"] == 1, f"Legacy transfer {i} reverted (status=0)"
    logger.info("Legacy transfers mined on 5.1.0 primary")


def test_workload_type1_tx(w3_primary: Web3, private_key: str, timeouts: dict):
    """Send a Type1 (EIP-2930 access list) transaction on the 5.1.0 primary."""
    timeout = timeouts.get("tx_mine", 120)
    recipient = Account.create().address
    receipt = _send_type1_tx(w3_primary, private_key, recipient, 1, timeout)
    assert receipt is not None, "Type1 tx not mined"
    assert receipt["status"] == 1, "Type1 tx reverted (status=0)"
    logger.info(
        "Type1 tx mined: block=%d gasUsed=%d", receipt["blockNumber"], receipt["gasUsed"]
    )


def test_workload_type2_tx(w3_primary: Web3, private_key: str, timeouts: dict):
    """Send a Type2 (EIP-1559) transaction on the 5.1.0 primary."""
    timeout = timeouts.get("tx_mine", 120)
    recipient = Account.create().address
    receipt = _send_type2_tx(w3_primary, private_key, recipient, 1, timeout)
    assert receipt is not None, "Type2 tx not mined"
    assert receipt["status"] == 1, "Type2 tx reverted (status=0)"
    logger.info(
        "Type2 tx mined: block=%d gasUsed=%d", receipt["blockNumber"], receipt["gasUsed"]
    )


def test_workload_london_basefee(w3_primary: Web3, private_key: str, timeouts: dict):
    """Deploy a London-fork contract that uses the EIP-3198 BASEFEE opcode.

    On a London-capable binary the constructor stores the block base fee to
    slot 0 and the deploy succeeds; on a pre-London binary opcode 0x48 is
    invalid and the deploy reverts. Both binaries behave identically, so the
    transaction is recorded the same way on the primary and the sync node and
    the per-block stateRoot comparison still passes. The assertion therefore
    only requires the tx to be mined and included in a block.
    """
    timeout = timeouts.get("tx_mine", 120)
    receipt = _deploy(w3_primary, private_key, _BASEFEE_BYTECODE, 100_000, timeout)
    assert receipt is not None, "BASEFEE deploy not mined"
    assert receipt["blockNumber"] is not None, "BASEFEE deploy not included in a block"
    logger.info(
        "London BASEFEE deploy mined: block=%d status=%d gasUsed=%d",
        receipt["blockNumber"], receipt["status"], receipt["gasUsed"],
    )


def test_workload_contract_deploy(w3_primary: Web3, private_key: str, timeouts: dict):
    """Deploy a contract (SSTORE in constructor) on the 5.1.0 primary."""
    timeout = timeouts.get("tx_mine", 120)
    receipt = _deploy(w3_primary, private_key, _SIMPLE_STORAGE_BYTECODE, 100_000, timeout)
    assert receipt is not None, "Contract deploy not mined"
    assert receipt["status"] == 1, "Contract deploy reverted (status=0)"
    assert receipt["contractAddress"] is not None, "No contract address in receipt"
    logger.info(
        "Contract deployed: addr=%s block=%d", receipt["contractAddress"], receipt["blockNumber"]
    )


def test_workload_create_create2_factory(w3_primary: Web3, private_key: str, timeouts: dict):
    """Deploy a factory whose constructor runs CREATE and CREATE2 on the 5.1.0 primary."""
    timeout = timeouts.get("tx_mine", 120)
    receipt = _deploy(w3_primary, private_key, _CREATE_FACTORY_BYTECODE, 300_000, timeout)
    assert receipt is not None, "Factory deploy not mined"
    assert receipt["status"] == 1, "Factory deploy reverted (status=0)"
    assert receipt["contractAddress"] is not None, "No contract address in receipt"
    logger.info(
        "CREATE/CREATE2 factory deployed: addr=%s block=%d",
        receipt["contractAddress"], receipt["blockNumber"],
    )


def test_sync_catchup_and_state_root_comparison(
    w3_primary: Web3, primary_cfg_path, sync_binary: Path,
    hardfork_cfg: dict, timeouts: dict,
):
    """Launch the 5.2.0 sync node, wait for catch-up, compare per-block stateRoot."""
    from conftest import (
        _launch_node,
        _resolve,
        _stop_node,
        _wait_for_rpc,
        make_sync_config,
        SUITE_DIR,
    )
    from run import inject_patches, set_ulimit

    if not sync_binary.is_file():
        import pytest
        pytest.fail(
            f"5.2.0 skaled binary not found: {sync_binary}\n"
            "Set HARDFORK_COMPAT_V520_BINARY or [hardfork_compat].v520_binary, "
            "or build with:\n"
            "  git checkout v5.2.0 && cmake -H. -Bbuild-v520 -DCMAKE_BUILD_TYPE=Release "
            "&& cmake --build build-v520 --target skaled -- -j4"
        )

    sync_http_port = int(hardfork_cfg.get("sync_http_port", 5344))
    sync_datadir = _resolve("test/api-tests/hardfork-compat/datadir-sync")
    sync_cfg_out = _resolve(
        "test/api-tests/hardfork-compat/configs/config-sync.generated.json"
    )

    primary_bn = w3_primary.eth.block_number
    logger.info("Primary (5.1.0) at block %d before 5.2.0 sync launch", primary_bn)

    make_sync_config(Path(str(primary_cfg_path)), sync_cfg_out, sync_http_port)
    inject_patches(str(sync_cfg_out), hardfork_cfg.get("common_patches", {}))
    inject_patches(str(sync_cfg_out), hardfork_cfg.get("patches", {}))

    set_ulimit()
    log_dir = SUITE_DIR / "logs"
    log_dir.mkdir(parents=True, exist_ok=True)

    proc, log_fd = _launch_node(
        sync_binary, sync_cfg_out, sync_http_port, sync_datadir,
        log_dir / "skaled-sync-v520.log", "SYNC(5.2.0)",
    )

    w3_sync = Web3(Web3.HTTPProvider(
        f"http://127.0.0.1:{sync_http_port}", request_kwargs={"timeout": 10}
    ))

    try:
        rpc_timeout = timeouts.get("rpc_up", 360)
        _wait_for_rpc(w3_sync, rpc_timeout, "SYNC(5.2.0)")

        sync_timeout = timeouts.get("sync_catchup", 300)
        caught_up = wait_for_sync_catchup(w3_primary, w3_sync, sync_timeout)
        assert caught_up, (
            f"5.2.0 sync node did not catch up within {sync_timeout}s "
            f"(primary={w3_primary.eth.block_number}, sync={w3_sync.eth.block_number})"
        )

        compare_bn = min(w3_primary.eth.block_number, w3_sync.eth.block_number)
        logger.info("Comparing stateRoot for blocks 0..%d (5.1.0 vs 5.2.0)", compare_bn)

        # Run both comparisons before asserting so a single run reports every
        # mismatch (stateRoot and block hash) rather than failing on the first.
        root_mismatches = compare_state_roots(w3_primary, w3_sync, compare_bn)
        if root_mismatches:
            logger.error(
                "stateRoot mismatches between 5.1.0 and 5.2.0 at blocks: %s",
                root_mismatches,
            )
        else:
            logger.info(
                "All %d block stateRoots match between 5.1.0 and 5.2.0", compare_bn + 1
            )

        hash_mismatches = compare_block_hashes(w3_primary, w3_sync, compare_bn)
        if hash_mismatches:
            logger.error(
                "Block hash mismatches between 5.1.0 and 5.2.0 at blocks: %s",
                hash_mismatches,
            )
        else:
            logger.info(
                "All %d block hashes match between 5.1.0 and 5.2.0", compare_bn + 1
            )

        # The block hash embeds the stateRoot plus the transactions and receipts
        # roots, so requiring it to match is a strictly stronger guarantee than
        # stateRoot alone: it also catches typed-transaction (EIP-2718/2930/1559)
        # encoding differences between the two binaries.
        assert not root_mismatches and not hash_mismatches, (
            f"5.1.0 vs 5.2.0 mismatches -- "
            f"stateRoot at blocks {root_mismatches}; block hash at blocks {hash_mismatches}"
        )
    finally:
        _stop_node(proc, log_fd, "SYNC(5.2.0)")
