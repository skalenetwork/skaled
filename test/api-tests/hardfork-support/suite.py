"""
Hardfork-support test suite.

Provides deploy() and run_tests() entry points called by the top-level run.py.
Runs local EIP contract checks and, when configured, selected ethereum/execution-specs
execute-remote workloads against each endpoint.
"""

import json
import logging
import os
import shlex
import subprocess
import importlib.util
from pathlib import Path

from result import TestResult

logger = logging.getLogger("hardfork-support.suite")

SUITE_DIR = Path(__file__).resolve().parent
REPO_ROOT = SUITE_DIR.parent.parent.parent

NONCE_OVERFLOW_CREATE_STUB = "berlin_nonce_overflow_create"
NONCE_OVERFLOW_CREATE2_STUB = "berlin_nonce_overflow_create2"
NONCE_OVERFLOW_TEST = Path("tests/berlin/eip2929_gas_cost_increases/test_create.py")
NONCE_OVERFLOW_PATCH_MARKER = "NONCE_OVERFLOW_CREATE_STUB"

NONCE_OVERFLOW_CONSTANTS_ORIGINAL = '''REFERENCE_SPEC_GIT_PATH = "EIPS/eip-2929.md"
REFERENCE_SPEC_VERSION = "0e11417265a623adb680c527b15d0cb6701b870b"


@pytest.mark.valid_from("Berlin")'''

NONCE_OVERFLOW_CONSTANTS_PATCHED = '''REFERENCE_SPEC_GIT_PATH = "EIPS/eip-2929.md"
REFERENCE_SPEC_VERSION = "0e11417265a623adb680c527b15d0cb6701b870b"

NONCE_OVERFLOW_CREATE_STUB = "berlin_nonce_overflow_create"
NONCE_OVERFLOW_CREATE2_STUB = "berlin_nonce_overflow_create2"


def _has_stub(pre: Alloc, stub: str) -> bool:
    """
    Return True when execute/fill was configured with a matching address stub.

    execute-remote cannot materialize arbitrary pre-state nonce values by
    transaction, so a live-RPC run can preseed the account in genesis and pass
    it as an address stub. Normal fillers keep using explicit alloc pre-state.
    """
    address_stubs = getattr(pre, "_address_stubs", None)
    if address_stubs is not None and stub in address_stubs:
        return True

    stub_accounts = getattr(pre, "_stub_accounts", None)
    return stub_accounts is not None and stub in stub_accounts


@pytest.mark.valid_from("Berlin")'''

NONCE_OVERFLOW_DEPLOY_ORIGINAL = '''    # Nonce at max value (2^64-1) causes CREATE to abort
    creator_address = pre.deploy_contract(
        creator_code, nonce=2**64 - 1, storage={0: 1}
    )
'''

NONCE_OVERFLOW_DEPLOY_PATCHED = '''    # Nonce at max value (2^64-1) causes CREATE to abort. In execute-remote,
    # use a genesis-preseeded stub because the live RPC setup path cannot set
    # arbitrary account nonces by transaction.
    creator_stub = (
        NONCE_OVERFLOW_CREATE2_STUB
        if create_opcode == Op.CREATE2
        else NONCE_OVERFLOW_CREATE_STUB
    )
    deploy_kwargs = {"nonce": 2**64 - 1, "storage": {0: 1}}
    if _has_stub(pre, creator_stub):
        deploy_kwargs["stub"] = creator_stub

    creator_address = pre.deploy_contract(creator_code, **deploy_kwargs)
'''


def _load_run_eip_tests():
    eip_tests_path = SUITE_DIR / "subroutine" / "eip_tests.py"
    spec = importlib.util.spec_from_file_location("hardfork_support_eip_tests", eip_tests_path)
    if spec is None or spec.loader is None:
        raise RuntimeError(f"Failed to load {eip_tests_path}")
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod.run_eip_tests


run_eip_tests = _load_run_eip_tests()


def _tail_file(path: Path, max_lines: int = 120) -> str:
    try:
        lines = path.read_text(errors="replace").splitlines()
    except OSError as exc:
        return f"<failed to read {path}: {exc}>"
    return "\n".join(lines[-max_lines:])


def _ensure_nonce_overflow_stub_patch(project_dir: Path) -> bool:
    test_path = project_dir / NONCE_OVERFLOW_TEST
    text = test_path.read_text()
    if NONCE_OVERFLOW_PATCH_MARKER in text:
        return True

    if NONCE_OVERFLOW_CONSTANTS_ORIGINAL not in text or NONCE_OVERFLOW_DEPLOY_ORIGINAL not in text:
        raise RuntimeError(f"execution-specs nonce-overflow patch does not match {test_path}")

    text = text.replace(NONCE_OVERFLOW_CONSTANTS_ORIGINAL, NONCE_OVERFLOW_CONSTANTS_PATCHED)
    text = text.replace(NONCE_OVERFLOW_DEPLOY_ORIGINAL, NONCE_OVERFLOW_DEPLOY_PATCHED)
    test_path.write_text(text)
    return True


def _restore_nonce_overflow_stub_patch(project_dir: Path) -> None:
    test_path = project_dir / NONCE_OVERFLOW_TEST
    text = test_path.read_text()
    if NONCE_OVERFLOW_PATCH_MARKER not in text:
        return

    text = text.replace(NONCE_OVERFLOW_CONSTANTS_PATCHED, NONCE_OVERFLOW_CONSTANTS_ORIGINAL)
    text = text.replace(NONCE_OVERFLOW_DEPLOY_PATCHED, NONCE_OVERFLOW_DEPLOY_ORIGINAL)
    test_path.write_text(text)


def _execution_specs_config_for_label(execution_cfg: dict, label: str) -> dict:
    label_cfg = dict(execution_cfg)
    endpoint_overrides = execution_cfg.get("endpoint_overrides", {})
    if isinstance(endpoint_overrides, dict):
        label_cfg.update(endpoint_overrides.get(label, {}))
    return label_cfg


def _run_execution_specs(label: str, url: str, w3, private_key: str, cfg: dict) -> TestResult:
    execution_cfg = _execution_specs_config_for_label(cfg.get("execution_specs", {}), label)
    paths = [str(path) for path in execution_cfg.get("paths", [])]
    test_name = f"execution-specs-{execution_cfg.get('fork', 'Berlin')}"

    if not paths:
        return TestResult(
            name=test_name,
            passed=False,
            message="[execution_specs].paths must not be empty",
        )
    if not private_key:
        return TestResult(
            name=test_name,
            passed=False,
            message="private_key is required for execution-specs execute remote",
        )

    project_dir = REPO_ROOT / str(
        execution_cfg.get("project_dir", "test/api-tests/execution-specs")
    )
    if not project_dir.is_dir():
        return TestResult(
            name=test_name,
            passed=False,
            message=(
                f"execution-specs checkout not found: {project_dir}; "
                "initialize submodules with git submodule update --init --recursive"
            ),
        )

    nonce_patch_applied = False
    address_stubs = execution_cfg.get("address_stubs")
    if address_stubs and {
        NONCE_OVERFLOW_CREATE_STUB,
        NONCE_OVERFLOW_CREATE2_STUB,
    }.issubset(address_stubs):
        try:
            nonce_patch_applied = _ensure_nonce_overflow_stub_patch(project_dir)
        except (OSError, RuntimeError) as exc:
            return TestResult(
                name=test_name,
                passed=False,
                message=f"failed to patch execution-specs nonce-overflow stubs: {exc}",
            )

    fork = str(execution_cfg.get("fork", "Berlin"))
    timeout_sec = int(execution_cfg.get("timeout_sec", 1200))
    cmd = [
        "uv", "run", "--locked", "--project", ".",
        "execute", "remote",
        "--fork", fork,
        "--rpc-endpoint", url,
        "--chain-id", str(w3.eth.chain_id),
        "--tx-wait-timeout", str(int(execution_cfg.get("tx_wait_timeout", 120))),
        "--max-tx-per-batch", str(int(execution_cfg.get("max_tx_per_batch", 50))),
        "--default-max-fee-per-blob-gas",
        str(int(execution_cfg.get("default_max_fee_per_blob_gas", 1))),
    ]

    max_gas_per_test = execution_cfg.get("max_gas_per_test")
    if max_gas_per_test is not None:
        cmd.extend(["--max-gas-per-test", str(int(max_gas_per_test))])

    eoa_start = execution_cfg.get("eoa_start")
    if eoa_start is not None:
        cmd.extend(["--eoa-start", str(int(eoa_start))])

    if address_stubs:
        cmd.extend([
            "--address-stubs",
            json.dumps(address_stubs, separators=(",", ":")),
        ])

    cmd.extend(str(arg) for arg in execution_cfg.get("extra_args", []))
    cmd.extend(paths)

    log_dir = SUITE_DIR / "logs"
    log_dir.mkdir(parents=True, exist_ok=True)
    safe_label = label.replace("/", "_").replace(":", "_")
    log_path = log_dir / f"execution-specs-{safe_label}.log"

    logger.info(
        "Running execution-specs against %s (%s, fork=%s, paths=%s)",
        label, url, fork, paths,
    )
    run_env = os.environ.copy()
    run_env["RPC_SEED_KEY"] = private_key
    run_env.pop("UV_EXCLUDE_NEWER", None)
    run_env.pop("UV_EXCLUDE_NEWER_PACKAGE", None)
    run_env.pop("VIRTUAL_ENV", None)
    run_env["UV_NO_CONFIG"] = "1"

    try:
        with open(log_path, "w") as log_fd:
            log_fd.write("$ " + " ".join(cmd) + "\n\n")
            log_fd.flush()
            try:
                result = subprocess.run(
                    cmd,
                    cwd=project_dir,
                    stdout=log_fd,
                    stderr=subprocess.STDOUT,
                    text=True,
                    timeout=timeout_sec,
                    check=False,
                    env=run_env,
                )
            except FileNotFoundError:
                return TestResult(
                    name=test_name,
                    passed=False,
                    message="uv executable not found; install uv to run execution-specs tests",
                )
            except subprocess.TimeoutExpired:
                return TestResult(
                    name=test_name,
                    passed=False,
                    message=(
                        f"execution-specs timed out after {timeout_sec}s; "
                        f"log tail:\n{_tail_file(log_path)}"
                    ),
                    details={"log": str(log_path)},
                )
    finally:
        if nonce_patch_applied:
            try:
                _restore_nonce_overflow_stub_patch(project_dir)
            except OSError as exc:
                logger.warning("Failed to restore execution-specs nonce-overflow patch: %s", exc)

    if result.returncode != 0:
        return TestResult(
            name=test_name,
            passed=False,
            message=(
                f"execution-specs failed with rc={result.returncode}; "
                f"log tail:\n{_tail_file(log_path)}"
            ),
            details={"log": str(log_path)},
        )

    return TestResult(
        name=test_name,
        passed=True,
        message=f"passed; log: {log_path}",
        details={"log": str(log_path)},
    )


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
    execution_cfg = cfg.get("execution_specs", {})
    execution_specs_enabled = bool(execution_cfg.get("enabled", False))
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

    if execution_specs_enabled:
        logger.info("Running execution-specs workload")
        for label, url in env.rpc_urls.items():
            label_execution_cfg = _execution_specs_config_for_label(execution_cfg, label)
            test_name = f"execution-specs-{label_execution_cfg.get('fork', 'Berlin')}"
            if not bool(label_execution_cfg.get("enabled", True)):
                tr = TestResult(
                    name=test_name,
                    passed=True,
                    message=f"skipped for endpoint {label} by execution_specs endpoint override",
                )
            else:
                w3 = env.web3s.get(label)
                if w3 is None:
                    tr = TestResult(
                        name=test_name,
                        passed=False,
                        message=f"No Web3 connection for endpoint {label}",
                    )
                else:
                    tr = _run_execution_specs(label, url, w3, private_key, cfg)

            by_label_and_name.setdefault(label, {})[tr.name] = tr
            logger.info(
                "  %s: %s — %s", tr.name, "PASS" if tr.passed else "FAIL", tr.message,
            )

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
