"""Shared utilities and constants for the BITE/BITE2 compatibility test suite.

Imported by both test_phase1_bite.py and test_bite_compat.py.
The underscore prefix prevents pytest from collecting this as a test module.
"""

import json
import logging
import os
import shlex
import subprocess
import sys
import time
from pathlib import Path
from typing import Optional

import pytest
from web3 import Web3

logger = logging.getLogger("bite-compat.utils")

SUITE_DIR = Path(__file__).resolve().parent
REPO_ROOT  = SUITE_DIR.parent.parent.parent

BITE_MAGIC_ADDRESS = Web3.to_checksum_address(
    "0x42495445204d452049276d20454e435259505444"
)
BITE_CIPHERTEXT_MIN_LEN = 276  # BITE_ENCRYPTED_AES_KEY_LEN + BITE_TE_RANDOM_LEN + ADDRESS_SIZE

TS_ENCRYPT_TRANSFER_SCRIPT = SUITE_DIR / "scripts" / "make_transaction_bite.ts"
TS_SIMPLE_SECRET_SCRIPT    = SUITE_DIR / "scripts" / "make_transaction_bite2.ts"


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
    extra_args: Optional[list[str]] = None,
    extra_env: Optional[dict[str, str]] = None,
) -> dict:
    """
    Run a TypeScript helper script and return the parsed JSON payload dict.
    Calls ``pytest.fail`` on any error.

    ``extra_args`` are appended after the script path (e.g. ``["--deploy"]``).
    ``extra_env`` is merged into the subprocess environment.
    """
    if not script_path.is_file():
        pytest.fail(f"TS script not found: {script_path}")

    cmd_str = bite_cfg.get("bite_ts_command", "bun run")
    cmd = shlex.split(cmd_str) + [str(script_path)] + (extra_args or [])
    env = os.environ.copy()
    env["BITE_OUTPUT_JSON"] = "1"
    env["BITE_COMPAT_TOML"] = str(SUITE_DIR / "bite-compat.toml")
    if private_key:
        env["BITE_PRIVATE_KEY"] = private_key
    if extra_env:
        env.update(extra_env)

    try:
        cp = subprocess.run(
            cmd,
            cwd=REPO_ROOT,
            env=env,
            capture_output=True,
            text=True,
            timeout=max(30, timeout_s),
        )
    except FileNotFoundError:
        pytest.fail(
            f"TS runtime not found: {cmd[0]!r}. "
            "Set [bite_compat].bite_ts_command (e.g. 'bun run')."
        )
    except subprocess.TimeoutExpired:
        pytest.fail("TS subprocess timed out")

    if cp.stdout:
        print(cp.stdout, end="")
    if cp.stderr:
        print(cp.stderr, end="", file=sys.stderr)

    if cp.returncode != 0:
        pytest.fail(f"TS subprocess failed (rc={cp.returncode})")

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


def _bite_provider_env(bite_cfg: dict, extra: Optional[dict] = None) -> dict:
    """Return an env dict with BITE_PROVIDER_URL pointing at the BITE node port."""
    port = bite_cfg.get("bite_http_port", 4232)
    env: dict = {"BITE_PROVIDER_URL": f"http://127.0.0.1:{port}"}
    if extra:
        env.update(extra)
    return env


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
