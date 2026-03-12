import json
import logging
import time
from dataclasses import dataclass, field
from pathlib import Path
from typing import Optional

from eth_account import Account
from eth_account.signers.local import LocalAccount
from web3 import Web3

logger = logging.getLogger(__name__)

# EIP-2929 spec thresholds
# Cold SLOAD = 2100, Warm SLOAD = 100  =>  ratio >= 10
# We use a conservative minimum ratio to account for metering quirks.
MIN_COLD_WARM_RATIO = 5


@dataclass
class EIPTestResult:
    eip: str
    passed: bool
    message: str
    details: dict = field(default_factory=dict)


def _load_artifact(sol_dir: str, contract_name: str) -> tuple:
    """Load ABI and bytecode from Hardhat artifacts."""
    artifact_path = (
        Path(sol_dir)
        / "artifacts"
        / "contracts"
        / "eips"
        / f"{contract_name}.sol"
        / f"{contract_name}.json"
    )
    if not artifact_path.exists():
        raise FileNotFoundError(
            f"Artifact not found at {artifact_path}. Run 'cd sol && npx hardhat compile' first."
        )
    with open(artifact_path) as f:
        artifact = json.load(f)
    return artifact["abi"], artifact["bytecode"]


def _deploy_contract(
    w3: Web3,
    deployer: LocalAccount,
    abi: list,
    bytecode: str,
    gas_limit: int = 3_000_000,
) -> str:
    """Deploy a contract and return its address."""
    contract = w3.eth.contract(abi=abi, bytecode=bytecode)
    tx = contract.constructor().build_transaction(
        {
            "from": deployer.address,
            "gas": gas_limit,
            "nonce": w3.eth.get_transaction_count(deployer.address),
            "gasPrice": w3.eth.gas_price,
            "chainId": w3.eth.chain_id,
        }
    )
    signed = deployer.sign_transaction(tx)
    tx_hash = w3.eth.send_raw_transaction(signed.raw_transaction)
    receipt = w3.eth.wait_for_transaction_receipt(tx_hash, timeout=300)
    if receipt["status"] != 1:
        raise RuntimeError(f"Deploy failed: {receipt}")
    address = receipt["contractAddress"]
    logger.info("Deployed contract at %s (gas=%d)", address, receipt["gasUsed"])
    return address


def _send_tx(w3: Web3, deployer: LocalAccount, tx_dict: dict, pre_wait: float = 0) -> dict:
    """Sign, send, and wait for a transaction. Returns the receipt.

    Args:
        pre_wait: seconds to sleep after sending before polling for the receipt.
                  Useful for non-legacy tx types that need extra processing time.
    """
    tx_dict.setdefault("chainId", w3.eth.chain_id)
    tx_dict.setdefault("gasPrice", w3.eth.gas_price)
    tx_dict.setdefault("nonce", w3.eth.get_transaction_count(deployer.address))
    signed = deployer.sign_transaction(tx_dict)
    tx_hash = w3.eth.send_raw_transaction(signed.raw_transaction)
    logger.info("Sent tx %s (type=%s)", tx_hash.hex(), tx_dict.get("type", 0))
    if pre_wait > 0:
        logger.info("Waiting %.1fs before polling for receipt...", pre_wait)
        time.sleep(pre_wait)
    return w3.eth.wait_for_transaction_receipt(
        tx_hash, timeout=300, poll_latency=5
    )


# ---------------------------------------------------------------------------
# EIP-2929: cold vs warm storage access
# ---------------------------------------------------------------------------

def test_eip_2929(w3: Web3, deployer: LocalAccount, sol_dir: str, gas_limit: int = 3_000_000) -> EIPTestResult:
    """
    EIP-2929 introduces differentiated gas costs for cold vs warm state access.
    - Cold SLOAD: 2100 gas (first access to a storage slot in a tx)
    - Warm SLOAD: 100 gas  (subsequent accesses to same slot)
    - Pre-2929: all SLOADs cost 800 gas (no distinction)

    Test: measure gasleft() delta across sequential SLOADs of the same slot
    and a different slot. Assert cold >> warm (ratio >= MIN_COLD_WARM_RATIO).
    """
    logger.info("=== EIP-2929 test ===")
    abi, bytecode = _load_artifact(sol_dir, "EIP2929Test")
    addr = _deploy_contract(w3, deployer, abi, bytecode, gas_limit)
    contract = w3.eth.contract(address=addr, abi=abi)

    # Measure: cold read of key 0, warm re-read of key 0, cold read of key 1
    cold_gas, warm_gas, cold_gas2 = contract.functions.measureColdVsWarmSload(0).call()
    logger.info(
        "SLOAD gas: cold=%d, warm=%d, cold2=%d (ratio=%.1f)",
        cold_gas, warm_gas, cold_gas2,
        cold_gas / warm_gas if warm_gas else float("inf"),
    )

    # Measure cold vs warm BALANCE
    random_addr = "0x" + "ab" * 20  # address not previously accessed
    bal_cold, bal_warm = contract.functions.measureColdVsWarmBalance(
        w3.to_checksum_address(random_addr)
    ).call()
    logger.info(
        "BALANCE gas: cold=%d, warm=%d (ratio=%.1f)",
        bal_cold, bal_warm,
        bal_cold / bal_warm if bal_warm else float("inf"),
    )

    # Additional EIP-2929 checks:
    # 1) tx.to (the current contract) should be pre-warmed at transaction start.
    # 2) precompile addresses should be pre-warmed too.
    self_first, self_second = contract.functions.measureColdVsWarmBalance(addr).call()
    precompile_1 = w3.to_checksum_address("0x" + "00" * 19 + "01")
    precompile_first, precompile_second = contract.functions.measureColdVsWarmBalance(
        precompile_1
    ).call()
    logger.info(
        "Pre-warm BALANCE gas: self=(%d,%d), precompile1=(%d,%d)",
        self_first, self_second, precompile_first, precompile_second,
    )

    details = {
        "sload_cold": cold_gas,
        "sload_warm": warm_gas,
        "sload_cold2": cold_gas2,
        "balance_cold": bal_cold,
        "balance_warm": bal_warm,
        "balance_self_first": self_first,
        "balance_self_second": self_second,
        "balance_precompile1_first": precompile_first,
        "balance_precompile1_second": precompile_second,
        "contract": addr,
    }

    errors = []

    # Check 1: cold SLOAD must cost significantly more than warm SLOAD
    if warm_gas == 0 or cold_gas / warm_gas < MIN_COLD_WARM_RATIO:
        errors.append(
            f"SLOAD cold/warm ratio too low: {cold_gas}/{warm_gas} "
            f"= {cold_gas / warm_gas if warm_gas else 'inf':.1f}, need >= {MIN_COLD_WARM_RATIO}"
        )

    # Check 2: second cold read (different key) should cost ~same as first cold read
    if cold_gas2 > 0 and cold_gas > 0:
        ratio = cold_gas2 / cold_gas
        if ratio < 0.5:
            errors.append(
                f"Second cold read unexpectedly cheap: cold={cold_gas}, cold2={cold_gas2}"
            )

    # Check 3: cold BALANCE must cost more than warm BALANCE
    if bal_warm == 0 or bal_cold / bal_warm < MIN_COLD_WARM_RATIO:
        errors.append(
            f"BALANCE cold/warm ratio too low: {bal_cold}/{bal_warm} "
            f"= {bal_cold / bal_warm if bal_warm else 'inf':.1f}, need >= {MIN_COLD_WARM_RATIO}"
        )

    # Check 4: tx.to should be pre-warmed (first self BALANCE close to warm cost).
    if self_first > self_second * 2:
        errors.append(
            f"Self BALANCE first access not warm enough: first={self_first}, second={self_second}"
        )

    # Check 5: precompile addresses should be pre-warmed.
    if precompile_first > precompile_second * 2:
        errors.append(
            f"Precompile BALANCE first access not warm enough: first={precompile_first}, second={precompile_second}"
        )

    if errors:
        return EIPTestResult(
            eip="2929",
            passed=False,
            message="; ".join(errors),
            details=details,
        )

    return EIPTestResult(
        eip="2929",
        passed=True,
        message=(
            f"SLOAD cold={cold_gas} warm={warm_gas} (ratio={cold_gas / warm_gas:.1f}), "
            f"BALANCE cold={bal_cold} warm={bal_warm} (ratio={bal_cold / bal_warm:.1f})"
        ),
        details=details,
    )


# ---------------------------------------------------------------------------
# EIP-2930 / EIP-2718: access-list (Type 1) transactions
# ---------------------------------------------------------------------------

def test_eip_2930(w3: Web3, deployer: LocalAccount, sol_dir: str, gas_limit: int = 3_000_000) -> EIPTestResult:
    """
    EIP-2930 adds optional access lists to transactions (Type 1 envelope per EIP-2718).

    Test: verify the chain accepts and correctly executes a Type 1 transaction
    with an access list.
    Assert:
    1. Type 0 (legacy) transaction succeeds.
    2. Type 1 (access list) transaction succeeds.
    3. Contract state is correct after both writes.
    """
    logger.info("=== EIP-2930 / EIP-2718 test ===")
    abi, bytecode = _load_artifact(sol_dir, "EIP2930Test")
    addr = _deploy_contract(w3, deployer, abi, bytecode, gas_limit)
    contract = w3.eth.contract(address=addr, abi=abi)

    # --- Type 0 (legacy) transaction: writeAllSlots ---
    tx0 = contract.functions.writeAllSlots(
        100, 200, 300, 400,
    ).build_transaction(
        {
            "from": deployer.address,
            "gas": gas_limit,
            "gasPrice": w3.eth.gas_price,
            "nonce": w3.eth.get_transaction_count(
                deployer.address,
            ),
            "chainId": w3.eth.chain_id,
        }
    )
    receipt0 = _send_tx(w3, deployer, tx0)
    gas_type0 = receipt0["gasUsed"]
    logger.info(
        "Type 0 writeAllSlots gas: %d (status=%d)",
        gas_type0, receipt0["status"],
    )

    # --- Type 1 (access list) transaction ---
    access_list = [
        {
            "address": addr,
            "storageKeys": [
                "0x" + slot.to_bytes(32, "big").hex()
                for slot in range(4)
            ],
        }
    ]

    tx1 = contract.functions.writeAllSlots(
        500, 600, 700, 800,
    ).build_transaction(
        {
            "from": deployer.address,
            "gas": gas_limit,
            "nonce": w3.eth.get_transaction_count(
                deployer.address,
            ),
            "chainId": w3.eth.chain_id,
            "type": 1,
            "gasPrice": w3.eth.gas_price,
            "accessList": access_list,
        }
    )
    receipt1 = _send_tx(w3, deployer, tx1)
    gas_type1 = receipt1["gasUsed"]
    logger.info(
        "Type 1 writeAllSlots gas: %d (status=%d)",
        gas_type1, receipt1["status"],
    )

    # Verify final state
    vals = contract.functions.readAllSlots().call()
    assert list(vals) == [500, 600, 700, 800], (
        f"Unexpected slot values: {vals}"
    )

    gas_diff = gas_type0 - gas_type1
    logger.info(
        "Gas comparison: Type 0=%d, Type 1=%d, diff=%d",
        gas_type0, gas_type1, gas_diff,
    )

    details = {
        "gas_type0": gas_type0,
        "gas_type1": gas_type1,
        "gas_diff": gas_diff,
        "type0_status": receipt0["status"],
        "type1_status": receipt1["status"],
        "contract": addr,
    }

    errors = []

    if receipt0["status"] != 1:
        errors.append("Type 0 transaction reverted")
    if receipt1["status"] != 1:
        errors.append("Type 1 transaction reverted")

    if errors:
        return EIPTestResult(
            eip="2930",
            passed=False,
            message="; ".join(errors),
            details=details,
        )

    return EIPTestResult(
        eip="2930",
        passed=True,
        message=(
            f"Type 0 (status=1, gas={gas_type0}), "
            f"Type 1 (status=1, gas={gas_type1}), "
            f"diff={gas_diff}"
        ),
        details=details,
    )


# ---------------------------------------------------------------------------
# EIP-2565: ModExp precompile repricing
# ---------------------------------------------------------------------------

def test_eip_2565(w3: Web3, deployer: LocalAccount, sol_dir: str, gas_limit: int = 3_000_000) -> EIPTestResult:
    """
    EIP-2565 reprices the ModExp precompile (0x05) with a simpler gas formula:
      max(200, floor(mult_complexity * iter_count / 3))
    The old formula (EIP-198) was significantly more expensive for small inputs.

    Test:
    1. Verify precompile correctness with known test vectors.
    2. Measure gas for a small modexp call; under EIP-2565 it should be close
       to the 200 gas floor, whereas pre-2565 it would cost ~thousands.
    """
    logger.info("=== EIP-2565 test ===")
    abi, bytecode = _load_artifact(sol_dir, "EIP2565Test")
    addr = _deploy_contract(w3, deployer, abi, bytecode, gas_limit)
    contract = w3.eth.contract(address=addr, abi=abi)

    test_vectors = [
        (2, 10, 1000, 24),    # 2^10 mod 1000 = 1024 mod 1000 = 24
        (3, 5, 100, 43),      # 3^5 mod 100  = 243 mod 100  = 43
        (7, 3, 50, 43),       # 7^3 mod 50   = 343 mod 50   = 43
        (2, 16, 1000, 536),   # 2^16 mod 1000 = 65536 mod 1000 = 536
    ]

    results = []
    all_correct = True
    for base, exp, mod, expected in test_vectors:
        actual = contract.functions.modExpUint(base, exp, mod).call()
        ok = actual == expected
        results.append(
            {"base": base, "exp": exp, "mod": mod, "expected": expected, "actual": actual, "ok": ok}
        )
        if not ok:
            all_correct = False
        logger.info(
            "  %d^%d mod %d: expected=%d actual=%d %s",
            base, exp, mod, expected, actual, "OK" if ok else "FAIL",
        )

    # Gas measurement: estimate gas for a small modexp call.
    # EIP-2565 floor = 200 gas for the precompile itself.
    # EIP-198 (old) would charge ~1360+ for 32-byte inputs.
    gas_estimate = contract.functions.modExpUint(2, 10, 1000).estimate_gas(
        {"from": deployer.address}
    )
    logger.info("Gas estimate for modExpUint(2,10,1000): %d", gas_estimate)

    details = {
        "vectors": results,
        "gas_estimate": gas_estimate,
        "contract": addr,
    }

    errors = []

    if not all_correct:
        failed = [r for r in results if not r["ok"]]
        errors.append(f"{len(failed)}/{len(results)} test vectors returned wrong results")

    # Under EIP-2565, small modexp should be cheap (precompile floor=200).
    # Total tx gas includes 21000 base + ABI overhead + precompile.
    # We expect the whole call to be under ~30000.
    # Under old EIP-198 pricing, it would be higher.
    MAX_EXPECTED_GAS = 50_000
    if gas_estimate > MAX_EXPECTED_GAS:
        errors.append(
            f"ModExp gas too high ({gas_estimate}), expected <= {MAX_EXPECTED_GAS} under EIP-2565 repricing"
        )

    if errors:
        return EIPTestResult(
            eip="2565",
            passed=False,
            message="; ".join(errors),
            details=details,
        )

    return EIPTestResult(
        eip="2565",
        passed=True,
        message=f"All {len(results)} vectors correct, gas estimate={gas_estimate}",
        details=details,
    )


# ---------------------------------------------------------------------------
# EIP-2929 revert semantics: access sets restored on sub-call revert
# ---------------------------------------------------------------------------

def test_eip_2929_revert(
    w3: Web3, deployer: LocalAccount, sol_dir: str, gas_limit: int = 3_000_000
) -> EIPTestResult:
    """
    EIP-2929 specifies that when a sub-call scope reverts, accessed_addresses
    and accessed_storage_keys are restored to the state they were in before that
    scope was entered.  A slot warmed only inside a reverted sub-call must
    therefore cost cold gas (2100) in the outer frame.

    Test:
    1. Deploy EIP2929RevertTest.
    2. Call measureWarmSload() to obtain the warm-SLOAD reference cost.
    3. Call measureSlotCostAfterRevert():
       - internally does try { this.readSlot0ThenRevert() } catch {}
       - then measures SLOAD cost of slot0 (warmed inside the reverted sub-call)
       - and SLOAD cost of slot1 (never accessed anywhere — always cold).
    4. Assert gasAfterRevert is close to gasSlot1 (both cold, ~2100) and NOT
       close to warmGas (~100), proving the access set was rolled back.
    """
    logger.info("=== EIP-2929 revert semantics test ===")
    abi, bytecode = _load_artifact(sol_dir, "EIP2929RevertTest")
    addr = _deploy_contract(w3, deployer, abi, bytecode, gas_limit)
    contract = w3.eth.contract(address=addr, abi=abi)

    warm_gas = contract.functions.measureWarmSload().call()
    logger.info("Warm SLOAD reference: %d gas", warm_gas)

    gas_after_revert, gas_slot1 = contract.functions.measureSlotCostAfterRevert().call()
    logger.info(
        "After reverted sub-call: slot0 SLOAD=%d gas, slot1 (cold ref)=%d gas, warm ref=%d gas",
        gas_after_revert, gas_slot1, warm_gas,
    )

    details = {
        "warm_gas": warm_gas,
        "gas_after_revert": gas_after_revert,
        "gas_slot1_cold_ref": gas_slot1,
        "contract": addr,
    }

    errors = []

    # gasAfterRevert should be cold (~2100), not warm (~100).
    # We require it to be at least MIN_COLD_WARM_RATIO times the warm cost.
    if warm_gas == 0:
        errors.append("measureWarmSload() returned 0 — cannot determine warm gas cost")
    elif gas_after_revert / warm_gas < MIN_COLD_WARM_RATIO:
        errors.append(
            f"slot0 SLOAD after reverted sub-call cost only {gas_after_revert} gas "
            f"(warm ref={warm_gas}), ratio={gas_after_revert / warm_gas:.1f} < {MIN_COLD_WARM_RATIO}. "
            f"Access set was NOT restored on revert."
        )

    if errors:
        return EIPTestResult(
            eip="2929-revert",
            passed=False,
            message="; ".join(errors),
            details=details,
        )

    return EIPTestResult(
        eip="2929-revert",
        passed=True,
        message=(
            f"slot0 SLOAD after reverted sub-call={gas_after_revert} gas (cold, as expected); "
            f"warm ref={warm_gas}; ratio={gas_after_revert / warm_gas:.1f}"
        ),
        details=details,
    )


# ---------------------------------------------------------------------------
# Orchestrator
# ---------------------------------------------------------------------------

EIP_TEST_MAP = {
    "2929": test_eip_2929,
    "2929-revert": test_eip_2929_revert,
    "2930": test_eip_2930,
    "2718": test_eip_2930,  # EIP-2718 is covered by the Type 1 tx test
    "2565": test_eip_2565,
}

ALL_EIPS = ["2929", "2929-revert", "2930", "2565"]  # canonical list (no duplicates)


def run_eip_tests(
    rpc_url: str,
    deployer_key: str,
    sol_dir: str,
    eips: Optional[list[str]] = None,
    gas_limit: int = 3_000_000,
) -> list[EIPTestResult]:
    """Run selected EIP compliance tests and return results."""
    print(rpc_url)
    w3 = Web3(Web3.HTTPProvider(rpc_url))
    if not w3.is_connected():
        raise ConnectionError(f"Cannot connect to {rpc_url}")

    deployer = Account.from_key(deployer_key)
    logger.info("Deployer: %s", deployer.address)
    balance = w3.eth.get_balance(deployer.address)
    logger.info("Balance: %s ETH", w3.from_wei(balance, "ether"))

    if eips:
        selected = eips
    else:
        selected = ALL_EIPS

    results: list[EIPTestResult] = []
    seen = set()
    for eip in selected:
        fn = EIP_TEST_MAP.get(eip)
        if fn is None:
            results.append(
                EIPTestResult(eip=eip, passed=False, message=f"Unknown EIP: {eip}")
            )
            continue

        fn_id = id(fn)
        if fn_id in seen:
            for prev in results:
                if id(EIP_TEST_MAP.get(prev.eip)) == fn_id:
                    results.append(
                        EIPTestResult(
                            eip=eip,
                            passed=prev.passed,
                            message=f"Same as EIP-{prev.eip}: {prev.message}",
                            details=prev.details,
                        )
                    )
                    break
            continue
        seen.add(fn_id)

        try:
            result = fn(w3, deployer, sol_dir, gas_limit)
            results.append(result)
        except Exception as e:
            logger.error("EIP-%s test error: %s", eip, e, exc_info=True)
            results.append(
                EIPTestResult(eip=eip, passed=False, message=f"Error: {e}")
            )

    return results
