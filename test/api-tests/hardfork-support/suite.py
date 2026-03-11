"""
Hardfork-support test suite.

Provides deploy() and run_tests() entry points called by the top-level run.py.
"""

import logging
import os
import shlex
import subprocess
import importlib.util
from pathlib import Path

from result import TestResult

logger = logging.getLogger("hardfork-support.suite")

SUITE_DIR = Path(__file__).resolve().parent


def _load_run_eip_tests():
    eip_tests_path = SUITE_DIR / "subroutine" / "eip_tests.py"
    spec = importlib.util.spec_from_file_location("hardfork_support_eip_tests", eip_tests_path)
    if spec is None or spec.loader is None:
        raise RuntimeError(f"Failed to load {eip_tests_path}")
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod.run_eip_tests


run_eip_tests = _load_run_eip_tests()


def deploy(cfg: dict, env):
    """Step 2: deploy EIP test contracts via hardhat to each RPC endpoint."""
    from eth_account import Account
    from web3 import Web3

    deploy_cfg = cfg.get("deploy", {})
    sol_dir = SUITE_DIR / "sol"
    command = deploy_cfg.get(
        "command",
        "bun hardhat run scripts/deploy_eip_tests.ts --network custom",
    )
    command_parts = shlex.split(command)
    if not command_parts:
        raise RuntimeError("Deploy command is empty")
    private_key = cfg["type"].get("private_key", "")

    if private_key:
        deployer = Account.from_key(private_key)
        logger.info("Deploy account: %s (from run.toml private_key)", deployer.address)
    else:
        logger.warning("No private_key in run.toml — deploy may fail")

    for label, url in env.rpc_urls.items():
        logger.info("Deploying to %s (%s)", label, url)

        if private_key:
            w3 = env.web3s.get(label)
            if w3:
                balance = w3.eth.get_balance(deployer.address)
                logger.info("  Deployer %s balance on %s: %s ETH",
                            deployer.address, label,
                            Web3.from_wei(balance, "ether"))

        deploy_env = os.environ.copy()
        deploy_env["ENDPOINT"] = url
        if private_key:
            deploy_env["PRIVATE_KEY"] = private_key

        try:
            subprocess.run(
                command_parts,
                shell=False,
                cwd=sol_dir,
                env=deploy_env,
                check=True,
            )
        except OSError as e:
            raise RuntimeError(f"Deploy to {label} FAILED ({e})") from e
        except subprocess.CalledProcessError as e:
            raise RuntimeError(f"Deploy to {label} FAILED (rc={e.returncode})") from e

        logger.info("Deploy to %s OK.", label)


def run_tests(cfg: dict, env) -> dict[str, list[TestResult]]:
    """Step 3: run EIP compliance tests against each RPC endpoint."""
    eip_cfg = cfg.get("eip_tests", {})
    iterations = eip_cfg.get("iterations", 1)
    eips_raw = eip_cfg.get("eips", []) or None
    gas_limit = eip_cfg.get("gas_limit", 3_000_000)
    private_key = cfg["type"].get("private_key", "")
    sol_dir = str(SUITE_DIR / "sol")

    if not private_key:
        msg = "private_key required for EIP tests"
        logger.error(msg)
        return {
            label: [TestResult(name="EIP-suite-config", passed=False, message=msg)]
            for label in env.rpc_urls
        }

    from eth_account import Account
    from web3 import Web3
    deployer = Account.from_key(private_key)
    logger.info("Test account: %s (from run.toml private_key)", deployer.address)

    for label, url in env.rpc_urls.items():
        w3 = env.web3s.get(label)
        if w3:
            balance = w3.eth.get_balance(deployer.address)
            logger.info("  %s balance on %s: %s ETH",
                        deployer.address, label,
                        Web3.from_wei(balance, "ether"))

    by_label_and_name: dict[str, dict[str, TestResult]] = {}
    endpoint_failed: dict[str, str] = {}
    iteration = 0

    while True:
        iteration += 1
        if iterations > 0 and iteration > iterations:
            break

        logger.info("EIP test iteration %d / %s",
                     iteration, iterations if iterations > 0 else "inf")

        for label, url in env.rpc_urls.items():
            logger.info("--- %s (%s) ---", label, url)
            try:
                eip_results = run_eip_tests(
                    rpc_url=url,
                    deployer_key=private_key,
                    sol_dir=sol_dir,
                    eips=[str(e) for e in eips_raw] if eips_raw else None,
                    gas_limit=gas_limit,
                )
                # Convert EIPTestResult -> TestResult
                for r in eip_results:
                    tr = TestResult(
                        name=f"EIP-{r.eip}",
                        passed=r.passed,
                        message=r.message,
                        details=r.details,
                    )
                    label_results = by_label_and_name.setdefault(label, {})
                    prev = label_results.get(tr.name)
                    if prev is None:
                        label_results[tr.name] = tr
                    elif not prev.passed:
                        # Keep first failure sticky across iterations.
                        continue
                    else:
                        label_results[tr.name] = tr

                    tag = "PASS" if r.passed else "FAIL"
                    logger.info("  EIP-%s: %s — %s", r.eip, tag, r.message)

            except Exception as e:
                logger.error("Error on %s: %s", label, e, exc_info=True)
                endpoint_failed[label] = f"{type(e).__name__}: {e}"

    for label, err in endpoint_failed.items():
        label_results = by_label_and_name.setdefault(label, {})
        if "EIP-suite-execution" not in label_results:
            label_results["EIP-suite-execution"] = TestResult(
                name="EIP-suite-execution",
                passed=False,
                message=f"Execution error: {err}",
            )

    for label in env.rpc_urls:
        if label not in by_label_and_name:
            by_label_and_name[label] = {
                "EIP-suite-execution": TestResult(
                    name="EIP-suite-execution",
                    passed=False,
                    message="No test results produced for endpoint",
                )
            }

    return {
        label: sorted(results.values(), key=lambda r: r.name)
        for label, results in by_label_and_name.items()
    }
