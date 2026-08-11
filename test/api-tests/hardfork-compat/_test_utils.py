"""Shared utilities for the hardfork-compat test suite.

The underscore prefix prevents pytest from collecting this as a test module.
"""

import logging
import time
from typing import Optional

from web3 import Web3

logger = logging.getLogger("hardfork-compat.utils")


def wait_for_new_block(w3: Web3, from_block: int, timeout_s: int) -> Optional[int]:
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


def wait_for_tx(w3: Web3, tx_hash, timeout_s: int) -> Optional[dict]:
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


def wait_for_block_timestamp(w3: Web3, target_timestamp: int, timeout_s: int) -> Optional[int]:
    """Wait until the latest block timestamp reaches ``target_timestamp``."""
    deadline = time.time() + timeout_s
    last_seen = None
    while time.time() < deadline:
        try:
            block = w3.eth.get_block("latest")
            last_seen = (block["number"], block["timestamp"])
            logger.info(
                "Waiting for timestamp >= %d: latest block=%d timestamp=%d",
                target_timestamp, block["number"], block["timestamp"],
            )
            if int(block["timestamp"]) >= target_timestamp:
                return int(block["number"])
        except Exception:
            pass
        time.sleep(2)
    logger.error(
        "Timed out waiting for block timestamp >= %d (last seen=%s)",
        target_timestamp, last_seen,
    )
    return None


def wait_for_sync_catchup(
    w3_primary: Web3, w3_sync: Web3, timeout_s: int,
) -> bool:
    """Wait until the sync node's block number reaches the primary's."""
    deadline = time.time() + timeout_s
    while time.time() < deadline:
        try:
            primary_bn = w3_primary.eth.block_number
            sync_bn = w3_sync.eth.block_number
            logger.info(
                "Sync progress: sync=%d primary=%d (delta=%d)",
                sync_bn, primary_bn, primary_bn - sync_bn,
            )
            if sync_bn >= primary_bn:
                return True
        except Exception:
            pass
        time.sleep(5)
    return False


def _block_state_root(w3: Web3, bn: int) -> str:
    """Return the lowercase hex stateRoot of block ``bn``."""
    block = w3.eth.get_block(bn)
    root = block["stateRoot"]
    return root.hex() if hasattr(root, "hex") else str(root)


def compare_state_roots(
    w3_primary: Web3, w3_sync: Web3, up_to_block: int,
) -> list[int]:
    """Compare per-block stateRoot from block 0 to ``up_to_block``.

    Returns the list of mismatched block numbers (empty means all match).
    A mismatch means the archive sync node derived a different world state than
    the upgraded primary while replaying the same block -- i.e. an unguarded
    state-transition change across the fork/upgrade path.
    """
    mismatches = []
    for bn in range(0, up_to_block + 1):
        try:
            primary_root = _block_state_root(w3_primary, bn)
            sync_root = _block_state_root(w3_sync, bn)
            if primary_root != sync_root:
                logger.error(
                    "STATE ROOT MISMATCH block %d: primary=%s sync=%s",
                    bn, primary_root, sync_root,
                )
                mismatches.append(bn)
            else:
                logger.debug("Block %d stateRoot OK: %s", bn, primary_root)
        except Exception as e:
            logger.error("Error comparing block %d stateRoot: %s", bn, e)
            mismatches.append(bn)
    return mismatches


def compare_block_hashes(
    w3_primary: Web3, w3_sync: Web3, up_to_block: int,
) -> list[int]:
    """Compare block hashes from block 0 to ``up_to_block``.

    The block hash embeds the stateRoot plus receipts/transactions roots, so a
    hash match is a strictly stronger guarantee than a stateRoot match.
    Returns the list of mismatched block numbers (empty means all match).
    """
    mismatches = []
    for bn in range(0, up_to_block + 1):
        try:
            primary_hash = w3_primary.eth.get_block(bn)["hash"].hex()
            sync_hash = w3_sync.eth.get_block(bn)["hash"].hex()
            if primary_hash != sync_hash:
                logger.error(
                    "HASH MISMATCH block %d: primary=%s sync=%s",
                    bn, primary_hash, sync_hash,
                )
                mismatches.append(bn)
            else:
                logger.debug("Block %d hash OK: %s", bn, primary_hash)
        except Exception as e:
            logger.error("Error comparing block %d hash: %s", bn, e)
            mismatches.append(bn)
    return mismatches
