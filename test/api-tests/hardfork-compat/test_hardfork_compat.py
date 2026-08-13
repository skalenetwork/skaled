"""
hardfork-compat: fork-boundary RPC and Paris replay compatibility.

Test flow:
  1. Exercise staggered EIP-1559/London block RPC output on a temporary current node.
  2. Start the primary node with the London-capable binary.
  3. Produce a pre-upgrade London workload:
       - native token transfers
       - basic ERC20 deploy/mint/transfer
       - a DIFFICULTY/PREVRANDAO opcode transaction
  4. Restart the same primary node/datadir on the current binary and inject a
     future ParisForkPatch timestamp.
  5. Produce the same essential workload before Paris activation.
  6. Wait until the Paris timestamp is active and produce the workload again.
  7. Launch a current-version sync node with archiveMode=true and verify that
     replaying the whole chain has no per-block stateRoot or block-hash
     mismatches.
"""

import json
import logging
from pathlib import Path

import pytest
from eth_account import Account
from web3 import Web3

from _basefee_compat import assert_basefee_rpc_compatibility
from _test_utils import (
    compare_block_hashes,
    compare_state_roots,
    wait_for_block_timestamp,
    wait_for_new_block,
    wait_for_sync_catchup,
    wait_for_tx,
)

logger = logging.getLogger("hardfork-compat.test")

SUITE_DIR = Path(__file__).resolve().parent
ERC20_BYTECODE_PATH = (
    SUITE_DIR.parent.parent / "unittests/libweb3jsonrpc/contracts/ERC20_bytecode.txt"
)

# ERC20 from test/unittests/libweb3jsonrpc/contracts/ERC20.sol.  It exposes
# mint(address,uint256), transfer(address,uint256), and balanceOf(address).
_ERC20_BYTECODE = "0x" + ERC20_BYTECODE_PATH.read_text().strip()

# Constructor: DIFFICULTY/PREVRANDAO, PUSH1 0, SSTORE, then empty runtime.
# This exercises EIP-4399 in a state-changing transaction post-Paris.
_PREVRANDAO_RECORDER_BYTECODE = "0x4460005560006000f3"

_ERC20_MINT_SELECTOR = "40c10f19"
_ERC20_TRANSFER_SELECTOR = "a9059cbb"
_ERC20_BALANCE_OF_SELECTOR = "70a08231"


# ---------------------------------------------------------------------------
# Transaction helpers
# ---------------------------------------------------------------------------

def _raw_signed_transaction(signed) -> bytes:
    if hasattr(signed, "raw_transaction"):
        return signed.raw_transaction
    return signed.rawTransaction


def _legacy_gas_price(w3: Web3) -> int:
    return max(int(w3.eth.gas_price), 1)


def _type2_fee_cap(w3: Web3) -> int:
    gas_price = _legacy_gas_price(w3)
    latest = w3.eth.get_block("latest")
    base_fee = int(latest.get("baseFeePerGas") or gas_price)
    return max(gas_price * 2, base_fee + gas_price)


def _send_raw_tx(w3: Web3, private_key: str, tx: dict, timeout_s: int, label: str):
    account = Account.from_key(private_key)
    tx = dict(tx)
    tx.setdefault("chainId", w3.eth.chain_id)
    tx.setdefault("nonce", w3.eth.get_transaction_count(account.address))

    signed = Account.sign_transaction(tx, private_key)
    tx_hash = w3.eth.send_raw_transaction(_raw_signed_transaction(signed))
    logger.info("Sent %s tx: %s", label, tx_hash.hex())
    receipt = wait_for_tx(w3, tx_hash, timeout_s)
    assert receipt is not None, f"{label} tx not mined within {timeout_s}s"
    logger.info(
        "%s tx mined: block=%d status=%s gasUsed=%d",
        label, receipt["blockNumber"], receipt.get("status"), receipt["gasUsed"],
    )
    return receipt


def _send_legacy_transfer(
    w3: Web3, private_key: str, recipient: str, value: int, timeout_s: int,
):
    return _send_raw_tx(
        w3,
        private_key,
        {
            "to": recipient,
            "value": value,
            "gas": 21000,
            "gasPrice": _legacy_gas_price(w3),
        },
        timeout_s,
        "legacy native transfer",
    )


def _send_type2_transfer(
    w3: Web3, private_key: str, recipient: str, value: int, timeout_s: int,
):
    return _send_raw_tx(
        w3,
        private_key,
        {
            "type": 2,
            "to": recipient,
            "value": value,
            "gas": 30000,
            "maxFeePerGas": _type2_fee_cap(w3),
            "maxPriorityFeePerGas": 0,
        },
        timeout_s,
        "type2 native transfer",
    )


def _deploy(w3: Web3, private_key: str, bytecode: str, gas: int, timeout_s: int, label: str):
    return _send_raw_tx(
        w3,
        private_key,
        {
            "gas": gas,
            "gasPrice": _legacy_gas_price(w3),
            "data": bytecode,
        },
        timeout_s,
        label,
    )


def _address_word(address: str) -> str:
    raw = address[2:] if address.startswith("0x") else address
    assert len(raw) == 40, f"Bad address length for {address}"
    return raw.lower().rjust(64, "0")


def _uint_word(value: int) -> str:
    return int(value).to_bytes(32, "big").hex()


def _erc20_call_data(selector: str, address: str, amount: int | None = None) -> str:
    data = selector + _address_word(address)
    if amount is not None:
        data += _uint_word(amount)
    return "0x" + data


def _send_contract_call(
    w3: Web3, private_key: str, to: str, data: str, gas: int, timeout_s: int, label: str,
):
    return _send_raw_tx(
        w3,
        private_key,
        {
            "to": to,
            "gas": gas,
            "gasPrice": _legacy_gas_price(w3),
            "data": data,
        },
        timeout_s,
        label,
    )


def _erc20_balance_of(w3: Web3, token: str, address: str) -> int:
    result = w3.eth.call({"to": token, "data": _erc20_call_data(_ERC20_BALANCE_OF_SELECTOR, address)})
    return int.from_bytes(bytes(result), "big")


def _erc20_mint(w3: Web3, private_key: str, token: str, to: str, amount: int, timeout_s: int):
    receipt = _send_contract_call(
        w3,
        private_key,
        token,
        _erc20_call_data(_ERC20_MINT_SELECTOR, to, amount),
        120_000,
        timeout_s,
        "ERC20 mint",
    )
    assert receipt["status"] == 1, "ERC20 mint reverted"
    return receipt


def _erc20_transfer(
    w3: Web3, private_key: str, token: str, recipient: str, amount: int, timeout_s: int,
):
    sender = Account.from_key(private_key).address
    balance = _erc20_balance_of(w3, token, sender)
    assert balance >= amount, f"ERC20 sender balance too low: {balance} < {amount}"

    receipt = _send_contract_call(
        w3,
        private_key,
        token,
        _erc20_call_data(_ERC20_TRANSFER_SELECTOR, recipient, amount),
        120_000,
        timeout_s,
        "ERC20 transfer",
    )
    assert receipt["status"] == 1, "ERC20 transfer reverted"
    assert _erc20_balance_of(w3, token, recipient) >= amount, "ERC20 recipient balance not updated"
    return receipt


def _deploy_erc20(w3: Web3, private_key: str, timeout_s: int) -> str:
    receipt = _deploy(w3, private_key, _ERC20_BYTECODE, 2_500_000, timeout_s, "ERC20 deploy")
    assert receipt["status"] == 1, "ERC20 deploy reverted"
    token = receipt["contractAddress"]
    assert token, "ERC20 deploy did not return a contract address"
    logger.info("ERC20 deployed at %s", token)
    return token


def _deploy_prevrandao_recorder(w3: Web3, private_key: str, timeout_s: int):
    receipt = _deploy(
        w3,
        private_key,
        _PREVRANDAO_RECORDER_BYTECODE,
        100_000,
        timeout_s,
        "PREVRANDAO recorder deploy",
    )
    assert receipt["status"] == 1, "PREVRANDAO recorder deploy reverted"
    return receipt


def _read_schain_value(config_path: Path, key: str):
    with open(config_path) as f:
        cfg = json.load(f)
    return cfg["skaleConfig"]["sChain"].get(key)


def _configure_single_node_ports(config_path: Path, http_port: int) -> None:
    """Keep the compatibility node's consensus and RPC sockets isolated."""
    with open(config_path) as f:
        cfg = json.load(f)

    ports = {
        "basePort": http_port - 3,
        "httpRpcPort": http_port,
        "httpsRpcPort": http_port + 5,
        "wsRpcPort": http_port - 1,
        "wssRpcPort": http_port + 4,
    }
    node_info = cfg["skaleConfig"]["nodeInfo"]
    node_info.update(ports)
    node_info["infoHttpRpcPort"] = http_port + 6
    for node in cfg["skaleConfig"]["sChain"]["nodes"]:
        node.update(ports)

    with open(config_path, "w") as f:
        json.dump(cfg, f, indent=2)


def _assert_receipts_before_timestamp(w3: Web3, receipts: list, timestamp: int, label: str) -> None:
    for receipt in receipts:
        block = w3.eth.get_block(receipt["blockNumber"])
        assert int(block["timestamp"]) < timestamp, (
            f"{label} receipt landed after Paris activation: "
            f"block={block['number']} timestamp={block['timestamp']} activation={timestamp}"
        )


def _assert_receipts_after_paris(w3: Web3, receipts: list, timestamp: int, label: str) -> None:
    mix_hash_by_block = {}
    for receipt in receipts:
        block = w3.eth.get_block(receipt["blockNumber"])
        assert int(block["timestamp"]) >= timestamp, (
            f"{label} receipt landed before Paris activation: "
            f"block={block['number']} timestamp={block['timestamp']} activation={timestamp}"
        )
        assert int(block["difficulty"]) == 0, (
            f"Post-Paris block {block['number']} has non-zero difficulty {block['difficulty']}"
        )
        # Post-Paris skaled headers carry prevRandao (in the mixHash position):
        # a RANDAO-style accumulator of the parent's value XOR the previous block's
        # BLS threshold signature hash — non-zero and distinct per block for every
        # block after block 1.
        mix_hash = int.from_bytes(bytes(block["mixHash"]), "big")
        assert mix_hash != 0, (
            f"Post-Paris block {block['number']} has zero prevRandao/mixHash"
        )
        mix_hash_by_block[int(block["number"])] = mix_hash
    distinct_values = set(mix_hash_by_block.values())
    assert len(distinct_values) == len(mix_hash_by_block), (
        f"{label}: prevRandao values repeat across post-Paris blocks: {mix_hash_by_block}"
    )


def _assert_prevrandao_recorder(
    w3: Web3, recorder_receipt, paris_active: bool, label: str
) -> None:
    """Check the value the PREVRANDAO recorder captured into slot 0 at deploy time.

    Pre-Paris, opcode 0x44 is DIFFICULTY and must equal the deploy block's difficulty.
    Post-Paris on skaled it is the beacon-derived prevRandao, which must be non-zero
    and equal to the deploy block's header mixHash (the EIP-4399 invariant).
    """
    addr = recorder_receipt["contractAddress"]
    assert addr, f"{label}: PREVRANDAO recorder receipt has no contract address"
    recorded = int.from_bytes(bytes(w3.eth.get_storage_at(addr, 0)), "big")
    block = w3.eth.get_block(recorder_receipt["blockNumber"])
    if paris_active:
        mix_hash = int.from_bytes(bytes(block["mixHash"]), "big")
        assert recorded != 0, f"{label}: post-Paris PREVRANDAO recorded as zero"
        assert recorded == mix_hash, (
            f"{label}: recorded PREVRANDAO {hex(recorded)} != header mixHash "
            f"{hex(mix_hash)} at block {block['number']}"
        )
    else:
        assert recorded == int(block["difficulty"]), (
            f"{label}: recorded DIFFICULTY {recorded} != block difficulty "
            f"{block['difficulty']} at block {block['number']}"
        )


def _run_paris_workload_phase(
    w3: Web3,
    private_key: str,
    timeouts: dict,
    workload_state: dict,
    phase: str,
    *,
    deploy_token: bool = False,
) -> list:
    """Run the minimal Paris-compat workload and return all receipts."""
    timeout = timeouts.get("tx_mine", 120)
    account = Account.from_key(private_key)
    receipts = []

    logger.info("=== Paris hardfork-compat workload phase: %s ===", phase)

    receipts.append(
        _send_legacy_transfer(
            w3, private_key, Account.create().address, 100 + len(phase), timeout,
        )
    )
    assert receipts[-1]["status"] == 1, f"{phase}: legacy native transfer reverted"

    receipts.append(
        _send_type2_transfer(
            w3, private_key, Account.create().address, 200 + len(phase), timeout,
        )
    )
    assert receipts[-1]["status"] == 1, f"{phase}: type2 native transfer reverted"

    if deploy_token or "erc20" not in workload_state:
        token = _deploy_erc20(w3, private_key, timeout)
        workload_state["erc20"] = token
        receipts.append(_erc20_mint(w3, private_key, token, account.address, 1_000_000, timeout))
    token = workload_state["erc20"]
    receipts.append(
        _erc20_transfer(
            w3, private_key, token, Account.create().address, 10 + len(phase), timeout,
        )
    )

    receipts.append(_deploy_prevrandao_recorder(w3, private_key, timeout))

    logger.info("Completed workload phase %s with %d transactions", phase, len(receipts))
    return receipts


# ---------------------------------------------------------------------------
# Tests (ordered by file position)
# ---------------------------------------------------------------------------

def test_current_binary_basefee_rpc_compatibility(
    current_binary: Path, run_cfg: dict, hardfork_cfg: dict, private_key: str,
):
    """Check block RPC output before EIP-1559, before London, and after London."""
    from conftest import _launch_node, _resolve, _resolve_ft, _stop_node, _wait_for_rpc
    from run import (
        configure_single_node_skaled,
        ensure_genesis_balance,
        inject_patches,
        render_template_file,
        set_ulimit,
    )

    if not current_binary.is_file():
        pytest.fail(
            f"Current skaled binary not found: {current_binary}\n"
            "Set HARDFORK_COMPAT_CURRENT_BINARY or [hardfork_compat].current_binary."
        )

    basefee_cfg = hardfork_cfg.get("basefee_compat", {})
    http_port = int(basefee_cfg.get("http_port", 6234))
    cfg_out = _resolve(
        "test/api-tests/hardfork-compat/configs/config-basefee.generated.json"
    )
    datadir = _resolve("test/api-tests/hardfork-compat/datadir-basefee")
    template = _resolve_ft("hardfork-compat/config-templates/config-template.json.j2")
    template_context = run_cfg.get("skaled", {}).get("template", {}).get("context", {})

    render_template_file(str(template), str(cfg_out), template_context)
    ensure_genesis_balance(str(cfg_out), private_key)
    configure_single_node_skaled(str(cfg_out))
    _configure_single_node_ports(cfg_out, http_port)
    inject_patches(str(cfg_out), basefee_cfg.get("patches", {}))

    eip1559 = _read_schain_value(cfg_out, "EIP1559TransactionsPatchTimestamp")
    london = _read_schain_value(cfg_out, "londonForkPatchTimestamp")
    assert isinstance(eip1559, int) and isinstance(london, int), (
        "baseFeePerGas compatibility patch timestamps must resolve to integers"
    )

    set_ulimit()
    log_dir = SUITE_DIR / "logs"
    log_dir.mkdir(parents=True, exist_ok=True)
    log_path = log_dir / "skaled-basefee-compat.log"
    proc, log_fd = _launch_node(
        current_binary,
        cfg_out,
        http_port,
        datadir,
        log_path,
        "BASEFEE(current)",
    )
    w3 = Web3(Web3.HTTPProvider(
        f"http://127.0.0.1:{http_port}", request_kwargs={"timeout": 10}
    ))

    try:
        rpc_timeout = hardfork_cfg.get("timeouts", {}).get("rpc_up", 360)
        _wait_for_rpc(w3, rpc_timeout, "BASEFEE(current)")
        details = assert_basefee_rpc_compatibility(
            w3,
            eip1559,
            london,
            timeout_sec=float(basefee_cfg.get("transition_timeout_sec", 120)),
            poll_interval_sec=float(basefee_cfg.get("poll_interval_sec", 0.5)),
        )
        logger.info("baseFeePerGas RPC compatibility checks passed: %s", details)
    except Exception as exc:
        pytest.fail(f"{exc}\n--- {log_path} tail ---\n{log_path.read_text(errors='replace')[-8000:]}")
    finally:
        _stop_node(proc, log_fd, "BASEFEE(current)")


def test_london_primary_rpc_up(w3_primary: Web3):
    """Verify the London primary node RPC is reachable."""
    bn = w3_primary.eth.block_number
    assert bn >= 0, "London primary RPC is not responding"
    logger.info("London primary RPC is up at block %d", bn)


def test_london_primary_block_production(w3_primary: Web3, timeouts: dict):
    """Verify the London primary node is producing blocks."""
    start = w3_primary.eth.block_number
    timeout = timeouts.get("block_produce", 60)
    new_bn = wait_for_new_block(w3_primary, start, timeout)
    assert new_bn is not None, (
        f"No new block within {timeout}s (stuck at block {start})"
    )
    logger.info("Block %d produced (was %d)", new_bn, start)


def test_london_pre_upgrade_workload(
    w3_primary: Web3, private_key: str, timeouts: dict, workload_state: dict,
):
    """Run native/ERC20/PREVRANDAO workload before the binary upgrade."""
    receipts = _run_paris_workload_phase(
        w3_primary,
        private_key,
        timeouts,
        workload_state,
        "london-pre-upgrade",
        deploy_token=True,
    )
    assert receipts, "No pre-upgrade receipts produced"
    _assert_prevrandao_recorder(
        w3_primary, receipts[-1], paris_active=False, label="london-pre-upgrade"
    )


def test_upgrade_primary_to_current_with_paris_timestamp(
    primary_node, current_binary: Path, hardfork_cfg: dict, timeouts: dict, workload_state: dict,
):
    """Restart primary on current binary with a future ParisForkPatch timestamp."""
    if not current_binary.is_file():
        pytest.fail(
            f"Current skaled binary not found: {current_binary}\n"
            "Set HARDFORK_COMPAT_CURRENT_BINARY or [hardfork_compat].current_binary."
        )

    before_bn = primary_node.w3.eth.block_number
    logger.info("Primary at block %d before current-binary upgrade", before_bn)

    primary_node.upgrade_to_current(
        binary=current_binary,
        patches=hardfork_cfg.get("patches", {}),
        timeout_s=timeouts.get("rpc_up", 360),
    )

    activation = _read_schain_value(primary_node.cfg_path, "parisForkPatchTimestamp")
    assert isinstance(activation, int) and activation > 0, (
        "[hardfork_compat.patches].parisForkPatchTimestamp must resolve to a positive integer"
    )
    workload_state["paris_activation_timestamp"] = activation

    latest = primary_node.w3.eth.get_block("latest")
    assert int(latest["timestamp"]) < activation, (
        "Paris activation timestamp must be in the future at upgrade time so the suite can "
        f"run a pre-activation workload (latest={latest['timestamp']}, activation={activation})"
    )
    logger.info(
        "Primary upgraded to current binary at block=%d timestamp=%d; Paris activates at %d",
        latest["number"], latest["timestamp"], activation,
    )


def test_current_pre_paris_workload(
    w3_primary: Web3, private_key: str, timeouts: dict, workload_state: dict,
):
    """Run another workload on current binary before Paris activation."""
    activation = workload_state["paris_activation_timestamp"]
    latest = w3_primary.eth.get_block("latest")
    assert int(latest["timestamp"]) < activation, (
        "Paris activated before the current-version pre-activation workload could start"
    )

    receipts = _run_paris_workload_phase(
        w3_primary, private_key, timeouts, workload_state, "current-pre-paris",
    )
    _assert_receipts_before_timestamp(w3_primary, receipts, activation, "current-pre-paris")
    _assert_prevrandao_recorder(
        w3_primary, receipts[-1], paris_active=False, label="current-pre-paris"
    )


def test_wait_for_paris_activation(w3_primary: Web3, timeouts: dict, workload_state: dict):
    """Wait until the chain timestamp reaches ParisForkPatch activation."""
    activation = workload_state["paris_activation_timestamp"]
    timeout = timeouts.get("paris_activation", 180)
    activation_block = wait_for_block_timestamp(w3_primary, activation, timeout)
    assert activation_block is not None, (
        f"No block reached Paris activation timestamp {activation} within {timeout}s"
    )
    workload_state["paris_activation_block"] = activation_block
    logger.info("Paris active at block %d (timestamp >= %d)", activation_block, activation)


def test_current_post_paris_workload(
    w3_primary: Web3, private_key: str, timeouts: dict, workload_state: dict,
):
    """Run the workload after Paris activation; block difficulty must be zero."""
    activation = workload_state["paris_activation_timestamp"]
    receipts = _run_paris_workload_phase(
        w3_primary, private_key, timeouts, workload_state, "current-post-paris",
    )
    _assert_receipts_after_paris(w3_primary, receipts, activation, "current-post-paris")
    _assert_prevrandao_recorder(
        w3_primary, receipts[-1], paris_active=True, label="current-post-paris"
    )


def test_sync_catchup_and_state_root_comparison(
    w3_primary: Web3, primary_cfg_path, sync_binary: Path,
    hardfork_cfg: dict, timeouts: dict,
):
    """Launch current archive sync node, catch up, compare state roots and hashes."""
    from conftest import (
        _launch_node,
        _resolve,
        _stop_node,
        _wait_for_rpc,
        make_sync_config,
        SUITE_DIR,
    )
    from run import set_ulimit

    if not sync_binary.is_file():
        pytest.fail(
            f"Current sync skaled binary not found: {sync_binary}\n"
            "Set HARDFORK_COMPAT_CURRENT_BINARY or [hardfork_compat].current_binary."
        )

    sync_http_port = int(hardfork_cfg.get("sync_http_port", 5344))
    sync_datadir = _resolve("test/api-tests/hardfork-compat/datadir-sync")
    sync_cfg_out = _resolve(
        "test/api-tests/hardfork-compat/configs/config-sync.generated.json"
    )

    primary_bn = w3_primary.eth.block_number
    logger.info("Primary at block %d before archive sync launch", primary_bn)

    make_sync_config(Path(str(primary_cfg_path)), sync_cfg_out, sync_http_port)

    set_ulimit()
    log_dir = SUITE_DIR / "logs"
    log_dir.mkdir(parents=True, exist_ok=True)

    proc, log_fd = _launch_node(
        sync_binary, sync_cfg_out, sync_http_port, sync_datadir,
        log_dir / "skaled-sync-current.log", "SYNC(current archive)",
    )

    w3_sync = Web3(Web3.HTTPProvider(
        f"http://127.0.0.1:{sync_http_port}", request_kwargs={"timeout": 10}
    ))

    try:
        rpc_timeout = timeouts.get("rpc_up", 360)
        _wait_for_rpc(w3_sync, rpc_timeout, "SYNC(current archive)")

        sync_timeout = timeouts.get("sync_catchup", 300)
        caught_up = wait_for_sync_catchup(w3_primary, w3_sync, sync_timeout)
        assert caught_up, (
            f"Current archive sync node did not catch up within {sync_timeout}s "
            f"(primary={w3_primary.eth.block_number}, sync={w3_sync.eth.block_number})"
        )

        compare_bn = min(w3_primary.eth.block_number, w3_sync.eth.block_number)
        logger.info("Comparing stateRoot and block hash for blocks 0..%d", compare_bn)

        # Run both comparisons before asserting so a failure reports the full
        # picture (stateRoot and hash mismatches) in one go.
        root_mismatches = compare_state_roots(w3_primary, w3_sync, compare_bn)
        hash_mismatches = compare_block_hashes(w3_primary, w3_sync, compare_bn)

        if root_mismatches:
            logger.error("stateRoot mismatches at blocks: %s", root_mismatches)
        else:
            logger.info("All %d block stateRoots match", compare_bn + 1)

        if hash_mismatches:
            logger.error("Block hash mismatches at blocks: %s", hash_mismatches)
        else:
            logger.info("All %d block hashes match", compare_bn + 1)

        assert not root_mismatches and not hash_mismatches, (
            f"Paris hardfork replay divergence: stateRoot mismatches at blocks "
            f"{root_mismatches}, block hash mismatches at blocks {hash_mismatches}"
        )
        logger.info(
            "All %d block stateRoots and hashes match after London->Paris replay",
            compare_bn + 1,
        )
    finally:
        _stop_node(proc, log_fd, "SYNC(current archive)")
